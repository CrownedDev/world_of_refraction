// ApparelBonusLineSpec.cpp
// B-475 (D-208): an apparel piece rolls one bonus line when it is first owned: a sub-stat (never Turn Speed) or +5% to
// one pillar. Two drops of one apparel can roll different lines; a piece keeps its line (the same id rolls the same
// line, a loaded piece is not re-rolled); the line adds to the piece's stat lines and is named for the screens.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Apparel/ApparelBonusLine.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Math/RandomStream.h"
#include "Pool/PoolSubsystem.h"
#include "Character/CharacterData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	/** Enough rolls that every outcome shows up (14 outcomes). */
	constexpr int32 APPAREL_BONUS_TEST_ROLLS = 256;
	constexpr int32 APPAREL_BONUS_TEST_SEED = 4752;
	constexpr int32 APPAREL_BONUS_TEST_DROPS = 16;
}

BEGIN_DEFINE_SPEC(FApparelBonusLineSpec, "WoR.Apparel.BonusLine", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FApparelBonusLineSpec)

void FApparelBonusLineSpec::Define()
{
	It("rolls one line: a sub-stat but never Turn Speed, or +5% to a pillar (D-208)", [this]()
	{
		FRandomStream Stream(APPAREL_BONUS_TEST_SEED);
		bool bSawSubStat = false;
		bool bSawPillar = false;
		for (int32 Roll = 0; Roll < APPAREL_BONUS_TEST_ROLLS; ++Roll)
		{
			const FApparelBonusLine Line = ApparelBonusLine::Roll(Stream);
			TestTrue(TEXT("a line"), Line.IsSet());
			if (Line.Kind == EApparelBonusKind::SubStat)
			{
				bSawSubStat = true;
				TestTrue(TEXT("never Turn Speed"), Line.SubStat != ESubStat::TurnSpeed && Line.SubStat != ESubStat::None);
				TestEqual(TEXT("a small sub-stat line"), Line.Value, ApparelBonusLine::SUB_STAT_LINE_POINTS);
			}
			else
			{
				bSawPillar = true;
				TestEqual(TEXT("a pillar line is +5%"), Line.Value, ApparelBonusLine::PILLAR_LINE_PERCENT);
			}
		}
		TestTrue(TEXT("sub-stat lines roll"), bSawSubStat);
		TestTrue(TEXT("pillar lines roll"), bSawPillar);
		TestFalse(TEXT("Turn Speed is not rollable"), ApparelBonusLine::GetRollableSubStats().Contains(ESubStat::TurnSpeed));
	});

	It("turns the line into stats: a sub-stat's points, a pillar's percent", [this]()
	{
		FApparelEntry Piece;
		Piece.BonusLine.Kind = EApparelBonusKind::SubStat;
		Piece.BonusLine.SubStat = ESubStat::Luck;
		Piece.BonusLine.Value = ApparelBonusLine::SUB_STAT_LINE_POINTS;
		TestEqual(TEXT("+2 Luck"), Piece.GetBonusLineStats().BonusLuck, FMath::RoundToInt(ApparelBonusLine::SUB_STAT_LINE_POINTS));
		TestFalse(TEXT("named"), ApparelBonusLine::GetText(Piece.BonusLine).IsEmpty());

		Piece.BonusLine.Kind = EApparelBonusKind::Pillar;
		Piece.BonusLine.Pillar = EWorldPillar::Spirit;
		Piece.BonusLine.Value = ApparelBonusLine::PILLAR_LINE_PERCENT;
		const FEquipmentStatBonus Lines = Piece.GetBonusLineStats();
		TestEqual(TEXT("+5% Spirit"), Lines.BonusSpiritModifierPercent, ApparelBonusLine::PILLAR_LINE_PERCENT);
		TestEqual(TEXT("no pillar points elsewhere"), Lines.BonusMindModifierPercent, 0.0f);
		TestTrue(TEXT("named as a percent"), ApparelBonusLine::GetText(Piece.BonusLine).ToString().Contains(TEXT("%")));
	});

	It("rolls the same line for the same piece", [this]()
	{
		const FGuid Id = FGuid::NewGuid();
		TestTrue(TEXT("same id, same line"), ApparelBonusLine::RollFor(Id) == ApparelBonusLine::RollFor(Id));
	});

	Describe("owned in the pool", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("adds a worn piece's line flat, the same at every tier (D-208)", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, nullptr);
			if (!TestNotNull(TEXT("Loadout"), Loadout))
			{
				return;
			}
			UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			const int32 LuckBefore = Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusLuck;
			FApparelEntry Piece = FApparelEntry::CreateFromApparel(NewObject<UApparelData>(GetTransientPackage()));
			Piece.Tier = EItemTier::S_Tier;
			Piece.BonusLine.Kind = EApparelBonusKind::SubStat;
			Piece.BonusLine.SubStat = ESubStat::Luck;
			Piece.BonusLine.Value = ApparelBonusLine::SUB_STAT_LINE_POINTS;
			TestTrue(TEXT("worn"), ApparelRules::AddApparel(Active, Piece));
			TestEqual(TEXT("an S piece's +2 Luck is +2"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusLuck - LuckBefore,
					  FMath::RoundToInt(ApparelBonusLine::SUB_STAT_LINE_POINTS));
		});

		It("gives two drops of one apparel lines that can differ, and keeps a loaded piece's line (D-208)", [this]()
		{
			UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
			if (!TestNotNull(TEXT("Pool"), Pool))
			{
				return;
			}
			UApparelData *Coat = NewObject<UApparelData>(GetTransientPackage());
			TArray<FApparelBonusLine> Lines;
			for (int32 Drop = 0; Drop < APPAREL_BONUS_TEST_DROPS; ++Drop)
			{
				const FGuid Id = Pool->AddApparelToPool(FApparelEntry::CreateFromApparel(Coat));
				const FApparelEntry *Owned = Pool->FindApparelInPool(Id);
				if (TestNotNull(TEXT("owned"), Owned))
				{
					TestTrue(TEXT("each drop rolled a line"), Owned->BonusLine.IsSet());
					Lines.AddUnique(Owned->BonusLine);
				}
			}
			TestTrue(TEXT("copies of one apparel roll different lines"), Lines.Num() > 1);

			FApparelEntry Loaded = FApparelEntry::CreateFromApparel(Coat);
			Loaded.PersistentID = FGuid::NewGuid();
			Loaded.BonusLine.Kind = EApparelBonusKind::Pillar;
			Loaded.BonusLine.Pillar = EWorldPillar::Body;
			Loaded.BonusLine.Value = ApparelBonusLine::PILLAR_LINE_PERCENT;
			const FApparelEntry *Back = Pool->FindApparelInPool(Pool->AddApparelToPool(Loaded));
			if (TestNotNull(TEXT("loaded"), Back))
			{
				TestTrue(TEXT("its saved line is kept"), Back->BonusLine == Loaded.BonusLine);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
