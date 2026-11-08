// CurseSpec.cpp
// B-378 (MemoryCatalogue §3.8): a memory's Curse reward gives the curse when the journal records it; the curse is a
// key (CurseHeld); a RemoveCurse reward lifts it; held curses round-trip through their save struct.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Memory/CurseSubsystem.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"

namespace
{
	const FName CURSE_TEST_ID(TEXT("Curse_CurseSpec"));

	UMemoryData *NewMemoryWithReward(EMemoryRewardKind Kind)
	{
		UMemoryData *Memory = NewObject<UMemoryData>(GetTransientPackage());
		FMemoryReward Reward;
		Reward.Kind = Kind;
		Reward.RewardId = CURSE_TEST_ID;
		Memory->Rewards.Add(Reward);
		return Memory;
	}
}

BEGIN_DEFINE_SPEC(FCurseSpec, "WoR.Memory.Curse", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UMemoryJournalSubsystem *Journal = nullptr;
	UCurseSubsystem *Curses = nullptr;
END_DEFINE_SPEC(FCurseSpec)

void FCurseSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		Curses = TestWorld->GetSubsystem<UCurseSubsystem>();
		TestTrue(TEXT("Subsystems"), Journal && Curses);
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("is given by its memory, opens what needs it, and is lifted by its own memory", [this]()
	{
		FMemoryCondition Held;
		Held.Kind = EMemoryConditionKind::CurseHeld;
		Held.CurseId = CURSE_TEST_ID;
		UMemoryData *Locked = NewObject<UMemoryData>(GetTransientPackage());
		Locked->UnlockConditions = {Held};
		FMemoryEvent Event;

		TestFalse(TEXT("no curse: the key is missing"), MemoryConditions::AreMet(*Locked, Event, Journal));
		Journal->RecordMemory(NewMemoryWithReward(EMemoryRewardKind::Curse), EMemorySource::Fragment);
		TestTrue(TEXT("its memory gave it"), Curses->HasCurse(CURSE_TEST_ID));
		TestTrue(TEXT("the curse opens it"), MemoryConditions::AreMet(*Locked, Event, Journal));
		Journal->RecordMemory(NewMemoryWithReward(EMemoryRewardKind::RemoveCurse), EMemorySource::Fragment);
		TestFalse(TEXT("its own memory lifted it"), Curses->HasCurse(CURSE_TEST_ID));
		TestFalse(TEXT("closed again"), MemoryConditions::AreMet(*Locked, Event, Journal));
	});

	It("lasts across runs through its save struct (D-059)", [this]()
	{
		Curses->AddCurse(CURSE_TEST_ID);
		const FSavedCurses Saved = Curses->Capture();
		Curses->Restore(FSavedCurses());
		TestFalse(TEXT("cleared"), Curses->HasCurse(CURSE_TEST_ID));
		Curses->Restore(Saved);
		TestTrue(TEXT("restored"), Curses->HasCurse(CURSE_TEST_ID));
		TestFalse(TEXT("held twice: no"), Curses->AddCurse(CURSE_TEST_ID));
		TestTrue(TEXT("the debug text names it"), Curses->GetCursesString().Contains(CURSE_TEST_ID.ToString()));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
