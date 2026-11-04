// RowEffectSpec.cpp
// B-342 (D-134, Q-342): three rows, no columns. The placeholder row effect: the front row deals and takes +20%, the
// back row deals and takes -20%, the middle row neither. Checked on both sides (party and enemies) and through the
// damage maths (WoRDamageMath::Compute), which multiplies by the attacker's damage modifier and divides by the
// defender's defense modifier.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Combat/Grid/CombatGridSubsystem.h"

namespace
{
	constexpr int32 ROW_EFFECT_PARTY_TEAM = 0;
	constexpr int32 ROW_EFFECT_ENEMY_TEAM = 1;
	constexpr int32 ROW_EFFECT_SLOT = 1;
	constexpr int32 ROW_EFFECT_BASE_DAMAGE = 1000;
	constexpr int32 ROW_EFFECT_FRONT_DAMAGE = 1200;
	constexpr int32 ROW_EFFECT_BACK_DAMAGE = 800;
	constexpr float ROW_EFFECT_FRONT = 1.20f;
	constexpr float ROW_EFFECT_BACK = 0.80f;
	constexpr float ROW_EFFECT_TOLERANCE = 0.001f;
}

BEGIN_DEFINE_SPEC(FRowEffectSpec, "WoR.Combat.RowEffect", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCombatGridSubsystem *Grid = nullptr;

	AActor *SpawnIn(int32 Team, ECombatRow Row)
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Actor)
		{
			Grid->AssignPosition(Actor, FCombatGridPosition(Team, Row, ROW_EFFECT_SLOT));
		}
		return Actor;
	}

	/** A plain hit of ROW_EFFECT_BASE_DAMAGE from Attacker to Defender, with only the grid modifiers set. */
	int32 Hit(AActor *Attacker, AActor *Defender) const
	{
		FDamageMathInputs Inputs;
		Inputs.BaseDamage = ROW_EFFECT_BASE_DAMAGE;
		Inputs.GridDamageModifier = Grid->GetDamageModifier(Attacker);
		Inputs.GridDefenseModifier = Grid->GetDefenseModifier(Defender);
		return WoRDamageMath::Compute(Inputs).FinalDamage;
	}
END_DEFINE_SPEC(FRowEffectSpec)

void FRowEffectSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
		TestNotNull(TEXT("Grid"), Grid);
	});

	AfterEach([this]()
	{
		Grid = nullptr;
		TestWorld.Reset();
	});

	It("pins the placeholder: front +20% dealt and taken, back -20%", [this]()
	{
		TestNearlyEqual(TEXT("front dealt"), CombatGridConstants::FRONT_ROW_DAMAGE_DEALT_MULTIPLIER, ROW_EFFECT_FRONT, ROW_EFFECT_TOLERANCE);
		TestNearlyEqual(TEXT("front taken"), CombatGridConstants::FRONT_ROW_DAMAGE_TAKEN_MULTIPLIER, ROW_EFFECT_FRONT, ROW_EFFECT_TOLERANCE);
		TestNearlyEqual(TEXT("back dealt"), CombatGridConstants::BACK_ROW_DAMAGE_DEALT_MULTIPLIER, ROW_EFFECT_BACK, ROW_EFFECT_TOLERANCE);
		TestNearlyEqual(TEXT("back taken"), CombatGridConstants::BACK_ROW_DAMAGE_TAKEN_MULTIPLIER, ROW_EFFECT_BACK, ROW_EFFECT_TOLERANCE);
	});

	It("scales the damage a row deals, on both sides", [this]()
	{
		if (!Grid) return;
		for (const int32 Team : {ROW_EFFECT_PARTY_TEAM, ROW_EFFECT_ENEMY_TEAM})
		{
			const int32 OtherTeam = (Team == ROW_EFFECT_PARTY_TEAM) ? ROW_EFFECT_ENEMY_TEAM : ROW_EFFECT_PARTY_TEAM;
			AActor *Target = SpawnIn(OtherTeam, ECombatRow::Middle);
			AActor *Front = SpawnIn(Team, ECombatRow::Front);
			AActor *Middle = SpawnIn(Team, ECombatRow::Middle);
			AActor *Back = SpawnIn(Team, ECombatRow::Back);
			if (!TestTrue(TEXT("spawned"), Target && Front && Middle && Back)) return;
			TestEqual(FString::Printf(TEXT("team %d front deals +20%%"), Team), Hit(Front, Target), ROW_EFFECT_FRONT_DAMAGE);
			TestEqual(FString::Printf(TEXT("team %d middle deals the base"), Team), Hit(Middle, Target), ROW_EFFECT_BASE_DAMAGE);
			TestEqual(FString::Printf(TEXT("team %d back deals -20%%"), Team), Hit(Back, Target), ROW_EFFECT_BACK_DAMAGE);
			Grid->ClearAllPositions();
		}
	});

	It("scales the damage a row takes, on both sides", [this]()
	{
		if (!Grid) return;
		for (const int32 Team : {ROW_EFFECT_PARTY_TEAM, ROW_EFFECT_ENEMY_TEAM})
		{
			const int32 OtherTeam = (Team == ROW_EFFECT_PARTY_TEAM) ? ROW_EFFECT_ENEMY_TEAM : ROW_EFFECT_PARTY_TEAM;
			AActor *Attacker = SpawnIn(OtherTeam, ECombatRow::Middle);
			AActor *Front = SpawnIn(Team, ECombatRow::Front);
			AActor *Middle = SpawnIn(Team, ECombatRow::Middle);
			AActor *Back = SpawnIn(Team, ECombatRow::Back);
			if (!TestTrue(TEXT("spawned"), Attacker && Front && Middle && Back)) return;
			TestEqual(FString::Printf(TEXT("team %d front takes +20%%"), Team), Hit(Attacker, Front), ROW_EFFECT_FRONT_DAMAGE);
			TestEqual(FString::Printf(TEXT("team %d middle takes the base"), Team), Hit(Attacker, Middle), ROW_EFFECT_BASE_DAMAGE);
			TestEqual(FString::Printf(TEXT("team %d back takes -20%%"), Team), Hit(Attacker, Back), ROW_EFFECT_BACK_DAMAGE);
			Grid->ClearAllPositions();
		}
	});
}

#endif
