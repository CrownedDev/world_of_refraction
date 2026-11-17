// GuestKeepSpec.cpp
// B-563 (Q-561, Crown 2026-11-17): a co-op guest keeps what it earns in its own save. The folds, headless. Unproven
// here: the hooks on a real client (the checkpoint copy arriving, the slot written; two-player PIE, sheet B-563).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Misc/AutomationTest.h"
#include "Pool/PoolSubsystem.h"
#include "UObject/Package.h"
#include "Save/GuestKeepSubsystem.h"
#include "Save/WoRSaveTypes.h"

namespace GuestKeepSpecValues
{
	constexpr uint8 PRISMS = 1; // any two currency keys do
	constexpr uint8 ESSENCE = 2;
}

BEGIN_DEFINE_SPEC(FGuestKeepSpec, "WoR.Coop.GuestKeep", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static FSavedWallet MakeWallet(int32 Prisms, int32 Essence)
	{
		FSavedWallet Wallet;
		Wallet.bHasWallet = true;
		for (const TPair<uint8, int32> &Entry : {TPair<uint8, int32>(GuestKeepSpecValues::PRISMS, Prisms), TPair<uint8, int32>(GuestKeepSpecValues::ESSENCE, Essence)})
		{
			if (Entry.Value > 0)
			{
				FSavedBalance &Balance = Wallet.Balances.AddDefaulted_GetRef();
				Balance.Currency = Entry.Key;
				Balance.Amount = Entry.Value;
			}
		}
		return Wallet;
	}
	static int32 AmountOf(const FSavedWallet &Wallet, uint8 Currency)
	{
		const FSavedBalance *Found = Wallet.Balances.FindByPredicate([Currency](const FSavedBalance &Balance) { return Balance.Currency == Currency; });
		return Found ? Found->Amount : 0;
	}
	static FPlayerPicks MakePicks()
	{
		FPlayerPicks Picks;
		Picks.Name = TEXT("Rin");
		Picks.Class = ECharacterClass::Caster;
		Picks.Element = ESpellElement::Water;
		return Picks;
	}
END_DEFINE_SPEC(FGuestKeepSpec)

void FGuestKeepSpec::Define()
{
	It("adds only what the wallet earned in the friend's game to the joined character's balances", [this]()
	{
		const FSavedWallet Own = MakeWallet(100, 5);
		const FSavedWallet AtConnect = MakeWallet(40, 0); // another character's balance, reported at connect
		const FSavedWallet Now = MakeWallet(70, 3);
		const FSavedWallet Kept = UGuestKeepSubsystem::AddWalletDelta(Own, AtConnect, Now);
		TestEqual(TEXT("Prisms: 100 + 30 earned"), AmountOf(Kept, GuestKeepSpecValues::PRISMS), 130);
		TestEqual(TEXT("essence: 5 + 3 earned"), AmountOf(Kept, GuestKeepSpecValues::ESSENCE), 8);
		const FSavedWallet Spent = UGuestKeepSubsystem::AddWalletDelta(MakeWallet(10, 0), MakeWallet(40, 0), MakeWallet(0, 0));
		TestEqual(TEXT("spending more than it had never goes below zero"), AmountOf(Spent, GuestKeepSpecValues::PRISMS), 0);
		TestFalse(TEXT("a wallet not replicated yet (empty) is not folded"), UGuestKeepSubsystem::CanFoldWallet(AtConnect, FSavedWallet()));
		TestTrue(TEXT("a real wallet is"), UGuestKeepSubsystem::CanFoldWallet(AtConnect, Now));
	});

	It("finds the guest's member in the host's save by its picks, not the host's own player", [this]()
	{
		FWoRSaveData Host;
		FSavedPartyMember &HostPlayer = Host.Party.Members.AddDefaulted_GetRef();
		HostPlayer.Picks = MakePicks(); // the host made a character of the same name and class
		FSavedPartyMember &Guest = Host.Party.Members.AddDefaulted_GetRef();
		Guest.Picks = MakePicks();
		Guest.Picks.bGuest = true;
		TestTrue(TEXT("the guest member"), UGuestKeepSubsystem::FindGuestMember(Host, MakePicks()) == &Host.Party.Members[1]);
		FPlayerPicks Other = MakePicks();
		Other.Element = ESpellElement::Fire;
		TestNull(TEXT("another character: none"), UGuestKeepSubsystem::FindGuestMember(Host, Other));

		// D-228: two guests of the same kind share a name; the joining player's name tells them apart.
		Guest.Picks.GuestKey = TEXT("Player 2");
		FSavedPartyMember &SecondGuest = Host.Party.Members.AddDefaulted_GetRef();
		SecondGuest.Picks = MakePicks();
		SecondGuest.Picks.bGuest = true;
		SecondGuest.Picks.GuestKey = TEXT("Player 3");
		FPlayerPicks Joined = MakePicks();
		Joined.GuestKey = TEXT("Player 3");
		TestTrue(TEXT("the guest with its own player name"), UGuestKeepSubsystem::FindGuestMember(Host, Joined) == &Host.Party.Members[2]);
	});

	It("gives the run's kept levels to the guest's own character, and nothing when the run kept none", [this]()
	{
		FWoRSaveData Own = UGuestKeepSubsystem::MakeNewCharacterSave(MakePicks());
		FSavedPartyMember Member;
		TestFalse(TEXT("no kept levels: nothing"), UGuestKeepSubsystem::FoldKeptLevels(Own, Member));
		Member.bHasKeptLevels = true;
		Member.KeptMind = 3;
		Member.KeptSpirit = 1;
		TestTrue(TEXT("kept"), UGuestKeepSubsystem::FoldKeptLevels(Own, Member));
		const FSavedPartyMember &Mine = Own.Party.Members[0];
		TestTrue(TEXT("its levels replace the last ones"), Mine.bHasKeptLevels && Mine.KeptMind == 3 && Mine.KeptBody == 0 && Mine.KeptSpirit == 1);
	});

	It("keys a saved member as the pool keys its built character", [this]()
	{
		FSavedPartyMember Guest;
		Guest.Picks = MakePicks();
		Guest.Picks.bGuest = true;
		const UCharacterData *Template = NewObject<UCharacterData>(GetTransientPackage());
		const UCharacterData *Built = WoRPlayerBuild::BuildFrom(Guest.Picks, Template, nullptr, 0, nullptr, WoRPlayerBuild::GetBuiltName(Guest.Picks));
		if (TestNotNull(TEXT("built"), Built))
		{
			TestEqual(TEXT("the same key"), UGuestKeepSubsystem::GetSavedMemberKey(Guest), UPoolSubsystem::GetCharacterKey(Built));
		}
	});

	It("moves only the guest's kept hub gear and its learned skills to its own character, once", [this]()
	{
		const FName GuestKey(TEXT("Guest"));
		const FName OwnKey(TEXT("Own"));
		FWoRSaveData Host;
		const FGuid Kept = FGuid::NewGuid();
		const FGuid HostPlayers = FGuid::NewGuid();
		const FGuid StartingKit = FGuid::NewGuid();
		for (const FGuid &Id : {Kept, HostPlayers, StartingKit})
		{
			Host.Pool.Gear.AddDefaulted_GetRef().InstanceID = Id;
		}
		const auto Own = [&Host](const FGuid &Id, FName Key)
		{
			FSavedWearer &Owner = Host.Pool.Owners.AddDefaulted_GetRef();
			Owner.GearID = Id;
			Owner.Character = Key;
		};
		Own(Kept, GuestKey);
		Own(HostPlayers, TEXT("HostPlayer"));
		Own(StartingKit, GuestKey);
		Host.Pool.HubGear = {Kept, HostPlayers}; // the starting kit is home gear, not found
		FSavedCharacterSkills &Learned = Host.Pool.CharacterSkills.AddDefaulted_GetRef();
		Learned.Character = GuestKey;
		Learned.Abilities.AddDefaulted_GetRef().Def.Path = FSoftObjectPath(TEXT("/Game/Test/DA_Skill_Test.DA_Skill_Test"));

		FWoRSaveData Mine;
		TestEqual(TEXT("one piece and one skill"), UGuestKeepSubsystem::FoldGearAndSkills(Mine, Host, GuestKey, OwnKey), 2);
		TestTrue(TEXT("the kept piece is its own, as hub gear"), Mine.Pool.Gear.Num() == 1 && Mine.Pool.Gear[0].InstanceID == Kept &&
				 Mine.Pool.Owners.Num() == 1 && Mine.Pool.Owners[0].Character == OwnKey && Mine.Pool.HubGear.Contains(Kept));
		TestTrue(TEXT("the skill under its own key"), Mine.Pool.CharacterSkills.Num() == 1 && Mine.Pool.CharacterSkills[0].Character == OwnKey);
		TestEqual(TEXT("a second copy adds nothing"), UGuestKeepSubsystem::FoldGearAndSkills(Mine, Host, GuestKey, OwnKey), 0);

		// A found evolution kept in the socket of the guest's starting weapon: it comes over (loose).
		const FGuid Evolution = FGuid::NewGuid();
		Host.Pool.Evolutions.AddDefaulted_GetRef().InstanceID = Evolution;
		Host.Pool.HubGear.Add(Evolution);
		FSavedOwnedItem *Kit = Host.Pool.Gear.FindByPredicate([&StartingKit](const FSavedOwnedItem &Entry) { return Entry.InstanceID == StartingKit; });
		Kit->Gear.Attached.Kind = EAttachedItemKind::Evolution;
		Kit->Gear.Attached.Evolution.InstanceID = Evolution;
		TestEqual(TEXT("the socketed evolution"), UGuestKeepSubsystem::FoldGearAndSkills(Mine, Host, GuestKey, OwnKey), 1);
		TestTrue(TEXT("in its own evolutions, as hub gear"), Mine.Pool.Evolutions.Num() == 1 && Mine.Pool.HubGear.Contains(Evolution));
	});

	It("saves a new character made to join as its own player, with its picks", [this]()
	{
		FPlayerPicks Picks = MakePicks();
		Picks.bGuest = true;
		const FWoRSaveData Data = UGuestKeepSubsystem::MakeNewCharacterSave(Picks);
		if (!TestEqual(TEXT("one member"), Data.Party.Members.Num(), 1))
		{
			return;
		}
		TestTrue(TEXT("its own player, not a guest"), Data.Party.Members[0].Picks.IsSet() && !Data.Party.Members[0].Picks.bGuest);
		TestTrue(TEXT("played by the player, with an id"), Data.Party.Members[0].bPlayerControlled && Data.Party.Members[0].MemberId.IsValid());
	});
}

#endif
