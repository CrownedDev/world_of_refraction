// RowMoveSpec.cpp
// D-096 (Crown, 2026-10-26; B-180): a character can step one row forward or back on their turn,
// free and once per turn, swapping with an ally in that cell; skills can push or pull a target
// one row. Row bonuses (PA-30) apply from the new row at once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
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

		It("is refused into an ally's cell: no swapping in battle (Q-180)", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			Place(Ally, ECombatRow::Front, PositionOf(Hero).Column);
			const ECombatRow HeroRow = RowOf(Hero);
			TestFalse(TEXT("refused before trying"), Orchestrator->GetRowMoveRefusedReason(Hero, ROW_MOVE_FORWARD).IsEmpty());
			FText Reason;
			TestFalse(TEXT("not moved"), Orchestrator->TryMoveRow(Hero, ROW_MOVE_FORWARD, Reason));
			TestTrue(TEXT("hero stays"), RowOf(Hero) == HeroRow);
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

		It("does nothing when the cell behind is taken", [this]()
		{
			if (!StartFight())
			{
				return;
			}
			Place(Ally, ECombatRow::Back, PositionOf(Hero).Column);
			ApplyRowEffect(Hero, ESkillEffectType::PushBack);
			TestTrue(TEXT("hero stays in the middle"), RowOf(Hero) == ECombatRow::Middle);
			TestTrue(TEXT("ally stays at the back"), RowOf(Ally) == ECombatRow::Back);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
