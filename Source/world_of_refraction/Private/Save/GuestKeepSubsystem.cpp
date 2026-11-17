// GuestKeepSubsystem.cpp

#include "Save/GuestKeepSubsystem.h"

#include "Character/PlayerBuild.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "UI/Menus/TitleScreenModel.h"
#include "UObject/Package.h"
#include "WoRLog.h"

namespace
{
	FSavedPartyMember *FindOwnCharacter(FWoRSaveData &Own)
	{
		return Own.Party.Members.FindByPredicate([](const FSavedPartyMember &Member) { return Member.Picks.IsSet(); });
	}

	bool SamePicksCharacter(const FPlayerPicks &A, const FPlayerPicks &B)
	{
		// D-228: the name is now the kind name, so two guests of one kind differ only by their player name (GuestKey).
		const bool bSameKey = A.GuestKey.IsEmpty() || B.GuestKey.IsEmpty() || A.GuestKey == B.GuestKey;
		return bSameKey && A.Name == B.Name && A.Class == B.Class && A.Element == B.Element && A.bBrokenDarkness == B.bBrokenDarkness;
	}

	FAutoConsoleCommandWithWorld GPrintGuestKeep(
		TEXT("wor.Coop.PrintGuestKeep"),
		TEXT("B-563: the character this player joined a friend's game with, and what is counted of its wallet."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
			const UGuestKeepSubsystem *Keep = GI ? GI->GetSubsystem<UGuestKeepSubsystem>() : nullptr;
			UE_LOG(LogWoR, Display, TEXT("%s"), Keep ? *Keep->GetGuestKeepString() : TEXT("[GuestKeep] no game instance"));
		}));
}

bool UGuestKeepSubsystem::CanFoldWallet(const FSavedWallet &Counted, const FSavedWallet &Now)
{
	return !Now.Balances.IsEmpty() || Counted.Balances.IsEmpty();
}

FSavedWallet UGuestKeepSubsystem::AddWalletDelta(const FSavedWallet &Own, const FSavedWallet &Before, const FSavedWallet &Now)
{
	const auto AmountIn = [](const FSavedWallet &Wallet, const FSavedBalance &Key)
	{
		const FSavedBalance *Found = Wallet.Balances.FindByPredicate([&Key](const FSavedBalance &Entry)
		{
			return Entry.Currency == Key.Currency && Entry.SubKey == Key.SubKey;
		});
		return Found ? Found->Amount : 0;
	};
	FSavedWallet Out = Own;
	Out.bHasWallet = true;
	TArray<FSavedBalance> Keys = Before.Balances;
	Keys.Append(Now.Balances);
	for (const FSavedBalance &Key : Keys)
	{
		FSavedBalance *Mine = Out.Balances.FindByPredicate([&Key](const FSavedBalance &Entry)
		{
			return Entry.Currency == Key.Currency && Entry.SubKey == Key.SubKey;
		});
		if (!Mine)
		{
			Mine = &Out.Balances.AddDefaulted_GetRef();
			Mine->Currency = Key.Currency;
			Mine->SubKey = Key.SubKey;
		}
		// Read from Own, not Out: a key in both lists is met twice and comes out the same.
		Mine->Amount = FMath::Max(0, AmountIn(Own, Key) + AmountIn(Now, Key) - AmountIn(Before, Key));
	}
	Out.Balances.RemoveAll([](const FSavedBalance &Balance) { return Balance.Amount <= 0; });
	return Out;
}

const FSavedPartyMember *UGuestKeepSubsystem::FindGuestMember(const FWoRSaveData &HostCopy, const FPlayerPicks &Picks)
{
	return HostCopy.Party.Members.FindByPredicate([&Picks](const FSavedPartyMember &Member)
	{
		return Member.Picks.bGuest && SamePicksCharacter(Member.Picks, Picks);
	});
}

bool UGuestKeepSubsystem::FoldKeptLevels(FWoRSaveData &Own, const FSavedPartyMember &Member)
{
	FSavedPartyMember *Mine = FindOwnCharacter(Own);
	if (!Mine || !Member.bHasKeptLevels)
	{
		return false;
	}
	// D-237: the last run's levels replace the ones kept before, as in the guest's own game.
	Mine->KeptMind = Member.KeptMind;
	Mine->KeptBody = Member.KeptBody;
	Mine->KeptSpirit = Member.KeptSpirit;
	Mine->bHasKeptLevels = true;
	return true;
}

