// KeptRunLevelsSpec.cpp
// B-555 (D-237, one-player stage 3): run levels are kept in the hub the way gear is. A run's end keeps the levels each
// member ends it with (a quit keeps the ones before it); the hub adds them, a trial starts without them; they are saved
// and the title slot shows them. A wipe takes DefeatRules::WIPE_POINTS_LOST run points, each from a rolled pillar, a
// pillar's over-cap points first.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Menus/TitleScreenModel.h"

namespace
{
	constexpr int32 KEPT_SEED = 23;
	constexpr int32 KEPT_MIND = 2;
	constexpr int32 KEPT_SPIRIT = 1;
	constexpr int32 KEPT_ASSET_BODY = 3;
	constexpr int32 KEPT_EARLIER_BODY = 4;
	constexpr int32 KEPT_ROLLS = 200;
	const FName KEPT_ROLL_KEY(TEXT("KeptRoll"));
}

BEGIN_DEFINE_SPEC(FKeptRunLevelsSpec, "WoR.Trial.KeptRunLevels", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UTrialRunSubsystem *Run = nullptr;
	UCharacterData *Data = nullptr;

	FPartyMember *GetMember() const
	{
		UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		return Party && !Party->Members.IsEmpty() ? &Party->Members.Last() : nullptr;
	}

	/** A run with the member's earned Mind and Spirit. */
	void StartRun()
	{
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), KEPT_SEED, 0, ETrialDifficulty::Medium, {}, {}, {});
		const FName Key = UPoolSubsystem::GetCharacterKey(*GetMember());
		Run->AddRunEarnedLevels(Key, EWorldPillar::Mind, KEPT_MIND);
		Run->AddRunEarnedLevels(Key, EWorldPillar::Spirit, KEPT_SPIRIT);
	}
END_DEFINE_SPEC(FKeptRunLevelsSpec)

void FKeptRunLevelsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestTrue(TEXT("party and run"), Party && Run)) return;
		Data = FWoRTestWorld::NewCharacterData();
		Data->WorldBodyLevel = KEPT_ASSET_BODY;
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Member.MemberId = FGuid::NewGuid();
		Member.KeptBody = KEPT_EARLIER_BODY; // what an earlier run left
		Member.bHasKeptLevels = true;
		Party->Members.Add(Member);
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		Run = nullptr;
		Data = nullptr;
		TestWorld.Reset();
	});

	It("keeps the levels the run ends with, replacing the last run's", [this]()
	{
		if (!TestNotNull(TEXT("member"), GetMember())) return;
		StartRun();
		Run->KeepRunLevels(); // what the run's end does on a win, an extract or out of wipes
		const FPartyMember *Member = GetMember();
		TestTrue(TEXT("Mind and Spirit kept, the last run's Body gone"),
				 Member->KeptMind == KEPT_MIND && Member->KeptBody == 0 && Member->KeptSpirit == KEPT_SPIRIT && Member->bHasKeptLevels);
	});

	It("keeps the levels from before the run on a quit", [this]()
	{
		if (!TestNotNull(TEXT("member"), GetMember())) return;
		StartRun();
		Run->SettleRunAsQuit(/*bTravelIfAway=*/false);
		const FPartyMember *Member = GetMember();
		TestTrue(TEXT("the last run's levels stay"), Member->KeptBody == KEPT_EARLIER_BODY && Member->KeptMind == 0);
	});

	It("adds the kept levels in the hub and leaves them out of a trial", [this]()
	{
		const FPartyMember *Member = GetMember();
		if (!TestNotNull(TEXT("member"), Member)) return;
		AActor *Pawn = TestWorld->SpawnCombatant(Data);
		UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("pawn"), CharComp)) return;
		CharComp->PartyMemberId = Member->MemberId;
		CharComp->SeedWorldStats();
		TestEqual(TEXT("hub: the asset's Body plus the kept"), CharComp->GetWorldStat(EWorldPillar::Body), KEPT_ASSET_BODY + KEPT_EARLIER_BODY);
		StartRun();
		CharComp->SeedWorldStats();
		TestEqual(TEXT("trial: the asset's Body alone"), CharComp->GetWorldStat(EWorldPillar::Body), KEPT_ASSET_BODY);
		TestEqual(TEXT("trial: the run's Mind"), CharComp->GetWorldStat(EWorldPillar::Mind), KEPT_MIND);
	});

	It("saves the kept levels, and the title slot shows them", [this]()
	{
		UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		if (!TestNotNull(TEXT("party"), Party)) return;
		const FSavedParty Saved = USaveSubsystem::CaptureParty(Party, /*bKeepVitals=*/false);
		PartySession->ForgetLocalParty();
		TestTrue(TEXT("restored"), USaveSubsystem::RestoreParty(Saved, *PartySession));
		const FPartyMember *Member = GetMember();
		TestTrue(TEXT("kept through the save"), Member && Member->KeptBody == KEPT_EARLIER_BODY && Member->bHasKeptLevels);

		FWoRSaveData SaveData;
		SaveData.Party = Saved;
		const FTitleSaveSummary Summary = WoRTitleScreen::Summarize(SaveData, true, 0.0);
		TestTrue(TEXT("the slot reads the kept levels"), Summary.bHasRunLevels && Summary.RunBody == KEPT_EARLIER_BODY);
	});

	It("takes a wipe's points from rolled pillars that have any, over-cap first", [this]()
	{
		if (!TestNotNull(TEXT("run"), Run)) return;
		Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), KEPT_SEED, 0, ETrialDifficulty::Medium, {}, {}, {});
		FRandomStream Stream(KEPT_SEED);
		bool bRolledBoth = false;
		for (int32 Roll = 0; Roll < KEPT_ROLLS && !bRolledBoth; ++Roll)
		{
			Run->ForgetRunLevels(KEPT_ROLL_KEY);
			Run->AddRunEarnedLevels(KEPT_ROLL_KEY, EWorldPillar::Mind, 1);
			Run->AddRunEarnedLevels(KEPT_ROLL_KEY, EWorldPillar::Body, 1);
			TestEqual(TEXT("one point taken"), Run->TakeWipePoints(KEPT_ROLL_KEY, 1, Stream), 1);
			TestEqual(TEXT("never from Spirit, which has none"), Run->GetRunEarnedLevels(KEPT_ROLL_KEY, EWorldPillar::Spirit), 0);
			bRolledBoth = Run->GetRunEarnedLevels(KEPT_ROLL_KEY, EWorldPillar::Mind) == 0;
		}
		TestTrue(TEXT("the roll reaches either pillar"), bRolledBoth);
		TestEqual(TEXT("two asked, two there"), Run->TakeWipePoints(KEPT_ROLL_KEY, DefeatRules::WIPE_POINTS_LOST, Stream), 1);
		TestEqual(TEXT("nothing left"), Run->TakeWipePoints(KEPT_ROLL_KEY, DefeatRules::WIPE_POINTS_LOST, Stream), 0);

		Run->AddRunEarnedLevels(KEPT_ROLL_KEY, EWorldPillar::Body, 1);
		Run->AddRunOverCap(KEPT_ROLL_KEY, EWorldPillar::Body);
		TestEqual(TEXT("one taken"), Run->TakeWipePoints(KEPT_ROLL_KEY, 1, Stream), 1);
		TestTrue(TEXT("the over-cap point went first"),
				 Run->GetRunOverCap(KEPT_ROLL_KEY, EWorldPillar::Body) == 0 && Run->GetRunEarnedLevels(KEPT_ROLL_KEY, EWorldPillar::Body) == 1);
	});
}

#endif
