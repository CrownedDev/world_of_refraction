// CombatGridSpec.cpp
// B-135: the combat grid as it works today. One actor per cell; placing an actor moves it out of its
// old cell; rows set the damage and defense modifiers (an actor off the grid reads Middle, no change);
// a push or pull steps one row and stops at the edge or at an occupied cell; a swap needs two actors on
// the same team. The turn-level row step (D-096) is WoR.Combat.RowMove; start cells are
// WoR.Combat.FormationStart.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Combat/Grid/CombatGridSubsystem.h"

namespace
{
	constexpr int32 GRID_TEST_PLAYER_TEAM = 0;
	constexpr int32 GRID_TEST_ENEMY_TEAM = 1;
	constexpr int32 GRID_TEST_COLUMN = 1;
	constexpr int32 GRID_TEST_OTHER_COLUMN = 0;
	constexpr int32 GRID_TEST_INVALID_TEAM = 2;
	constexpr float GRID_TEST_TOLERANCE = 0.001f;
}

BEGIN_DEFINE_SPEC(FCombatGridSpec, "WoR.Combat.Grid",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCombatGridSubsystem *Grid = nullptr;
	AActor *Hero = nullptr;
	AActor *Ally = nullptr;
	AActor *Foe = nullptr;

	static FCombatGridPosition Cell(int32 Team, ECombatRow Row, int32 Column = GRID_TEST_COLUMN)
	{
		return FCombatGridPosition(Team, Row, Column);
	}

	bool Place(AActor *Actor, ECombatRow Row, int32 Team = GRID_TEST_PLAYER_TEAM, int32 Column = GRID_TEST_COLUMN)
	{
		return Grid->AssignPosition(Actor, Cell(Team, Row, Column));
	}
END_DEFINE_SPEC(FCombatGridSpec)

void FCombatGridSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Grid = TestWorld->GetSubsystem<UCombatGridSubsystem>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("CombatGridSubsystem"), Grid) || !TestNotNull(TEXT("Hero"), Hero) ||
			!TestNotNull(TEXT("Ally"), Ally) || !TestNotNull(TEXT("Foe"), Foe))
		{
			Grid = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		if (Grid)
		{
			Grid->ClearAllPositions();
		}
		Grid = nullptr;
		Hero = nullptr;
		Ally = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("holds one actor per cell, and placing an actor again moves it", [this]()
	{
		if (!Grid) return;
		TestTrue(TEXT("Hero placed"), Place(Hero, ECombatRow::Front));
		TestFalse(TEXT("The cell is taken"), Place(Ally, ECombatRow::Front));
		TestTrue(TEXT("Re-placing in its own cell is fine"), Place(Hero, ECombatRow::Front));
		TestFalse(TEXT("An invalid team is refused"), Place(Ally, ECombatRow::Front, GRID_TEST_INVALID_TEAM));

		TestTrue(TEXT("Hero moved"), Place(Hero, ECombatRow::Back));
		TestFalse(TEXT("The old cell is free"), Grid->IsPositionOccupied(Cell(GRID_TEST_PLAYER_TEAM, ECombatRow::Front)));
		TestEqual(TEXT("The new cell holds the hero"), Grid->GetActorAtPosition(Cell(GRID_TEST_PLAYER_TEAM, ECombatRow::Back)), Hero);
		TestEqual(TEXT("Row"), Grid->GetActorRow(Hero), ECombatRow::Back);

		Grid->RemoveFromGrid(Hero);
		FCombatGridPosition Position;
		TestFalse(TEXT("Removed"), Grid->GetActorPosition(Hero, Position));
	});

	It("keeps the teams apart", [this]()
	{
		if (!Grid) return;
		Place(Hero, ECombatRow::Front);
		Place(Ally, ECombatRow::Back);
		TestTrue(TEXT("The same cell on the other team is free"), Place(Foe, ECombatRow::Front, GRID_TEST_ENEMY_TEAM));
		TestEqual(TEXT("Player team count"), Grid->GetTeamCount(GRID_TEST_PLAYER_TEAM), 2);
		TestEqual(TEXT("Enemy team count"), Grid->GetTeamCount(GRID_TEST_ENEMY_TEAM), 1);
		const TArray<AActor *> Front = Grid->GetActorsInRow(GRID_TEST_PLAYER_TEAM, ECombatRow::Front);
		TestTrue(TEXT("Only the hero in the player front row"), Front.Num() == 1 && Front.Contains(Hero));
	});

	It("sets damage and defense by row; off the grid there is no change", [this]()
	{
		if (!Grid) return;
		TestEqual(TEXT("Off the grid reads Middle"), Grid->GetActorRow(Hero), ECombatRow::Middle);
		TestNearlyEqual(TEXT("Off the grid: damage"), Grid->GetDamageModifier(Hero), 1.0f, GRID_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Off the grid: defense"), Grid->GetDefenseModifier(Hero), 1.0f, GRID_TEST_TOLERANCE);

		Place(Hero, ECombatRow::Front);
		TestNearlyEqual(TEXT("Front: damage"), Grid->GetDamageModifier(Hero), CombatGridConstants::FRONT_ROW_DAMAGE_MODIFIER, GRID_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Front: defense"), Grid->GetDefenseModifier(Hero), CombatGridConstants::FRONT_ROW_DEFENSE_MODIFIER, GRID_TEST_TOLERANCE);
		Place(Hero, ECombatRow::Middle);
		TestNearlyEqual(TEXT("Middle: damage"), Grid->GetDamageModifier(Hero), CombatGridConstants::MIDDLE_ROW_DAMAGE_MODIFIER, GRID_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Middle: defense"), Grid->GetDefenseModifier(Hero), CombatGridConstants::MIDDLE_ROW_DEFENSE_MODIFIER, GRID_TEST_TOLERANCE);
		Place(Hero, ECombatRow::Back);
		TestNearlyEqual(TEXT("Back: damage"), Grid->GetDamageModifier(Hero), CombatGridConstants::BACK_ROW_DAMAGE_MODIFIER, GRID_TEST_TOLERANCE);
		TestNearlyEqual(TEXT("Back: defense"), Grid->GetDefenseModifier(Hero), CombatGridConstants::BACK_ROW_DEFENSE_MODIFIER, GRID_TEST_TOLERANCE);
	});

	It("pushes and pulls one row at a time and stops at the edge", [this]()
	{
		if (!Grid) return;
		Place(Hero, ECombatRow::Front);
		TestTrue(TEXT("Push: Front to Middle"), Grid->PushActorBack(Hero, FVector::ZeroVector));
		TestEqual(TEXT("Middle"), Grid->GetActorRow(Hero), ECombatRow::Middle);
		TestTrue(TEXT("Push: Middle to Back"), Grid->PushActorBack(Hero, FVector::ZeroVector));
		TestFalse(TEXT("No push past the back"), Grid->PushActorBack(Hero, FVector::ZeroVector));
		TestEqual(TEXT("Still Back"), Grid->GetActorRow(Hero), ECombatRow::Back);

		TestTrue(TEXT("Pull: Back to Middle"), Grid->PullActorForward(Hero, FVector::ZeroVector));
		TestTrue(TEXT("Pull: Middle to Front"), Grid->PullActorForward(Hero, FVector::ZeroVector));
		TestFalse(TEXT("No pull past the front"), Grid->PullActorForward(Hero, FVector::ZeroVector));
		TestEqual(TEXT("Still Front"), Grid->GetActorRow(Hero), ECombatRow::Front);
		TestFalse(TEXT("An actor off the grid can't move"), Grid->PushActorBack(Ally, FVector::ZeroVector));
	});

	It("blocks a move into an occupied cell and keeps the actor where it was", [this]()
	{
		if (!Grid) return;
		Place(Hero, ECombatRow::Front);
		Place(Ally, ECombatRow::Middle);
		TestFalse(TEXT("Push blocked"), Grid->PushActorBack(Hero, FVector::ZeroVector));
		TestEqual(TEXT("Hero stays Front"), Grid->GetActorRow(Hero), ECombatRow::Front);
		TestTrue(TEXT("Moving to its own row is a no-op success"), Grid->MoveActorToRow(Hero, ECombatRow::Front, FVector::ZeroVector));
		TestTrue(TEXT("Another column is free"), Place(Ally, ECombatRow::Middle, GRID_TEST_PLAYER_TEAM, GRID_TEST_OTHER_COLUMN));
		TestTrue(TEXT("Now the push goes through"), Grid->PushActorBack(Hero, FVector::ZeroVector));
	});

	It("swaps two allies, never across teams", [this]()
	{
		if (!Grid) return;
		Place(Hero, ECombatRow::Front);
		Place(Ally, ECombatRow::Back);
		Place(Foe, ECombatRow::Front, GRID_TEST_ENEMY_TEAM);
		TestTrue(TEXT("Allies swap"), Grid->SwapActorPositions(Hero, Ally, FVector::ZeroVector));
		TestEqual(TEXT("Hero now Back"), Grid->GetActorRow(Hero), ECombatRow::Back);
		TestEqual(TEXT("Ally now Front"), Grid->GetActorRow(Ally), ECombatRow::Front);
		TestFalse(TEXT("No swap across teams"), Grid->SwapActorPositions(Hero, Foe, FVector::ZeroVector));
		TestFalse(TEXT("No swap with itself"), Grid->SwapActorPositions(Hero, Hero, FVector::ZeroVector));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
