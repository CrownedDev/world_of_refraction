// LiveWorldLevelsSpec.cpp
// B-217 (Q-217): combat reads the live run world levels (asset + head start + earned/bought), not the
// character asset's authored levels, for the requirement gap, the weapon requirement factor and the
// Broken Darkness strain deficit.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Debug/Combat/RequirementGapDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/WorldStatRequirements.h"

namespace
{
	constexpr int32 LIVE_LEVELS_TEST_REQUIRED = 4;
	constexpr int32 LIVE_LEVELS_TEST_ASSET_LEVEL = 1;
}

BEGIN_DEFINE_SPEC(FLiveWorldLevelsSpec, "WoR.Requirements.LiveLevels",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UActionExecutor *Executor = nullptr;
AActor *User = nullptr;
UAbilityData *Ability = nullptr;

FAction AbilityAction() const
{
	FAction Action;
	Action.ActionType = EActionType::Ability;
	Action.SkillData = Ability;
	return Action;
}

/** The ability's requirement-gap effect on RawDamage, as a multiplier on 1. */
float RawDamageGapFactor() const
{
	return Executor->ComputeActionStatModifiers(AbilityAction(), User).ApplyTo(1.0f, ESubStat::RawDamage);
}

void RaiseLiveBodyToRequirement() const
{
	User->FindComponentByClass<UCharacterDataComponent>()->LiveWorldBody = LIVE_LEVELS_TEST_REQUIRED;
}
END_DEFINE_SPEC(FLiveWorldLevelsSpec)

void FLiveWorldLevelsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();

		FWorldStatRequirements Requirements;
		Requirements.RequiredWorldBody = LIVE_LEVELS_TEST_REQUIRED;
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->Requirements = Requirements;
		Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Requirements = Requirements;

		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->WorldBodyLevel = LIVE_LEVELS_TEST_ASSET_LEVEL;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Weapon);
		User = Loadout ? Loadout->GetOwner() : nullptr;
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("starts from the asset's levels", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && User))
		{
			return;
		}
		TestEqual(TEXT("live body seeded from the asset"), User->FindComponentByClass<UCharacterDataComponent>()->GetWorldBody(),
				  LIVE_LEVELS_TEST_ASSET_LEVEL);
		TestTrue(TEXT("under the skill's requirement: penalty"), RawDamageGapFactor() < 1.0f);
		TestTrue(TEXT("under the weapon's requirement: penalty"),
				 Executor->GetWeaponRequirementFactor(User, AbilityAction()) < 1.0f);
	});

	It("a pillar raised during the run meets the skill's requirement", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && User))
		{
			return;
		}
		RaiseLiveBodyToRequirement();
		TestEqual(TEXT("no requirement-gap penalty"), RawDamageGapFactor(), 1.0f);
	});

	It("a pillar raised during the run meets the weapon's requirement", [this]()
	{
		if (!TestTrue(TEXT("setup"), Executor && User))
		{
			return;
		}
		RaiseLiveBodyToRequirement();
		TestEqual(TEXT("weapon factor"), Executor->GetWeaponRequirementFactor(User, AbilityAction()), 1.0f);
	});

	It("the live debug readout shows the raised level", [this]()
	{
		if (!TestNotNull(TEXT("setup"), User))
		{
			return;
		}
		RaiseLiveBodyToRequirement();
		const FString Readout =
			URequirementGapDebug::GetLiveRequirementGapString(Ability, User->FindComponentByClass<UCharacterDataComponent>());
		TestTrue(TEXT("body gap 0"), Readout.Contains(FString::Printf(TEXT("Char %d  Gap +0"), LIVE_LEVELS_TEST_REQUIRED)));
	});

	It("the strain deficit counts the live levels", [this]()
	{
		if (!TestNotNull(TEXT("setup"), User))
		{
			return;
		}
		const UCharacterDataComponent *CharComp = User->FindComponentByClass<UCharacterDataComponent>();
		TestEqual(TEXT("asset deficit"), Ability->Requirements.GetTotalDeficit(CharComp->CharacterData),
				  LIVE_LEVELS_TEST_REQUIRED - LIVE_LEVELS_TEST_ASSET_LEVEL);
		RaiseLiveBodyToRequirement();
		TestEqual(TEXT("live deficit"), Ability->Requirements.GetTotalDeficit(FWorldPillarLevels::FromComponent(*CharComp)), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
