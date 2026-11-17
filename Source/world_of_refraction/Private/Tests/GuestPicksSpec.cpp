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

	It("reads a card as You for its own player, the gamer tag for another, and a recruit by its name (D-229)", [this]()
	{
		UPackage *Package = GetTransientPackage();
		UCharacterData *Player = NewObject<UCharacterData>(Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(),
																						  *(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_CardSpec"))));
		Player->Name = TEXT("Fire Refractor");
		Player->CharacterClass = ECharacterClass::Caster;
		Player->InnateElement = ESpellElement::Fire;
		UCharacterData *Recruit = NewObject<UCharacterData>(Package, MakeUniqueObjectName(Package, UCharacterData::StaticClass(),
																						   *(FString(WoRPlayerBuild::BUILT_NAME) + TEXT("_Recruit_CardSpec"))));
		Recruit->Name = TEXT("Fire Spirit");
		TestTrue(TEXT("a built player is a player character; a recruit is not"),
				 WoRPlayerBuild::IsPlayerCharacter(Player) && !WoRPlayerBuild::IsPlayerCharacter(Recruit));
		TestEqual(TEXT("own: You"), WoRPlayerBuild::MakeCardName(*Player, true, true, TEXT("Rin")).ToString(), FString(TEXT("You")));
		TestEqual(TEXT("another player's: its tag, no brackets"), WoRPlayerBuild::MakeCardName(*Player, true, false, TEXT("Rin")).ToString(), FString(TEXT("Rin")));
		TestEqual(TEXT("a recruit: its own name"), WoRPlayerBuild::MakeCardName(*Recruit, false, true, TEXT("Rin")).ToString(), FString(TEXT("Fire Spirit")));
		TestEqual(TEXT("no world (solo): You"), WoRPlayerBuild::GetCardName(Player, FGuid(), nullptr).ToString(), FString(TEXT("You")));
		TestTrue(TEXT("a recruit: no detail line"), WoRPlayerBuild::GetDetailLine(Recruit, FGuid(), nullptr).IsEmpty());
		TestEqual(TEXT("no world: the Spirit line only"), WoRPlayerBuild::GetDetailLine(Player, FGuid(), nullptr).ToString(), FString(TEXT("Spirit of Fire")));
	});

	It("reads a character as Spirit of over Body of (D-241)", [this]()
	{
		TestEqual(TEXT("a Refractor: its element"), WoRPlayerBuild::GetSpiritName(ECharacterClass::Caster, ESpellElement::Fire, false).ToString(), FString(TEXT("Fire")));
		TestEqual(TEXT("Reality: Unknown"), WoRPlayerBuild::GetSpiritName(ECharacterClass::Caster, ESpellElement::Reality, false).ToString(), FString(TEXT("Unknown")));
		TestEqual(TEXT("Broken Darkness"), WoRPlayerBuild::GetSpiritName(ECharacterClass::Caster, ESpellElement::Darkness, true).ToString(), FString(TEXT("Broken Darkness")));
		TestEqual(TEXT("a Resonator: its class"), WoRPlayerBuild::GetSpiritName(ECharacterClass::Resonator, ESpellElement::None, false).ToString(), FString(TEXT("Resonator")));
		const FString Lines = WoRPlayerBuild::MakeIdentityLines(FText::FromString(TEXT("Fire")), FText::FromString(TEXT("Orthos"))).ToString();
		TestTrue(TEXT("two lines"), Lines.StartsWith(TEXT("Spirit of Fire")) && Lines.EndsWith(TEXT("Body of Orthos")) && Lines.Contains(TEXT("\n")));
		TestEqual(TEXT("no body: one line"), WoRPlayerBuild::MakeIdentityLines(FText::FromString(TEXT("Fire")), FText::GetEmpty()).ToString(), FString(TEXT("Spirit of Fire")));
	});

	It("refuses a join with no picks, without a host", [this]()
	{
		URunStateComponent *Run = NewObject<URunStateComponent>(GetTransientPackage());
		TestFalse(TEXT("no owner, no picks: refused"), Run->RequestJoinWithPicks(nullptr, FPlayerPicks()));
	});
}

#endif
