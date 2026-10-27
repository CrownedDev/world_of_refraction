// WeaponRequirementSpec.cpp
// B-162 (D-082, Q-146c, Q-162): under a weapon's requirement, attacks and abilities through it deal
// less (the requirement-gap ladder, worst pillar, clamped to [WEAPON_REQ_FACTOR_MIN, 1]); at or over
// the requirement nothing changes; spells ignore the weapon's requirement.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Debug/Combat/RequirementGapDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/WorldStatRequirements.h"

namespace
{
	constexpr int32 WEAPON_REQ_TEST_BASE_DAMAGE = 100;
	constexpr int32 WEAPON_REQ_TEST_REQUIRED_LEVEL = 4;
	constexpr int32 WEAPON_REQ_TEST_SMALL_GAP = 1;
	constexpr int32 WEAPON_REQ_TEST_LARGE_GAP = 3;
	constexpr int32 WEAPON_REQ_TEST_FAR_UNDER_GAP = 7;

	UCharacterData *NewCharacterAt(int32 Mind, int32 Body, int32 Spirit)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->WorldMindLevel = Mind;
		Data->WorldBodyLevel = Body;
		Data->WorldSpiritLevel = Spirit;
		return Data;
	}

	FWorldStatRequirements BodyRequirement(int32 Level)
	{
		FWorldStatRequirements Requirements;
		Requirements.RequiredWorldBody = Level;
		return Requirements;
	}
}

BEGIN_DEFINE_SPEC(FWeaponRequirementSpec, "WoR.Weapon.RequirementFactor",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UActionExecutor *Executor = nullptr;
UWeaponData *Weapon = nullptr;

/** Pre-defense damage of an ability (or spell) used by a character at the given Body level,
 *  holding Weapon. */
int32 PreviewDamageAtBody(int32 BodyLevel, EActionType ActionType)
{
	ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(NewCharacterAt(0, BodyLevel, 0), Weapon);
	if (!Loadout)
	{
		return INDEX_NONE;
	}
	FAction Action;
	Action.ActionType = ActionType;
	if (ActionType == EActionType::Spell)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseDamage = WEAPON_REQ_TEST_BASE_DAMAGE;
		Spell->Element = ESpellElement::Fire;
		Action.SpellData = Spell;
	}
	else
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseDamage = WEAPON_REQ_TEST_BASE_DAMAGE;
		Action.SkillData = Ability;
	}
	return Executor->PreviewActionDamage(Loadout->GetOwner(), Action, FActionStatModifiers()).FinalDamage;
}
END_DEFINE_SPEC(FWeaponRequirementSpec)

void FWeaponRequirementSpec::Define()
{
	Describe("the factor", [this]()
	{
		It("is 1 with no requirement, at the requirement and over it", [this]()
		{
			const UCharacterData *Character = NewCharacterAt(WEAPON_REQ_TEST_REQUIRED_LEVEL, WEAPON_REQ_TEST_REQUIRED_LEVEL,
															 WEAPON_REQ_TEST_REQUIRED_LEVEL);
			TestEqual(TEXT("no requirement"), FWorldStatRequirements().GetWeaponRequirementFactor(Character), 1.0f);
			TestEqual(TEXT("at the requirement"),
					  BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL).GetWeaponRequirementFactor(Character), 1.0f);
			TestEqual(TEXT("over the requirement gives nothing extra (Q-146c)"),
					  BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP).GetWeaponRequirementFactor(Character),
					  1.0f);
			TestEqual(TEXT("no character"), BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL).GetWeaponRequirementFactor(nullptr), 1.0f);
		});

		It("under the requirement is the requirement-gap ladder", [this]()
		{
			const UCharacterData *Character = NewCharacterAt(0, WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_SMALL_GAP, 0);
			TestEqual(TEXT("gap 1"), BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL).GetWeaponRequirementFactor(Character),
					  TierGapDamage::GetRequirementGapMultiplier(WEAPON_REQ_TEST_SMALL_GAP));
			TestTrue(TEXT("below 1"), BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL).GetWeaponRequirementFactor(Character) < 1.0f);
		});

		It("takes the worst pillar (Q-162)", [this]()
		{
			FWorldStatRequirements Requirements;
			Requirements.RequiredWorldMind = WEAPON_REQ_TEST_REQUIRED_LEVEL;
			Requirements.RequiredWorldBody = WEAPON_REQ_TEST_REQUIRED_LEVEL;
			const UCharacterData *Character = NewCharacterAt(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP,
															 WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_SMALL_GAP, 0);
			TestEqual(TEXT("the larger gap decides"), Requirements.GetWeaponRequirementFactor(Character),
					  TierGapDamage::GetRequirementGapMultiplier(WEAPON_REQ_TEST_LARGE_GAP));
		});

		It("far under is the ladder's end, clamped to the floor", [this]()
		{
			FWorldStatRequirements Requirements;
			Requirements.RequiredWorldBody = WEAPON_REQ_TEST_FAR_UNDER_GAP;
			const UCharacterData *Character = NewCharacterAt(0, 0, 0);
			TestEqual(TEXT("ladder end, at least WEAPON_REQ_FACTOR_MIN"), Requirements.GetWeaponRequirementFactor(Character),
					  FMath::Max(TierGapDamage::GetRequirementGapMultiplier(WEAPON_REQ_TEST_FAR_UNDER_GAP),
								 WeaponRequirement::WEAPON_REQ_FACTOR_MIN));
		});

		It("the debug readout shows the live factor", [this]()
		{
			UWeaponData *DebugWeapon = NewObject<UWeaponData>(GetTransientPackage());
			DebugWeapon->Requirements = BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL);
			const UCharacterData *Character = NewCharacterAt(0, WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP, 0);
			const FString Expected = FString::Printf(TEXT("Factor (worst pillar): x%.2f"),
													 DebugWeapon->Requirements.GetWeaponRequirementFactor(Character));
			TestTrue(TEXT("factor line"),
					 URequirementGapDebug::GetWeaponRequirementFactorString(DebugWeapon, Character).Contains(Expected));
		});
	});

	Describe("in the damage preview", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
			Executor = TestWorld->GetSubsystem<UActionExecutor>();
			Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Weapon->Requirements = BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL);
		});

		AfterEach([this]()
		{
			TestWorld.Reset();
		});

		It("an ability through an under-requirement weapon deals less", [this]()
		{
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			const int32 AtRequirement = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL, EActionType::Ability);
			const int32 Under = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP, EActionType::Ability);
			TestTrue(TEXT("damage resolved"), AtRequirement > 0);
			TestEqual(TEXT("under = at x ladder"), Under,
					  FMath::RoundToInt(AtRequirement * TierGapDamage::GetRequirementGapMultiplier(WEAPON_REQ_TEST_LARGE_GAP)));
		});

		It("at or over the requirement, an ability is unchanged", [this]()
		{
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			const int32 AtRequirement = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL, EActionType::Ability);
			const int32 Over = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL + WEAPON_REQ_TEST_SMALL_GAP, EActionType::Ability);
			TestEqual(TEXT("over = at"), Over, AtRequirement);
		});

		It("a spell ignores the weapon's requirement", [this]()
		{
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			const int32 AtRequirement = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL, EActionType::Spell);
			const int32 Under = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP, EActionType::Spell);
			TestEqual(TEXT("spell unchanged"), Under, AtRequirement);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
