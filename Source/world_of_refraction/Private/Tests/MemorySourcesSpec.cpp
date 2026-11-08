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
#include "Memory/MemoryConditions.h"
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
	constexpr int32 SOURCES_TEST_ONE_LEVEL = 1;

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
		Bound.UpgradeLevel = SOURCES_TEST_ONE_LEVEL;
		Bound.bNeedsOwnerWearing = true;
		Bound.FacingCharacter = Rival;
		const TArray<AActor *> FacingRival = {TestWorld->SpawnCombatant(Rival)};
		const EItemTier Levelled = SourcesTestTierAbove(Weapon->Tier, SOURCES_TEST_ONE_LEVEL);

		TestEqual(TEXT("Not levelled: none"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Weapon->Tier)}, FacingRival), 0);
		TestEqual(TEXT("Not facing the rival: none"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Levelled)}, {}), 0);
		TestEqual(TEXT("Levelled, worn by its owner, facing the rival: one"),
				  MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {SpawnWearing(Lord, Weapon, Levelled)}, FacingRival), 1);
	});

	It("reads the owned copy's level, which a level-up changes, over the loadout's copy", [this]()
	{
		UCharacterData *Lord = FWoRTestWorld::NewCharacterData();
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->BoundOwner = Lord;
		FBoundMemory &Bound = Weapon->BoundMemories.AddDefaulted_GetRef();
		Bound.Memory = Memory;
		Bound.UpgradeLevel = SOURCES_TEST_ONE_LEVEL;
		Bound.bNeedsOwnerWearing = true;

		AActor *Wearer = SpawnWearing(Lord, Weapon, Weapon->Tier);
		UInventoryComponent *Inventory = Wearer->FindComponentByClass<UInventoryComponent>();
		FWeaponInventoryEntry &Copy = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry;
		Copy.PersistentID = FGuid::NewGuid();
		FWeaponInventoryEntry Owned = Copy;
		// What UEconomyService::LevelUpWeapon writes: the owned entry's tier.
		Owned.Tier = SourcesTestTierAbove(Weapon->Tier, SOURCES_TEST_ONE_LEVEL);
		Inventory->Weapons.Add(Owned);

		TestEqual(TEXT("Levelled owned copy counts"), MemoryUnlock::UnlockAtFightStart(GameInstance, {}, {Wearer}, {}), 1);
	});

	It("grants no memory already in the journal again, from any source", [this]()
	{
		Journal->RecordMemory(Memory, EMemorySource::Fragment);
		UBossData *Boss = NewObject<UBossData>(GetTransientPackage());
		Boss->MemoryOnDefeat = Memory;
		TestFalse(TEXT("Boss: not again"), MemoryUnlock::UnlockBossDrop(GameInstance, Boss));
		TestFalse(TEXT("Level-up: not again"), MemoryUnlock::Unlock(GameInstance, Memory, EMemorySource::BoundGear));
		TestTrue(TEXT("Collected"), MemoryUnlock::IsCollected(GameInstance, Memory));
		TestFalse(TEXT("Not added to this run"), TrialRun->GetRunMemories().Contains(Memory));
		TestEqual(TEXT("Keeps its first source"), Journal->GetSource(Memory), TOptional<EMemorySource>(EMemorySource::Fragment));

		AMemorySpot *Spot = TestWorld->GetWorld()->SpawnActor<AMemorySpot>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("Spot"), Spot))
		{
			return;
		}
		Spot->Memory = Memory;
		TestFalse(TEXT("Spot: not again"), Spot->Reach());
		TestFalse(TEXT("Spot not recorded as found"), TrialRun->HasFound(Spot->GetFeatureId()));
	});

	It("leaves a map spot unspent when its memory's unlock conditions aren't met", [this]()
	{
		AMemorySpot *Spot = TestWorld->GetWorld()->SpawnActor<AMemorySpot>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("Spot"), Spot))
		{
			return;
		}
		FMemoryCondition NotHere;
		NotHere.Kind = EMemoryConditionKind::Character;
		NotHere.Character = FWoRTestWorld::NewCharacterData();
		Memory->UnlockConditions.Add(NotHere);
		Spot->Memory = Memory;
		TestFalse(TEXT("Not reached"), Spot->Reach());
		TestFalse(TEXT("Not in the journal"), Journal->HasMemory(Memory));
		TestFalse(TEXT("Not recorded as found"), TrialRun->HasFound(Spot->GetFeatureId()));
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

	It("is for a character it lists above None, or for everyone when it lists no one (B-235d)", [this]()
	{
		UMemoryData *Shared = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *Personal = NewObject<UMemoryData>(GetTransientPackage());
		UCharacterData *Mine = NewObject<UCharacterData>(GetTransientPackage());
		UCharacterData *Theirs = NewObject<UCharacterData>(GetTransientPackage());
		Personal->CharacterDepth.Add(Mine, EMemoryDepth::Deep);
		Personal->CharacterDepth.Add(Theirs, EMemoryDepth::None);
		TestTrue(TEXT("no character listed: everyone's"), MemoryUnlock::QualifiesFor(Shared, {Theirs}));
		TestTrue(TEXT("it means something to my character"), MemoryUnlock::QualifiesFor(Personal, {Mine}));
		TestFalse(TEXT("listed as None: not theirs"), MemoryUnlock::QualifiesFor(Personal, {Theirs}));
		TestFalse(TEXT("no characters"), MemoryUnlock::QualifiesFor(Personal, {}));
		TestFalse(TEXT("no memory"), MemoryUnlock::QualifiesFor(nullptr, {Mine}));
		TestEqual(TEXT("standalone: no friends"), MemoryUnlock::GetRemotePlayers(TestWorld->GetWorld()->GetGameInstance()).Num(), 0);
	});

	It("goes to the player whose character found it, a same-character player who met it too, or everyone (Q-235d, Crown)", [this]()
	{
		UMemoryData *ForEveryone = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *ForX = NewObject<UMemoryData>(GetTransientPackage());
		UCharacterData *X = NewObject<UCharacterData>(GetTransientPackage());
		UCharacterData *Y = NewObject<UCharacterData>(GetTransientPackage());
		ForX->CharacterDepth.Add(X, EMemoryDepth::Deep);

		// 3: a memory for every character goes to everyone, met or not.
		TestTrue(TEXT("everyone's: a friend"), MemoryUnlock::ShouldFriendReceive(ForEveryone, {Y}, false));
		TestTrue(TEXT("everyone's: the host"), MemoryUnlock::ShouldHostReceive(ForEveryone, {Y}, false, true));
		// 1-2: a friend playing X who met it themselves; not one playing Y, nor one who did not meet it.
		TestTrue(TEXT("plays X and met it"), MemoryUnlock::ShouldFriendReceive(ForX, {X}, true));
		TestFalse(TEXT("plays X, did not meet it"), MemoryUnlock::ShouldFriendReceive(ForX, {X}, false));
		TestFalse(TEXT("met it, plays Y"), MemoryUnlock::ShouldFriendReceive(ForX, {Y}, true));
		// The host: the same, and keeps it when no friend found it (the host's run did; solo always).
		TestFalse(TEXT("a friend playing X found it: not the host playing Y"), MemoryUnlock::ShouldHostReceive(ForX, {Y}, true, true));
		TestTrue(TEXT("host plays X and met it"), MemoryUnlock::ShouldHostReceive(ForX, {X}, true, true));
		TestTrue(TEXT("no friend got it: the host's find"), MemoryUnlock::ShouldHostReceive(ForX, {Y}, false, false));
		TestFalse(TEXT("no memory"), MemoryUnlock::ShouldFriendReceive(nullptr, {X}, true));

		// Each player's part of an event: their own characters' members.
		FMemoryEvent Event;
		Event.Party.Add({X, {}});
		Event.Party.Add({Y, {}});
		const FMemoryEvent Kept = MemoryConditions::FilterParty(Event, {X}, true);
		TestTrue(TEXT("kept to X"), Kept.Party.Num() == 1 && Kept.Party[0].Character == X);
		const FMemoryEvent Dropped = MemoryConditions::FilterParty(Event, {X}, false);
		TestTrue(TEXT("X left out"), Dropped.Party.Num() == 1 && Dropped.Party[0].Character == Y);

		const FMemoryReceivers Solo = MemoryConditions::PickReceivers(TestWorld->GetWorld()->GetGameInstance(), *ForX, Event, nullptr);
		TestTrue(TEXT("standalone: the host"), Solo.bHost);
		TestEqual(TEXT("standalone: no friends"), Solo.Friends.Num(), 0);
	});

	It("reads a friend's PriorMemory on the memories they have, and a find with no friend is the host's (B-235e)", [this]()
	{
		UMemoryData *First = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *After = NewObject<UMemoryData>(GetTransientPackage());
		FMemoryCondition Prior;
		Prior.Kind = EMemoryConditionKind::PriorMemory;
		Prior.PriorMemory = First;
		After->UnlockConditions = {Prior};
		FMemoryEvent Event;

		const TSet<FName> FriendHasIt = {First->GetMemoryId()};
		const TSet<FName> FriendLacksIt;
		TestTrue(TEXT("the friend has the first memory"), MemoryConditions::AreMet(*After, Event, Journal, &FriendHasIt));
		TestFalse(TEXT("the friend does not, though the host might"), MemoryConditions::AreMet(*After, Event, Journal, &FriendLacksIt));
		Journal->RecordMemory(First, EMemorySource::Fragment);
		TestTrue(TEXT("no known set: the host's journal, as before"), MemoryConditions::AreMet(*After, Event, Journal));
		TestFalse(TEXT("the known set wins over the host's journal"), MemoryConditions::AreMet(*After, Event, Journal, &FriendLacksIt));

		UCharacterData *Someone = NewObject<UCharacterData>(GetTransientPackage());
		TestNull(TEXT("standalone: no friend plays them"), MemoryUnlock::FindRemotePlayerOf(GameInstance, Someone));
		TestTrue(TEXT("standalone: a worn item's memory is the host's find"),
				 MemoryUnlock::UnlockFoundBy(GameInstance, Memory, EMemorySource::BoundGear, Someone));
		TestTrue(TEXT("into the run and the host's journal"), InRunAndJournalFrom(EMemorySource::BoundGear));
	});

	It("in co-op reads the team's memories together for PriorMemory (MemoryCatalogue 3.5, Crown)", [this]()
	{
		UMemoryData *Garnet = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *Orthos = NewObject<UMemoryData>(GetTransientPackage());
		UMemoryData *Both = NewObject<UMemoryData>(GetTransientPackage());
		FMemoryCondition NeedsGarnet;
		NeedsGarnet.Kind = EMemoryConditionKind::PriorMemory;
		NeedsGarnet.PriorMemory = Garnet;
		FMemoryCondition NeedsOrthos = NeedsGarnet;
		NeedsOrthos.PriorMemory = Orthos;
		Both->UnlockConditions = {NeedsGarnet, NeedsOrthos};
		FMemoryEvent Event;

		Journal->RecordMemory(Garnet, EMemorySource::Fragment); // the host's quest is done
		const TSet<FName> FriendHasOrthos = {Orthos->GetMemoryId()}; // the friend's is
		TestFalse(TEXT("the host alone: not both"), MemoryConditions::AreMet(*Both, Event, Journal));
		TestFalse(TEXT("the friend alone: not both"), MemoryConditions::AreMet(*Both, Event, Journal, &FriendHasOrthos));
		const TSet<FName> Team = MemoryConditions::GetTeamMemoryIds(Journal, {&FriendHasOrthos});
		TestTrue(TEXT("the team's together: both"), MemoryConditions::AreMet(*Both, Event, Journal, &Team));
		TestEqual(TEXT("no journal, no friends: none"), MemoryConditions::GetTeamMemoryIds(nullptr, {}).Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
