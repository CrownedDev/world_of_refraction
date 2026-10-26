// TierGapScalingSpec.cpp
// Pure ladder functions in TierGapConstants.h: tier-gap damage + cost multipliers,
// per-pillar requirement-gap multiplier.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Damage/TierGapConstants.h"

namespace
{
	constexpr int32 REQ_GAP_LADDER_END = 5;
	/** World levels run 0-7, so a raw requirement gap can reach +/-7, past the ladder's end. */
	constexpr int32 REQ_GAP_BEYOND_LADDER = 7;
}

BEGIN_DEFINE_SPEC(FTierGapScalingSpec, "WoR.Scaling",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTierGapScalingSpec)

void FTierGapScalingSpec::Define()
{
	using namespace TierGapDamage;

	Describe("Tier-gap damage", [this]()
	{
		It("is neutral at matched tier", [this]()
		{
			TestEqual(TEXT("B through B"), GetTierGapDamageMultiplier(EItemTier::B_Tier, EItemTier::B_Tier), MATCHED_TIER);
		});

		It("boosts an action used through a higher-tier channel", [this]()
		{
			TestEqual(TEXT("C through B"), GetTierGapDamageMultiplier(EItemTier::C_Tier, EItemTier::B_Tier), BOOST_GAP_NEG1);
			TestEqual(TEXT("F through S"), GetTierGapDamageMultiplier(EItemTier::F_Tier, EItemTier::S_Tier), BOOST_GAP_NEG4_PLUS);
		});

		It("penalises an action that out-tiers its channel", [this]()
		{
			TestEqual(TEXT("A through C"), GetTierGapDamageMultiplier(EItemTier::A_Tier, EItemTier::C_Tier), PENALTY_GAP_2);
			TestEqual(TEXT("S through F"), GetTierGapDamageMultiplier(EItemTier::S_Tier, EItemTier::F_Tier), PENALTY_GAP_4_PLUS);
		});
	});

	Describe("Tier-gap cost", [this]()
	{
		It("is neutral at matched tier", [this]()
		{
			TestEqual(TEXT("B through B"), GetTierGapCostMultiplier(EItemTier::B_Tier, EItemTier::B_Tier), MATCHED_COST);
		});

		It("runs opposite to damage: cheaper through a stronger channel, dearer through a weaker one", [this]()
		{
			TestEqual(TEXT("C through B"), GetTierGapCostMultiplier(EItemTier::C_Tier, EItemTier::B_Tier), COST_GAP_NEG1);
			TestEqual(TEXT("B through C"), GetTierGapCostMultiplier(EItemTier::B_Tier, EItemTier::C_Tier), COST_GAP_1);
			TestEqual(TEXT("F through S"), GetTierGapCostMultiplier(EItemTier::F_Tier, EItemTier::S_Tier), COST_GAP_NEG4_PLUS);
			TestEqual(TEXT("S through F"), GetTierGapCostMultiplier(EItemTier::S_Tier, EItemTier::F_Tier), COST_GAP_4_PLUS);
		});
	});

	Describe("Requirement gap", [this]()
	{
		It("is neutral when the character meets the requirement exactly", [this]()
		{
			TestEqual(TEXT("Gap 0"), GetRequirementGapMultiplier(0), MATCHED_TIER);
		});

		It("clamps past the +/-5 ladder ends", [this]()
		{
			TestEqual(TEXT("Gap -5"), GetRequirementGapMultiplier(-REQ_GAP_LADDER_END), REQ_GAP_NEG5);
			TestEqual(TEXT("Gap -7"), GetRequirementGapMultiplier(-REQ_GAP_BEYOND_LADDER), REQ_GAP_NEG5);
			TestEqual(TEXT("Gap +5"), GetRequirementGapMultiplier(REQ_GAP_LADDER_END), REQ_GAP_5);
			TestEqual(TEXT("Gap +7"), GetRequirementGapMultiplier(REQ_GAP_BEYOND_LADDER), REQ_GAP_5);
		});

		It("falls strictly from over-statted to under-statted", [this]()
		{
			for (int32 Gap = -REQ_GAP_LADDER_END; Gap < REQ_GAP_LADDER_END; ++Gap)
			{
				TestTrue(FString::Printf(TEXT("Gap %d beats gap %d"), Gap, Gap + 1),
						 GetRequirementGapMultiplier(Gap) > GetRequirementGapMultiplier(Gap + 1));
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
