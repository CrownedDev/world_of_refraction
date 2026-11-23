// RowMoveSpec.cpp
// D-096 (Crown, 2026-10-26; B-180): a character can step one row forward or back on their turn,
// free and once per turn; skills can push or pull a target one row. Row bonuses (PA-30) apply from
// the new row at once. B-342 (D-134): three rows, no columns, so an ally in the row never blocks.
// B-588 (D-263): the Position command moves to any row, still once per turn; push and pull stay one row.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr bool ROW_MOVE_FORWARD = true;
	constexpr bool ROW_MOVE_BACK = false;
	/** Test-world ticks are 0.01s; plenty for the ally's and foe's turns to pass. */
	constexpr int32 ROW_MOVE_NEXT_TURN_MAX_FRAMES = 2400;
}

BEGIN_DEFINE_SPEC(FRowMoveSpec, "WoR.Combat.RowMove",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UCombatGridSubsystem *Grid = nullptr;
	AActor *Hero = nullptr;
	AActor *Ally = nullptr;
	AActor *Foe = nullptr;

	/** Equal stats: the tie-break gives the local party turn 1. */
	bool StartFight()
	{
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Fight set up"), Orchestrator && Grid && Hero && Ally && Foe))
		{
			return false;
		}
		Orchestrator->StartCombat({Hero, Ally}, {Foe});
		if (Orchestrator->GetCurrentActor() == Ally)
		{
			Swap(Hero, Ally); // "Hero" is whoever acts first
		}
		return TestTrue(TEXT("a party member acts first"), Orchestrator->GetCurrentActor() == Hero);
	}

	FCombatGridPosition PositionOf(AActor *Actor) const
	{
		FCombatGridPosition Position;
		Grid->GetActorPosition(Actor, Position);
		return Position;
	}

	ECombatRow RowOf(AActor *Actor) const
	{
		return PositionOf(Actor).Row;
	}

	/** Puts Actor at Row / Column on its side (test setup, not a player move). */
	void Place(AActor *Actor, ECombatRow Row, int32 Column)
	{
		FCombatGridPosition Position = PositionOf(Actor);
		Position.Row = Row;
		Position.Column = Column;
		TestTrue(TEXT("placed"), Grid->AssignPosition(Actor, Position));
	}

	void ApplyRowEffect(AActor *Target, ESkillEffectType Type)
	{
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		FActiveSkillEffect Effect;
		Effect.EffectName = TEXT("Row move test");
		Effect.EffectType = Type;
		Effect.RemainingTurns = 0; // instant
		Effects->ApplyEffect(Target, Effect, Hero);
	}
END_DEFINE_SPEC(FRowMoveSpec)

void FRowMoveSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		Grid = nullptr;
		Hero = Ally = Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("a free row move", [this]()
	{
		It("steps forward once per turn, and the front-row bonus applies at once", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			TestTrue(TEXT("starts in the middle"), RowOf(Hero) == ECombatRow::Middle);
			FText Reason;
			TestTrue(TEXT("stepped forward"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			TestTrue(TEXT("now in front"), RowOf(Hero) == ECombatRow::Front);
			TestTrue(TEXT("front-row damage bonus"), Grid->GetDamageModifier(Hero) > 1.0f);

			TestFalse(TEXT("a second step is refused"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_BACK, Reason));
			TestFalse(TEXT("with a reason"), Reason.IsEmpty());
			TestTrue(TEXT("still in front"), RowOf(Hero) == ECombatRow::Front);
		});

		It("steps back", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			FText Reason;
			TestTrue(TEXT("stepped back"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_BACK, Reason));
			TestTrue(TEXT("now at the back"), RowOf(Hero) == ECombatRow::Back);
		});

		It("steps into a row an ally stands in: three rows, no columns (B-342, D-134)", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			Place(Ally, ECombatRow::Front, PositionOf(Hero).Column);
			TestTrue(TEXT("not refused"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_FORWARD).IsEmpty());
			FText Reason;
			TestTrue(TEXT("moved"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			TestTrue(TEXT("hero in front"), RowOf(Hero) == ECombatRow::Front);
			TestTrue(TEXT("ally stays in front"), RowOf(Ally) == ECombatRow::Front);
		});

		It("comes back on your next turn", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			FText Reason;
			TestTrue(TEXT("stepped forward"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			FAction Defend;
			Defend.ActionType = EActionType::Defend;
			Orchestrator->SubmitActionAsync(Defend);
			for (int32 Frame = 0; Frame < ROW_MOVE_NEXT_TURN_MAX_FRAMES && Orchestrator->GetCurrentActor() != Hero; ++Frame)
			{
				Orchestrator->SubmitActionAsync(Defend); // the ally and the foe just defend (refused while one runs)
				TestWorld->Tick(1);
			}
			if (!TestTrue(TEXT("hero's turn again"), Orchestrator->GetCurrentActor() == Hero))
			{
				return;
			}
			TestTrue(TEXT("the free move is back"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_BACK).IsEmpty());
		});

		It("comes back on your next turn after End Turn (B-436g QA)", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			FText Reason;
			TestTrue(TEXT("stepped forward"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			FAction EndTurn;
			EndTurn.ActionType = EActionType::EndTurn;
			const int32 StepTurn = Orchestrator->GetCurrentTurnNumber();
			Orchestrator->SubmitActionAsync(EndTurn);
			FAction Defend;
			Defend.ActionType = EActionType::Defend;
			// The hero's NEXT turn: a later turn number, so a turn End Turn failed to end can't pass.
			const auto IsHerosNextTurn = [&]() { return Orchestrator->GetCurrentActor() == Hero && Orchestrator->GetCurrentTurnNumber() > StepTurn; };
			for (int32 Frame = 0; Frame < ROW_MOVE_NEXT_TURN_MAX_FRAMES && !IsHerosNextTurn(); ++Frame)
			{
				if (Orchestrator->GetCurrentActor() != Hero)
				{
					Orchestrator->SubmitActionAsync(Defend); // the ally and the foe just defend; the hero's turn only End Turn ends
				}
				TestWorld->Tick(1);
			}
			if (!TestTrue(TEXT("hero's next turn"), IsHerosNextTurn()))
			{
				return;
			}
			TestTrue(TEXT("the free move is back"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_BACK).IsEmpty());
		});

		It("is refused while stunned", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			TestTrue(TEXT("allowed before the stun"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_FORWARD).IsEmpty());
			FActiveSkillEffect Stun;
			Stun.EffectName = TEXT("Row move test stun");
			Stun.EffectType = ESkillEffectType::Stun;
			Stun.RemainingTurns = 1;
			TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(Hero, Stun, Foe);
			TestFalse(TEXT("refused"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_FORWARD).IsEmpty());
		});

		It("is refused in the last row and off your turn", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			Place(Hero, ECombatRow::Front, PositionOf(Hero).Column);
			TestFalse(TEXT("no row past the front"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_FORWARD).IsEmpty());
			TestTrue(TEXT("but back is fine"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_BACK).IsEmpty());
			TestFalse(TEXT("not the ally's turn"), Orchestrator->GetRowMoveRefusedReason(Ally, ROW_MOVE_BACK).IsEmpty());
			TestFalse(TEXT("not the foe's turn"), Orchestrator->GetRowMoveRefusedReason(Foe, ROW_MOVE_BACK).IsEmpty());
		});
	});

	Describe("the Position move (B-588, D-263)", [this]()
	{
		It("goes from the front to the back in one move, once per turn", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			Place(Hero, ECombatRow::Front, PositionOf(Hero).Column);
			FText Reason;
			TestTrue(TEXT("front to back"), Orchestrator->TryMoveToRow(Hero, ECombatRow::Back, Reason));
			TestTrue(TEXT("now at the back"), RowOf(Hero) == ECombatRow::Back);
			TestTrue(TEXT("back-row defence bonus at once"), Grid->GetDefenseModifier(Hero) > 1.0f);
			TestFalse(TEXT("a second move is refused"), Orchestrator->TryMoveToRow(Hero, ECombatRow::Front, Reason));
			TestFalse(TEXT("with a reason"), Reason.IsEmpty());
			TestFalse(TEXT("and so is a step: one move a turn"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			TestTrue(TEXT("still at the back"), RowOf(Hero) == ECombatRow::Back);
		});

		It("is refused after the turn's step", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			FText Reason;
			TestTrue(TEXT("stepped forward"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			TestFalse(TEXT("refused"), Orchestrator->TryMoveToRow(Hero, ECombatRow::Back, Reason));
			TestTrue(TEXT("still in front"), RowOf(Hero) == ECombatRow::Front);
		});

		It("is refused for the row you are in, off your turn and while stunned", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			TestTrue(TEXT("starts in the middle"), RowOf(Hero) == ECombatRow::Middle);
			TestFalse(TEXT("already there"), Orchestrator->GetMoveToRowRefusedReason(Hero, ECombatRow::Middle).IsEmpty());
			TestTrue(TEXT("front is fine"), Orchestrator->GetMoveToRowRefusedReason(Hero, ECombatRow::Front).IsEmpty());
			TestTrue(TEXT("back is fine"), Orchestrator->GetMoveToRowRefusedReason(Hero, ECombatRow::Back).IsEmpty());
			TestFalse(TEXT("not the ally's turn"), Orchestrator->GetMoveToRowRefusedReason(Ally, ECombatRow::Front).IsEmpty());
			TestFalse(TEXT("not the foe's turn"), Orchestrator->GetMoveToRowRefusedReason(Foe, ECombatRow::Front).IsEmpty());
			FActiveSkillEffect Stun;
			Stun.EffectName = TEXT("Position test stun");
			Stun.EffectType = ESkillEffectType::Stun;
			Stun.RemainingTurns = 1;
			TestWorld->GetSubsystem<USkillEffectManager>()->ApplyEffect(Hero, Stun, Foe);
			FText Reason;
			TestFalse(TEXT("stunned: refused"), Orchestrator->TryMoveToRow(Hero, ECombatRow::Front, Reason));
			TestTrue(TEXT("still in the middle"), RowOf(Hero) == ECombatRow::Middle);
		});

		It("sends each row as its own free command", [this]()
		{
			for (const ECombatRow Row : {ECombatRow::Front, ECombatRow::Middle, ECombatRow::Back})
			{
				ECombatRow Sent = ECombatRow::Middle == Row ? ECombatRow::Front : ECombatRow::Middle;
				TestTrue(TEXT("a Position command"), CombatFreeCommands::GetMoveToRow(CombatFreeCommands::MoveTo(Row), Sent));
				TestTrue(TEXT("the same row back"), Sent == Row);
			}
			ECombatRow Unused;
			TestFalse(TEXT("a weapon switch is not one"), CombatFreeCommands::GetMoveToRow(ECombatFreeCommand::SwitchWeapon, Unused));
		});
	});

	Describe("push and pull", [this]()
	{
		It("pushes a target back and pulls it forward one row", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			ApplyRowEffect(Foe, ESkillEffectType::PushBack);
			TestTrue(TEXT("pushed to the back"), RowOf(Foe) == ECombatRow::Back);
			TestTrue(TEXT("back-row defence bonus"), Grid->GetDefenseModifier(Foe) > 1.0f);
			ApplyRowEffect(Foe, ESkillEffectType::PullForward);
			ApplyRowEffect(Foe, ESkillEffectType::PullForward);
			TestTrue(TEXT("pulled to the front"), RowOf(Foe) == ECombatRow::Front);
		});

		It("pushes into a row an ally stands in (B-342, D-134)", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			Place(Hero, ECombatRow::Middle, PositionOf(Hero).Column);
			Place(Ally, ECombatRow::Back, PositionOf(Hero).Column);
			ApplyRowEffect(Hero, ESkillEffectType::PushBack);
			TestTrue(TEXT("hero pushed to the back"), RowOf(Hero) == ECombatRow::Back);
			TestTrue(TEXT("ally stays at the back"), RowOf(Ally) == ECombatRow::Back);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
