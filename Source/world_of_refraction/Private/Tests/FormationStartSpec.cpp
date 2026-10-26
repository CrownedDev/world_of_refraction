// FormationStartSpec.cpp
// B-149 (D-042): a fight starts each member in its chosen formation cell; an unset or taken
// cell falls back to the centre-out auto-placement; enemies start in their placeholder row
// (Q-149: a Refractor in the Back row, anyone else in the Front); row bonuses read the start row.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

namespace
{
	constexpr int32 FORMATION_LEFT_COLUMN = 0;
	constexpr int32 FORMATION_CENTRE_COLUMN = 1;
	constexpr int32 FORMATION_RIGHT_COLUMN = 2;
	constexpr int32 FORMATION_MEMBER_SLOT = 1;
	constexpr int32 LOCAL_TEAM = 0;
	constexpr int32 OPPOSING_TEAM = 1;
}

BEGIN_DEFINE_SPEC(FFormationStartSpec, "WoR.Grid.FormationStart",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCombatGridSubsystem *Grid = nullptr;

	bool HasCell(AActor *Actor, ECombatRow Row, int32 Column)
	{
		FCombatGridPosition Position;
		return Grid && Grid->GetActorPosition(Actor, Position) && Position.Row == Row && Position.Column == Column;
	}
END_DEFINE_SPEC(FFormationStartSpec)

void FFormationStartSpec::Define()
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

	It("starts a member in its chosen cell and auto-places one without a choice", [this]()
	{
		AActor *Chosen = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Unset = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Grid || !TestTrue(TEXT("Spawned"), Chosen && Unset)) return;
		Grid->AssignTeamWithPreferences({Chosen, Unset}, LOCAL_TEAM,
										{FGridStartPreference::Cell(ECombatRow::Back, FORMATION_LEFT_COLUMN), FGridStartPreference()});
		TestTrue(TEXT("Chosen member is Back/Left"), HasCell(Chosen, ECombatRow::Back, FORMATION_LEFT_COLUMN));
		TestTrue(TEXT("Unset member is Middle/Centre"), HasCell(Unset, ECombatRow::Middle, FORMATION_CENTRE_COLUMN));
		TestEqual(TEXT("Back row damage bonus applies"), Grid->GetDamageModifier(Chosen), CombatGridConstants::BACK_ROW_DAMAGE_MODIFIER);
	});

	It("auto-places the second of two members on one cell", [this]()
	{
		AActor *First = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *Second = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Grid || !TestTrue(TEXT("Spawned"), First && Second)) return;
		const FGridStartPreference SameCell = FGridStartPreference::Cell(ECombatRow::Front, FORMATION_RIGHT_COLUMN);
		Grid->AssignTeamWithPreferences({First, Second}, LOCAL_TEAM, {SameCell, SameCell});
		TestTrue(TEXT("First keeps the cell"), HasCell(First, ECombatRow::Front, FORMATION_RIGHT_COLUMN));
		// It keeps its chosen row: the next free cell centre-out.
		TestTrue(TEXT("Second is auto-placed in its row"), HasCell(Second, ECombatRow::Front, FORMATION_CENTRE_COLUMN));
	});

	It("starts a fight with the party's chosen cells and enemies in their placeholder rows", [this]()
	{
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		ACombatOrchestrator *Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Orchestrator"), Orchestrator) || !Grid) return;

		UCharacterData *LeaderData = FWoRTestWorld::NewCharacterData();
		UCharacterData *MemberData = FWoRTestWorld::NewCharacterData();
		for (UCharacterData *Data : {LeaderData, MemberData})
		{
			FPartyMember Member;
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Party->Members.Add(Member);
		}
		PartySession->SetMemberFormation(FORMATION_MEMBER_SLOT, ECombatRow::Back, FORMATION_LEFT_COLUMN);

		UCharacterData *FighterData = FWoRTestWorld::NewCharacterData();
		FighterData->CharacterClass = ECharacterClass::Generic;
		AActor *Leader = TestWorld->SpawnCombatant(LeaderData);
		AActor *Member = TestWorld->SpawnCombatant(MemberData);
		AActor *RefractorFoe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		AActor *FighterFoe = TestWorld->SpawnCombatant(FighterData);
		if (!TestTrue(TEXT("Spawned"), Leader && Member && RefractorFoe && FighterFoe)) return;

		Orchestrator->StartCombat({Leader, Member}, {RefractorFoe, FighterFoe});
		TestTrue(TEXT("Member starts in its chosen cell"), HasCell(Member, ECombatRow::Back, FORMATION_LEFT_COLUMN));
		TestTrue(TEXT("Unplaced leader starts Middle/Centre"), HasCell(Leader, ECombatRow::Middle, FORMATION_CENTRE_COLUMN));
		TestTrue(TEXT("Refractor enemy starts in the Back row"), Grid->GetActorRow(RefractorFoe) == ECombatRow::Back);
		TestTrue(TEXT("Other enemy starts in the Front row"), Grid->GetActorRow(FighterFoe) == ECombatRow::Front);
		Orchestrator->ForceEndCombat();
	});
}

#endif
