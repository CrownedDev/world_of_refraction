// SkillCopyTierSpec.cpp
// B-526 (D-224 question 12, skills track S3d): a skill's own-tier power reads the user's learned copy's tier, so a copy
// dropped by a lost run (B-525) hits softer: a C copy dropped to F goes from C's tier power to F's. A skill the user
// doesn't own (an enemy's) reads its asset's tier. The code is B-519 part 5 (UActionExecutor::ResolveActionTier); no
// weapon channel here, so the tier gap is x1.0 and the ratio is the tier power's.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 COPY_TIER_BASE_DAMAGE = 1000; // large, so rounding stays inside the tolerance
	constexpr EItemTier COPY_TIER_ASSET = EItemTier::C_Tier;
	constexpr EItemTier COPY_TIER_DROPPED = EItemTier::F_Tier; // C less 3, never below F
	constexpr float COPY_TIER_RATIO_TOLERANCE = 0.01f;
}

BEGIN_DEFINE_SPEC(FSkillCopyTierSpec, "WoR.Combat.SkillCopyTier", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;
	AActor *User = nullptr;
	UInventoryComponent *Inventory = nullptr;

	int32 DamageOf(const FAction &Action) const
	{
		return Executor->PreviewActionDamage(User, Action, Executor->ComputeActionStatModifiers(Action, User)).FinalDamage;
	}

	float ExpectedDropRatio() const
	{
		return TierPowerScaling::GetTierPowerMultiplier(COPY_TIER_DROPPED) / TierPowerScaling::GetTierPowerMultiplier(COPY_TIER_ASSET);
	}
END_DEFINE_SPEC(FSkillCopyTierSpec)

void FSkillCopyTierSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		User = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Inventory = User ? FWoRTestWorld::AddComponent<UInventoryComponent>(User) : nullptr;
	});

	AfterEach([this]()
	{
		Executor = nullptr;
		User = nullptr;
		Inventory = nullptr;
		TestWorld.Reset();
	});

	It("hits softer with a spell copy dropped from C to F", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && User && Inventory)) return;
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseDamage = COPY_TIER_BASE_DAMAGE;
		Spell->Tier = COPY_TIER_ASSET;
		Spell->Element = ESpellElement::Generic;
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;

		const int32 Unowned = DamageOf(Action); // the asset's tier, as an enemy's
		if (!TestTrue(TEXT("learned"), Inventory->LearnSpell(Spell))) return;
		const int32 AtC = DamageOf(Action);
		TestEqual(TEXT("a fresh copy is at the asset's tier"), AtC, Unowned);
		Inventory->Spells.LearnedSpells[0].Tier = COPY_TIER_DROPPED; // what a lost run leaves (B-525)
		const int32 AtF = DamageOf(Action);
		TestTrue(TEXT("the dropped copy hits softer"), AtF < AtC);
		TestEqual(TEXT("by the tier power, F over C"), static_cast<float>(AtF) / AtC, ExpectedDropRatio(), COPY_TIER_RATIO_TOLERANCE);
	});

	It("hits softer with an ability copy dropped from C to F", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && User && Inventory)) return;
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseDamage = COPY_TIER_BASE_DAMAGE;
		Ability->Tier = COPY_TIER_ASSET;
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;

		if (!TestTrue(TEXT("learned"), Inventory->LearnAbility(Ability))) return;
		const int32 AtC = DamageOf(Action);
		Inventory->Abilities.LearnedAbilities[0].Tier = COPY_TIER_DROPPED;
		const int32 AtF = DamageOf(Action);
		TestTrue(TEXT("the dropped copy hits softer"), AtF < AtC);
		TestEqual(TEXT("by the tier power, F over C"), static_cast<float>(AtF) / AtC, ExpectedDropRatio(), COPY_TIER_RATIO_TOLERANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