FName UGuestKeepSubsystem::GetSavedMemberKey(const FSavedPartyMember &Member)
{
	// UPoolSubsystem::GetCharacterKey on the built or loaded character: its object path, #MemberId for a later copy.
	const FString Path = Member.Picks.IsSet()
		? FString::Printf(TEXT("%s.%s"), *GetTransientPackage()->GetPathName(), *WoRPlayerBuild::GetBuiltName(Member.Picks).ToString())
		: Member.CharacterData.Path.ToString();
	return FName(Member.bKeyedByMember ? FString::Printf(TEXT("%s#%s"), *Path, *Member.MemberId.ToString(EGuidFormats::Digits)) : Path);
}

int32 UGuestKeepSubsystem::FoldGearAndSkills(FWoRSaveData &Own, const FWoRSaveData &HostCopy, FName GuestKey, FName OwnKey)
{
	int32 Moved = 0;
	// D-219, D-221: found gear the run let the guest keep is the host's hub gear owned by the guest's member. The host
	// built the guest fresh from its picks, so the rest it owns there is its starting kit, which its own save has.
	for (const FGuid &GearID : HostCopy.Pool.HubGear)
	{
		const FSavedWearer *Owner = HostCopy.Pool.Owners.FindByPredicate([&GearID](const FSavedWearer &Entry) { return Entry.GearID == GearID; });
		const FSavedOwnedItem *Piece = Owner && Owner->Character == GuestKey
			? HostCopy.Pool.Gear.FindByPredicate([&GearID](const FSavedOwnedItem &Entry) { return Entry.InstanceID == GearID; })
			: nullptr;
		if (!Piece || Own.Pool.Gear.ContainsByPredicate([&GearID](const FSavedOwnedItem &Entry) { return Entry.InstanceID == GearID; }))
		{
			continue; // not the guest's, or kept by an earlier copy
		}
		Own.Pool.Gear.Add(*Piece);
		FSavedWearer &Owned = Own.Pool.Owners.AddDefaulted_GetRef();
		Owned.GearID = GearID;
		Owned.Character = OwnKey;
		Own.Pool.HubGear.AddUnique(GearID);
		++Moved;
	}
	// A kept found evolution is hub gear in the evolution list: the guest's when it owns it or one of its pieces holds
	// it (a socket in its starting kit too: the evolution comes over loose then).
	TSet<FGuid> SocketedInGuests;
	for (const FSavedOwnedItem &Piece : HostCopy.Pool.Gear)
	{
		const FSavedWearer *Owner = HostCopy.Pool.Owners.FindByPredicate([&Piece](const FSavedWearer &Entry) { return Entry.GearID == Piece.InstanceID; });
		if (Owner && Owner->Character == GuestKey && Piece.Gear.Attached.IsEvolution())
		{
			SocketedInGuests.Add(Piece.Gear.Attached.Evolution.InstanceID);
		}
	}
	for (const FGuid &GearID : HostCopy.Pool.HubGear)
	{
		const FSavedWearer *Owner = HostCopy.Pool.Owners.FindByPredicate([&GearID](const FSavedWearer &Entry) { return Entry.GearID == GearID; });
		const bool bGuests = (Owner && Owner->Character == GuestKey) || SocketedInGuests.Contains(GearID);
		const FSavedOwnedItem *Evolution = bGuests
			? HostCopy.Pool.Evolutions.FindByPredicate([&GearID](const FSavedOwnedItem &Entry) { return Entry.InstanceID == GearID; })
			: nullptr;
		if (!Evolution || Own.Pool.Evolutions.ContainsByPredicate([&GearID](const FSavedOwnedItem &Entry) { return Entry.InstanceID == GearID; }))
		{
			continue;
		}
		Own.Pool.Evolutions.Add(*Evolution);
		FSavedWearer &Owned = Own.Pool.Owners.AddDefaulted_GetRef();
		Owned.GearID = GearID;
		Owned.Character = OwnKey;
		Own.Pool.HubGear.AddUnique(GearID);
		++Moved;
	}
	// D-224: the skills the guest's member learned there (it started with none), added to its own character's.
	const FSavedCharacterSkills *Learned = HostCopy.Pool.CharacterSkills.FindByPredicate([GuestKey](const FSavedCharacterSkills &Entry) { return Entry.Character == GuestKey; });
	if (Learned)
	{
		FSavedCharacterSkills *Mine = Own.Pool.CharacterSkills.FindByPredicate([OwnKey](const FSavedCharacterSkills &Entry) { return Entry.Character == OwnKey; });
		if (!Mine)
		{
			Mine = &Own.Pool.CharacterSkills.AddDefaulted_GetRef();
			Mine->Character = OwnKey;
		}
		const auto AddMissing = [&Moved](TArray<FSavedOwnedItem> &Into, const TArray<FSavedOwnedItem> &From)
		{
			for (const FSavedOwnedItem &Skill : From)
			{
				if (!Into.ContainsByPredicate([&Skill](const FSavedOwnedItem &Entry) { return Entry.Def.Path == Skill.Def.Path && Entry.Def.Id == Skill.Def.Id; }))
				{
					Into.Add(Skill);
					++Moved;
				}
			}
		};
		AddMissing(Mine->Spells, Learned->Spells);
		AddMissing(Mine->Abilities, Learned->Abilities);
	}
	return Moved;
}

