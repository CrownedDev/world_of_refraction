// MemorySourcesSpec.cpp
// B-209 (D-100): every memory source unlocks into the run and the journal, noting the source: a
// boss or mini-boss beaten, a reaction that fired, a special spot reached, bound gear its owner
// wears. Each once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Boss/BossData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/MemorySpot.h"
#include "Memory/MemoryUnlock.h"
#include "Reaction/ReactionTrigger.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	const FName SOURCES_TEST_MEMORY_NAME(TEXT("Memory_SourcesSpec"));

	template <class TComponent>
	TComponent *SourcesTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FMemorySourcesSpec, "WoR.Memory.Sources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UGameInstance *GameInstance = nullptr;
	UMemoryJournalSubsystem *Journal = nullptr;
	UTrialRunSubsystem *TrialRun = nullptr;
	UMemoryData *Memory = nullptr;

	/** A combatant for Data wearing Weapon as its primary. */
	AActor *SpawnWearing(UCharacterData *Data, UWeaponData *Weapon)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = SourcesTestAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = SourcesTestAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
		return Actor;
	}

	bool InRunAndJournalFrom(EMemorySource Source)
	{
		return TrialRun->GetRunMemories().Contains(Memory) && Journal->HasMemory(Memory) && Journal->GetSource(Memory) == Source;
	}
END_DEFINE_SPEC(FMemorySourcesSpec)

void FMemorySourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		GameInstance = TestWorld->GetWorld() ? TestWorld->GetWorld()->GetGameInstance() : nullptr;
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		Memory = NewObject<UMemoryData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(),
										UMemoryData::StaticClass(), SOURCES_TEST_MEMORY_NAME));
		TestTrue(TEXT("Subsystems"), GameInstance && Journal && TrialRun);
	});

	AfterEach([this]()
	{
		GameInstance = nullptr;
		Journal = nullptr;
		TrialRun = nullptr;
		Memory = nullptr;
		TestWorld.Reset();
	});

	It("unlocks a beaten boss's memory, once", [this]()
	{
		UBossData *Boss = NewObject<UBossData>(GetTransientPackage());
		TestFalse(TEXT("No memory authored: nothing"), MemoryUnlock::UnlockBossDrop(GameInstance, Boss));
		Boss->MemoryOnDefeat = Memory;
		TestTrue(TEXT("Unlocked"), MemoryUnlock::UnlockBossDrop(GameInstance, Boss));
		TestTrue(TEXT("In the run and the journal, from a boss"), InRunAndJournalFrom(EMemorySource::Boss));
		TestFalse(TEXT("Not again"), MemoryUnlock::UnlockBossDrop(GameInstance, Boss));
		TestTrue(TEXT("Journal names the source"), Journal->GetJournalString().Contains(TEXT("Boss or mini-boss beaten")));
	});

	It("unlocks a fired reaction's memory", [this]()
	{
		FReactionTrigger Trigger;
		Trigger.UnlockMemory = Memory;
		FFiredReaction Fired;
		Fired.Trigger = &Trigger;
		TestEqual(TEXT("One unlocked"), MemoryUnlock::UnlockAtFightStart(GameInstance, {Fired}, {}), 1);
		TestTrue(TEXT("From a reaction"), InRunAndJournalFrom(EMemorySource::Reaction));
	});

	It("unlocks bound gear's memories only for its owner", [this]()
	{
		UCharacterData *Lord = FWoRTestWorld::NewCharacterData();
		UCharacterData *Other = FWoRTestWorld::NewCharacterData();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->BoundOwner = Lord;
		Weapon->BoundMemories.Add(Memory);

		TestEqual(TEXT("Another wearer: none"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Other, Weapon)}), 0);
		TestFalse(TEXT("Not in the journal"), Journal->HasMemory(Memory));
		TestEqual(TEXT("Its owner: one"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon)}), 1);
		TestTrue(TEXT("From bound gear"), InRunAndJournalFrom(EMemorySource::BoundGear));
	});

	It("unlocks a map spot's memory once per run", [this]()
	{
		AMemorySpot *Spot = TestWorld->GetWorld()->SpawnActor<AMemorySpot>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("Spot"), Spot))
		{
			return;
		}
		Spot->Memory = Memory;
		TestTrue(TEXT("Reached"), Spot->Reach());
		TestTrue(TEXT("From a map spot"), InRunAndJournalFrom(EMemorySource::MapSpot));
		TestTrue(TEXT("Recorded as found"), TrialRun->HasFound(Spot->GetFeatureId()));
		TestFalse(TEXT("Spent for the run"), Spot->Reach());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
