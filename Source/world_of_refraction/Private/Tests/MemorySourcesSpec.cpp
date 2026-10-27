// MemorySourcesSpec.cpp
// B-209 (D-100): every memory source unlocks into the run and the journal, noting the source: a
// boss or mini-boss beaten, a reaction that fired, a special spot reached, a bound item's memory by
// its upgrade level and/or trigger conditions (both when both are set). Each once.

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
	constexpr int32 SOURCES_TEST_UPGRADE_LEVEL = 2;

	/** Tier steps above Base (clamped to S). */
	EItemTier SourcesTestTierAbove(EItemTier Base, int32 Steps)
	{
		return static_cast<EItemTier>(FMath::Min(static_cast<int32>(Base) + Steps, static_cast<int32>(EItemTier::S_Tier)));
	}

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

	/** A combatant for Data wearing Weapon as its primary, levelled to Tier. */
	AActor *SpawnWearing(UCharacterData *Data, UWeaponData *Weapon, EItemTier Tier)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = SourcesTestAddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = SourcesTestAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Weapon;
		Active.PrimaryWeapon.WeaponEntry.Weapon = Weapon;
		Active.PrimaryWeapon.WeaponEntry.Tier = Tier;
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
		TestEqual(TEXT("One unlocked"), MemoryUnlock::UnlockAtFightStart(GameInstance, {Fired}, {}, {}), 1);
		TestTrue(TEXT("From a reaction"), InRunAndJournalFrom(EMemorySource::Reaction));
	});

	It("unlocks a bound memory with an owner trigger only when its owner wears it", [this]()
	{
		UCharacterData *Lord = FWoRTestWorld::NewCharacterData();
		UCharacterData *Other = FWoRTestWorld::NewCharacterData();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->BoundOwner = Lord;
		FBoundMemory &Bound = Weapon->BoundMemories.AddDefaulted_GetRef();
		Bound.Memory = Memory;
		Bound.bNeedsOwnerWearing = true;

		TestEqual(TEXT("Another wearer: none"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Other, Weapon, Weapon->Tier)}, {}), 0);
		TestEqual(TEXT("A level-up can't hold a trigger"), MemoryUnlock::UnlockBoundOnLevelUp(GameInstance, Weapon, EItemTier::S_Tier), 0);
		TestFalse(TEXT("Not in the journal"), Journal->HasMemory(Memory));
		TestEqual(TEXT("Its owner: one"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Weapon->Tier)}, {}), 1);
		TestTrue(TEXT("From bound gear"), InRunAndJournalFrom(EMemorySource::BoundGear));
	});

	It("unlocks a bound memory when the item is levelled far enough", [this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		FBoundMemory &Bound = Weapon->BoundMemories.AddDefaulted_GetRef();
		Bound.Memory = Memory;
		Bound.UpgradeLevel = SOURCES_TEST_UPGRADE_LEVEL;

		TestEqual(TEXT("One level short: none"), MemoryUnlock::UnlockBoundOnLevelUp(
			GameInstance, Weapon, SourcesTestTierAbove(Weapon->Tier, SOURCES_TEST_UPGRADE_LEVEL - 1)), 0);
		TestEqual(TEXT("Reached: one"), MemoryUnlock::UnlockBoundOnLevelUp(
			GameInstance, Weapon, SourcesTestTierAbove(Weapon->Tier, SOURCES_TEST_UPGRADE_LEVEL)), 1);
		TestTrue(TEXT("From bound gear"), InRunAndJournalFrom(EMemorySource::BoundGear));
	});

	It("needs both the upgrade level and the trigger when both are set", [this]()
	{
		UCharacterData *Lord = FWoRTestWorld::NewCharacterData();
		UCharacterData *Rival = FWoRTestWorld::NewCharacterData();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->BoundOwner = Lord;
		FBoundMemory &Bound = Weapon->BoundMemories.AddDefaulted_GetRef();
		Bound.Memory = Memory;
		Bound.UpgradeLevel = 1;
		Bound.bNeedsOwnerWearing = true;
		Bound.FacingCharacter = Rival;
		const TArray<AActor *> FacingRival = {TestWorld->SpawnCombatant(Rival)};
		const EItemTier Levelled = SourcesTestTierAbove(Weapon->Tier, 1);

		TestEqual(TEXT("Not levelled: none"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Weapon->Tier)}, FacingRival), 0);
		TestEqual(TEXT("Not facing the rival: none"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Levelled)}, {}), 0);
		TestEqual(TEXT("Levelled, worn by its owner, facing the rival: one"),
				  MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Levelled)}, FacingRival), 1);
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
