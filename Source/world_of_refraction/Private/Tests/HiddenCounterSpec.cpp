// HiddenCounterSpec.cpp
// B-379 (MemoryCatalogue §3.9): hidden counters total per character and for all; a CounterReached condition opens
// at its milestone (for a character or for all); counts round-trip through their save struct.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Memory/HiddenCounterSubsystem.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"

namespace
{
	constexpr int32 COUNTER_TEST_MILESTONE = 3;
}

BEGIN_DEFINE_SPEC(FHiddenCounterSpec, "WoR.Memory.HiddenCounter", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UMemoryJournalSubsystem *Journal = nullptr;
	UHiddenCounterSubsystem *Counters = nullptr;
	UCharacterData *Sapphire = nullptr;
	UCharacterData *Other = nullptr;
END_DEFINE_SPEC(FHiddenCounterSpec)

void FHiddenCounterSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		Counters = TestWorld->GetSubsystem<UHiddenCounterSubsystem>();
		Sapphire = FWoRTestWorld::NewCharacterData();
		Other = FWoRTestWorld::NewCharacterData();
		TestTrue(TEXT("Subsystems"), Journal && Counters);
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("counts per character and for all", [this]()
	{
		const FName Parries(HiddenCounters::PARRIES);
		Counters->Add(Parries, Sapphire);
		Counters->Add(Parries, Sapphire, 2);
		Counters->Add(Parries, Other);
		TestEqual(TEXT("as Sapphire"), Counters->Get(Parries, Sapphire), 3);
		TestEqual(TEXT("as the other"), Counters->Get(Parries, Other), 1);
		TestEqual(TEXT("all characters"), Counters->Get(Parries, nullptr), 4);
		TestEqual(TEXT("never counted"), Counters->Get(FName(HiddenCounters::KILLS), Sapphire), 0);
		Counters->Add(Parries, Sapphire, 0);
		TestEqual(TEXT("nothing added for 0"), Counters->Get(Parries, Sapphire), 3);
	});

	It("opens a counter memory at its milestone, for the character named", [this]()
	{
		FMemoryCondition Reached;
		Reached.Kind = EMemoryConditionKind::CounterReached;
		Reached.CounterId = FName(HiddenCounters::PARRIES);
		Reached.CounterTarget = COUNTER_TEST_MILESTONE;
		Reached.Character = Sapphire;
		UMemoryData *Milestone = NewObject<UMemoryData>(GetTransientPackage());
		Milestone->UnlockConditions = {Reached};
		FMemoryEvent Event;

		Counters->Add(Reached.CounterId, Other, COUNTER_TEST_MILESTONE);
		TestFalse(TEXT("another character's parries do not count"), MemoryConditions::AreMet(*Milestone, Event, Journal));
		Counters->Add(Reached.CounterId, Sapphire, COUNTER_TEST_MILESTONE - 1);
		TestFalse(TEXT("one short"), MemoryConditions::AreMet(*Milestone, Event, Journal));
		Counters->Add(Reached.CounterId, Sapphire);
		TestTrue(TEXT("reached"), MemoryConditions::AreMet(*Milestone, Event, Journal));
	});

	It("keeps counting across runs through its save struct (D-059)", [this]()
	{
		Counters->Add(FName(HiddenCounters::KILLS), Sapphire, 2);
		const FSavedCounters Saved = Counters->Capture();
		Counters->Restore(FSavedCounters());
		TestEqual(TEXT("cleared"), Counters->Get(FName(HiddenCounters::KILLS), nullptr), 0);
		Counters->Restore(Saved);
		TestEqual(TEXT("as Sapphire"), Counters->Get(FName(HiddenCounters::KILLS), Sapphire), 2);
		TestEqual(TEXT("all"), Counters->Get(FName(HiddenCounters::KILLS), nullptr), 2);
		TestFalse(TEXT("the debug text"), Counters->GetCountersString().IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
