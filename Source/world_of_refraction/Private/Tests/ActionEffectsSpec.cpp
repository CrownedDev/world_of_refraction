// ActionEffectsSpec.cpp
// B-170d (ActionPips.md §1, §3.4; Q-170f, Q-170g, Q-170d2): effects that give or take banked actions.
// - Gain / Lose Action now (Duration 0) change the bank at once; a gain may pass Max (for that turn, Q-170f2); a loss stops at 0.
// - Timed ones (Duration N) wait on the holder and land at its Nth turn start: gains first, then losses.
// - The "+1 now, -1 next turn" pair (Divinity: Original Sin 2's Adrenaline).
// - Max Actions Buff / Debuff move Max for their span, never below the floor; the regain adds nothing over Max.
// - Emerald in an action-pips fight: on an enemy, they lose their next turn; on yourself, actions by tier (Crown's
//   ladder); with the switch off it is still a bonus turn.
// - The turn-start order through a real orchestrator (MultiActionTurnSpec's deferred cases): a waiting loss that
//   empties the bank skips the turn; a delayed gain landing with a loss counts before the check.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/TurnManager.h"
#include "Equipment/Crystals/AugmentStoneConstants.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemExecutor.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	const TCHAR *const ACTION_EFFECTS_SWITCH = TEXT("wor.Turns.ActionPips");
	constexpr int32 ACTION_EFFECTS_ON = 1;
	constexpr int32 ACTION_EFFECTS_OFF = 0;
	constexpr int32 ACTION_EFFECTS_ID = 17001;
	constexpr int32 ACTION_EFFECTS_NOW = 0;
	constexpr int32 ACTION_EFFECTS_NEXT_TURN = 1;
	constexpr int32 ACTION_EFFECTS_TWO_TURNS = 2;
	constexpr int32 ACTION_EFFECTS_ONE = 1;
	constexpr int32 ACTION_EFFECTS_TWO = 2;
	constexpr int32 ACTION_EFFECTS_BIG_DEBUFF = 10;
	/** Test-world ticks are 0.01s; as MultiActionTurnSpec, long enough for a submitted action to settle. */
	constexpr int32 ACTION_EFFECTS_SETTLE_FRAMES = 1100;
	/** Tier indices into the Emerald ladder (F..S). */
	constexpr int32 EMERALD_F_INDEX = 0;
	constexpr int32 EMERALD_D_INDEX = 2;
	constexpr int32 EMERALD_S_INDEX = 6;
}

BEGIN_DEFINE_SPEC(FActionEffectsSpec, "WoR.Combat.ActionEffects",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *Effects = nullptr;
	UTurnManager *TurnManager = nullptr;
	IConsoleVariable *PipsSwitch = nullptr;
	int32 SwitchBefore = ACTION_EFFECTS_OFF;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
	UCharacterDataComponent *Char = nullptr;

	void SetSwitch(int32 Value)
	{
		if (PipsSwitch)
		{
			PipsSwitch->Set(Value, ECVF_SetByConsole);
		}
	}

	/** Hero and Foe; Hero starts with the fight-start bank (1 with no Reflex). */
	bool SpawnPair()
	{
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("Hero data"), Char) || !TestNotNull(TEXT("Foe"), Foe))
		{
			return false;
		}
		Char->ResetActionsForFight();
		return TestEqual(TEXT("Fight start = the regain"), Char->CurrentActions, ActionPipsConstants::ACTIONS_REGAIN_BASE);
	}

	EEffectApplicationResult Apply(ESkillEffectType Type, int32 Value, int32 Duration)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Action effect"), ACTION_EFFECTS_ID + static_cast<int32>(Type), Type,
																   static_cast<float>(Value), Duration);
		return Effects->ApplyEffect(Hero, Effect, Hero, TEXT("ActionEffectsSpec"));
	}
END_DEFINE_SPEC(FActionEffectsSpec)