FWoRSaveData UGuestKeepSubsystem::MakeNewCharacterSave(const FPlayerPicks &Picks)
{
	FWoRSaveData Data;
	FSavedPartyMember &Leader = Data.Party.Members.AddDefaulted_GetRef();
	Leader.Picks = Picks;
	Leader.Picks.bGuest = false; // its own game builds it as its player
	Leader.Picks.bRecruit = false;
	Leader.bPlayerControlled = true;
	Leader.MemberId = FGuid::NewGuid();
	Data.Forms.BaseForm = static_cast<uint8>(Picks.Body);
	return Data;
}

bool UGuestKeepSubsystem::FoldIntoSlot(TFunctionRef<bool(FWoRSaveData &)> Fold)
{
	USaveSubsystem *Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	FWoRSaveData Data;
	double PlaySeconds = 0.0;
	if (!Saves || !Saves->ReadSlot(JoinedSlot, Data, PlaySeconds))
	{
		UE_LOG(LogWoR, Warning, TEXT("[GuestKeep] Slot %d not read: nothing kept"), JoinedSlot);
		return false;
	}
	return Fold(Data) && Saves->WriteSlotData(JoinedSlot, Data);
}

bool UGuestKeepSubsystem::BeginJoin(int32 Slot, const FPlayerPicks &Picks, const UCurrencyComponent *Wallet)
{
	FoldWallet(Wallet, /*bLeaving=*/false); // a delta owed to the character joined before goes to it
	USaveSubsystem *Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	if (!Saves)
	{
		return false;
	}
	if (Slot == INDEX_NONE)
	{
		// Q-561 (Crown 2026-11-17): a new character made to join is kept like any other.
		Slot = Saves->FindFreeSlot(WoRTitleScreen::GetMaxSlots());
		FWoRSaveData Fresh = MakeNewCharacterSave(Picks);
		const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
		Fresh.Party.Members[0].PawnClass = PartySession ? FSoftClassPath(PartySession->DefaultSoloPawnClass.ToSoftObjectPath().ToString()) : FSoftClassPath();
		if (Slot == INDEX_NONE || !Saves->WriteSlotData(Slot, Fresh))
		{
			UE_LOG(LogWoR, Warning, TEXT("[GuestKeep] No slot made for the new character %s: %s"), *Picks.Name,
				   Slot == INDEX_NONE ? TEXT("every slot is in use") : TEXT("the write failed"));
			return false; // the character joined before (if any) stays the one kept
		}
		UE_LOG(LogWoR, Log, TEXT("[GuestKeep] New character %s saved to slot %d"), *Picks.Name, Slot);
	}
	JoinedSlot = Slot;
	JoinedPicks = Picks;
	UE_LOG(LogWoR, Log, TEXT("[GuestKeep] Joining as %s (slot %d): what it earns goes back there"), *Picks.Name, Slot);
	return true;
}

void UGuestKeepSubsystem::NoteReportedWallet(const FSavedWallet &Reported, const UObject *Connection)
{
	if (bHasCountedWallet && VisitConnection.Get() == Connection)
	{
		return; // a seamless travel inside the visit: the host applied the report once (bReportedWalletApplied)
	}
	UE_CLOG(IsJoined(), LogWoR, Log, TEXT("[GuestKeep] A new visit: %s from the last one is let go"), *JoinedPicks.Name);
	JoinedSlot = INDEX_NONE;
	JoinedPicks = FPlayerPicks();
	CountedWallet = Reported;
	bHasCountedWallet = true;
	VisitConnection = Connection;
}

