// HiddenQuestSpec.cpp
// B-377 (MemoryCatalogue §3.6, Crown 2026-11-08): a hidden quest starts from its memory at the set depth, its steps
// are done in order (a step can need several events in one run; that count resets at run end, steps done stay), the
// last step unlocks its end memory, and progress round-trips through its save struct.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"

namespace
{
	const FName QUEST_TEST_LOCATION(TEXT("Spot_HiddenQuestSpec"));
	constexpr int32 QUEST_TEST_TIMES = 2;
}

BEGIN_DEFINE_SPEC(FHiddenQuestSpec, "WoR.Memory.HiddenQuest", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UMemoryJournalSubsystem *Journal = nullptr;
	UHiddenQuestSubsystem *Quests = nullptr;
	UMemoryData *StartMemory = nullptr;
	UMemoryData *EndMemory = nullptr;
	UHiddenQuestData *Quest = nullptr;

	/** An event at the test location (what every step here needs). */
	static FMemoryEvent AtLocation()
	{
		FMemoryEvent Event;
		Event.Locations.Add(QUEST_TEST_LOCATION);
		return Event;
	}
END_DEFINE_SPEC(FHiddenQuestSpec)

void FHiddenQuestSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		Quests = TestWorld->GetSubsystem<UHiddenQuestSubsystem>();
		if (!TestTrue(TEXT("Subsystems"), Journal && Quests))
		{
			return;
		}

		StartMemory = NewObject<UMemoryData>(GetTransientPackage());
		EndMemory = NewObject<UMemoryData>(GetTransientPackage());
		Quest = NewObject<UHiddenQuestData>(GetTransientPackage());
		FHiddenQuestStart AnyDepth;
		AnyDepth.Memory = StartMemory;
		Quest->Starts.Add(AnyDepth);
		FWoRCondition There;
		There.Subject = EWoRConditionSubject::AtLocation;
		There.LocationId = QUEST_TEST_LOCATION;
		FHiddenQuestStep First;
		First.Conditions.Conditions.Add(There);
		FHiddenQuestStep Second = First;
		Second.TimesInOneRun = QUEST_TEST_TIMES;
		Quest->Steps = {First, Second};
		Quest->EndMemory = EndMemory;
		Quests->SetQuestsForStarts({Quest});
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("starts when its memory is recorded, at the depth set for it", [this]()
	{
		TestFalse(TEXT("not before"), Quests->IsStarted(Quest->GetQuestId()));
		Journal->RecordMemory(StartMemory, EMemorySource::Fragment);
		TestTrue(TEXT("started by the memory"), Quests->IsStarted(Quest->GetQuestId()));

		UCharacterData *Garnet = FWoRTestWorld::NewCharacterData();
		UMemoryData *DeepOnly = NewObject<UMemoryData>(GetTransientPackage());
		DeepOnly->CharacterDepth.Add(Garnet, EMemoryDepth::Deep);
		UHiddenQuestData *ForDeep = NewObject<UHiddenQuestData>(GetTransientPackage());
		FHiddenQuestStart Deep;
		Deep.Memory = DeepOnly;
		Deep.Depth = EMemoryDepth::Deep;
		ForDeep->Starts.Add(Deep);
		TestTrue(TEXT("any depth"), Quest->StartsFrom(StartMemory, EMemoryDepth::Minor));
		TestFalse(TEXT("set for Deep: a Minor one does not start it"), ForDeep->StartsFrom(DeepOnly, EMemoryDepth::Minor));
		TestTrue(TEXT("set for Deep: a Deep one does"), ForDeep->StartsFrom(DeepOnly, EMemoryDepth::Deep));
	});

	It("does its steps in order; a step's count resets at run end, steps done stay", [this]()
	{
		const FName Id = Quest->GetQuestId();
		TestEqual(TEXT("not started: nothing moves"), Quests->CheckEvent(AtLocation(), {Quest}), 0);
		Quests->StartQuest(Quest);
		TestEqual(TEXT("the event that started it does not count (Q-377)"), Quests->CheckEvent(AtLocation(), {Quest}), 0);
		TestEqual(TEXT("elsewhere: no step"), Quests->CheckEvent(FMemoryEvent(), {Quest}), 0);
		TestEqual(TEXT("step 1 done"), Quests->CheckEvent(AtLocation(), {Quest}), 1);
		TestEqual(TEXT("step 2 needs two this run: once"), Quests->CheckEvent(AtLocation(), {Quest}), 0);
		TestEqual(TEXT("its count"), Quests->GetRunCount(Id), 1);
		Quests->EndRun();
		TestEqual(TEXT("run end: the count resets"), Quests->GetRunCount(Id), 0);
		TestEqual(TEXT("run end: step 1 stays"), Quests->GetStepsDone(Id), 1);
		Quests->CheckEvent(AtLocation(), {Quest});
		TestFalse(TEXT("once this run: not yet"), Quests->IsFinished(Id));
		Quests->CheckEvent(AtLocation(), {Quest});
		TestTrue(TEXT("twice this run: finished"), Quests->IsFinished(Id));
		TestTrue(TEXT("its end memory unlocked"), Journal->HasMemory(EndMemory));
		TestEqual(TEXT("finished: nothing more"), Quests->CheckEvent(AtLocation(), {Quest}), 0);
	});

	It("keeps steps done through its save struct (D-059)", [this]()
	{
		Quests->StartQuest(Quest);
		Quests->CheckEvent(FMemoryEvent(), {Quest}); // the start's own event
		Quests->CheckEvent(AtLocation(), {Quest});
		const FSavedHiddenQuests Saved = Quests->Capture();
		Quests->Restore(FSavedHiddenQuests());
		TestFalse(TEXT("cleared"), Quests->IsStarted(Quest->GetQuestId()));
		Quests->Restore(Saved);
		TestEqual(TEXT("restored: step 1 done"), Quests->GetStepsDone(Quest->GetQuestId()), 1);
		TestFalse(TEXT("restored: not finished"), Quests->IsFinished(Quest->GetQuestId()));
		TestFalse(TEXT("the debug text names it"), Quests->GetQuestsString().IsEmpty());
	});

	It("starts a quest a worn item carries while its conditions hold (B-381, D-165)", [this]()
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		UCharacterData *Wearer = FWoRTestWorld::NewCharacterData();
		FBoundQuest Carried;
		Carried.Quest = Quest;
		FWoRCondition There;
		There.Subject = EWoRConditionSubject::AtLocation;
		There.LocationId = QUEST_TEST_LOCATION;
		Carried.Conditions.Conditions.Add(There);
		Sword->BoundQuests.Add(Carried);

		FMemoryEvent NotWorn = AtLocation();
		NotWorn.Party.Add({Wearer, {}});
		TestEqual(TEXT("not worn: nothing starts"), Quests->StartGearQuests(NotWorn), 0);

		FMemoryEventMember Wearing;
		Wearing.Character = Wearer;
		Wearing.Worn.Emplace(Sword, EItemTier::F_Tier);
		FMemoryEvent WornElsewhere;
		WornElsewhere.Party.Add(Wearing);
		TestEqual(TEXT("worn, conditions unmet: nothing starts"), Quests->StartGearQuests(WornElsewhere), 0);

		FMemoryEvent Worn = AtLocation();
		Worn.Party.Add(Wearing);
		TestEqual(TEXT("worn and met: it starts"), Quests->StartGearQuests(Worn), 1);
		TestTrue(TEXT("started"), Quests->IsStarted(Quest->GetQuestId()));
		TestEqual(TEXT("once"), Quests->StartGearQuests(Worn), 0);
	});

	It("saves the journal and restores it without starting quests again (B-377b, D-059)", [this]()
	{
		Journal->RecordMemory(StartMemory, EMemorySource::MapSpot);
		Journal->RecordLore(TEXT("Lore_HiddenQuestSpec"), FText::FromString(TEXT("T")), FText::FromString(TEXT("B")));
		Journal->MarkVLineHeard(TEXT("VLine_HiddenQuestSpec"));
		const FSavedJournal Saved = Journal->Capture();
		TestEqual(TEXT("one memory saved"), Saved.Memories.Num(), 1);

		Journal->Restore(FSavedJournal());
		Quests->Restore(FSavedHiddenQuests());
		TestFalse(TEXT("cleared"), Journal->HasMemory(StartMemory));

		Journal->Restore(Saved);
		TestTrue(TEXT("the memory is back"), Journal->HasMemory(StartMemory));
		TestTrue(TEXT("from where it came"), Journal->GetSource(StartMemory) == EMemorySource::MapSpot);
		TestEqual(TEXT("its lore"), Journal->GetLore().Num(), 1);
		TestTrue(TEXT("the V line"), Journal->HasHeardVLine(TEXT("VLine_HiddenQuestSpec")));
		TestFalse(TEXT("restoring announces nothing: the quest is not started again"), Quests->IsStarted(Quest->GetQuestId()));
	});

	It("counts the host's step-met message only for the step it was checked on (B-377b)", [this]()
	{
		const FName Id = Quest->GetQuestId();
		Quests->StartQuest(Quest);
		Quests->ApplyStepMetFromHost(Id, 0);
		TestEqual(TEXT("step 1 done by the host's message"), Quests->GetStepsDone(Id), 1);
		Quests->ApplyStepMetFromHost(Id, 0);
		TestEqual(TEXT("a late message for step 1 does not count on step 2"), Quests->GetStepsDone(Id), 1);
		TestEqual(TEXT("nor its count"), Quests->GetRunCount(Id), 0);
		Quests->ApplyStepMetFromHost(Id, 1);
		TestEqual(TEXT("step 2 met once (needs two this run)"), Quests->GetRunCount(Id), 1);
		Quests->ApplyStepMetFromHost(TEXT("NotAQuest_HiddenQuestSpec"), 0); // ignored
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
