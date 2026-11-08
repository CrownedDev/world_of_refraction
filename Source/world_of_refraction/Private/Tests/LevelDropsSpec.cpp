// LevelDropsSpec.cpp
// B-223 (D-102, Q-223): defeated enemies drop pillar levels into the run; the player places each on a party
// member's chosen pillar (WoRLevelDrops::TryPlaceLevel), capped at 7, and a re-seed keeps it.
// B-360c (Q-360c): a later copy of a character places its own levels, under its own member key.
// B-364 (D-157, Q-157): a point placed at 7 becomes an over-cap point (3 per pillar, +5 % each to the pillar).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Character/OverCapRules.h"
#include "Combat/Tier/CombatTier.h"
#include "Currency/CurrencyComponent.h"
#include "Player/WoRPlayerState.h"
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
/** B-360c: the party member playing Character (the first copy). */
const FPartyMember &PartyOf(const UCharacterData *Character) const
{
	static const FPartyMember NoMember;
	const UPartySessionSubsystem *Session = TestWorld->GetSubsystem<UPartySessionSubsystem>();
	const UParty *Party = Session ? Session->GetLocalParty() : nullptr;
	const FPartyMember *Found = Party ? Party->Members.FindByPredicate([Character](const FPartyMember &M) { return M.CharacterData == Character; }) : nullptr;
	return Found ? *Found : NoMember;
}
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
			Run->SetAtRestSpot(true); // B-363 (D-156): points are placed at a rest spot
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
		TestTrue(TEXT("placed"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind).IsEmpty());
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), PartyOf(Member));
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

	It("places and buys points only at a rest spot; the price climbs per character (B-363, Q-156)", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		Run->AddUnspentLevels(KeyOf(Member), 1);
		Run->SetAtRestSpot(false);
		TestFalse(TEXT("away from a rest spot: refused"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body).IsEmpty());
		TestFalse(TEXT("buying too"), WoRLevelDrops::TryBuyPoint(TestWorld->GetWorld(), PartyOf(Member)).IsEmpty());
		TestEqual(TEXT("the point still waits"), Run->GetUnspentLevels(KeyOf(Member)), 1);
		Run->SetAtRestSpot(true);
		const int32 First = WoRLevelDrops::GetPointPrice(TestWorld->GetWorld(), PartyOf(Member));
		Run->AddRunLevelPurchase(KeyOf(Member));
		TestTrue(TEXT("the next costs more"), WoRLevelDrops::GetPointPrice(TestWorld->GetWorld(), PartyOf(Member)) > First);
		TestEqual(TEXT("another character's price is its own"), WoRLevelDrops::GetPointPrice(TestWorld->GetWorld(), PartyOf(Absent)), First);
		TestFalse(TEXT("no wallet here: buying refused"), WoRLevelDrops::TryBuyPoint(TestWorld->GetWorld(), PartyOf(Member)).IsEmpty());
	});

	It("a win banks the drop without placing it; buying spends the ladder price (B-363 review)", [this]()
	{
		AWoRPlayerState *Player = TestWorld->GetWorld()->SpawnActor<AWoRPlayerState>();
		UCurrencyComponent *Wallet = Player ? Player->GetWallet() : nullptr;
		if (!TestTrue(TEXT("setup"), Run && Member && Wallet))
		{
			return;
		}
		const int32 MindBefore = WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind);
		Run->AddUnspentLevels(KeyOf(Member), 1); // what a won fight's drop does (CombatOrchestrator::FinishCombat)
		TestEqual(TEXT("banked"), Run->GetUnspentLevels(KeyOf(Member)), 1);
		TestEqual(TEXT("not placed"), WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind), MindBefore);

		const int32 Price = WoRLevelDrops::GetPointPrice(TestWorld->GetWorld(), PartyOf(Member));
		Wallet->Add(ECurrencyType::Gold, Price - 1); // the run currency (shown as Prisms, D-159)
		TestFalse(TEXT("one short: refused"), WoRLevelDrops::TryBuyPoint(TestWorld->GetWorld(), PartyOf(Member), Wallet).IsEmpty());
		TestEqual(TEXT("nothing spent"), Wallet->GetBalance(ECurrencyType::Gold), Price - 1);
		Wallet->Add(ECurrencyType::Gold, 1);
		TestTrue(TEXT("bought"), WoRLevelDrops::TryBuyPoint(TestWorld->GetWorld(), PartyOf(Member), Wallet).IsEmpty());
		TestEqual(TEXT("the price spent"), Wallet->GetBalance(ECurrencyType::Gold), 0);
		TestEqual(TEXT("one more to place"), Run->GetUnspentLevels(KeyOf(Member)), 2);
		TestEqual(TEXT("one purchase"), Run->GetRunLevelPurchases(KeyOf(Member)), 1);
		TestTrue(TEXT("the next costs more"), WoRLevelDrops::GetPointPrice(TestWorld->GetWorld(), PartyOf(Member)) > Price);
	});

	It("a member not in the level keeps the level in the run", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Absent))
		{
			return;
		}
		Run->AddUnspentLevels(KeyOf(Absent), 1);
		TestTrue(TEXT("placed"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Absent), EWorldPillar::Spirit).IsEmpty());
		TestNull(TEXT("not spawned"), WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), PartyOf(Absent)));
		TestEqual(TEXT("run holds it"), Run->GetRunEarnedLevels(UPoolSubsystem::GetCharacterKey(Absent), EWorldPillar::Spirit), 1);
		TestEqual(TEXT("live reads it"), WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), PartyOf(Absent), EWorldPillar::Spirit), 1);
	});

	It("refuses with nothing waiting", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		TestFalse(TEXT("nothing waiting"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind).IsEmpty());
	});

	It("B-364: at 7 a point goes over-cap, 3 at most, each lifting the pillar 5 % and not the Combat Tier", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), PartyOf(Member));
		if (!TestNotNull(TEXT("spawned"), Comp))
		{
			return;
		}
		Comp->AddEarnedWorldStat(EWorldPillar::Body, WorldStatConstants::LIVE_MAX);
		const float BodyAtCap = Comp->GetEvolutionModifiedBody();
		const float MindBefore = Comp->GetEvolutionModifiedMind();
		const EItemTier TierBefore = WoRCombatTier::GetCombatTier(Comp->GetOwner());
		Run->AddUnspentLevels(KeyOf(Member), OverCapRules::MAX_PER_PILLAR + 1);

		TestTrue(TEXT("placed at 7"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body).IsEmpty());
		TestEqual(TEXT("live stays 7"), Comp->GetWorldStat(EWorldPillar::Body), WorldStatConstants::LIVE_MAX);
		TestEqual(TEXT("one over-cap point"), Comp->GetOverCapPoints(EWorldPillar::Body), 1);
		TestEqual(TEXT("helper reads it"), WoRLevelDrops::GetOverCap(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body), 1);
		TestEqual(TEXT("body +5 %"), Comp->GetEvolutionModifiedBody(), BodyAtCap * OverCapRules::ScaleFor(1), KINDA_SMALL_NUMBER);
		TestEqual(TEXT("mind untouched"), Comp->GetEvolutionModifiedMind(), MindBefore, KINDA_SMALL_NUMBER);
		TestEqual(TEXT("Combat Tier unchanged"), WoRCombatTier::GetCombatTier(Comp->GetOwner()), TierBefore);

		for (int32 Index = 1; Index < OverCapRules::MAX_PER_PILLAR; ++Index)
		{
			WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body);
		}
		TestEqual(TEXT("three over-cap"), Comp->GetOverCapPoints(EWorldPillar::Body), OverCapRules::MAX_PER_PILLAR);
		TestTrue(TEXT("pillar full"), WoRLevelDrops::IsPillarFull(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body));
		TestFalse(TEXT("a 4th is refused"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body).IsEmpty());
		TestEqual(TEXT("its point still waits"), Run->GetUnspentLevels(KeyOf(Member)), 1);
		TestEqual(TEXT("body +15 %"), Comp->GetEvolutionModifiedBody(), BodyAtCap * OverCapRules::ScaleFor(OverCapRules::MAX_PER_PILLAR), KINDA_SMALL_NUMBER);
		TestTrue(TEXT("debug shows it"), WoRLevelDrops::GetLevelDropString(TestWorld->GetWorld()).Contains(TEXT("+3 over")));

		Comp->ResetRunWorldStats();
		TestEqual(TEXT("the run's end clears them"), Comp->GetOverCapPoints(EWorldPillar::Body), 0);
	});

	It("B-364: buying stops only when every pillar is full", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), PartyOf(Member));
		if (!TestNotNull(TEXT("spawned"), Comp))
		{
			return;
		}
		for (const EWorldPillar Pillar : {EWorldPillar::Mind, EWorldPillar::Body, EWorldPillar::Spirit})
		{
			Comp->AddEarnedWorldStat(Pillar, WorldStatConstants::LIVE_MAX);
		}
		TestFalse(TEXT("at 7 everywhere: not full yet"), WoRLevelDrops::IsPillarFull(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind));
		for (const EWorldPillar Pillar : {EWorldPillar::Mind, EWorldPillar::Body, EWorldPillar::Spirit})
		{
			for (int32 Index = 0; Index < OverCapRules::MAX_PER_PILLAR; ++Index)
			{
				Run->AddRunOverCap(KeyOf(Member), Pillar);
			}
		}
		TestFalse(TEXT("a 4th over-cap point is refused by the run"), Run->AddRunOverCap(KeyOf(Member), EWorldPillar::Mind));
		const FText Refusal = WoRLevelDrops::TryBuyPoint(TestWorld->GetWorld(), PartyOf(Member));
		TestTrue(TEXT("all full: buying refused for that"), Refusal.ToString().Contains(TEXT("over-cap")));
	});

	It("outside a run a drop can't be placed and a level isn't kept in the run", [this]()
	{
		if (!TestTrue(TEXT("setup"), Run && Member))
		{
			return;
		}
		Run->RestoreRun(nullptr, 0, 0, ETrialDifficulty::Medium, {}, {}, {});
		Run->AddUnspentLevels(KeyOf(Member), 1);
		TestFalse(TEXT("refused"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind).IsEmpty());
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), PartyOf(Member));
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
		WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Body);
		UCharacterDataComponent *Comp = WoRLevelDrops::FindSpawnedMember(TestWorld->GetWorld(), PartyOf(Member));
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
		TestFalse(TEXT("the other can't place it"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Absent), EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("still the member's"), Run->GetUnspentLevels(KeyOf(Member)), 1);
		TestTrue(TEXT("the member places it"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Member), EWorldPillar::Mind).IsEmpty());
		TestEqual(TEXT("nothing waits in all"), Run->GetTotalUnspentLevels(), 0);
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

	It("lets a later copy of a character place its own levels, not the first copy's (B-360c)", [this]()
	{
		UParty *Party = TestWorld->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty();
		if (!TestTrue(TEXT("setup"), Run && Absent && Party)) return;
		const FPartyMember First = PartyOf(Absent); // a copy: never add an element of the array to itself
		FPartyMember &Copy = Party->Members.Add_GetRef(First);
		Copy.MemberId = FGuid::NewGuid();
		Copy.bKeyedByMember = true;
		const FPartyMember CopyMember = Copy;
		const FName CopyKey = UPoolSubsystem::GetCharacterKey(CopyMember);
		if (!TestNotEqual(TEXT("the copy has its own key"), CopyKey, KeyOf(Absent))) return;
		Run->AddUnspentLevels(CopyKey, 1);
		TestFalse(TEXT("the first copy has none to place"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), PartyOf(Absent), EWorldPillar::Body).IsEmpty());
		TestTrue(TEXT("the copy places its own"), WoRLevelDrops::TryPlaceLevel(TestWorld->GetWorld(), CopyMember, EWorldPillar::Body).IsEmpty());
		TestEqual(TEXT("under the copy's key"), Run->GetRunEarnedLevels(CopyKey, EWorldPillar::Body), 1);
		TestEqual(TEXT("not the first copy's"), Run->GetRunEarnedLevels(KeyOf(Absent), EWorldPillar::Body), 0);
		TestEqual(TEXT("the copy's live level"), WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), CopyMember, EWorldPillar::Body),
				  WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), PartyOf(Absent), EWorldPillar::Body) + 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
