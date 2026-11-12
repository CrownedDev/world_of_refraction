// AIActionPipsSpec.cpp
// B-170f (ActionPips.md §3.6, Q-170f2): the AI in an action-pips fight (wor.Turns.ActionPips 1).
// - The spend-or-bank rule (AIActionPips::DecideSpendOrBank), branch by branch; bonus actions above Max are spent (Q-170f2).
// - Affordability: a pick the bank can't pay is skipped; within Max it is held; with the switch off nothing changes.
// - The worth of the action effects (Gain / Lose Action, Max Actions Up / Down), signed by side; 0 with the switch off.
// - Through a real orchestrator (setup as MultiActionTurnSpec / BalanceSimOneFightSpec): an AI with 2 actions takes
//   two in one turn with the switch on and one with it off; an AI whose best pick is worth nothing ends its turn
//   (End Turn) and keeps its action banked.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "AI/AIActionPips.h"
#include "AI/AIDecisionManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/CombatOrchestrator.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	const TCHAR *const AI_PIPS_SWITCH = TEXT("wor.Turns.ActionPips");
	const TCHAR *const AI_PIPS_THINK_SCALE = TEXT("wor.AI.ThinkDelayScale");
	const TCHAR *const AI_PIPS_INSTANT_IMPACTS = TEXT("wor.Combat.InstantImpacts");
	constexpr int32 AI_PIPS_ON = 1;
	constexpr int32 AI_PIPS_OFF = 0;
	constexpr float AI_PIPS_THINK_LIVE = 1.0f;
	constexpr float AI_PIPS_THINK_NOW = 0.0f;

	/** A default character: regain 1, Max 2. */
	constexpr int32 AI_PIPS_REGAIN = 1;
	constexpr int32 AI_PIPS_MAX = 2;
	constexpr int32 AI_PIPS_COST_ONE = 1;
	constexpr int32 AI_PIPS_COST_TWO = 2;
	constexpr int32 AI_PIPS_COST_ABOVE_MAX = 3;
	/** Q-170f2: bonus actions above Max (an Emerald, a Gain Action effect). */
	constexpr int32 AI_PIPS_BONUS = 1;

	constexpr float AI_PIPS_PICK_SCORE = 20.0f;
	/** More than twice the pick (AI_BANK_FOR_HELD_PICK_FACTOR 0.5): worth banking for. */
	constexpr float AI_PIPS_BIG_HELD_SCORE = 50.0f;
	/** Less than twice the pick: not worth a turn's wait. */
	constexpr float AI_PIPS_SMALL_HELD_SCORE = 30.0f;
	/** Beats the held pick, so only the above-Max rule keeps it from being held. */
	constexpr float AI_PIPS_ABOVE_MAX_SCORE = 100.0f;
	/** Far above any test combatant's EP: a skill nobody can pay in Energy. */
	constexpr int32 AI_PIPS_UNPAYABLE_ENERGY = 100000;
	constexpr float AI_PIPS_ROLL_LOW = 0.1f;
	constexpr float AI_PIPS_ROLL_HIGH = 0.9f;
	constexpr float AI_PIPS_THREAT = 40.0f;

	constexpr int32 AI_PIPS_STRIKE_DAMAGE = 5;
	constexpr int32 AI_PIPS_STRIKE_HITS = 1;
	constexpr int32 AI_PIPS_ACTION_EFFECT_SIZE = 1;
	constexpr int32 AI_PIPS_ACTION_EFFECT_TURNS = 2;
	/** Test-world ticks are 0.01s: room for the foe's actions, each waiting up to the defense failsafe (8s). */
	constexpr int32 AI_PIPS_FIGHT_FRAME_CAP = 3000;
}

