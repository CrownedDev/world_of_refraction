// CurseSpec.cpp
// B-378 (MemoryCatalogue §3.8): a memory's Curse reward gives the curse when the journal records it; the curse is a
// key (CurseHeld); a RemoveCurse reward lifts it; held curses round-trip through their save struct.
// Q-378 (changed by Crown, 2026-11-08): a hidden quest's step gives one too; a step needing it moves while it is held,
// not once the player clears it, and again once it is regained (its giver's conditions met again).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Memory/CurseData.h"
#include "Memory/CurseSubsystem.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "UI/Hub/CurseClearWidget.h"

namespace
{
	const FName CURSE_TEST_ID(TEXT("Curse_CurseSpec"));
	const FName CURSE_TEST_LOCATION(TEXT("Spot_CurseSpec"));
	constexpr int32 CURSE_TEST_HP_PENALTY = -4;

	FMemoryEvent AtCurseLocation()
	{
		FMemoryEvent Event;
		Event.Locations.Add(CURSE_TEST_LOCATION);
		return Event;
	}

	FMemoryReward CurseReward()
	{
		FMemoryReward Reward;
		Reward.Kind = EMemoryRewardKind::Curse;
		Reward.RewardId = CURSE_TEST_ID;
		return Reward;
	}

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
		TestTrue(TEXT("Q-378: a lifted curse can be given again"), Curses->AddCurse(CURSE_TEST_ID));
	});

	It("Q-378: a quest step gives it; a step needing it moves while held, not once cleared, and again once regained", [this]()
	{
		UHiddenQuestSubsystem *Quests = TestWorld->GetSubsystem<UHiddenQuestSubsystem>();
		if (!TestNotNull(TEXT("quests"), Quests))
		{
			return;
		}
		FWoRCondition There;
		There.Subject = EWoRConditionSubject::AtLocation;
		There.LocationId = CURSE_TEST_LOCATION;
		FWoRCondition Cursed;
		Cursed.Subject = EWoRConditionSubject::CurseHeld;
		Cursed.CurseId = CURSE_TEST_ID;
		FHiddenQuestStep Gives; // at the spot: the quest curses the player
		Gives.Conditions.Conditions.Add(There);
		Gives.Rewards.Add(CurseReward());
		FHiddenQuestStep Needs; // "parry while this curse is on you", here only the curse
		Needs.Conditions.Conditions.Add(Cursed);
		UHiddenQuestData *Quest = NewObject<UHiddenQuestData>(GetTransientPackage());
		Quest->Steps = {Gives, Needs, Needs};
		const FName Id = Quest->GetQuestId();

		Quests->StartQuest(Quest);
		Quests->CheckEvent(FMemoryEvent(), {Quest}); // the start's own event (Q-377)
		Quests->CheckEvent(AtCurseLocation(), {Quest});
		TestTrue(TEXT("the step gave the curse"), Curses->HasCurse(CURSE_TEST_ID));
		TestEqual(TEXT("cursed: the step needing it moves"), Quests->CheckEvent(FMemoryEvent(), {Quest}), 1);
		TestTrue(TEXT("the player clears it"), Curses->ClearCurse(CURSE_TEST_ID));
		TestEqual(TEXT("cleared: it doesn't"), Quests->CheckEvent(FMemoryEvent(), {Quest}), 0);
		TestEqual(TEXT("cleared: still two steps done"), Quests->GetStepsDone(Id), 2);
		Quests->CheckEvent(AtCurseLocation(), {Quest}); // its giving step met again: regained first, then the steps move
		TestTrue(TEXT("regained"), Curses->HasCurse(CURSE_TEST_ID));
		TestTrue(TEXT("regained: it moves again on the same event (finished)"), Quests->IsFinished(Id));
	});

	It("Q-378: a memory the player has gives back its cleared curse when its conditions are met again", [this]()
	{
		FMemoryCondition There;
		There.Kind = EMemoryConditionKind::Location;
		There.LocationId = CURSE_TEST_LOCATION;
		UMemoryData *Giver = NewObject<UMemoryData>(GetTransientPackage());
		Giver->UnlockConditions = {There};
		Giver->Rewards.Add(CurseReward());
		Journal->RecordMemory(Giver, EMemorySource::Fragment);
		Curses->ClearCurse(CURSE_TEST_ID);
		TestFalse(TEXT("cleared"), Curses->HasCurse(CURSE_TEST_ID));
		TestEqual(TEXT("not met: still cleared"), Curses->CheckRegain(FMemoryEvent(), {Giver}), 0);
		TestEqual(TEXT("met again: it comes back"), Curses->CheckRegain(AtCurseLocation(), {Giver}), 1);
		TestTrue(TEXT("held again"), Curses->HasCurse(CURSE_TEST_ID));
		TestEqual(TEXT("held: nothing to regain"), Curses->CheckRegain(AtCurseLocation(), {Giver}), 0);
		TestTrue(TEXT("the memory itself wasn't unlocked twice"), Journal->HasMemory(Giver));
	});

	It("Q-378 review: a cure found where the curse was given isn't undone by the same event, but a later one regains it", [this]()
	{
		FMemoryCondition There;
		There.Kind = EMemoryConditionKind::Location;
		There.LocationId = CURSE_TEST_LOCATION;
		UMemoryData *Giver = NewObject<UMemoryData>(GetTransientPackage());
		Giver->UnlockConditions = {There};
		Giver->Rewards.Add(CurseReward());
		UMemoryData *Cure = NewMemoryWithReward(EMemoryRewardKind::RemoveCurse);
		Cure->UnlockConditions = {There};
		Journal->RecordMemory(Giver, EMemorySource::Fragment);
		UGameInstance *GameInstance = TestWorld->GetWorld()->GetGameInstance();

		MemoryConditions::CheckEvent(GameInstance, AtCurseLocation(), {Giver, Cure});
		TestTrue(TEXT("the cure unlocked"), Journal->HasMemory(Cure));
		TestFalse(TEXT("lifted, and not regained by the same event"), Curses->HasCurse(CURSE_TEST_ID));
		MemoryConditions::CheckEvent(GameInstance, AtCurseLocation(), {Giver, Cure});
		TestTrue(TEXT("a later event at the giver regains it"), Curses->HasCurse(CURSE_TEST_ID));
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

	It("Q-378: clearing a curse stops its penalty, regaining it brings it back; the hub service clears outside a run", [this]()
	{
		UCurseData *Curse = NewObject<UCurseData>(GetTransientPackage());
		Curse->CurseId = CURSE_TEST_ID;
		Curse->StatPenalty.BonusMaxHP = CURSE_TEST_HP_PENALTY;
		Curses->SetCurseAssets({Curse});
		Curses->AddCurse(CURSE_TEST_ID);
		TestTrue(TEXT("the player clears it"), Curses->ClearCurse(CURSE_TEST_ID));
		TestEqual(TEXT("cleared: no penalty"), Curses->GetStatPenalty().BonusMaxHP, 0);
		TestFalse(TEXT("cleared twice: no"), Curses->ClearCurse(CURSE_TEST_ID));
		TestEqual(TEXT("regained: the penalty is back"), Curses->RegainFrom({CurseReward()}), 1);
		TestEqual(TEXT("regained: HP"), Curses->GetStatPenalty().BonusMaxHP, CURSE_TEST_HP_PENALTY);
		TestTrue(TEXT("the hub (no run): the service clears"), UCurseClearWidget::CanClearHere(TestWorld->GetWorld()));
		// The in-run refusal needs an active trial run; PIE B-378b check 7 covers it.
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