void FActionEffectsSpec::Define()
{
	using namespace ActionPipsConstants;

	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		TestNotNull(TEXT("SkillEffectManager"), Effects);
		TestNotNull(TEXT("TurnManager"), TurnManager);
		PipsSwitch = IConsoleManager::Get().FindConsoleVariable(ACTION_EFFECTS_SWITCH);
		SwitchBefore = PipsSwitch ? PipsSwitch->GetInt() : ACTION_EFFECTS_OFF;
	});

	AfterEach([this]()
	{
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		SetSwitch(SwitchBefore);
		PipsSwitch = nullptr;
		Char = nullptr;
		Hero = Foe = nullptr;
		Effects = nullptr;
		TurnManager = nullptr;
		TestWorld.Reset();
	});

	Describe("Gain and Lose Action", [this]()
	{
		It("change the bank now: a gain passes Max for the turn, a loss stops at 0", [this]()
		{
			if (!Effects || !SpawnPair()) return;
			Apply(ESkillEffectType::GainActions, MAX_ACTIONS_BASE, ACTION_EFFECTS_NOW);
			TestEqual(TEXT("1 + 2 passes Max 2"), Char->CurrentActions, ACTIONS_REGAIN_BASE + MAX_ACTIONS_BASE);
			Char->RegainActions();
			TestEqual(TEXT("The regain adds nothing over Max"), Char->CurrentActions, ACTIONS_REGAIN_BASE + MAX_ACTIONS_BASE);
			TestFalse(TEXT("Never stored as a lasting effect"), Effects->HasEffectOfType(Hero, ESkillEffectType::GainActions));
			Apply(ESkillEffectType::LoseActions, ACTION_EFFECTS_BIG_DEBUFF, ACTION_EFFECTS_NOW);
			TestEqual(TEXT("Never below 0"), Char->CurrentActions, 0);
		});

		It("timed, wait for the holder's turn start: next turn, or in 2 turns", [this]()
		{
			if (!Effects || !SpawnPair()) return;
			Apply(ESkillEffectType::GainActions, ACTION_EFFECTS_ONE, ACTION_EFFECTS_TWO_TURNS);
			Apply(ESkillEffectType::LoseActions, ACTION_EFFECTS_ONE, ACTION_EFFECTS_NEXT_TURN);
			TestEqual(TEXT("Nothing lands now"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("Two waiting"), Char->GetWaitingActionChanges().Num(), 2);
			TestFalse(TEXT("Not stored"), Effects->HasEffectOfType(Hero, ESkillEffectType::LoseActions));

			Char->ApplyWaitingActionChanges(); // next turn start
			TestEqual(TEXT("The loss lands next turn"), Char->CurrentActions, ACTIONS_REGAIN_BASE - ACTION_EFFECTS_ONE);
			TestEqual(TEXT("The gain still waits"), Char->GetWaitingActionChanges().Num(), 1);
			Char->ApplyWaitingActionChanges(); // the one after
			TestEqual(TEXT("The gain lands in 2 turns"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("Nothing waits"), Char->GetWaitingActionChangesString(), FString(TEXT("none")));
		});

		It("make the '+1 now, -1 next turn' pair", [this]()
		{
			if (!Effects || !SpawnPair()) return;
			Apply(ESkillEffectType::GainActions, ACTION_EFFECTS_ONE, ACTION_EFFECTS_NOW);
			Apply(ESkillEffectType::LoseActions, ACTION_EFFECTS_ONE, ACTION_EFFECTS_NEXT_TURN);
			TestEqual(TEXT("+1 now"), Char->CurrentActions, ACTIONS_REGAIN_BASE + ACTION_EFFECTS_ONE);
			Char->RegainActions();
			Char->ApplyWaitingActionChanges();
			TestEqual(TEXT("-1 after next turn's regain (already at Max: no regain)"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
		});

		It("land gains before losses at the same turn start", [this]()
		{
			if (!Effects || !SpawnPair()) return;
			Char->LoseActions(ACTIONS_REGAIN_BASE);
			Char->AddWaitingActionChange(-ACTION_EFFECTS_ONE, ACTION_EFFECTS_NEXT_TURN, false);
			Char->AddWaitingActionChange(ACTION_EFFECTS_TWO, ACTION_EFFECTS_NEXT_TURN, true);
			Char->ApplyWaitingActionChanges();
			TestEqual(TEXT("0 + 2 - 1 (the loss isn't wasted on an empty bank)"), Char->CurrentActions, ACTION_EFFECTS_TWO - ACTION_EFFECTS_ONE);
		});

		It("refuse a permanent action change", [this]()
		{
			if (!Effects || !SpawnPair()) return;
			FActiveSkillEffect Permanent = FActiveSkillEffect::CreateBuff(TEXT("Permanent gain"), ACTION_EFFECTS_ID, ESkillEffectType::GainActions,
																		  ACTION_EFFECTS_ONE, ACTION_EFFECTS_NOW);
			Permanent.bPermanent = true;
			AddExpectedError(TEXT("an action change needs"), EAutomationExpectedErrorFlags::Contains, 1);
			TestEqual(TEXT("Refused"), Effects->ApplyEffect(Hero, Permanent, Hero, TEXT("ActionEffectsSpec")), EEffectApplicationResult::Rejected);
			TestEqual(TEXT("Bank unchanged"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
		});
	});

	Describe("Max Actions Buff and Debuff", [this]()
	{
		It("raise and lower Max for their span, never below the floor", [this]()
		{
			if (!Effects || !SpawnPair()) return;
			Apply(ESkillEffectType::MaxActionsBuff, ACTION_EFFECTS_ONE, ACTION_EFFECTS_NEXT_TURN);
			TestEqual(TEXT("Max 3"), Char->GetMaxActions(), MAX_ACTIONS_BASE + ACTION_EFFECTS_ONE);
			Char->RegainActions();
			Char->RegainActions();
			TestEqual(TEXT("The regain fills to the raised Max"), Char->CurrentActions, MAX_ACTIONS_BASE + ACTION_EFFECTS_ONE);

			Apply(ESkillEffectType::MaxActionsDebuff, ACTION_EFFECTS_BIG_DEBUFF, ACTION_EFFECTS_NEXT_TURN);
			TestEqual(TEXT("Max never below the floor"), Char->GetMaxActions(), MAX_ACTIONS_FLOOR);
			TestEqual(TEXT("A lowered Max never trims the bank"), Char->CurrentActions, MAX_ACTIONS_BASE + ACTION_EFFECTS_ONE);

			// Landed off the holder's turn: held through its next turn, gone as the one after starts (D-051).
			Effects->ProcessStartOfTurnEffects(Hero);
			Effects->ProcessEndOfTurnEffects(Hero);
			Effects->ProcessStartOfTurnEffects(Hero);
			TestEqual(TEXT("Back to Max 2 when both end"), Char->GetMaxActions(), MAX_ACTIONS_BASE);
		});
	});

	Describe("Emerald", [this]()
	{
		It("in an action-pips fight: actions on yourself by tier, a lost turn on an enemy", [this]()
		{
			UItemExecutor *Items = TestWorld->GetSubsystem<UItemExecutor>();
			if (!Effects || !TestNotNull(TEXT("ItemExecutor"), Items) || !TestNotNull(TEXT("the switch exists"), PipsSwitch)) return;
			SetSwitch(ACTION_EFFECTS_ON);
			if (!SpawnPair()) return;
			TurnManager->InitializeCombat({Hero}, {Foe});
			if (!TestTrue(TEXT("Rotation mode, Hero first"), TurnManager->IsRotationMode() && TurnManager->GetCurrentActor() == Hero)) return;
			const int32 Start = Char->CurrentActions;

			TestTrue(TEXT("D used"), Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::D_Tier), Hero).bSuccess);
			TestEqual(TEXT("D: +1 now"), Char->CurrentActions, Start + AugmentStoneConstants::EMERALD_ALLY_ACTIONS_GAINED[EMERALD_D_INDEX]);
			TestTrue(TEXT("S used"), Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::S_Tier), Hero).bSuccess);
			const int32 AfterS = Start + AugmentStoneConstants::EMERALD_ALLY_ACTIONS_GAINED[EMERALD_D_INDEX] +
								 AugmentStoneConstants::EMERALD_ALLY_ACTIONS_GAINED[EMERALD_S_INDEX];
			TestEqual(TEXT("S: +3 now, past Max"), Char->CurrentActions, AfterS);
			TestTrue(TEXT("F used"), Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::F_Tier), Hero).bSuccess);
			if (TestEqual(TEXT("F waits"), Char->GetWaitingActionChanges().Num(), 1))
			{
				TestEqual(TEXT("F: in 2 turns"), Char->GetWaitingActionChanges()[0].TurnsLeft,
						  AugmentStoneConstants::EMERALD_ALLY_GAIN_DELAY_TURNS[EMERALD_F_INDEX]);
			}

			TestTrue(TEXT("Used on Foe"), Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::D_Tier), Foe).bSuccess);
			TurnManager->AdvanceToNextTurn();
			TestTrue(TEXT("Foe's turn is skipped: Hero again"), TurnManager->GetCurrentActor() == Hero);
		});

		It("with the switch off, still grants a bonus turn and no actions", [this]()
		{
			UItemExecutor *Items = TestWorld->GetSubsystem<UItemExecutor>();
			if (!Effects || !TestNotNull(TEXT("ItemExecutor"), Items)) return;
			SetSwitch(ACTION_EFFECTS_OFF);
			if (!SpawnPair()) return;
			TurnManager->InitializeCombat({Hero}, {Foe});
			if (!TestTrue(TEXT("Hero takes turn 1"), TurnManager->GetCurrentActor() == Hero)) return;
			TestTrue(TEXT("Used"), Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::D_Tier), Hero).bSuccess);
			TestEqual(TEXT("No actions given"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			TurnManager->AdvanceToNextTurn();
			TestTrue(TEXT("Hero's bonus turn"), TurnManager->GetCurrentActor() == Hero && TurnManager->GetCurrentTurnType() == ETurnType::Bonus);
		});
	});

	Describe("The turn-start order (orchestrator)", [this]()
	{
		// Starts Hero vs Foe with the switch on, then ends Hero's first turn and lets Prepare queue Hero's changes on Foe's
		// turn; Foe ends its turn and Hero's turn starts (regain, the changes, the check).
		const auto RunToHeroSecondTurn = [this](TFunctionRef<void()> Prepare) -> ACombatOrchestrator *
		{
			if (!TestNotNull(TEXT("the switch exists"), PipsSwitch)) return nullptr;
			SetSwitch(ACTION_EFFECTS_ON);
			ACombatOrchestrator *Fight = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestNotNull(TEXT("Orchestrator"), Fight) || !TestNotNull(TEXT("Hero data"), Char) || !Foe) return nullptr;
			Fight->StartCombat({Hero}, {Foe});
			if (!TestTrue(TEXT("Hero opens"), Fight->GetCurrentActor() == Hero && Fight->IsActionPipsFight()))
			{
				Fight->ForceEndCombat();
				return nullptr;
			}
			FAction EndTurn;
			EndTurn.ActionType = EActionType::EndTurn;
			Fight->SubmitActionAsync(EndTurn);
			TestWorld->Tick(ACTION_EFFECTS_SETTLE_FRAMES);
			if (!TestTrue(TEXT("Foe's turn"), Fight->GetCurrentActor() == Foe))
			{
				Fight->ForceEndCombat();
				return nullptr;
			}
			TestEqual(TEXT("Hero banked its action"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			Prepare();
			Fight->SubmitActionAsync(EndTurn);
			TestWorld->Tick(ACTION_EFFECTS_SETTLE_FRAMES);
			return Fight;
		};

		It("takes an authored skill's Value as whole actions through the action executor (Value 1, Magnitude 0)", [this]()
		{
			if (!TestNotNull(TEXT("the switch exists"), PipsSwitch)) return;
			SetSwitch(ACTION_EFFECTS_ON);
			ACombatOrchestrator *Fight = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			UCharacterDataComponent *FoeData = Foe ? Foe->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			if (!TestNotNull(TEXT("Orchestrator"), Fight) || !TestNotNull(TEXT("Hero data"), Char) || !TestNotNull(TEXT("Foe data"), FoeData)) return;

			const auto Payload = [](ESkillEffectType Type, int32 Value, int32 Duration, ETargetType Target)
			{
				FSkillEffectPayload P;
				P.EffectType = Type;
				P.Value = Value;
				P.Duration = Duration;
				P.Target = Target;
				return P;
			};
			UEffectDefinition *Def = NewObject<UEffectDefinition>(GetTransientPackage());
			FSkillEffect Effect;
			Effect.Payloads.Add(Payload(ESkillEffectType::GainActions, ACTION_EFFECTS_TWO, ACTION_EFFECTS_NOW, ETargetType::Self));
			Effect.Payloads.Add(Payload(ESkillEffectType::MaxActionsBuff, ACTION_EFFECTS_ONE, ACTION_EFFECTS_TWO_TURNS, ETargetType::Self));
			Effect.Payloads.Add(Payload(ESkillEffectType::LoseActions, ACTION_EFFECTS_ONE, ACTION_EFFECTS_NEXT_TURN, ETargetType::Enemy));
			Def->Effects.Add(Effect);
			UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
			Ability->Name = TEXT("Action effects ability");
			Ability->TargetType = ETargetType::Enemy;
			Ability->ReferencedEffects.Add(Def);
			Ability->AddToRoot();

			Fight->StartCombat({Hero}, {Foe});
			if (TestTrue(TEXT("Hero opens an action-pips fight"), Fight->GetCurrentActor() == Hero && Fight->IsActionPipsFight()))
			{
				FAction Action;
				Action.ActionType = EActionType::Ability;
				Action.SkillData = Ability;
				Action.Targets.Add(Foe);
				Fight->SubmitActionAsync(Action);
				TestWorld->Tick(ACTION_EFFECTS_SETTLE_FRAMES);

				TestEqual(TEXT("1 - cost 1 + Gain 2"), Char->CurrentActions, ACTIONS_REGAIN_BASE - ACTION_COST_DEFAULT + ACTION_EFFECTS_TWO);
				TestEqual(TEXT("Max Actions Up 1"), Char->GetMaxActions(), MAX_ACTIONS_BASE + ACTION_EFFECTS_ONE);
				if (TestEqual(TEXT("Foe's loss waits"), FoeData->GetWaitingActionChanges().Num(), 1))
				{
					TestEqual(TEXT("-1, unscaled"), FoeData->GetWaitingActionChanges()[0].Change, -ACTION_EFFECTS_ONE);
				}
				TestTrue(TEXT("Hero's turn goes on"), Fight->GetCurrentActor() == Hero);
			}
			Fight->ForceEndCombat();
			TestEqual(TEXT("FinishCombat clears the foe's waiting loss (D-064)"), FoeData->GetWaitingActionChanges().Num(), 0);
			Ability->RemoveFromRoot();
		});

		It("skips Hero's turn when a waiting loss empties the bank after the regain", [this, RunToHeroSecondTurn]()
		{
			ACombatOrchestrator *Fight = RunToHeroSecondTurn([this]() { Char->AddWaitingActionChange(-MAX_ACTIONS_BASE, ACTION_EFFECTS_NEXT_TURN, false); });
			if (!Fight) return;
			TestEqual(TEXT("Regain 1 -> 2, then -2 -> 0"), Char->CurrentActions, 0);
			TestTrue(TEXT("No prompt for Hero: Foe's turn again"), Fight->GetCurrentActor() == Foe);
			Fight->ForceEndCombat();
		});

		It("counts a delayed gain landing with a loss before the check", [this, RunToHeroSecondTurn]()
		{
			ACombatOrchestrator *Fight = RunToHeroSecondTurn([this]()
			{
				Char->AddWaitingActionChange(-MAX_ACTIONS_BASE, ACTION_EFFECTS_NEXT_TURN, false);
				Char->AddWaitingActionChange(ACTION_EFFECTS_ONE, ACTION_EFFECTS_NEXT_TURN, true);
			});
			if (!Fight) return;
			TestEqual(TEXT("Regain 2, +1, -2 -> 1"), Char->CurrentActions, ACTION_EFFECTS_ONE);
			TestTrue(TEXT("Hero plays with 1 action"), Fight->GetCurrentActor() == Hero);
			Fight->ForceEndCombat();
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
