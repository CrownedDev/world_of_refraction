// TurnSpeedSourcesSpec.cpp
// GAS stage 3 (B-042v): turn speed on GAS. The attribute's base is the turn manager's cached base (the combatant
// entry's in a fight, worked out the same way outside one), a UWoRGE_StatSources_TurnSpeed effect multiplies the
// attached stone and the net buff, the attribute set rounds it and floors it at 1, and the current value equals
// UTurnManager::GetEffectiveTurnSpeed.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 TURN_SPEED_SOURCES_BUFF_ID = 4831;
	constexpr int32 TURN_SPEED_SOURCES_TURNS = 3;
	/** A percent that leaves a fraction before the round. */
	constexpr float TURN_SPEED_SOURCES_BUFF_PERCENT = 33.0f;
	/** Past -100%, so only the floor of 1 holds it. */
	constexpr float TURN_SPEED_SOURCES_HUGE_DEBUFF_PERCENT = 500.0f;
	constexpr float TURN_SPEED_SOURCES_FLOOR = 1.0f;
}

BEGIN_DEFINE_SPEC(FTurnSpeedSourcesSpec, "WoR.GAS.TurnSpeedSources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;
	UTurnManager *Turns = nullptr;

	float TurnSpeed() const { return ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetTurnSpeedAttribute()); }

	TArray<FActiveGameplayEffectHandle> TurnSpeedSources() const
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UWoRGE_StatSources::GetClassFor(ESubStat::TurnSpeed);
		return ASC->GetActiveEffects(Query);
	}

	void Buff(ESkillEffectType Type, float Percent) const
	{
		TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(
			Hero, FActiveSkillEffect::CreateBuff(TEXT("Turn speed sources test"), TURN_SPEED_SOURCES_BUFF_ID, Type, Percent,
												 TURN_SPEED_SOURCES_TURNS), Hero);
		Sync->SyncAttributes();
	}
END_DEFINE_SPEC(FTurnSpeedSourcesSpec)

void FTurnSpeedSourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Turns = TestWorld->GetSubsystem<UTurnManager>();
		// The stone layer sits behind the loadout.
		const ULoadoutComponent *Loadout =
			TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), NewObject<UWeaponData>(GetTransientPackage()));
		Hero = Loadout ? Loadout->GetOwner() : nullptr;
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
		}
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) || !TestNotNull(TEXT("Turn manager"), Turns) ||
			!TestNotNull(TEXT("Skill effect manager"), TestWorld->GetSubsystem<USkillEffectManager>()))
		{
			ASC = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		if (Turns)
		{
			Turns->EndCombat();
		}
		Hero = nullptr;
		ASC = nullptr;
		Sync = nullptr;
		Turns = nullptr;
		TestWorld.Reset();
	});

	It("holds the cached base as the base and turn order's speed as the current value", [this]()
	{
		if (!ASC) return;
		FWoRStatLayers Layers;
		TestTrue(TEXT("Turn speed layers"), Turns->GetTurnSpeedLayers(Hero, Layers));
		TestEqual(TEXT("Base = cached base"), ASC->GetNumericAttributeBase(UWoRAttributeSet_Stats::GetTurnSpeedAttribute()),
				  Layers.StatBase);
		TestEqual(TEXT("Current = turn order's speed"), TurnSpeed(), static_cast<float>(Turns->GetEffectiveTurnSpeed(Hero)));
		TestEqual(TEXT("One turn speed sources effect"), TurnSpeedSources().Num(), 1);
		TestTrue(TEXT("The compare sees no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});

	It("rounds a turn speed buff in by updating its sources effect in place", [this]()
	{
		if (!ASC) return;
		const int32 Before = Turns->GetEffectiveTurnSpeed(Hero);
		const TArray<FActiveGameplayEffectHandle> HandlesBefore = TurnSpeedSources();
		Buff(ESkillEffectType::TurnSpeedBuff, TURN_SPEED_SOURCES_BUFF_PERCENT);
		TestTrue(TEXT("The buff raised turn order's speed"), Turns->GetEffectiveTurnSpeed(Hero) > Before);
		TestEqual(TEXT("Current = turn order's speed"), TurnSpeed(), static_cast<float>(Turns->GetEffectiveTurnSpeed(Hero)));
		TestEqual(TEXT("A whole number"), FMath::Frac(TurnSpeed()), 0.0f);
		TestTrue(TEXT("Same effect, updated in place"), TurnSpeedSources() == HandlesBefore);
	});

	It("floors at 1 under a debuff past -100%, as turn order does", [this]()
	{
		if (!ASC) return;
		Buff(ESkillEffectType::TurnSpeedDebuff, TURN_SPEED_SOURCES_HUGE_DEBUFF_PERCENT);
		TestEqual(TEXT("Turn order floored"), Turns->GetEffectiveTurnSpeed(Hero), static_cast<int32>(TURN_SPEED_SOURCES_FLOOR));
		TestEqual(TEXT("Attribute floored"), TurnSpeed(), TURN_SPEED_SOURCES_FLOOR);
	});

	It("follows the combatant entry's cached base in a fight", [this]()
	{
		if (!ASC) return;
		AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("Enemy"), Enemy)) return;
		Turns->InitializeCombat({Hero}, {Enemy});
		Sync->SyncAttributes();
		TestEqual(TEXT("Current = turn order's speed"), TurnSpeed(), static_cast<float>(Turns->GetEffectiveTurnSpeed(Hero)));
		TestEqual(TEXT("One turn speed sources effect"), TurnSpeedSources().Num(), 1);
		TestTrue(TEXT("The compare sees no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});
}

#endif
