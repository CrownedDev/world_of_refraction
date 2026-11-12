// BalanceSimOneFightSpec.cpp
// B-291 (docs/Design/BalanceSimulator.md §2): the headless seams. With wor.Combat.InstantImpacts 1, a skill with no
// montage lands each of its impacts at once through ResolveImpactFrame (the defended path), and with
// wor.AI.ThinkDelayScale 0 an all-AI 1v1 runs to its end in a bounded number of ticks, counting rounds through
// UTurnManager::OnRoundAdded.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "BalanceSimSignalRecorder.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	constexpr int32 SIM_TEST_HIT_COUNT = 2;
	constexpr int32 SIM_TEST_BASE_DAMAGE = 30;
	/** Next-tick timers only once the seams are on: a few frames settle one action. */
	constexpr int32 SIM_TEST_ACTION_FRAMES = 10;
	/** One battle seed for the defended-path case: the AI foe's random defense must not block every impact. */
	constexpr int32 SIM_TEST_BATTLE_SEED = 4307;
	/** A whole 1v1 at ~3 frames a turn; reaching it means a stall. */
	constexpr int32 SIM_TEST_FIGHT_TICK_CAP = 3000;

	void SetSimCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}

	void SetSimCVar(const TCHAR *Name, float Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}
}

BEGIN_DEFINE_SPEC(FBalanceSimOneFightSpec, "WoR.Balance.Sim.OneFight",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UBalanceSimSignalRecorder *Recorder = nullptr;
	UAbilityData *Strike = nullptr;

	/** A combatant whose weapon attack is the montage-less Strike, so the AI has an attack to choose (the AI reads
	 *  the weapon attack and the loadout's slotted abilities, not PresetAbilities). The weapon is also owned: combat
	 *  start clears a primary weapon that is not in the inventory, which left the AI only Defend. */
	AActor *SpawnArmed(bool bAIControlled)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = Strike;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon, bAIControlled);
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		if (UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr)
		{
			FWeaponInventoryEntry Owned;
			Owned.Weapon = Weapon;
			Inventory->Weapons.Add(Owned);
		}
		return Actor;
	}

	static int32 CurrentHPOf(const AActor *Actor)
	{
		const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return Char ? Char->CurrentHP : 0;
	}
END_DEFINE_SPEC(FBalanceSimOneFightSpec)

void FBalanceSimOneFightSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Strike = NewObject<UAbilityData>(GetTransientPackage());
		Strike->Name = TEXT("Montage-less Strike");
		Strike->BaseDamage = SIM_TEST_BASE_DAMAGE;
		Strike->HitCount = SIM_TEST_HIT_COUNT;
		Strike->bIsAttack = true;
		Strike->AddToRoot();
		Recorder = NewObject<UBalanceSimSignalRecorder>();
		Recorder->AddToRoot();
		Recorder->Listen(TestWorld->GetSubsystem<UActionExecutor>(), TestWorld->GetSubsystem<UTurnManager>(),
						 TestWorld->GetSubsystem<UStatusBuildupManager>());
		SetSimCVar(TEXT("wor.Combat.InstantImpacts"), 1);
		SetSimCVar(TEXT("wor.AI.ThinkDelayScale"), 0.0f);
	});

	AfterEach([this]()
	{
		SetSimCVar(TEXT("wor.Combat.InstantImpacts"), 0);
		SetSimCVar(TEXT("wor.AI.ThinkDelayScale"), 1.0f);
		SetSimCVar(TEXT("wor.Battle.Seed"), 0);
		if (Orchestrator && Orchestrator->GetCombatState() == ECombatState::InProgress)
		{
			Orchestrator->ForceEndCombat();
		}
		if (Recorder)
		{
			Recorder->RemoveFromRoot();
			Recorder = nullptr;
		}
		if (Strike)
		{
			Strike->RemoveFromRoot();
			Strike = nullptr;
		}
		Orchestrator = nullptr;
		TestWorld.Reset();
	});

	It("lands each impact of a montage-less ability at once, on the defended path", [this]()
	{
		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator)) return;
		// Equal stats: the tie-break gives the local party (Hero) turn 1. The foe is AI, so its defense is
		// synthesized per impact (the defended path); with the live think delay its own turn waits far past
		// these frames and adds no impacts.
		SetSimCVar(TEXT("wor.AI.ThinkDelayScale"), 1.0f);
		SetSimCVar(TEXT("wor.Battle.Seed"), SIM_TEST_BATTLE_SEED);
		AActor *Hero = SpawnArmed(false);
		AActor *Foe = SpawnArmed(true);
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe)) return;
		Orchestrator->StartCombat({Hero}, {Foe});
		const int32 FoeHPBefore = CurrentHPOf(Foe);

		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Strike;
		Action.Targets.Add(Foe);
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(SIM_TEST_ACTION_FRAMES);

		TestEqual(TEXT("one impact frame per hit"), Recorder->ImpactIndices.Num(), SIM_TEST_HIT_COUNT);
		TestTrue(TEXT("impacts in order"),
				 Recorder->ImpactIndices.Num() == SIM_TEST_HIT_COUNT && Recorder->ImpactIndices[0] == 0 && Recorder->ImpactIndices[1] == 1);
		// SIM_TEST_ACTION_FRAMES is far under the 800-frame failsafe, so the damage came from the impacts, not the
		// failsafe's undefended close.
		TestTrue(TEXT("the foe took damage within a few frames"), CurrentHPOf(Foe) < FoeHPBefore);
	});

	It("runs an all-AI 1v1 to its end, counting rounds", [this]()
	{
		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator)) return;
		AActor *Hero = SpawnArmed(true);
		AActor *Foe = SpawnArmed(true);
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe)) return;
		Orchestrator->StartCombat({Hero}, {Foe});

		int32 Ticks = 0;
		while (Orchestrator->GetCombatState() == ECombatState::InProgress && Ticks < SIM_TEST_FIGHT_TICK_CAP)
		{
			TestWorld->Tick();
			++Ticks;
		}
		if (Orchestrator->GetCombatState() == ECombatState::InProgress)
		{
			const AActor *Current = Orchestrator->GetCurrentActor();
			AddError(FString::Printf(TEXT("stalled after %d ticks: rounds %d, impacts %d, HP hero %d / foe %d, turn of %s"),
									 Ticks, Recorder->Rounds, Recorder->ImpactIndices.Num(), CurrentHPOf(Hero), CurrentHPOf(Foe),
									 Current ? *Current->GetName() : TEXT("none")));
		}
		TestNotEqual(TEXT("the fight ended (not stalled at the tick cap)"), Orchestrator->GetCombatState(), ECombatState::InProgress);
		TestTrue(TEXT("one side is down"), CurrentHPOf(Hero) <= 0 || CurrentHPOf(Foe) <= 0);
		TestTrue(TEXT("rounds were counted"), Recorder->Rounds > 0);
		TestTrue(TEXT("impacts landed"), Recorder->ImpactIndices.Num() > 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
