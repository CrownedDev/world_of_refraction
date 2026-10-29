// TurnSpeedTieBreakSpec.cpp
// B-289: the turn-speed numbers (ET-04: base 10, stat cap 15, gear ceiling 20) and the turn-order
// tie-break chain (ET-08), one case per step. Each tie-break case sets two combatants level on
// every earlier step and makes every later step (and the party-first fallback) favour the loser,
// so the case fails if its step is missing, inverted or moved below a later one.
// ET-08 step 6 (higher Spirit) is not tested: with Mind+Body+Spirit, Body and Mind all equal,
// Spirit is equal too, so that step can never decide (see TurnManager.cpp ShouldBreakTieInFavor).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/StatLayers.h"
#include "Combat/CombatConstants.h"
#include "Combat/TurnManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	/** ET-04's numbers, as the rules page states them. */
	constexpr int32 SPEED_TIES_RULE_BASE = 10;
	constexpr int32 SPEED_TIES_RULE_STAT_CAP = 15;
	constexpr int32 SPEED_TIES_RULE_GEAR_CEILING = 20;

	/** Far past max investment — the stat term lands on its cap. */
	constexpr int32 SPEED_TIES_OVERCAP_POINTS = 500;
	/** Gear points that lift a capped 15 part of the way to 20 (15 x 1.18 = 17.7). */
	constexpr int32 SPEED_TIES_GEAR_POINTS_PART_WAY = 5;
	/** Gear points far past what reaching the ceiling needs. */
	constexpr int32 SPEED_TIES_GEAR_POINTS_PAST_CEILING = 1000;

	/** A turn speed buff that takes a base of 10 to an effective 11. */
	constexpr float SPEED_TIES_BUFF_PERCENT = 10.0f;
	constexpr int32 SPEED_TIES_BUFF_ID = 2891;
	constexpr int32 SPEED_TIES_BUFF_TURNS = 3;
	/** The base an unbuffed rival needs to match that buffed 11. */
	constexpr int32 SPEED_TIES_BASE_PLUS_ONE = SPEED_TIES_RULE_BASE + 1;

	/** One point or world level above the rival. */
	constexpr int32 SPEED_TIES_ONE_ABOVE = 1;
	/** Turns recorded for the three-combatant team / slot cases (one full round). */
	constexpr int32 SPEED_TIES_ROUND_TURNS = 3;
}

BEGIN_DEFINE_SPEC(FTurnSpeedTieBreakSpec, "WoR.TurnManager.SpeedAndTies",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;

	/** The cached base turn speed turn order holds for Actor (its combatant entry's in a fight). */
	int32 BaseTurnSpeedOf(const AActor *Actor) const
	{
		FWoRStatLayers Layers;
		if (!TurnManager || !Actor || !TurnManager->GetTurnSpeedLayers(Actor, Layers))
		{
			return INDEX_NONE;
		}
		return FMath::RoundToInt(Layers.StatBase);
	}

	/** A combatant with a loadout whose primary weapon carries GearPoints of Bonus Turn Speed. */
	AActor *SpawnGeared(int32 TurnSpeedPoints, int32 GearPoints)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->TurnSpeed = TurnSpeedPoints;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("Inventory"), Inventory) ||
			!TestTrue(TEXT("An active loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return nullptr;
		}
		Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.StatBonus.BonusTurnSpeed = GearPoints;
		TestEqual(TEXT("The gear's turn speed points are active"), Loadout->GetActiveStatBonus(Actor).BonusTurnSpeed, GearPoints);
		return Actor;
	}

	/** Actor of each turn from the start of the fight, the first turn (taken inside InitializeCombat) included. */
	TArray<AActor *> RecordTurns(const TArray<AActor *> &Local, const TArray<AActor *> &Opposing, int32 NumTurns)
	{
		TArray<AActor *> Order;
		auto IsMissing = [](const AActor *Actor) { return Actor == nullptr; };
		if (!TurnManager || Local.ContainsByPredicate(IsMissing) || Opposing.ContainsByPredicate(IsMissing))
		{
			return Order; // setup failure already recorded; the empty order fails the caller's checks
		}
		TurnManager->InitializeCombat(Local, Opposing);
		for (int32 Turn = 0; Turn < NumTurns; ++Turn)
		{
			if (Turn > 0)
			{
				TurnManager->AdvanceToNextTurn();
			}
			Order.Add(TurnManager->GetCurrentActor());
		}
		return Order;
	}

	/** One-on-one: the opposing combatant must take the first turn, beating the party-first fallback. */
	void ExpectOpposingFirst(UCharacterData *LocalData, UCharacterData *OpposingData, const TCHAR *Step)
	{
		AActor *Local = TestWorld->SpawnCombatant(LocalData);
		AActor *Opposing = TestWorld->SpawnCombatant(OpposingData);
		const TArray<AActor *> Order = RecordTurns({Local}, {Opposing}, 1);
		TestTrue(FString::Printf(TEXT("%s: the enemy acts first"), Step), Order.Num() == 1 && Order[0] == Opposing);
	}
