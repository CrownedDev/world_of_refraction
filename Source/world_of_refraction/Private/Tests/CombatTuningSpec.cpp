// CombatTuningSpec.cpp
// B-218: the combat tuning data starts equal to the C++ constants. A ladder curve table filled from
// the C++ ladders gives every key's C++ value (the gap ladders also beyond their keys); an edited
// key changes the result; with no asset every getter keeps its C++ value.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Tuning/CombatTuningData.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Engine/CurveTable.h"

namespace
{
	const ECombatLadder TUNING_TEST_LADDERS[] = {
		ECombatLadder::TierPower, ECombatLadder::TierGapDamage, ECombatLadder::TierGapCost,
		ECombatLadder::RequirementGap, ECombatLadder::ScalingGrade};
	/** Past every ladder's last key, to check what holds beyond the range. */
	constexpr int32 TUNING_TEST_BEYOND = 3;
	constexpr float TUNING_TEST_EDITED_VALUE = 9.5f;
}

BEGIN_DEFINE_SPEC(FCombatTuningSpec, "WoR.Tuning.CombatData",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UCombatTuningData *Tuning = nullptr;
	UCurveTable *Table = nullptr;
END_DEFINE_SPEC(FCombatTuningSpec)

void FCombatTuningSpec::Define()
{
	BeforeEach([this]()
	{
		Tuning = NewObject<UCombatTuningData>(GetTransientPackage());
		Table = NewObject<UCurveTable>(GetTransientPackage());
	});

	AfterEach([this]()
	{
		Tuning = nullptr;
		Table = nullptr;
		UCurveTable::InvalidateAllCachedCurves();
	});

	It("starts equal to the C++ constants", [this]()
	{
		const FString Differences = Tuning->GetDifferencesFromCode();
		TestTrue(FString::Printf(TEXT("no differences [%s]"), *Differences), Differences.IsEmpty());
	});

	It("fills a curve table whose every key is the C++ value", [this]()
	{
		if (!TestTrue(TEXT("filled"), Tuning->FillLadderRows(Table)))
		{
			return;
		}
		for (const ECombatLadder Ladder : TUNING_TEST_LADDERS)
		{
			int32 First, Last;
			UCombatTuningData::GetLadderKeyRange(Ladder, First, Last);
			for (int32 Key = First - TUNING_TEST_BEYOND; Key <= Last + TUNING_TEST_BEYOND; ++Key)
			{
				const float Code = UCombatTuningData::GetCodeLadderValue(Ladder, Key);
				TestEqual(FString::Printf(TEXT("ladder %d key %d"), static_cast<int32>(Ladder), Key),
						  Tuning->GetLadder(Ladder, Key, Code), Code);
			}
		}
		TestTrue(TEXT("still no differences"), Tuning->GetDifferencesFromCode().IsEmpty());
	});

	It("uses an edited curve key", [this]()
	{
		if (!TestTrue(TEXT("filled"), Tuning->FillLadderRows(Table)))
		{
			return;
		}
		const int32 SKey = static_cast<int32>(EItemTier::S_Tier) - static_cast<int32>(EItemTier::F_Tier);
		FRichCurve *Curve = Table->FindRichCurve(FName(TEXT("TierPower")), TEXT("CombatTuningSpec"));
		if (!TestNotNull(TEXT("TierPower row"), Curve))
		{
			return;
		}
		Curve->UpdateOrAddKey(static_cast<float>(SKey), TUNING_TEST_EDITED_VALUE);
		UCurveTable::InvalidateAllCachedCurves();
		TestEqual(TEXT("S tier power from the curve"), Tuning->GetLadder(ECombatLadder::TierPower, SKey, 0.0f), TUNING_TEST_EDITED_VALUE);
		TestFalse(TEXT("reported as a difference"), Tuning->GetDifferencesFromCode().IsEmpty());
	});

	It("keeps the C++ value for a table without the ladder's row, and starts each ladder at scale 1", [this]()
	{
		Tuning->TierGapDamage.Curve.CurveTable = Table;
		Tuning->TierGapDamage.Curve.RowName = FName(TEXT("NoSuchRow"));
		TestEqual(TEXT("ladder scale starts at 1"), Tuning->TierGapDamage.Value, 1.0f);
		TestEqual(TEXT("missing row: the value passed in"), Tuning->GetLadder(ECombatLadder::TierGapDamage, 1, TierGapDamage::PENALTY_GAP_1),
				  TierGapDamage::PENALTY_GAP_1);
		TestTrue(TEXT("the missing row is reported"), Tuning->GetDifferencesFromCode().Contains(TEXT("NoSuchRow")));
	});

	It("keeps the C++ value for a ladder with no curve", [this]()
	{
		TestEqual(TEXT("no curve: the value passed in"), Tuning->GetLadder(ECombatLadder::TierGapDamage, 1, TierGapDamage::PENALTY_GAP_1),
				  TierGapDamage::PENALTY_GAP_1);
	});

	It("routes every getter to its own ladder and key while no tuning asset exists", [this]()
	{
		if (UCombatTuningData::GetActive())
		{
			AddInfo(TEXT("A tuning asset exists in this project; skipped"));
			return;
		}
		TestEqual(TEXT("S tier power"), TierPowerScaling::GetTierPowerMultiplier(EItemTier::S_Tier), TierPowerScaling::TIER_POWER[6]);
		TestEqual(TEXT("gap +2 damage"), TierGapDamage::GetTierGapDamageMultiplier(EItemTier::D_Tier, EItemTier::F_Tier),
				  TierGapDamage::CodeTierGapDamage(2));
		TestEqual(TEXT("gap -3 cost"), TierGapDamage::GetTierGapCostMultiplier(EItemTier::F_Tier, EItemTier::C_Tier),
				  TierGapDamage::CodeTierGapCost(-3));
		TestEqual(TEXT("requirement gap +5"), TierGapDamage::GetRequirementGapMultiplier(7), TierGapDamage::REQ_GAP_5);
		TestEqual(TEXT("grade B"), GetScalingTierCoefficient(EScalingTier::B), CodeScalingTierCoefficient(EScalingTier::B));
	});

	It("refuses a simple-curve table", [this]()
	{
		Table->AddSimpleCurve(FName(TEXT("Simple")));
		TestFalse(TEXT("refused"), Tuning->FillLadderRows(Table));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
