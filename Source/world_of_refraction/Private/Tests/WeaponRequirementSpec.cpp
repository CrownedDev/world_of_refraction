// WeaponRequirementSpec.cpp
// B-162 (D-082, Q-146c), B-326 (D-119): under a weapon's requirement, by any amount on any pillar, attacks and
// abilities through it deal x0.6 on the weapon part with no scaling; at or over it nothing changes; spells ignore the
// weapon's requirement. An ability or spell under its own requirement still works at x0.6 on its skill part.

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
	/** B-220: the weapon's own part, which the requirement cuts (the skill part isn't). */
	constexpr int32 WEAPON_REQ_TEST_POWER = 40;

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
int32 PreviewDamageAtBody(int32 BodyLevel, EActionType ActionType, int32 SkillRequiredBody = 0)
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
		Spell->Requirements = BodyRequirement(SkillRequiredBody);
		Action.SpellData = Spell;
	}
	else
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseDamage = WEAPON_REQ_TEST_BASE_DAMAGE;
		Ability->Requirements = BodyRequirement(SkillRequiredBody);
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

		It("any shortfall is the one flat factor (D-119), 1 level under or far under", [this]()
		{
			const UCharacterData *OneUnder = NewCharacterAt(0, WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_SMALL_GAP, 0);
			TestEqual(TEXT("one under"), BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL).GetWeaponRequirementFactor(OneUnder),
					  WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR);
			FWorldStatRequirements FarRequirement;
			FarRequirement.RequiredWorldBody = WEAPON_REQ_TEST_FAR_UNDER_GAP;
			TestEqual(TEXT("far under: the same"), FarRequirement.GetWeaponRequirementFactor(NewCharacterAt(0, 0, 0)),
					  WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR);
		});

		It("one unmet pillar is enough, and two unmet don't stack", [this]()
		{
			FWorldStatRequirements Requirements;
			Requirements.RequiredWorldMind = WEAPON_REQ_TEST_REQUIRED_LEVEL;
			Requirements.RequiredWorldBody = WEAPON_REQ_TEST_REQUIRED_LEVEL;
			const UCharacterData *MindShort = NewCharacterAt(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_SMALL_GAP,
															 WEAPON_REQ_TEST_REQUIRED_LEVEL, 0);
			const UCharacterData *BothShort = NewCharacterAt(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP,
															 WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_SMALL_GAP, 0);
			TestEqual(TEXT("one pillar short"), Requirements.GetWeaponRequirementFactor(MindShort),
					  WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR);
			TestEqual(TEXT("both short"), Requirements.GetWeaponRequirementFactor(BothShort),
					  WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR);
		});

		It("the debug readout shows the live factor", [this]()
		{
			UWeaponData *DebugWeapon = NewObject<UWeaponData>(GetTransientPackage());
			DebugWeapon->Requirements = BodyRequirement(WEAPON_REQ_TEST_REQUIRED_LEVEL);
			const UCharacterData *Character = NewCharacterAt(0, WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP, 0);
			const FString Expected = FString::Printf(TEXT("Factor: x%.2f"),
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
			// The copy SpawnCombatantWithLoadout makes is F; an F asset keeps Power unscaled (B-220).
			Weapon->Tier = EItemTier::F_Tier;
		});

		AfterEach([this]()
		{
			TestWorld.Reset();
		});

		It("an ability through an under-requirement weapon deals less: its weapon part is cut (B-220)", [this]()
		{
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			Weapon->Power = WEAPON_REQ_TEST_POWER;
			const int32 AtRequirement = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL, EActionType::Ability);
			const int32 Under = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP, EActionType::Ability);
			const int32 CutPower = FMath::RoundToInt(WEAPON_REQ_TEST_POWER * WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR);
			TestTrue(TEXT("damage resolved"), AtRequirement > 0);
			TestEqual(TEXT("under = at - (Power - cut Power)"), Under, AtRequirement - (WEAPON_REQ_TEST_POWER - CutPower));
		});

		It("B-326 (D-119): an ability or spell under its own requirement still works, its skill part x0.6", [this]()
		{
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			Weapon->Power = 0; // the skill part alone
			const int32 Body = WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_SMALL_GAP;
			for (const EActionType Type : {EActionType::Ability, EActionType::Spell})
			{
				const int32 Met = PreviewDamageAtBody(Body, Type, Body);
				const int32 Unmet = PreviewDamageAtBody(Body, Type, WEAPON_REQ_TEST_REQUIRED_LEVEL);
				TestTrue(TEXT("still deals damage"), Unmet > 0);
				TestEqual(TEXT("x0.6"), Unmet, FMath::RoundToInt(Met * WeaponRequirement::REQUIREMENT_UNMET_DAMAGE_FACTOR));
			}
		});

		It("the weapon's requirement doesn't cut the skill part (a weapon with Power 0)", [this]()
		{
			if (!TestNotNull(TEXT("executor"), Executor))
			{
				return;
			}
			const int32 AtRequirement = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL, EActionType::Ability);
			const int32 Under = PreviewDamageAtBody(WEAPON_REQ_TEST_REQUIRED_LEVEL - WEAPON_REQ_TEST_LARGE_GAP, EActionType::Ability);
			TestEqual(TEXT("under = at"), Under, AtRequirement);
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
