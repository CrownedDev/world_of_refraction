// CurseSpec.cpp
// B-378 (MemoryCatalogue §3.8): a memory's Curse reward gives the curse when the journal records it; the curse is a
// key (CurseHeld); a RemoveCurse reward lifts it; held curses round-trip through their save struct.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Memory/CurseData.h"
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
		TestFalse(TEXT("a friend's part on the host does not read this player's curses"),
				  MemoryConditions::AreMet(*Locked, Event, Journal, nullptr, /*bReadsThisPlayersState=*/false));
		Journal->RecordMemory(NewMemoryWithReward(EMemoryRewardKind::RemoveCurse), EMemorySource::Fragment);
		TestFalse(TEXT("its own memory lifted it"), Curses->HasCurse(CURSE_TEST_ID));
		TestFalse(TEXT("closed again"), MemoryConditions::AreMet(*Locked, Event, Journal));
		TestFalse(TEXT("a lifted curse is not given again"), Curses->AddCurse(CURSE_TEST_ID));
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

	It("weakens its holder by its penalty while held, and only their own party characters (B-378b)", [this]()
	{
		UCurseData *Curse = NewObject<UCurseData>(GetTransientPackage());
		Curse->CurseId = CURSE_TEST_ID;
		Curse->StatPenalty.BonusMaxHP = -4;
		Curse->StatPenalty.BonusDefense = -2;
		Curse->ResistancePenalty.Fire = -5.0f;
		Curses->SetCurseAssets({Curse});

		TestEqual(TEXT("not held: no penalty"), Curses->GetStatPenalty().BonusMaxHP, 0);
		Curses->AddCurse(CURSE_TEST_ID);
		TestEqual(TEXT("held: HP"), Curses->GetStatPenalty().BonusMaxHP, -4);
		TestEqual(TEXT("held: Defence"), Curses->GetStatPenalty().BonusDefense, -2);
		TestEqual(TEXT("held: Fire resistance"), Curses->GetResistancePenalty().Fire, -5.0f);
		TestFalse(TEXT("a character outside the party is not weakened (an enemy)"), Curses->IsCursedHere(FWoRTestWorld::NewCharacterData()));
		Curses->RemoveCurse(CURSE_TEST_ID);
		TestEqual(TEXT("lifted: no penalty"), Curses->GetStatPenalty().BonusMaxHP, 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
