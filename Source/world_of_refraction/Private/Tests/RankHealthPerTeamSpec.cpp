// RankHealthPerTeamSpec.cpp
// B-224b (Q-224c, Q-224d): the rank's HP is each team's own. A caught fight of a Boss team and a Normal team fields
// the boss's strongest at boss HP (x8) and the normal team's at x1; a mini-boss form alone is a mini-boss, its teammates keep
// their team's rank; an enemy's tier power (its Combat Tier, B-325; here the EnemyTier fallback, no loadout) multiplies its HP, F changing nothing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/Forms/CharacterFormData.h"
#include "Combat/Encounter/EncounterData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 RANK_HP_BOSS_TEAM = 0;
	constexpr int32 RANK_HP_NORMAL_TEAM = 1;
	constexpr int32 RANK_HP_NORMAL_TEAM_SIZE = 2;
	constexpr float RANK_HP_BOSS = 8.0f; // B-293d (Q-293d)
	constexpr float RANK_HP_TIER_C = 2.2f;
	/** MaxHP is rounded once after the multiply. */
	constexpr float RANK_HP_ROUNDING = 1.0f;
	constexpr float NO_STRENGTH_CHANGE = 1.0f;
}

BEGIN_DEFINE_SPEC(FRankHealthPerTeamSpec, "WoR.Trial.RankHealthPerTeam",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	UCharacterDataComponent *SpawnEnemy(EItemTier Tier = EItemTier::F_Tier)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->EnemyTier = Tier;
		AActor *Enemy = TestWorld->SpawnCombatant(Data, true);
		return Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}
END_DEFINE_SPEC(FRankHealthPerTeamSpec)

void FRankHealthPerTeamSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("tells which caught team each fielded enemy came from, so the boss team's gets Boss and the normal team's Normal", [this]()
	{
		UCharacterData *Boss = FWoRTestWorld::NewCharacterData();
		TArray<UCharacterData *> NormalTeam;
		for (int32 Index = 0; Index < RANK_HP_NORMAL_TEAM_SIZE; ++Index)
		{
			NormalTeam.Add(FWoRTestWorld::NewCharacterData());
		}
		TArray<int32> Teams;
		const TArray<UCharacterData *> Roster = UEncounterData::BuildCaughtRoster({{Boss}, NormalTeam}, Teams);
		if (!TestEqual(TEXT("one from each team"), Roster.Num(), 2) || !TestEqual(TEXT("a team per member"), Teams.Num(), 2)) return;
		TestEqual(TEXT("the boss came from the boss team"), Teams[0], RANK_HP_BOSS_TEAM);
		TestEqual(TEXT("the other from the normal team"), Teams[1], RANK_HP_NORMAL_TEAM);
		const EEncounterRank TeamRanks[] = {EEncounterRank::Boss, EEncounterRank::Normal};
		TestTrue(TEXT("the boss is a Boss"), UEncounterData::GetMemberRank(nullptr, TeamRanks[Teams[0]], Roster[0]) == EEncounterRank::Boss);
		TestTrue(TEXT("the normal enemy stays Normal"), UEncounterData::GetMemberRank(nullptr, TeamRanks[Teams[1]], Roster[1]) == EEncounterRank::Normal);

		TArray<int32> OneTeam;
		const TArray<UCharacterData *> Whole = UEncounterData::BuildCaughtRoster({NormalTeam}, OneTeam);
		TestEqual(TEXT("one team fights whole"), Whole.Num(), RANK_HP_NORMAL_TEAM_SIZE);
		TestEqual(TEXT("every member from team 0"), OneTeam, TArray<int32>{RANK_HP_BOSS_TEAM, RANK_HP_BOSS_TEAM});
	});

	It("makes a mini-boss form alone a mini-boss; its teammates keep the team's rank", [this]()
	{
		UEncounterData *Encounter = NewObject<UEncounterData>(GetTransientPackage());
		UCharacterFormData *Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = FWoRTestWorld::NewCharacterData();
		Encounter->MiniBossForm = Form;
		UCharacterData *Teammate = FWoRTestWorld::NewCharacterData();
		TestTrue(TEXT("the form is a mini-boss"), UEncounterData::GetMemberRank(Encounter, EEncounterRank::Normal, Form->Character) == EEncounterRank::MiniBoss);
		TestTrue(TEXT("its teammate stays Normal"), UEncounterData::GetMemberRank(Encounter, EEncounterRank::Normal, Teammate) == EEncounterRank::Normal);
		TestTrue(TEXT("a boss team's form stays Boss"), UEncounterData::GetMemberRank(Encounter, EEncounterRank::Boss, Form->Character) == EEncounterRank::Boss);
	});

	It("gives only the boss-ranked enemy the boss HP (x8)", [this]()
	{
		UCharacterDataComponent *Boss = SpawnEnemy();
		UCharacterDataComponent *Normal = SpawnEnemy();
		if (!TestTrue(TEXT("enemies spawned"), Boss && Normal)) return;
		const int32 BaseMaxHP = Normal->MaxHP;
		TestTrue(TEXT("boss applied"), TrialGate::ApplyEnemyRankHealth(Boss->GetOwner(), NO_STRENGTH_CHANGE, EEncounterRank::Boss));
		TestTrue(TEXT("normal applied"), TrialGate::ApplyEnemyRankHealth(Normal->GetOwner(), NO_STRENGTH_CHANGE, EEncounterRank::Normal));
		TestEqual(TEXT("the boss has x8"), static_cast<float>(Boss->MaxHP), BaseMaxHP * RANK_HP_BOSS, RANK_HP_ROUNDING);
		TestEqual(TEXT("the boss starts full"), Boss->CurrentHP, Boss->MaxHP);
		TestEqual(TEXT("the normal enemy keeps its HP"), Normal->MaxHP, BaseMaxHP);
	});

	It("falls back to the fight's rank when no rank list was set for the roster", [this]()
	{
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("trial run"), TrialRun)) return;
		TrialRun->SetPendingEnemyRanks({EEncounterRank::Boss});
		// No pending roster (no EnterEncounter): the list doesn't match it, so every index reads the fight's rank.
		TestTrue(TEXT("a mismatched list is ignored"), TrialRun->GetPendingEnemyRank(0) == TrialRun->GetPendingRank());
		TrialRun->SetPendingEnemyRanks({});
		TestTrue(TEXT("no list: the fight's rank"), TrialRun->GetPendingEnemyRank(0) == TrialRun->GetPendingRank());
	});

	It("multiplies an enemy's HP by its tier power; F changes nothing", [this]()
	{
		UCharacterDataComponent *TierC = SpawnEnemy(EItemTier::C_Tier);
		UCharacterDataComponent *TierF = SpawnEnemy(EItemTier::F_Tier);
		if (!TestTrue(TEXT("enemies spawned"), TierC && TierF)) return;
		const int32 BaseMaxHP = TierF->MaxHP;
		TestEqual(TEXT("F is x1"), TrialGate::GetEnemyTierHealthMultiplier(TierF->GetOwner()), 1.0f, KINDA_SMALL_NUMBER);
		TestEqual(TEXT("C is x2.2"), TrialGate::GetEnemyTierHealthMultiplier(TierC->GetOwner()), RANK_HP_TIER_C, KINDA_SMALL_NUMBER);
		TrialGate::ApplyEnemyRankHealth(TierC->GetOwner(), NO_STRENGTH_CHANGE, EEncounterRank::Normal);
		TrialGate::ApplyEnemyRankHealth(TierF->GetOwner(), NO_STRENGTH_CHANGE, EEncounterRank::Normal);
		TestEqual(TEXT("a normal C enemy has x2.2"), static_cast<float>(TierC->MaxHP), BaseMaxHP * RANK_HP_TIER_C, RANK_HP_ROUNDING);
		TestEqual(TEXT("a normal F enemy is unchanged"), TierF->MaxHP, BaseMaxHP);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