void UGuestKeepSubsystem::FoldWallet(const UCurrencyComponent *Wallet, bool bLeaving)
{
	if (IsJoined() && bHasCountedWallet && Wallet)
	{
		const FSavedWallet Now = USaveSubsystem::CaptureWallet(Wallet);
		if (!CanFoldWallet(CountedWallet, Now))
		{
			UE_LOG(LogWoR, Log, TEXT("[GuestKeep] Wallet not folded: it has not replicated yet"));
		}
		else if (FoldIntoSlot([this, &Now](FWoRSaveData &Data)
		{
			Data.Wallet = AddWalletDelta(Data.Wallet, CountedWallet, Now);
			return true;
		}))
		{
			CountedWallet = Now;
			UE_LOG(LogWoR, Log, TEXT("[GuestKeep] Wallet earnings kept in slot %d"), JoinedSlot);
		}
	}
	if (bLeaving)
	{
		UE_CLOG(IsJoined(), LogWoR, Log, TEXT("[GuestKeep] Left the friend's game as %s"), *JoinedPicks.Name);
		JoinedSlot = INDEX_NONE;
		JoinedPicks = FPlayerPicks();
		bHasCountedWallet = false; // the next visit reports again
		VisitConnection.Reset();
	}
}

void UGuestKeepSubsystem::FoldCheckpoint(const FWoRSaveData &HostCopy, const UCurrencyComponent *Wallet)
{
	if (!IsJoined())
	{
		return;
	}
	FoldWallet(Wallet, /*bLeaving=*/false);
	const FSavedPartyMember *Member = FindGuestMember(HostCopy, JoinedPicks);
	if (HostCopy.Run.bHasRun || !Member)
	{
		return; // a run still open settles nothing yet (D-219, D-237); no member: not seated
	}
	int32 Moved = 0;
	const FName GuestKey = GetSavedMemberKey(*Member);
	if (FoldIntoSlot([Member, &HostCopy, GuestKey, &Moved](FWoRSaveData &Data)
	{
		const bool bLevels = FoldKeptLevels(Data, *Member);
		const FSavedPartyMember *Mine = FindOwnCharacter(Data);
		Moved = Mine ? FoldGearAndSkills(Data, HostCopy, GuestKey, GetSavedMemberKey(*Mine)) : 0;
		return bLevels || Moved > 0;
	}))
	{
		UE_LOG(LogWoR, Log, TEXT("[GuestKeep] To slot %d: kept levels %s (Mind %d, Body %d, Spirit %d); %d gear piece(s) and skill(s)"),
			   JoinedSlot, Member->bHasKeptLevels ? TEXT("yes") : TEXT("none"), Member->KeptMind, Member->KeptBody, Member->KeptSpirit, Moved);
		// The slot in play: its party and pool in memory are captured on the next save, so they take it too.
		USaveSubsystem *Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
		const bool bInPlay = Saves && Saves->GetActiveSlot() == JoinedSlot;
		UPoolSubsystem *Pool = bInPlay && Moved > 0 ? GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
		if (Pool)
		{
			Pool->ResetStore(); // as a return to the own save does (B-234d)
			USaveSubsystem::RestorePool(Saves->GetData().Pool, *Pool);
		}
		UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = bInPlay && Member->bHasKeptLevels && PartySession ? PartySession->GetLocalParty() : nullptr;
		FPartyMember *Live = Party ? Party->Members.FindByPredicate([this](const FPartyMember &Entry)
		{
			return !Entry.Picks.bGuest && SamePicksCharacter(Entry.Picks, JoinedPicks);
		}) : nullptr;
		if (Live)
		{
			Live->KeptMind = Member->KeptMind;
			Live->KeptBody = Member->KeptBody;
			Live->KeptSpirit = Member->KeptSpirit;
			Live->bHasKeptLevels = true;
		}
	}
}

FString UGuestKeepSubsystem::GetGuestKeepString() const
{
	if (!IsJoined())
	{
		return TEXT("[GuestKeep] Not joined to a friend's game as one of this player's characters");
	}
	return FString::Printf(TEXT("[GuestKeep] Joined as %s (%s, slot %d); wallet counted from: %s (%d balance(s))"),
						   *JoinedPicks.Name, *UEnum::GetValueAsString(JoinedPicks.Class), JoinedSlot,
						   bHasCountedWallet ? TEXT("the connect report") : TEXT("nothing yet"), CountedWallet.Balances.Num());
}
