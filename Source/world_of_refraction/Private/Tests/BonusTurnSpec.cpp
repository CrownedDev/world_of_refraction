// BonusTurnSpec.cpp
// B-283: Emerald's bonus turn and the Extra Action effect (ET-12, ET-13, ET-15).
// B-341 (D-133): both now grant through the one bonus-turn rule (UTurnManager::GrantBonusTurn): the
// bonus turn lands right after the current turn at every Emerald tier; S grants two back to back.
// The old F..S delay table stays in its constants (OBSOLETE (B-341)) but is no longer read. The
// delayed ScheduleBonusTurn path (OBSOLETE) still holds ET-13's counting: delays count every turn
// taken, pinned ones included; a dead holder's pinned turn is dropped.
// ET-14 is left out here: D-052 answered A (a real extra turn), built by B-295, whose
// WoR.Turns.EmeraldExtraTurn pins it. Every case here passes with and without B-295.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"
#include "Equipment/Crystals/AugmentStoneConstants.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemExecutor.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	const EItemTier BONUS_TURN_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier,
											   EItemTier::C_Tier, EItemTier::B_Tier, EItemTier::A_Tier,
											   EItemTier::S_Tier};

	/** The retired delay table (OBSOLETE (B-341)), as literals: kept, no longer read. */
	const int32 BONUS_TURN_TEST_RULE_DELAYS[] = {6, 5, 4, 3, 2, 1, 0};
	/** ET-12 since B-341: S grants two bonus turns, every other tier one. */
	constexpr int32 BONUS_TURN_TEST_RULE_S_COUNT = 2;
	constexpr int32 BONUS_TURN_TEST_RULE_COUNT = 1;

	/** Turns recorded after the Extra Action lands; equal speed alternates, so the holder gets 1 of 3
	 *  without it and 2 of 3 with it. */
	constexpr int32 BONUS_TURN_TEST_EXTRA_WINDOW = 3;
	constexpr int32 BONUS_TURN_TEST_EXTRA_HOLDER_TURNS = 2;

	constexpr int32 BONUS_TURN_TEST_READY_DELAY = 0;
	constexpr int32 BONUS_TURN_TEST_ONE_TURN_DELAY = 1;
	constexpr int32 BONUS_TURN_TEST_AFTER_DEATH_TURNS = 3;
}