BEGIN_DEFINE_SPEC(FAIActionPipsSpec, "WoR.AI.ActionPips",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UAIDecisionManager *AI = nullptr;
	UActionExecutedRecorder *Recorder = nullptr;
	TArray<UObject *> Rooted;
	int32 SwitchBefore = AI_PIPS_OFF;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	static IConsoleVariable *CVar(const TCHAR *Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	void SetSwitch(int32 Value)
	{
		if (IConsoleVariable *Switch = CVar(AI_PIPS_SWITCH))
		{
			Switch->Set(Value, ECVF_SetByConsole); // console priority, as MultiActionTurnSpec
		}
	}

	template <class T>
	T *NewRooted()
	{
		T *Object = NewObject<T>(GetTransientPackage());
		Object->AddToRoot();
		Rooted.Add(Object);
		return Object;
	}

	static UCharacterDataComponent *DataOf(AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	/** An AI combatant whose weapon attack is a montage-less strike of Damage costing Cost actions
	 *  (as BalanceSimOneFightSpec's SpawnArmed: the weapon is owned, or combat start clears it). */
	AActor *SpawnArmedAI(int32 Damage, int32 Cost)
	{
		UAbilityData *Strike = NewRooted<UAbilityData>();
		Strike->Name = TEXT("AI pips strike");
		Strike->BaseDamage = Damage;
		Strike->HitCount = AI_PIPS_STRIKE_HITS;
		Strike->bIsAttack = true;
		Strike->ActionCost = Cost;
		UWeaponData *Weapon = NewRooted<UWeaponData>();
		Weapon->WeaponAttack = Strike;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon, /*bAIControlled=*/true);
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		if (UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr)
		{
			FWeaponInventoryEntry Owned;
			Owned.Weapon = Weapon;
			Inventory->Weapons.Add(Owned);
		}
		return Actor;
	}

	/** Bare Hero (player-driven, so it waits) against Foe; the fight runs the mode the switch asks for. */
	bool StartFight(int32 SwitchValue, AActor *InFoe = nullptr)
	{
		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator) || !TestNotNull(TEXT("AIDecisionManager"), AI))
		{
			return false;
		}
		SetSwitch(SwitchValue);
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = InFoe ? InFoe : TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		if (!TestTrue(TEXT("Combatants spawned"), Hero && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero}, {Foe}, EAIDifficulty::Medium);
		Recorder = NewRooted<UActionExecutedRecorder>();
		Recorder->Listen(Orchestrator);
		return TestTrue(TEXT("Hero opens the fight"), Orchestrator->GetCurrentActor() == Hero) &&
			   TestEqual(TEXT("The fight runs the mode the switch asked for"), Orchestrator->IsActionPipsFight(), SwitchValue == AI_PIPS_ON);
	}

	/** Hero defends (ends its turn in both modes), then ticks until a later turn of Hero's starts (Foe's turn between). */
	bool PlayFoeTurn()
	{
		const int32 HeroTurn = Orchestrator->GetCurrentTurnNumber();
		FAction Defend;
		Defend.ActionType = EActionType::Defend;
		Orchestrator->SubmitActionAsync(Defend);
		const auto BackToHero = [this, HeroTurn]()
		{
			return Orchestrator->GetCurrentActor() == Hero && Orchestrator->GetCurrentTurnNumber() != HeroTurn;
		};
		for (int32 Frame = 0; Frame < AI_PIPS_FIGHT_FRAME_CAP && !BackToHero(); ++Frame)
		{
			TestWorld->Tick();
		}
		return TestTrue(TEXT("Foe's turn passed and Hero's came back"), BackToHero());
	}

	int32 CountFoeActions(EActionType Type) const
	{
		return Recorder->Entries.FilterByPredicate([this, Type](const UActionExecutedRecorder::FEntry &Entry)
		{
			return Entry.Actor.Get() == Foe && Entry.ActionType == Type;
		}).Num();
	}

	/** An enemy-aimed ability carrying one action-effect payload. */
	UAbilityData *NewActionEffectAbility(ESkillEffectType Type, ETargetType PayloadTarget, int32 Duration = 0)
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = Type;
		Payload.Value = AI_PIPS_ACTION_EFFECT_SIZE;
		Payload.Duration = Duration;
		Payload.Target = PayloadTarget;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Def = NewRooted<UEffectDefinition>();
		Def->Effects.Add(Effect);
		UAbilityData *Ability = NewRooted<UAbilityData>();
		Ability->Name = TEXT("AI pips action effect");
		Ability->TargetType = ETargetType::Enemy;
		Ability->ReferencedEffects.Add(Def);
		return Ability;
	}

	static FAISpendOrBankInput ScoredPick(int32 Current, float Score)
	{
		FAISpendOrBankInput In;
		In.CurrentActions = Current;
		In.MaxActions = AI_PIPS_MAX;
		In.PickActionCost = AI_PIPS_COST_ONE;
		In.PickScore = Score;
		In.bPickIsScored = true;
		return In;
	}
END_DEFINE_SPEC(FAIActionPipsSpec)

