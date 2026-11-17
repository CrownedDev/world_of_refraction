// RecruitRowsSpec.cpp
// B-557 (D-238, one-player stage 4b): the hub's recruits are the shared character built from a row of fixed picks on the
// class table, each named by its element or class ("Fire Spirit"), under its own built name so it never takes the player's.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/PlayerBuild.h"
#include "Character/PlayerPicks.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "UObject/Package.h"

namespace
{
	constexpr int32 DEFAULT_RECRUIT_COUNT = 12; // Generic, Resonator, nine elements, Broken Darkness (D-238)
}

BEGIN_DEFINE_SPEC(FRecruitRowsSpec, "WoR.Party.RecruitRows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FRecruitRowsSpec)

void FRecruitRowsSpec::Define()
{
	It("makes the 12 recruits as Spirits, each a recruit row named by its element or class", [this]()
	{
		const TArray<FPlayerPicks> Rows = UClassDefaultLoadoutData::MakeDefaultRecruits();
		TestEqual(TEXT("12 recruits"), Rows.Num(), DEFAULT_RECRUIT_COUNT);
		TestTrue(TEXT("all recruit rows with a name"), !Rows.ContainsByPredicate([](const FPlayerPicks &Row) { return !Row.bRecruit || !Row.IsSet(); }));
		UClassDefaultLoadoutData *Table = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		Table->Recruits = Rows;
		const FPlayerPicks *Fire = Table->FindRecruit(ECharacterClass::Caster, ESpellElement::Fire, false);
		TestTrue(TEXT("Fire Spirit"), Fire && Fire->Name == TEXT("Fire Spirit"));
		const FPlayerPicks *Darkness = Table->FindRecruit(ECharacterClass::Caster, ESpellElement::Darkness, false);
		const FPlayerPicks *Broken = Table->FindRecruit(ECharacterClass::Caster, ESpellElement::Darkness, true);
		TestTrue(TEXT("Darkness and Broken Darkness are two rows"), Darkness && Broken && Darkness != Broken && Broken->bBrokenDarkness);
		const FPlayerPicks *Generic = Table->FindRecruit(ECharacterClass::Generic, ESpellElement::Fire, false);
		TestTrue(TEXT("a Generic matches by class alone, with its two weapons"), Generic && Generic->Weapons.Num() == 2);
		const FPlayerPicks *Resonator = Table->FindRecruit(ECharacterClass::Resonator, ESpellElement::Generic, false);
		TestTrue(TEXT("a Resonator with its two crystal types"), Resonator && Resonator->Crystals.Num() == 2);
	});

	It("builds a recruit under its own name, found again instead of built twice", [this]()
	{
		FPlayerPicks Player;
		Player.Name = TEXT("Ash");
		const TArray<FPlayerPicks> Rows = UClassDefaultLoadoutData::MakeDefaultRecruits();
		const FPlayerPicks *FireRow = Rows.FindByPredicate([](const FPlayerPicks &Row) { return Row.Element == ESpellElement::Fire && Row.Class == ECharacterClass::Caster; });
		if (!TestNotNull(TEXT("a Fire row"), FireRow)) return;
		const FPlayerPicks Fire = *FireRow;
		FPlayerPicks Broken = Fire;
		Broken.Element = ESpellElement::Darkness;
		Broken.bBrokenDarkness = true;
		FPlayerPicks Darkness = Broken;
		Darkness.bBrokenDarkness = false;
		TestEqual(TEXT("the player keeps the fixed name"), WoRPlayerBuild::GetBuiltName(Player), FName(WoRPlayerBuild::BUILT_NAME));
		TestNotEqual(TEXT("a recruit has its own"), WoRPlayerBuild::GetBuiltName(Fire), FName(WoRPlayerBuild::BUILT_NAME));
		TestNotEqual(TEXT("Darkness and Broken Darkness differ"), WoRPlayerBuild::GetBuiltName(Darkness), WoRPlayerBuild::GetBuiltName(Broken));

		const UCharacterData *Template = NewObject<UCharacterData>(GetTransientPackage());
		UCharacterData *Built = WoRPlayerBuild::BuildFrom(Fire, Template, nullptr, 0, nullptr, WoRPlayerBuild::GetBuiltName(Fire));
		if (!TestNotNull(TEXT("built"), Built)) return;
		TestTrue(TEXT("built, named and classed from the row"), WoRPlayerBuild::IsBuilt(Built) && Built->Name == Fire.Name &&
				 Built->CharacterClass == ECharacterClass::Caster && Built->InnateElement == Fire.Element);
		TestTrue(TEXT("found again, not built twice"), WoRPlayerBuild::Build(Fire, nullptr) == Built);
	});
}

#endif
