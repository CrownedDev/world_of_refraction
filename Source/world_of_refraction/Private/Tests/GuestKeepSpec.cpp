// GuestKeepSpec.cpp
// B-563 (Q-561, Crown 2026-11-17): a co-op guest keeps what it earns in its own save. The folds, headless. Unproven
// here: the hooks on a real client (the checkpoint copy arriving, the slot written; two-player PIE, sheet B-563).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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
