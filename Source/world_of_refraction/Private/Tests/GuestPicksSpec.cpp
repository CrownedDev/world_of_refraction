// GuestPicksSpec.cpp
// B-561 (one-player stage 5): a co-op guest joins with its own character, sent as its picks and built on the host under
// a guest name of its own, so it never takes the host player's built name and comes back to the same pool key.
// Unproven here: the RPC and the seat over a real second client (PIE with two players; the PIE sheet B-561).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Party/Party.h"
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
		TestFalse(TEXT("an older save's typed name: its spaces and marks don't reach the object name"), WoRPlayerBuild::GetBuiltName(Spaced).ToString().Contains(TEXT(" ")));
		FPlayerPicks Keyed = Guest;
		Keyed.GuestKey = TEXT("Player 2");
		const FString KeyedName = WoRPlayerBuild::GetBuiltName(Keyed).ToString();
		TestTrue(TEXT("D-228: the joining player's name keys it, not the typed name"), KeyedName.Contains(TEXT("Player_2")) && !KeyedName.Contains(TEXT("Ash")));
		FPlayerPicks Other = Keyed;
		Other.GuestKey = TEXT("Player 3");
		TestNotEqual(TEXT("two guests of the same kind are told apart"), WoRPlayerBuild::GetBuiltName(Other), WoRPlayerBuild::GetBuiltName(Keyed));
	});

	It("finds a guest's character again rather than building it twice (it keeps its pool key)", [this]()
	{
		FPlayerPicks Guest = MakePicks(TEXT("Rin"));
		Guest.bGuest = true;
		const UCharacterData *Template = NewObject<UCharacterData>(GetTransientPackage());
		UCharacterData *Built = WoRPlayerBuild::BuildFrom(Guest, Template, nullptr, 0, nullptr, WoRPlayerBuild::GetBuiltName(Guest));
		if (!TestNotNull(TEXT("built"), Built)) return;
		TestTrue(TEXT("the same object on the second join"), WoRPlayerBuild::Build(Guest, nullptr) == Built);
		TestEqual(TEXT("named by what it is (D-228)"), Built->Name, FString(TEXT("Water Refractor")));
	});

	It("tags two players' characters of one kind with the player's name, and only then (D-229)", [this]()
	{
		const auto MakeMember = [](const FPlayerPicks &Picks)
		{
			FPartyMember Member;
			Member.Picks = Picks;
			UPackage *Package = GetTransientPackage();
			Member.CharacterData = NewObject<UCharacterData>(Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(),
																							*(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_TagSpec"))));
			Member.CharacterData->Name = TEXT("Water Refractor");
			return Member;
		};
		FPlayerPicks GuestPicks = MakePicks(TEXT("Water Refractor"));
		GuestPicks.bGuest = true;
		GuestPicks.GuestKey = TEXT("Rin");
		FPlayerPicks RecruitPicks = MakePicks(TEXT("Water Spirit"));
		RecruitPicks.bRecruit = true;
		TArray<FPartyMember> Members = {MakeMember(MakePicks(TEXT("Water Refractor"))), MakeMember(RecruitPicks)};
		FPartyMember Authored = MakeMember(MakePicks(TEXT("Water Refractor")));
		Authored.CharacterData = NewObject<UCharacterData>(GetTransientPackage()); // not a built character: never renamed
		Members[1].CharacterData->Name = TEXT("Water Spirit");
		TestFalse(TEXT("solo: no tag"), WoRPlayerBuild::RetagSameNames(Members, TEXT("Ash")));
		Members.Add(MakeMember(GuestPicks));
		TestTrue(TEXT("a guest of the same kind: renamed"), WoRPlayerBuild::RetagSameNames(Members, TEXT("Ash")));
		TestEqual(TEXT("the host's, by its player name"), Members[0].CharacterData->Name, FString(TEXT("Water Refractor (Ash)")));
		TestEqual(TEXT("the guest's, by its own"), Members[2].CharacterData->Name, FString(TEXT("Water Refractor (Rin)")));
		TestEqual(TEXT("a recruit keeps its name"), Members[1].CharacterData->Name, FString(TEXT("Water Spirit")));
		TestFalse(TEXT("a second pass changes nothing"), WoRPlayerBuild::RetagSameNames(Members, TEXT("Ash")));
		Members.RemoveAt(2);
		WoRPlayerBuild::RetagSameNames(Members, TEXT("Ash"));
		TestEqual(TEXT("the guest gone: no tag"), Members[0].CharacterData->Name, FString(TEXT("Water Refractor")));
		Members.Add(MakeMember(GuestPicks));
		WoRPlayerBuild::RetagSameNames(Members, FString());
		TestEqual(TEXT("no online name: Host"), Members[0].CharacterData->Name, FString(TEXT("Water Refractor (Host)")));
		Authored.CharacterData->Name = TEXT("Authored");
		TArray<FPartyMember> WithAuthored = {MakeMember(MakePicks(TEXT("Water Refractor"))), Authored};
		WoRPlayerBuild::RetagSameNames(WithAuthored, TEXT("Ash"));
		TestEqual(TEXT("an authored character asset is never renamed"), Authored.CharacterData->Name, FString(TEXT("Authored")));
	});

	It("refuses a join with no picks, without a host", [this]()
	{
		URunStateComponent *Run = NewObject<URunStateComponent>(GetTransientPackage());
		TestFalse(TEXT("no owner, no picks: refused"), Run->RequestJoinWithPicks(nullptr, FPlayerPicks()));
	});
}

#endif
