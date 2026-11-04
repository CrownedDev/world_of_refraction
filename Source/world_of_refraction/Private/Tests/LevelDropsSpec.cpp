// LevelDropsSpec.cpp
// B-223 (D-102, Q-223): defeated enemies drop pillar levels into the run; the player places each on a party
// member's chosen pillar (WoRLevelDrops::TryPlaceLevel), capped at 7, and a re-seed keeps it.
// B-360c (D-150 point 2, Q-360c): a second copy of one character places and reads its own levels.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Tuning/CombatBaselineConstants.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/LevelDropPlacement.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 LEVEL_DROPS_TEST_ASSET_MIND = 2;
	constexpr EItemTier LEVEL_DROPS_TEST_TIER = EItemTier::C_Tier;
	constexpr int32 LEVEL_DROPS_TEST_SEED = 7;
}

BEGIN_DEFINE_SPEC(FLevelDropsSpec, "WoR.Trial.LevelDrops", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UTrialRunSubsystem *Run = nullptr;
UCharacterData *Member = nullptr;
UCharacterData *Absent = nullptr;

UCharacterData *AddMember(UParty *Party, const TCHAR *Name)
{
	UCharacterData *Data = FWoRTestWorld::NewCharacterData();
	Data->Name = Name;
	Data->WorldMindLevel = LEVEL_DROPS_TEST_ASSET_MIND;
	FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
	PartyMember.PawnClass = ACombatCharacter::StaticClass();
	PartyMember.CharacterData = Data;
	return Data;
}
static FName KeyOf(const UCharacterData *Character) { return UPoolSubsystem::GetCharacterKey(Character); }
END_DEFINE_SPEC(FLevelDropsSpec)

void FLevelDropsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (Run)
		{
			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), LEVEL_DROPS_TEST_SEED, 0, ETrialDifficulty::Medium, {}, {}, {});
		}
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party))
		{
			return;
		}
		Party->Members.Empty();
		Member = AddMember(Party, TEXT("Spawned"));
		Absent = AddMember(Party, TEXT("Absent"));
		TestWorld->SpawnCombatant(Member);
	});

	AfterEach([this]()
	{
		Run = nullptr;
		Member = nullptr;
		Absent = nullptr;
		TestWorld.Reset();
	});

	It("a mini-boss and a boss always give their tier's levels; a normal enemy gives none for sure", [this]()
	{
		const int32 Expected = CombatBaseline::CodeMiniBossLevels(static_cast<int32>(LEVEL_DROPS_TEST_TIER));
		TestTrue(TEXT("C tier gives levels"), Expected > 0);
		TestEqual(TEXT("mini-boss"), TrialGate::GetGuaranteedLevels(LEVEL_DROPS_TEST_TIER, EEncounterRank::MiniBoss), Expected);
		TestEqual(TEXT("boss"), TrialGate::GetGuaranteedLevels(LEVEL_DROPS_TEST_TIER, EEncounterRank::Boss), Expected);
		TestEqual(TEXT("mini-boss roll"), TrialGate::RollLevelDrops(TestWorld->GetWorld(), LEVEL_DROPS_TEST_TIER, EEncounterRank::MiniBoss), Expected);
		TestEqual(TEXT("normal guaranteed"), TrialGate::GetGuaranteedLevels(LEVEL_DROPS_TEST_TIER, EEncounterRank::Normal), 0);
		TestEqual(TEXT("mini-boss has no roll"), TrialGate::GetLevelDropChance(LEVEL_DROPS_TEST_TIER, EEncounterRank::MiniBoss), 0.0f);
	});

	It("an elite's drop chance is double a normal enemy's", [this]()
	{
		const float Normal = TrialGate::GetLevelDropChance(LEVEL_DROPS_TEST_TIER, EEncounterRank::Normal);
		TestTrue(TEXT("normal can drop"), Normal > 0.0f);
		TestEqual(TEXT("elite"), TrialGate::GetLevelDropChance(LEVEL_DROPS_TEST_TIER, EEncounterRank::Elite),
				  FMath::Min(Normal * CombatBaseline::LEVEL_DROP_ELITE_CHANCE_FACTOR, 1.0f));
	});

	It("placing a level raises the chosen pillar of a spawned member and survives a re-seed", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		Run->AddUnspentLevels(KeyOf(Member), 1);
		TestTrue(TEXT("placed"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Member, EWorldPillar::Mind).IsEmpty());
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Member);
		if (!TestNotNull(TEXT("spawned"), Comp))
		{
			return;
		}
		TestEqual(TEXT("live mind +1"), Comp->GetWorldStat(EWorldPillar::Mind), LEVEL_DROPS_TEST_ASSET_MIND + 1);
		TestEqual(TEXT("body untouched"), Comp->GetWorldStat(EWorldPillar::Body), 0);
		TestEqual(TEXT("none left"), Run->GetUnspentLevels(KeyOf(Member)), 0);
		Comp->SeedWorldStats();
		TestEqual(TEXT("re-seed keeps the run's level"), Comp->GetWorldStat(EWorldPillar::Mind), LEVEL_DROPS_TEST_ASSET_MIND + 1);
	});

	It("a member not in the level keeps the level in the run", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Absent))
		{
			return;
		}
		Run->AddUnspentLevels(KeyOf(Absent), 1);
		TestTrue(TEXT("placed"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Absent, EWorldPillar::Spirit).IsEmpty());
		TestNull(TEXT("not spawned"), WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Absent));
		TestEqual(TEXT("run holds it"), Run->GetRunEarnedLevels(UPoolSubsystem::GetCharacterKey(Absent), EWorldPillar::Spirit), 1);
		TestEqual(TEXT("live reads it"), WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), Absent, EWorldPillar::Spirit), 1);
	});

	It("refuses with nothing waiting or the pillar at the cap", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		TestFalse(TEXT("nothing waiting"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Member, EWorldPillar::Mind).IsEmpty());
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Member);
		if (!TestNotNull(TEXT("spawned"), Comp))
		{
			return;
		}
		Comp->AddEarnedWorldStat(EWorldPillar::Mind, WorldStatConstants::LIVE_MAX);
		Run->AddUnspentLevels(KeyOf(Member), 1);
		TestFalse(TEXT("at the cap"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Member, EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("level still waits"), Run->GetUnspentLevels(KeyOf(Member)), 1);
	});

	It("outside a run a drop can't be placed and a level isn't kept in the run", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		Run->RestoreRun(nullptr, 0, 0, ETrialDifficulty::Medium, {}, {}, {});
		Run->AddUnspentLevels(KeyOf(Member), 1);
		TestFalse(TEXT("refused"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Member, EWorldPillar::Mind).IsEmpty());
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Member);
		if (!TestNotNull(TEXT("spawned"), Comp))
		{
			return;
		}
		Comp->AddEarnedWorldStat(EWorldPillar::Mind, 1);
		TestEqual(TEXT("run store untouched"), Run->GetRunEarnedLevels(UPoolSubsystem::GetCharacterKey(Member), EWorldPillar::Mind), 0);
	});

	It("a run reset forgets the member's run levels", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		Run->AddUnspentLevels(KeyOf(Member), 1);
		WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Member, EWorldPillar::Body);
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Member);
		if (!TestNotNull(TEXT("spawned"), Comp))
		{
			return;
		}
		Comp->ResetRunWorldStats();
		TestEqual(TEXT("body back to the asset"), Comp->GetWorldStat(EWorldPillar::Body), 0);
		TestEqual(TEXT("run forgot it"), Run->GetRunEarnedLevels(UPoolSubsystem::GetCharacterKey(Member), EWorldPillar::Body), 0);
	});

	It("B-223b: a character's levels go only to its own pillars", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member && Absent)) return;
		Run->AddUnspentLevels(KeyOf(Member), 1);
		TestEqual(TEXT("the member has one"), Run->GetUnspentLevels(KeyOf(Member)), 1);
		TestEqual(TEXT("the other has none"), Run->GetUnspentLevels(KeyOf(Absent)), 0);
		TestFalse(TEXT("the other can't place it"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Absent, EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("still the member's"), Run->GetUnspentLevels(KeyOf(Member)), 1);
		TestTrue(TEXT("the member places it"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Member, EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("nothing waits in all"), Run->GetTotalUnspentLevels(), 0);
	});

	It("B-360c: a second copy of a character spends and reads its own levels, not the first copy's", [this]()
	{
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		if (!TestTrue(TEXT("setup"), Run && Member && Party && Party->Members.Num() >= 1)) return;
		FPartyMember &FirstCopy = Party->Members[0];
		TestTrue(TEXT("the first member is the spawned character"), FirstCopy.CharacterData == Member);
		FPartyMember &SecondCopy = Party->Members.AddDefaulted_GetRef();
		SecondCopy.PawnClass = ACombatCharacter::StaticClass();
		SecondCopy.CharacterData = Member;
		SecondCopy.MemberId = FGuid::NewGuid();
		SecondCopy.bKeyedByMember = true;
		const FName FirstKey = UPoolSubsystem::GetCharacterKey(Party->Members[0]);
		const FName SecondKey = UPoolSubsystem::GetCharacterKey(Party->Members.Last());
		if (!TestTrue(TEXT("two keys"), FirstKey != SecondKey)) return;

		Run->AddUnspentLevels(SecondKey, 1);
		TestFalse(TEXT("the first copy can't spend the second's levels"),
				  WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Party->Members[0], EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("still the second copy's"), Run->GetUnspentLevels(SecondKey), 1);
		TestTrue(TEXT("the second copy places it"),
				 WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), Party->Members.Last(), EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("none left"), Run->GetUnspentLevels(SecondKey), 0);
		TestEqual(TEXT("the level is the second copy's"), Run->GetRunEarnedLevels(SecondKey, EWorldPillar::Mind), 1);
		TestEqual(TEXT("the first copy's run levels are untouched"), Run->GetRunEarnedLevels(FirstKey, EWorldPillar::Mind), 0);
		TestEqual(TEXT("the second copy reads the asset level plus its level"),
				  WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), Party->Members.Last(), EWorldPillar::Mind), LEVEL_DROPS_TEST_ASSET_MIND + 1);
		TestEqual(TEXT("the first copy (spawned) reads the asset level"),
				  WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), Party->Members[0], EWorldPillar::Mind), LEVEL_DROPS_TEST_ASSET_MIND);

		// A pawn stamped with the second copy's id is the second copy's pawn; the unstamped pawn stays the first's.
		AActor *CopyPawn = TestWorld->SpawnCombatant(Member);
		UCharacterDataComponent *CopyComp = CopyPawn ? CopyPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("copy pawn"), CopyComp)) return;
		CopyComp->PartyMemberId = Party->Members.Last().MemberId;
		TestTrue(TEXT("the stamped pawn is the second copy's"),
				 WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Party->Members.Last()) == CopyComp);
		TestTrue(TEXT("and not the first copy's"), WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), Party->Members[0]) != CopyComp);
	});

	It("the debug readout lists the waiting levels and the members", [this]()
	{
		if (!TestNotNull(TEXT("Run"), Run))
		{
			return;
		}
		Run->AddUnspentLevels(KeyOf(Member), 2);
		const FString Readout = WoRLevelDrops::GetLevelDropString(TestWorld->GetWorld());
		TestTrue(TEXT("member listed"), Readout.Contains(TEXT("live Spawned")));
		TestTrue(TEXT("absent flagged"), Readout.Contains(TEXT("not spawned")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
