// PartySessionSubsystem.cpp

#include "Party/PartySessionSubsystem.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Party/Party.h"

void UPartySessionSubsystem::Deinitialize()
{
	Parties.Empty();
	Super::Deinitialize();
}

UParty *UPartySessionSubsystem::GetLocalParty() const
{
	return Parties.IsValidIndex(0) ? Parties[0].Get() : nullptr;
}

UParty *UPartySessionSubsystem::CreateSoloParty(APlayerController *Leader, TSoftClassPtr<ACombatCharacter> LeaderPawnClass)
{
	// Re-entry (hub reload) rebinds rather than rebuilds — a party assembled
	// earlier this session must survive returning to the hub.
	if (UParty *Existing = GetLocalParty())
	{
		RebindLeader(Leader);
		UE_LOG(LogTemp, Log, TEXT("[PartySession] Local party already exists (%d members) - leader rebound"),
			   Existing->GetMemberCount());
		return Existing;
	}

	// ⚠️ Outer is the GameInstance, NOT the world: a world-outered UObject is
	// destroyed by OpenLevel and the party would empty on the first transition.
	UParty *NewParty = NewObject<UParty>(GetGameInstance());
	NewParty->Leader = Leader;
	NewParty->DisplayName = MakeDefaultDisplayName(Leader);
	Parties.Add(NewParty);

	if (!LeaderPawnClass.IsNull())
	{
		InviteMember(LeaderPawnClass);
	}

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Created solo party '%s' (%d members)"),
		   *NewParty->DisplayName.ToString(), NewParty->GetMemberCount());
	return NewParty;
}

UParty *UPartySessionSubsystem::EnsureLocalParty(APlayerController *PC, TSoftClassPtr<ACombatCharacter> PawnClass)
{
	if (UParty *Existing = GetLocalParty())
	{
		RebindLeader(PC);
		return Existing;
	}

	// Resolution chain. PC's current pawn is the common case: the player is
	// standing in the world as their character when this fires.
	TSoftClassPtr<ACombatCharacter> Resolved = PawnClass;
	if (Resolved.IsNull() && PC)
	{
		if (const APawn *CurrentPawn = PC->GetPawn())
		{
			Resolved = TSoftClassPtr<ACombatCharacter>(CurrentPawn->GetClass()->GetPathName());
		}
	}
	if (Resolved.IsNull())
	{
		Resolved = DefaultSoloPawnClass;
	}
	if (Resolved.IsNull())
	{
		UE_LOG(LogTemp, Error,
			   TEXT("[PartySession] Cannot create Trial Party - no pawn class from argument, PC0 pawn, or "
					"DefaultSoloPawnClass (author it in DefaultGame.ini)"));
		return nullptr;
	}

	UParty *Party = CreateSoloParty(PC, Resolved);
	UE_LOG(LogTemp, Log, TEXT("[PartySession] Trial Party lazily created with %d member(s) from %s"),
		   Party ? Party->GetMemberCount() : 0, *Resolved.ToString());
	return Party;
}

bool UPartySessionSubsystem::IsTrialPartyMember(const AActor *Actor) const
{
	const UParty *Party = GetLocalParty();
	if (!Party || !Actor)
	{
		return false;
	}

	// CLASS comparison, not instance — see the header's POC-limit note.
	const FSoftClassPath ActorClass(Actor->GetClass()->GetPathName());
	for (const FPartyMember &Member : Party->Members)
	{
		if (Member.PawnClass.ToSoftObjectPath() == ActorClass)
		{
			return true;
		}
	}
	return false;
}

bool UPartySessionSubsystem::InviteMember(TSoftClassPtr<ACombatCharacter> PawnClass)
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] InviteMember with no local party - call CreateSoloParty first"));
		return false;
	}

	if (Party->IsFull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] Party full (%d) - invite rejected"),
			   PartyConstants::MAX_PARTY_MEMBERS);
		return false;
	}

	if (PawnClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] InviteMember with null pawn class"));
		return false;
	}

	UCharacterData *Data = ResolveCharacterData(PawnClass);
	if (!Data)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] %s carries no CharacterData - invite rejected"),
			   *PawnClass.ToString());
		return false;
	}

	FPartyMember Member;
	Member.PawnClass = PawnClass;
	Member.CharacterData = Data;
	Party->Members.Add(Member);

	UE_LOG(LogTemp, Log, TEXT("[PartySession] Invited %s (slot %d of %d)"),
		   *Data->Name, Party->GetMemberCount() - 1, PartyConstants::MAX_PARTY_MEMBERS);
	return true;
}

bool UPartySessionSubsystem::DismissMember(int32 SlotIndex)
{
	UParty *Party = GetLocalParty();
	if (!Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PartySession] DismissMember: invalid slot %d"), SlotIndex);
		return false;
	}

	Party->Members.RemoveAt(SlotIndex);
	UE_LOG(LogTemp, Log, TEXT("[PartySession] Dismissed slot %d (%d remaining)"),
		   SlotIndex, Party->GetMemberCount());
	return true;
}

void UPartySessionSubsystem::SetDisplayName(const FText &NewName)
{
	if (UParty *Party = GetLocalParty())
	{
		Party->DisplayName = NewName;
	}
}

void UPartySessionSubsystem::RebindLeader(APlayerController *Leader)
{
	UParty *Party = GetLocalParty();
	if (!Party)
	{
		return;
	}

	Party->Leader = Leader;
	// Only re-derive the name while it is still the generated default — an
	// explicit SetDisplayName must not be clobbered by a level transition.
	if (Party->DisplayName.IsEmpty())
	{
		Party->DisplayName = MakeDefaultDisplayName(Leader);
	}
}

UCharacterData *UPartySessionSubsystem::ResolveCharacterData(const TSoftClassPtr<ACombatCharacter> &PawnClass)
{
	UClass *Loaded = PawnClass.LoadSynchronous();
	if (!Loaded)
	{
		return nullptr;
	}

	const ACombatCharacter *DefaultPawn = Loaded->GetDefaultObject<ACombatCharacter>();
	if (!DefaultPawn || !DefaultPawn->CharacterDataComponent)
	{
		return nullptr;
	}
	return DefaultPawn->CharacterDataComponent->CharacterData;
}

FText UPartySessionSubsystem::MakeDefaultDisplayName(const APlayerController *Leader)
{
	// "<Leader>'s Party" when a player name exists. PlayerState is often null this
	// early in a solo session, so fall back to a neutral label rather than
	// producing "'s Party".
	if (Leader && Leader->PlayerState)
	{
		const FString PlayerName = Leader->PlayerState->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			return FText::Format(NSLOCTEXT("Party", "NamedPartyName", "{0}'s Party"),
								 FText::FromString(PlayerName));
		}
	}
	return NSLOCTEXT("Party", "DefaultPartyName", "Party");
}
