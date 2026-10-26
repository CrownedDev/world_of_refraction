// EncounterDataSpec.cpp
// B-074: an enemy team authored as UEncounterData takes the field, up to three strong; a field
// encounter without one fights its own pawn alone.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Encounter/EncounterData.h"

BEGIN_DEFINE_SPEC(FEncounterDataSpec, "WoR.Combat.EncounterData",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	/** A team of Count fresh enemies. */
	UEncounterData *NewEncounter(int32 Count)
	{
		UEncounterData *Encounter = NewObject<UEncounterData>(GetTransientPackage());
		for (int32 i = 0; i < Count; ++i)
		{
			Encounter->Enemies.Add(FWoRTestWorld::NewCharacterData());
		}
		return Encounter;
	}
END_DEFINE_SPEC(FEncounterDataSpec)

void FEncounterDataSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("fields at most three enemies and skips empty entries", [this]()
	{
		UEncounterData *Encounter = NewEncounter(EncounterConstants::ENCOUNTER_MAX_ENEMIES + 1);
		Encounter->Enemies.Insert(nullptr, 1);
		const TArray<UCharacterData *> Roster = Encounter->GetEnemyRoster();
		TestEqual(TEXT("Three take the field"), Roster.Num(), EncounterConstants::ENCOUNTER_MAX_ENEMIES);
		TestFalse(TEXT("No empty entry"), Roster.Contains(nullptr));
		TestTrue(TEXT("In authored order"), Roster.Num() > 1 && Roster[0] == Encounter->Enemies[0] && Roster[1] == Encounter->Enemies[2]);
	});

	It("falls back to the encounter's own pawn when no team is set", [this]()
	{
		UCharacterData *Pawn = FWoRTestWorld::NewCharacterData();
		TestEqual(TEXT("No encounter data: the pawn alone"), UEncounterData::BuildOpposingRoster(nullptr, Pawn), TArray<UCharacterData *>{Pawn});
		TestEqual(TEXT("Empty team: the pawn alone"), UEncounterData::BuildOpposingRoster(NewEncounter(0), Pawn), TArray<UCharacterData *>{Pawn});
		TestEqual(TEXT("A team replaces the pawn"), UEncounterData::BuildOpposingRoster(NewEncounter(2), Pawn).Num(), 2);
	});

	It("starts a 1 v 3 fight from the data", [this]()
	{
		ACombatOrchestrator *Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		if (!TestNotNull(TEXT("Orchestrator"), Orchestrator)) return;
		AActor *Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), /*bAIControlled=*/true);
		TArray<AActor *> Foes;
		for (UCharacterData *Data : UEncounterData::BuildOpposingRoster(NewEncounter(EncounterConstants::ENCOUNTER_MAX_ENEMIES), nullptr))
		{
			Foes.Add(TestWorld->SpawnCombatant(Data, /*bAIControlled=*/true));
		}
		if (!TestTrue(TEXT("Everyone spawned"), Hero && Foes.Num() == EncounterConstants::ENCOUNTER_MAX_ENEMIES && !Foes.Contains(nullptr))) return;

		Orchestrator->StartCombat({Hero}, Foes);
		TestEqual(TEXT("Three enemies in the fight"), Orchestrator->GetLivingEnemies(Hero).Num(), EncounterConstants::ENCOUNTER_MAX_ENEMIES);
		Orchestrator->ForceEndCombat();
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
