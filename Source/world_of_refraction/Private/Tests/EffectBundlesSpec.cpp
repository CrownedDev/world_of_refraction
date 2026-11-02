// EffectBundlesSpec.cpp
// B-250 (DRY audit A10): one bundle flatten for gear and evolutions. A definition with one starting and one
// conditional effect gives the same lists through a weapon and an evolution; the gathered copies carry the
// definition's ID, the true bundle index and the item's source key; a null definition is skipped.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Effects/EffectBundles.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	constexpr int32 BUNDLE_TEST_SOURCE_KEY = 8101;

	UEffectDefinition *NewBundle()
	{
		UEffectDefinition *Def = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Starting;
		Starting.EffectName = TEXT("Starting");
		FSkillEffect Conditional;
		Conditional.EffectName = TEXT("Conditional");
		FSkillCondition OnHit;
		OnHit.Trigger = ESkillTrigger::OnHit;
		Conditional.Conditions.Add(OnHit);
		Def->Effects = {Conditional, Starting};
		return Def;
	}
}

BEGIN_DEFINE_SPEC(FEffectBundlesSpec, "WoR.Effects.Bundles", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FEffectBundlesSpec)

void FEffectBundlesSpec::Define()
{
	It("splits starting and conditional effects, skipping a null definition", [this]()
	{
		const TArray<TObjectPtr<UEffectDefinition>> Defs = {nullptr, NewBundle()};
		const TArray<FSkillEffect> Starting = EffectBundles::Flatten(Defs, false);
		const TArray<FSkillEffect> Conditional = EffectBundles::Flatten(Defs, true);
		TestTrue(TEXT("one starting"), Starting.Num() == 1 && Starting[0].EffectName == TEXT("Starting"));
		TestTrue(TEXT("one conditional"), Conditional.Num() == 1 && Conditional[0].EffectName == TEXT("Conditional"));
	});

	It("gathers with the definition's ID, the bundle index and the source key", [this]()
	{
		UEffectDefinition *Def = NewBundle();
		const TArray<FGatheredEffect> Gathered = EffectBundles::GatherWithIds({Def}, false, BUNDLE_TEST_SOURCE_KEY);
		if (!TestEqual(TEXT("one starting"), Gathered.Num(), 1)) return;
		TestEqual(TEXT("definition ID"), Gathered[0].DefID, static_cast<int32>(Def->GetUniqueID()));
		TestEqual(TEXT("true bundle index"), Gathered[0].BundleIndex, 1);
		TestEqual(TEXT("source key"), Gathered[0].SourceKey, BUNDLE_TEST_SOURCE_KEY);
	});

	It("gives a weapon and an evolution the same lists from the same definition", [this]()
	{
		UEffectDefinition *Def = NewBundle();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->ReferencedEffects = {Def};
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->ReferencedEffects = {Def};
		TestEqual(TEXT("starting"), Weapon->GetStartingEffects().Num(), Evolution->GetStartingEffects().Num());
		TestEqual(TEXT("conditional"), Weapon->GetConditionalEffects().Num(), Evolution->GetConditionalEffects().Num());
		TestEqual(TEXT("gathered starting"), Weapon->GetStartingEffectsGathered(nullptr).Num(), Evolution->GetStartingEffectsGathered().Num());
	});
}

#endif
