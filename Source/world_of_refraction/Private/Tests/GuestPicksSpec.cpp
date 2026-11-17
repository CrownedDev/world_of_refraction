// GuestPicksSpec.cpp
// B-561 (one-player stage 5): a co-op guest joins with its own character, sent as its picks and built on the host under
// a guest name of its own, so it never takes the host player's built name and comes back to the same pool key.
// Unproven here: the RPC and the seat over a real second client (PIE with two players; the PIE sheet B-561).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Character/PlayerPicks.h"
#include "Player/RunStateComponent.h"
#include "UObject/Package.h"

BEGIN_DEFINE_SPEC(FGuestPicksSpec, "WoR.Coop.GuestPicks", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FPlayerPicks MakePicks(const TCHAR *Name)
	{
		FPlayerPicks Picks;
		Picks.Name = Name;
		Picks.Class = ECharacterClass::Caster;
		Picks.Element = ESpellElement::Water;
		return Picks;
	}
END_DEFINE_SPEC(FGuestPicksSpec)

void FGuestPicksSpec::Define()
{
	It("names a guest's character apart from the host player's and the recruits'", [this]()
	{
		const FPlayerPicks Host = MakePicks(TEXT("Ash"));
		FPlayerPicks Guest = Host;
		Guest.bGuest = true;
		FPlayerPicks Recruit = Host;
		Recruit.bRecruit = true;
		const FName GuestName = WoRPlayerBuild::GetBuiltName(Guest);
		TestNotEqual(TEXT("not the host player's"), GuestName, WoRPlayerBuild::GetBuiltName(Host));
		TestNotEqual(TEXT("not a recruit's"), GuestName, WoRPlayerBuild::GetBuiltName(Recruit));
		FPlayerPicks Spaced = MakePicks(TEXT("Ash Grey!"));
		Spaced.bGuest = true;
		TestFalse(TEXT("a typed name's spaces and marks don't reach the object name"), WoRPlayerBuild::GetBuiltName(Spaced).ToString().Contains(TEXT(" ")));
	});

	It("finds a guest's character again rather than building it twice (it keeps its pool key)", [this]()
	{
		FPlayerPicks Guest = MakePicks(TEXT("Rin"));
		Guest.bGuest = true;
		const UCharacterData *Template = NewObject<UCharacterData>(GetTransientPackage());
		UCharacterData *Built = WoRPlayerBuild::BuildFrom(Guest, Template, nullptr, 0, nullptr, WoRPlayerBuild::GetBuiltName(Guest));
		if (!TestNotNull(TEXT("built"), Built)) return;
		TestTrue(TEXT("the same object on the second join"), WoRPlayerBuild::Build(Guest, nullptr) == Built);
		TestEqual(TEXT("its name is the guest's typed name"), Built->Name, FString(TEXT("Rin")));
	});

	It("refuses a join with no picks, without a host", [this]()
	{
		URunStateComponent *Run = NewObject<URunStateComponent>(GetTransientPackage());
		TestFalse(TEXT("no owner, no picks: refused"), Run->RequestJoinWithPicks(nullptr, FPlayerPicks()));
	});
}

#endif