BEGIN_DEFINE_SPEC(FBonusTurnSpec, "WoR.Turns.BonusTurn",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTurnManager *TurnManager = nullptr;
	UItemExecutor *Items = nullptr;
	USkillEffectManager *Effects = nullptr;
	/** Equal speed: the local Hero takes turn 1, then they alternate. */
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** The actor and kind of each of the next NumTurns turns. */
	TArray<FPreviewTurnEntry> RecordNextTurns(int32 NumTurns)
	{
		TArray<FPreviewTurnEntry> Turns;
		for (int32 Turn = 0; Turn < NumTurns; ++Turn)
		{
			TurnManager->AdvanceToNextTurn();
			FPreviewTurnEntry Entry;
			Entry.Actor = TurnManager->GetCurrentActor();
			Entry.Type = TurnManager->GetCurrentTurnType();
			Entry.bIsBonusTurn = TurnManager->GetCurrentTurnIsBonus();
			Turns.Add(Entry);
		}
		return Turns;
	}

	bool IsBonusFor(const FPreviewTurnEntry &Turn, const AActor *Holder) const
	{
		return Turn.Actor == Holder && Turn.Type == ETurnType::Bonus && Turn.bIsBonusTurn;
	}
	TOptional<FWoRActionPipsSwitchScope> SpeedTimeline; // B-170h: this spec checks the old speed timeline
END_DEFINE_SPEC(FBonusTurnSpec)

void FBonusTurnSpec::Define()
{
	BeforeEach([this]()
	{
		SpeedTimeline.Emplace(WOR_TEST_SPEED_TIMELINE);
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		Items = TestWorld->GetSubsystem<UItemExecutor>();
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("TurnManager"), TurnManager);
		TestNotNull(TEXT("ItemExecutor"), Items);
		TestNotNull(TEXT("SkillEffectManager"), Effects);
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		SpeedTimeline.Reset();
		if (TurnManager && TurnManager->IsCombatActive())
		{
			TurnManager->EndCombat();
		}
		TurnManager = nullptr;
		Items = nullptr;
		Effects = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("keeps the old delay table but lands every Emerald's bonus next; S grants two (ET-12, B-341)", [this]()
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BONUS_TURN_TEST_TIERS); ++Index)
		{
			const FCrystalId Emerald(ECrystalType::Emerald, BONUS_TURN_TEST_TIERS[Index]);
			const FString Tier = FString::Printf(TEXT(" at tier index %d"), Index);
			TestEqual(TEXT("EMERALD_BONUS_TURN_DELAY (kept, unread)") + Tier, AugmentStoneConstants::EMERALD_BONUS_TURN_DELAY[Index],
					  BONUS_TURN_TEST_RULE_DELAYS[Index]);
			TestEqual(TEXT("GetEmeraldBonusTurnDelay: lands next") + Tier, CrystalEffectTable::GetEmeraldBonusTurnDelay(Emerald), 0);
			TestEqual(TEXT("GetEmeraldBonusTurnCount") + Tier, CrystalEffectTable::GetEmeraldBonusTurnCount(Emerald),
					  BONUS_TURN_TEST_TIERS[Index] == EItemTier::S_Tier ? BONUS_TURN_TEST_RULE_S_COUNT : BONUS_TURN_TEST_RULE_COUNT);
		}
	});

	It("an Emerald's bonus turn lands right after the current turn at every tier (ET-12, B-341)", [this]()
	{
		if (!TurnManager || !Items) return;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BONUS_TURN_TEST_TIERS); ++Index)
		{
			if (BONUS_TURN_TEST_TIERS[Index] == EItemTier::S_Tier)
			{
				continue; // S grants two turns: its own It below
			}
			const FString Tier = FString::Printf(TEXT(" at tier index %d"), Index);
			TurnManager->InitializeCombat({Hero}, {Foe});
			if (!TestTrue(TEXT("Hero takes turn 1") + Tier, TurnManager->GetCurrentActor() == Hero))
			{
				continue;
			}
			TestTrue(TEXT("Emerald used") + Tier,
					 Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, BONUS_TURN_TEST_TIERS[Index]), Hero).bSuccess);

			const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(BONUS_TURN_TEST_RULE_COUNT + 1);
			TestTrue(TEXT("the next turn is Hero's bonus turn") + Tier, IsBonusFor(Turns[0], Hero));
			TestTrue(TEXT("then the speed pick resumes") + Tier, Turns[BONUS_TURN_TEST_RULE_COUNT].Type == ETurnType::Normal);
			TurnManager->EndCombat();
		}
	});

	It("an S Emerald grants two turns back to back at once (ET-12)", [this]()
	{
		if (!TurnManager || !Items) return;
		TurnManager->InitializeCombat({Hero}, {Foe});
		TestTrue(TEXT("Emerald S used"),
				 Items->UseItem(Hero, FCrystalId(ECrystalType::Emerald, EItemTier::S_Tier), Hero).bSuccess);
		const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(BONUS_TURN_TEST_RULE_S_COUNT + 1);
		for (int32 Turn = 0; Turn < BONUS_TURN_TEST_RULE_S_COUNT; ++Turn)
		{
			TestTrue(FString::Printf(TEXT("turn %d after use is Hero's bonus turn"), Turn + 1), IsBonusFor(Turns[Turn], Hero));
		}
		TestTrue(TEXT("then the speed pick resumes"), Turns[BONUS_TURN_TEST_RULE_S_COUNT].Type == ETurnType::Normal);
	});

	It("a pinned turn counts toward another pinned turn's delay (ET-13)", [this]()
	{
		if (!TurnManager) return;
		TurnManager->InitializeCombat({Hero}, {Foe});
		TurnManager->ScheduleBonusTurn(Foe, BONUS_TURN_TEST_READY_DELAY);
		TurnManager->ScheduleBonusTurn(Hero, BONUS_TURN_TEST_ONE_TURN_DELAY);
		const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(2);
		TestTrue(TEXT("Foe's ready bonus fires first"), IsBonusFor(Turns[0], Foe));
		TestTrue(TEXT("that bonus turn used up Hero's one-turn delay"), IsBonusFor(Turns[1], Hero));
	});

	It("a dead holder's pinned turn is dropped (ET-13)", [this]()
	{
		if (!TurnManager) return;
		TurnManager->InitializeCombat({Hero}, {Foe});
		TurnManager->ScheduleBonusTurn(Foe, BONUS_TURN_TEST_READY_DELAY);
		UCharacterDataComponent *FoeData = Foe->FindComponentByClass<UCharacterDataComponent>();
		FoeData->ServerTakeDamage(FoeData->MaxHP);
		if (!TestFalse(TEXT("Foe is dead"), FoeData->bIsAlive))
		{
			return;
		}
		const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(BONUS_TURN_TEST_AFTER_DEATH_TURNS);
		for (int32 Turn = 0; Turn < Turns.Num(); ++Turn)
		{
			TestTrue(FString::Printf(TEXT("turn %d is Hero's speed pick"), Turn + 1),
					 Turns[Turn].Actor == Hero && Turns[Turn].Type == ETurnType::Normal);
		}
	});

	It("Extra Action grants a bonus turn that lands next (ET-15, B-341)", [this]()
	{
		if (!TurnManager || !Effects) return;
		TurnManager->InitializeCombat({Hero}, {Foe});
		if (!TestTrue(TEXT("Hero takes turn 1"), TurnManager->GetCurrentActor() == Hero))
		{
			return;
		}
		// Duration 0: an instant effect, applied once on landing.
		Effects->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Test extra action"), 0, ESkillEffectType::ExtraAction, 0.0f, 0),
							 Hero);
		const TArray<FPreviewTurnEntry> Turns = RecordNextTurns(BONUS_TURN_TEST_EXTRA_WINDOW);
		TestTrue(TEXT("Hero acts again next, as a bonus turn"), IsBonusFor(Turns[0], Hero));
		int32 HeroTurns = 0;
		for (const FPreviewTurnEntry &Turn : Turns)
		{
			HeroTurns += Turn.Actor == Hero ? 1 : 0;
		}
		TestEqual(TEXT("Hero takes one turn more than alternation gives"), HeroTurns, BONUS_TURN_TEST_EXTRA_HOLDER_TURNS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
