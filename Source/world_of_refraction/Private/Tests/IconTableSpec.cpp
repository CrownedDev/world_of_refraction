// IconTableSpec.cpp
// B-485: the UI icon table. An icon that is set is returned, one that isn't is null (the screen keeps its text); the
// debug string says what is set and what is missing; the active table's getters are null with no table.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "UI/WoRIconTable.h"

BEGIN_DEFINE_SPEC(FIconTableSpec, "WoR.UI.IconTable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UWoRIconTable *Table = nullptr;
	UTexture2D *Texture = nullptr;
END_DEFINE_SPEC(FIconTableSpec)

void FIconTableSpec::Define()
{
	BeforeEach([this]()
	{
		Table = NewObject<UWoRIconTable>(GetTransientPackage());
		Texture = NewObject<UTexture2D>(GetTransientPackage());
		Table->ElementBadges.Add(ESpellElement::Fire, Texture);
		Table->TierBadges.Add(EItemTier::C_Tier, Texture);
		Table->WeaponTypes.Add(EWeaponType::Sword, Texture);
		Table->EffectKinds.Add(ESkillEffectType::DOT, Texture);
		Table->EmptySocket = Texture;
	});
	AfterEach([this]()
	{
		WoRIcons::SetActiveForTests(nullptr);
		Table = nullptr;
		Texture = nullptr;
	});

	It("returns an icon that is set and null for one that isn't", [this]()
	{
		TestTrue(TEXT("Fire's badge"), Table->GetElementBadge(ESpellElement::Fire) == Texture);
		TestNull(TEXT("no Water badge"), Table->GetElementBadge(ESpellElement::Water));
		TestTrue(TEXT("tier C"), Table->GetTierBadge(EItemTier::C_Tier) == Texture);
		TestNull(TEXT("no tier S"), Table->GetTierBadge(EItemTier::S_Tier));
		TestTrue(TEXT("the sword"), Table->GetWeaponType(EWeaponType::Sword) == Texture);
		TestNull(TEXT("no hammer"), Table->GetWeaponType(EWeaponType::Hammer));
		TestTrue(TEXT("damage over time"), Table->GetEffectKind(ESkillEffectType::DOT) == Texture);
		TestNull(TEXT("no skip-turn icon"), Table->GetEffectKind(ESkillEffectType::SkipTurn));
		TestTrue(TEXT("the empty socket"), Table->GetEmptySocket() == Texture);
		TestNull(TEXT("no stone icon"), Table->GetStoneSocket());
		Table->ElementBadges.Add(ESpellElement::Water, TSoftObjectPtr<UTexture2D>());
		TestNull(TEXT("an empty reference is no icon"), Table->GetElementBadge(ESpellElement::Water));
	});

	It("says which icons are set and which are missing", [this]()
	{
		const FString Report = Table->GetIconTableString();
		TestTrue(TEXT("the element badges, counted"), Report.Contains(TEXT("Element badges: 1 of")));
		TestTrue(TEXT("Fire is set"), Report.Contains(TEXT("set: Fire")));
		TestTrue(TEXT("what is missing is named"), Report.Contains(TEXT("missing:")) && Report.Contains(TEXT("Water")));
		TestTrue(TEXT("the stone socket is missing"), Report.Contains(TEXT("Stone socket: missing")));
		TestFalse(TEXT("the empty socket is not"), Report.Contains(TEXT("Empty socket: missing")));
	});

	It("gives the active table's icons, and none with no table", [this]()
	{
		WoRIcons::SetActiveForTests(Table);
		TestTrue(TEXT("the active table"), UWoRIconTable::GetActive() == Table);
		TestTrue(TEXT("its Fire badge"), WoRIcons::GetElementBadge(ESpellElement::Fire) == Texture);
		TestNull(TEXT("an unset icon"), WoRIcons::GetTierBadge(EItemTier::S_Tier));
		WoRIcons::SetActiveForTests(nullptr);
		if (!UWoRIconTable::GetActive()) // before the editor step (B-486) there is no asset
		{
			TestNull(TEXT("no table: no badge"), WoRIcons::GetElementBadge(ESpellElement::Fire));
			TestNull(TEXT("no table: no weapon icon"), WoRIcons::GetWeaponType(EWeaponType::Sword));
			TestNull(TEXT("no table: no effect icon"), WoRIcons::GetEffectKind(ESkillEffectType::DOT));
			TestNull(TEXT("no table: no socket"), WoRIcons::GetEmptySocket());
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