void FAIActionPipsSpec::Define()
{
	using namespace AIActionPipsConstants;

	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		IConsoleVariable *Switch = CVar(AI_PIPS_SWITCH);
		SwitchBefore = Switch ? Switch->GetInt() : AI_PIPS_OFF;
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		for (UObject *Object : Rooted)
		{
			Object->RemoveFromRoot();
		}
		Rooted.Reset();
		Recorder = nullptr;
		SetSwitch(SwitchBefore);
		Orchestrator = nullptr;
		AI = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("The spend-or-bank rule", [this]()
	{
		It("spends a pick worth something", [this]()
		{
			const FAISpendOrBankDecision Out = AIActionPips::DecideSpendOrBank(ScoredPick(AI_PIPS_REGAIN, AI_PIPS_PICK_SCORE));
			TestTrue(TEXT("Spend"), Out.Choice == EAISpendOrBank::Spend);
			TestFalse(TEXT("With a reason"), Out.Reason.IsEmpty());
		});

		It("banks a pick worth nothing, unless the bank is already at Max", [this]()
		{
			TestTrue(TEXT("Below Max: bank"),
					 AIActionPips::DecideSpendOrBank(ScoredPick(AI_PIPS_REGAIN, AI_END_TURN_SCORE_FLOOR)).Choice == EAISpendOrBank::Bank);
			TestTrue(TEXT("At Max: spend (banking would waste the regain)"),
					 AIActionPips::DecideSpendOrBank(ScoredPick(AI_PIPS_MAX, AI_END_TURN_SCORE_FLOOR)).Choice == EAISpendOrBank::Spend);
		});

		It("banks for a held pick worth the wait, within Max only", [this]()
		{
			FAISpendOrBankInput In = ScoredPick(AI_PIPS_REGAIN, AI_PIPS_PICK_SCORE);
			In.HeldActionCost = AI_PIPS_COST_TWO;
			In.HeldScore = AI_PIPS_BIG_HELD_SCORE;
			TestTrue(TEXT("Worth more than twice the pick: bank"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Bank);

			In.HeldScore = AI_PIPS_SMALL_HELD_SCORE;
			TestTrue(TEXT("Not worth the wait: spend"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Spend);

			In.HeldScore = AI_PIPS_BIG_HELD_SCORE;
			In.HeldActionCost = AI_PIPS_COST_ABOVE_MAX;
			TestTrue(TEXT("Above Max it can never be paid: spend"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Spend);
		});

		It("spends Defend and picks made by need, whatever they score", [this]()
		{
			FAISpendOrBankInput Defend = ScoredPick(AI_PIPS_REGAIN, AI_END_TURN_SCORE_FLOOR);
			Defend.bPickEndsTurn = true;
			Defend.PickActionCost = 0;
			TestTrue(TEXT("Defend"), AIActionPips::DecideSpendOrBank(Defend).Choice == EAISpendOrBank::Spend);

			FAISpendOrBankInput Heal = ScoredPick(AI_PIPS_REGAIN, AI_END_TURN_SCORE_FLOOR);
			Heal.bPickIsScored = false;
			TestTrue(TEXT("A heal / cleanse / fallback"), AIActionPips::DecideSpendOrBank(Heal).Choice == EAISpendOrBank::Spend);
		});

		It("spends bonus actions above Max on the turn it has them, Easy too (Q-170f2)", [this]()
		{
			FAISpendOrBankInput In = ScoredPick(AI_PIPS_MAX + AI_PIPS_BONUS, AI_END_TURN_SCORE_FLOOR);
			TestTrue(TEXT("A pick worth nothing is still spent above Max"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Spend);
			In.bEasy = true;
			In.bActedThisTurn = true;
			In.EasyRoll = AI_PIPS_ROLL_LOW;
			TestTrue(TEXT("Easy doesn't stop early above Max"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Spend);
			In.CurrentActions = AI_PIPS_MAX;
			TestTrue(TEXT("Back at Max, Easy may stop early again"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Bank);
		});

		It("lets Easy stop early only after its first action of a turn", [this]()
		{
			FAISpendOrBankInput In = ScoredPick(AI_PIPS_REGAIN, AI_END_TURN_SCORE_FLOOR);
			In.bEasy = true;
			In.EasyRoll = AI_PIPS_ROLL_LOW;
			TestTrue(TEXT("First action: Easy spends, even a pick worth nothing"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Spend);
			In.bActedThisTurn = true;
			TestTrue(TEXT("After it, a low roll stops"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Bank);
			In.EasyRoll = AI_PIPS_ROLL_HIGH;
			TestTrue(TEXT("A high roll goes on"), AIActionPips::DecideSpendOrBank(In).Choice == EAISpendOrBank::Spend);
		});

		It("values an action change by side: one action of the unit's threat, a Max change half", [this]()
		{
			const float One = AI_PIPS_THREAT * AI_ACTION_WORTH_IN_ACTIONS;
			TestEqual(TEXT("A gain on its own side"), AIActionPips::ActionChangeWorth(1, AI_PIPS_THREAT, true, true, false), One);
			TestEqual(TEXT("A loss on a foe"), AIActionPips::ActionChangeWorth(1, AI_PIPS_THREAT, false, false, false), One);
			TestEqual(TEXT("A gain on a foe"), AIActionPips::ActionChangeWorth(1, AI_PIPS_THREAT, false, true, false), -One);
			TestEqual(TEXT("A loss on its own side"), AIActionPips::ActionChangeWorth(1, AI_PIPS_THREAT, true, false, false), -One);
			TestEqual(TEXT("Max Actions Up on its own side"), AIActionPips::ActionChangeWorth(1, AI_PIPS_THREAT, true, true, true),
					  AI_PIPS_THREAT * AI_MAX_ACTION_WORTH_IN_ACTIONS);
		});
	});

	Describe("Affordability", [this]()
	{
		It("skips a pick the bank can't pay, holds one within Max, never one above it", [this]()
		{
			if (!StartFight(AI_PIPS_ON) || !TestEqual(TEXT("Hero's bank is the regain"), DataOf(Hero)->CurrentActions, AI_PIPS_REGAIN))
			{
				return;
			}
			AI->PickNotes = FAIPickNotes();
			TestTrue(TEXT("Cost 1 is paid"), AI->CanPayActionsOrHold(Hero, AI_PIPS_COST_ONE, AI_PIPS_PICK_SCORE));
			TestFalse(TEXT("Cost 2 waits"), AI->CanPayActionsOrHold(Hero, AI_PIPS_COST_TWO, AI_PIPS_BIG_HELD_SCORE));
			TestEqual(TEXT("... held"), AI->PickNotes.HeldActionCost, AI_PIPS_COST_TWO);
			TestFalse(TEXT("Cost 3 is above Max"), AI->CanPayActionsOrHold(Hero, AI_PIPS_COST_ABOVE_MAX, AI_PIPS_ABOVE_MAX_SCORE));
			TestEqual(TEXT("... never held"), AI->PickNotes.HeldActionCost, AI_PIPS_COST_TWO);
		});

		It("holds a pricey skill only when its Energy can be paid", [this]()
		{
			if (!StartFight(AI_PIPS_ON))
			{
				return;
			}
			UAbilityData *Pricey = NewRooted<UAbilityData>();
			Pricey->Name = TEXT("AI pips pricey skill");
			Pricey->ActionCost = AI_PIPS_COST_TWO;
			const TArray<USkillDataBase *> Candidates{Pricey};
			float BestScore = 0.0f;
			float UnaffordableBest = 0.0f;
			int32 UnaffordableCost = 0;

			AI->PickNotes = FAIPickNotes();
			AI->ChooseBestSkill(Hero, Foe, Candidates, BestScore, UnaffordableBest, UnaffordableCost);
			TestEqual(TEXT("Energy payable, cost 2 over a bank of 1: held"), AI->PickNotes.HeldActionCost, AI_PIPS_COST_TWO);

			Pricey->BaseEnergyCost = AI_PIPS_UNPAYABLE_ENERGY;
			AI->PickNotes = FAIPickNotes();
			AI->ChooseBestSkill(Hero, Foe, Candidates, BestScore, UnaffordableBest, UnaffordableCost);
			TestEqual(TEXT("Energy unpayable: never held"), AI->PickNotes.HeldActionCost, 0);
		});

		It("changes nothing with the switch off", [this]()
		{
			if (!StartFight(AI_PIPS_OFF))
			{
				return;
			}
			TestTrue(TEXT("Any cost passes"), AI->CanPayActionsOrHold(Hero, AI_PIPS_COST_ABOVE_MAX, AI_PIPS_PICK_SCORE));
		});
	});

	Describe("Scoring the action effects", [this]()
	{
		It("values Lose Action on the foe and Gain Action on itself, and counts a gain on the foe against", [this]()
		{
			if (!StartFight(AI_PIPS_ON))
			{
				return;
			}
			const float FoeAction = AI->GetActionWorth(Foe, Hero);
			const float OwnAction = AI->GetActionWorth(Hero, Foe);
			TestTrue(TEXT("Each unit has a threat"), FoeAction > 0.0f && OwnAction > 0.0f);
			TestEqual(TEXT("Lose Action on the foe"),
					  AI->ScoreActionEffects(Hero, Foe, NewActionEffectAbility(ESkillEffectType::LoseActions, ETargetType::Enemy)), FoeAction);
			TestEqual(TEXT("Gain Action on itself"),
					  AI->ScoreActionEffects(Hero, Foe, NewActionEffectAbility(ESkillEffectType::GainActions, ETargetType::Self)), OwnAction);
			TestEqual(TEXT("Gain Action on the foe counts against"),
					  AI->ScoreActionEffects(Hero, Foe, NewActionEffectAbility(ESkillEffectType::GainActions, ETargetType::Enemy)), -FoeAction);
			TestEqual(TEXT("Max Actions Down on the foe: half an action"),
					  AI->ScoreActionEffects(Hero, Foe, NewActionEffectAbility(ESkillEffectType::MaxActionsDebuff, ETargetType::Enemy,
																			 AI_PIPS_ACTION_EFFECT_TURNS)),
					  FoeAction * AI_MAX_ACTION_WORTH_IN_ACTIONS);
		});

		It("scores nothing with the switch off", [this]()
		{
			if (!StartFight(AI_PIPS_OFF))
			{
				return;
			}
			TestEqual(TEXT("No worth"), AI->ScoreActionEffects(Hero, Foe, NewActionEffectAbility(ESkillEffectType::LoseActions, ETargetType::Enemy)),
					  0.0f);
		});
	});

	Describe("Through the orchestrator", [this]()
	{
		BeforeEach([this]()
		{
			if (IConsoleVariable *Think = CVar(AI_PIPS_THINK_SCALE))
			{
				Think->Set(AI_PIPS_THINK_NOW, ECVF_SetByCode);
			}
			if (IConsoleVariable *Instant = CVar(AI_PIPS_INSTANT_IMPACTS))
			{
				Instant->Set(AI_PIPS_ON, ECVF_SetByCode);
			}
		});

		AfterEach([this]()
		{
			if (IConsoleVariable *Think = CVar(AI_PIPS_THINK_SCALE))
			{
				Think->Set(AI_PIPS_THINK_LIVE, ECVF_SetByCode);
			}
			if (IConsoleVariable *Instant = CVar(AI_PIPS_INSTANT_IMPACTS))
			{
				Instant->Set(AI_PIPS_OFF, ECVF_SetByCode);
			}
		});

		It("lets an AI with 2 actions take both in one turn with the switch on", [this]()
		{
			if (!StartFight(AI_PIPS_ON, SpawnArmedAI(AI_PIPS_STRIKE_DAMAGE, AI_PIPS_COST_ONE)))
			{
				return;
			}
			DataOf(Foe)->GainActions(AI_PIPS_MAX - AI_PIPS_REGAIN, /*bMayPassMax=*/false);
			if (!TestEqual(TEXT("Foe banks 2"), DataOf(Foe)->CurrentActions, AI_PIPS_MAX) || !PlayFoeTurn())
			{
				return;
			}
			TestEqual(TEXT("Foe struck twice in its turn"), CountFoeActions(EActionType::Ability), AI_PIPS_MAX);
			TestEqual(TEXT("Its bank is spent"), DataOf(Foe)->CurrentActions, 0);
			TestTrue(TEXT("The debug line says it spent"), AI->GetLastSpendOrBankString().Contains(TEXT(": spend")));
		});

		It("keeps one action a turn with the switch off", [this]()
		{
			if (!StartFight(AI_PIPS_OFF, SpawnArmedAI(AI_PIPS_STRIKE_DAMAGE, AI_PIPS_COST_ONE)))
			{
				return;
			}
			DataOf(Foe)->GainActions(AI_PIPS_MAX - AI_PIPS_REGAIN, /*bMayPassMax=*/false); // ignored outside an action-pips fight
			if (!PlayFoeTurn())
			{
				return;
			}
			TestEqual(TEXT("Foe struck once"), CountFoeActions(EActionType::Ability), 1);
			TestTrue(TEXT("No spend-or-bank decision"), AI->GetLastSpendOrBankString().IsEmpty());
		});

		It("ends its turn (End Turn) and banks when its best pick is worth nothing", [this]()
		{
			if (!StartFight(AI_PIPS_ON, SpawnArmedAI(/*Damage=*/0, AI_PIPS_COST_ONE)) || !PlayFoeTurn())
			{
				return;
			}
			TestEqual(TEXT("Foe didn't strike"), CountFoeActions(EActionType::Ability), 0);
			TestEqual(TEXT("Nor defend"), CountFoeActions(EActionType::Defend), 0);
			TestEqual(TEXT("Its action stays banked, plus nothing (no regain on a first turn)"), DataOf(Foe)->CurrentActions, AI_PIPS_REGAIN);
			TestTrue(TEXT("The debug line says it banked"), AI->GetLastSpendOrBankString().Contains(TEXT(": bank")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