END_DEFINE_SPEC(FTurnSpeedTieBreakSpec)

void FTurnSpeedTieBreakSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("TurnManager subsystem"), TurnManager);
	});

	AfterEach([this]()
	{
		if (TurnManager)
		{
			TurnManager->EndCombat();
		}
		TurnManager = nullptr;
		TestWorld.Reset();
	});

	Describe("Turn speed (ET-04)", [this]()
	{
		It("starts at base 10 with no Turn Speed points", [this]()
		{
			AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestEqual(TEXT("Base turn speed"), BaseTurnSpeedOf(Hero), SPEED_TIES_RULE_BASE);
			TestEqual(TEXT("TURN_SPEED_BASE"), FMath::RoundToInt(CombatConstants::TURN_SPEED_BASE), SPEED_TIES_RULE_BASE);
		});

		It("caps at 15 from stats alone", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->TurnSpeed = SPEED_TIES_OVERCAP_POINTS;
			Data->WorldSpiritLevel = WorldStatConstants::LIVE_MAX; // the Spirit pillar rides inside the cap too
			AActor *Hero = TestWorld->SpawnCombatant(Data);
			TestEqual(TEXT("Capped turn speed"), BaseTurnSpeedOf(Hero), SPEED_TIES_RULE_STAT_CAP);
			TestEqual(TEXT("TURN_SPEED_CAP"), FMath::RoundToInt(CombatConstants::TURN_SPEED_CAP), SPEED_TIES_RULE_STAT_CAP);
		});

		It("lets Bonus Turn Speed gear multiply past 15", [this]()
		{
			AActor *Hero = SpawnGeared(SPEED_TIES_OVERCAP_POINTS, SPEED_TIES_GEAR_POINTS_PART_WAY);
			const int32 Expected = FMath::RoundToInt(CombatConstants::TURN_SPEED_CAP *
				(1.0f + SPEED_TIES_GEAR_POINTS_PART_WAY * CombatConstants::TURN_SPEED_PER_POINT));
			const int32 Base = BaseTurnSpeedOf(Hero);
			TestEqual(TEXT("Capped stat x gear, rounded"), Base, Expected);
			TestTrue(FString::Printf(TEXT("Past 15 and under 20 (%d)"), Base),
					 Base > SPEED_TIES_RULE_STAT_CAP && Base < SPEED_TIES_RULE_GEAR_CEILING);
		});

		It("stops gear at the ceiling of 20", [this]()
		{
			AActor *Hero = SpawnGeared(SPEED_TIES_OVERCAP_POINTS, SPEED_TIES_GEAR_POINTS_PAST_CEILING);
			TestEqual(TEXT("Gear ceiling"), BaseTurnSpeedOf(Hero), SPEED_TIES_RULE_GEAR_CEILING);
			TestEqual(TEXT("TURN_SPEED_GEAR_CEILING"), FMath::RoundToInt(CombatConstants::TURN_SPEED_GEAR_CEILING),
					  SPEED_TIES_RULE_GEAR_CEILING);
		});

		It("keeps the base the fight caches equal to the base outside it", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->TurnSpeed = SPEED_TIES_OVERCAP_POINTS;
			AActor *Hero = TestWorld->SpawnCombatant(Data);
			AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			RecordTurns({Hero}, {Enemy}, 1);
			TestEqual(TEXT("Hero's cached base"), BaseTurnSpeedOf(Hero), SPEED_TIES_RULE_STAT_CAP);
			TestEqual(TEXT("Enemy's cached base"), BaseTurnSpeedOf(Enemy), SPEED_TIES_RULE_BASE);
		});
	});

	Describe("Tie-break (ET-08)", [this]()
	{
		It("step 1: higher base turn speed wins, buffs ignored", [this]()
		{
			// Local: base 10, buffed to an effective 11. Enemy: base 11. Same effective speed, so a tie.
			UCharacterData *EnemyData = FWoRTestWorld::NewCharacterData();
			EnemyData->TurnSpeed = FMath::CeilToInt(1.0f / CombatConstants::TURN_SPEED_PER_POINT); // 10 + ~1.01, rounds to 11
			AActor *Local = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Opposing = TestWorld->SpawnCombatant(EnemyData);
			USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("Skill effect manager"), Effects) || !TurnManager || !Local || !Opposing)
			{
				return;
			}
			Effects->ApplyEffect(Local, FActiveSkillEffect::CreateBuff(TEXT("Tie-break speed buff"), SPEED_TIES_BUFF_ID,
				ESkillEffectType::TurnSpeedBuff, SPEED_TIES_BUFF_PERCENT, SPEED_TIES_BUFF_TURNS), Local);

			const TArray<AActor *> Order = RecordTurns({Local}, {Opposing}, 1);
			TestEqual(TEXT("Local base"), BaseTurnSpeedOf(Local), SPEED_TIES_RULE_BASE);
			TestEqual(TEXT("Enemy base"), BaseTurnSpeedOf(Opposing), SPEED_TIES_BASE_PLUS_ONE);
			TestEqual(TEXT("Local effective (buffed)"), TurnManager->GetEffectiveTurnSpeed(Local), SPEED_TIES_BASE_PLUS_ONE);
			TestEqual(TEXT("Enemy effective"), TurnManager->GetEffectiveTurnSpeed(Opposing), SPEED_TIES_BASE_PLUS_ONE);
			TestTrue(TEXT("Higher base: the enemy acts first"), Order.Num() == 1 && Order[0] == Opposing);
		});

		It("step 2: higher action speed wins", [this]()
		{
			UCharacterData *EnemyData = FWoRTestWorld::NewCharacterData();
			EnemyData->ActionSpeed = SPEED_TIES_ONE_ABOVE;
			ExpectOpposingFirst(FWoRTestWorld::NewCharacterData(), EnemyData, TEXT("Higher action speed"));
		});

		It("step 3: lower Mind+Body+Spirit world-stat total wins", [this]()
		{
			// The local side's extra Mind would also win step 5, so only step 3 can hand the enemy the turn.
			UCharacterData *LocalData = FWoRTestWorld::NewCharacterData();
			LocalData->WorldMindLevel = SPEED_TIES_ONE_ABOVE;
			ExpectOpposingFirst(LocalData, FWoRTestWorld::NewCharacterData(), TEXT("Lower world-stat total"));
		});

		It("step 4: equal totals, higher Body wins", [this]()
		{
			UCharacterData *LocalData = FWoRTestWorld::NewCharacterData();
			LocalData->WorldMindLevel = SPEED_TIES_ONE_ABOVE; // wins step 5 if step 4 is skipped
			UCharacterData *EnemyData = FWoRTestWorld::NewCharacterData();
			EnemyData->WorldBodyLevel = SPEED_TIES_ONE_ABOVE;
			ExpectOpposingFirst(LocalData, EnemyData, TEXT("Higher Body"));
		});

		It("step 5: equal totals and Body, higher Mind wins", [this]()
		{
			UCharacterData *LocalData = FWoRTestWorld::NewCharacterData();
			LocalData->WorldSpiritLevel = SPEED_TIES_ONE_ABOVE; // wins step 6 if step 5 is skipped
			UCharacterData *EnemyData = FWoRTestWorld::NewCharacterData();
			EnemyData->WorldMindLevel = SPEED_TIES_ONE_ABOVE;
			ExpectOpposingFirst(LocalData, EnemyData, TEXT("Higher Mind"));
		});

		It("step 7: your party before enemies, ahead of slot order", [this]()
		{
			// Party slot 1 against enemy slot 0 after party slot 0 has acted: team decides before slot.
			AActor *LocalFirst = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *LocalSecond = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *Opposing = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			const TArray<AActor *> Order = RecordTurns({LocalFirst, LocalSecond}, {Opposing}, SPEED_TIES_ROUND_TURNS);
			TestTrue(TEXT("Party slot 1 before enemy slot 0"),
					 Order.Num() == SPEED_TIES_ROUND_TURNS && Order[1] == LocalSecond && Order[2] == Opposing);
		});

		It("step 8: lower slot wins within a side", [this]()
		{
			AActor *Local = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *OpposingFirst = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			AActor *OpposingSecond = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			const TArray<AActor *> Order = RecordTurns({Local}, {OpposingFirst, OpposingSecond}, SPEED_TIES_ROUND_TURNS);
			TestTrue(TEXT("Party, then enemy slot 0, then enemy slot 1"),
					 Order.Num() == SPEED_TIES_ROUND_TURNS && Order[0] == Local && Order[1] == OpposingFirst &&
						 Order[2] == OpposingSecond);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
