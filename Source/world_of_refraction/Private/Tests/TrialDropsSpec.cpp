// TrialDropsSpec.cpp
// B-569 (D-225, TrialPoolDesign stage 5): a won fight's drop, picked from a made-up pool with a fixed seed, headless.
// Unproven here: the real pool, the grant into a fighter's inventory and the results line (PIE sheet B-569).

#if WITH_DEV_AUTOMATION_TESTS

#include "Equipment/Weapons/WeaponData.h"
#include "Misc/AutomationTest.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Trial/EnemyKitRoll.h"
#include "Trial/TrialDrops.h"
#include "Trial/TrialPoolLook.h"
#include "UObject/Package.h"

BEGIN_DEFINE_SPEC(FTrialDropsSpec, "WoR.Trial.Drops", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static constexpr int32 SEED = 569;
	static constexpr int32 DRAWS = 4000;
	static constexpr float SHARE_SLACK = 0.04f;
END_DEFINE_SPEC(FTrialDropsSpec)

void FTrialDropsSpec::Define()
{
	It("weighs what the enemies used 3 to 1, and drops a worn piece at the tier it was worn", [this]()
	{
		UWeaponData *Used = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Other = NewObject<UWeaponData>(GetTransientPackage());
		const TArray<UObject *> Pool = {Used, Other};
		const TArray<FTrialDropUse> Uses = {{Used, EItemTier::D_Tier}};
		TestEqual(TEXT("used: 3"), TrialDrops::GetWeight(Used, Uses), TrialDrops::USED_WEIGHT);
		TestEqual(TEXT("the rest: 1"), TrialDrops::GetWeight(Other, Uses), TrialDrops::UNUSED_WEIGHT);
		const FTrialTierRange NoTiers{EItemTier::C_Tier, EItemTier::F_Tier}; // empty: no crystal entry, so only the two
		FRandomStream Roll(SEED);
		int32 UsedCount = 0;
		bool bWornTier = true;
		for (int32 Draw = 0; Draw < DRAWS; ++Draw)
		{
			const FTrialDrop Drop = TrialDrops::Pick(Pool, Uses, ESpellElement::Fire, NoTiers, Roll);
			if (Drop.Item == Used)
			{
				++UsedCount;
				bWornTier &= Drop.Tier == EItemTier::D_Tier && Drop.bUsed;
			}
		}
		const float Share = static_cast<float>(UsedCount) / DRAWS;
		const float Expected = TrialDrops::USED_WEIGHT / (TrialDrops::USED_WEIGHT + TrialDrops::UNUSED_WEIGHT);
		TestTrue(FString::Printf(TEXT("the used piece about 3 in 4 (%.3f)"), Share), FMath::Abs(Share - Expected) < SHARE_SLACK);
		TestTrue(TEXT("always at the tier it was worn"), bWornTier);
	});

	It("draws a crystal from the zone's tiers, a skill at its own tier, and reads the dropped line", [this]()
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Tier = EItemTier::E_Tier;
		const FTrialTierRange Range{EItemTier::F_Tier, EItemTier::D_Tier};
		const TArray<UObject *> Pool = {Spell};
		const TArray<UObject *> NoPool;
		const TArray<FTrialDropUse> NoUses;
		FRandomStream Roll(SEED);
		bool bCrystal = false;
		bool bSpell = false;
		for (int32 Draw = 0; Draw < DRAWS && !(bCrystal && bSpell); ++Draw)
		{
			const FTrialDrop Drop = TrialDrops::Pick(Pool, NoUses, ESpellElement::Fire, Range, Roll);
			if (!Drop.Item)
			{
				bCrystal |= Drop.Crystal.Type != ECrystalType::None && Range.Contains(Drop.Tier);
			}
			else
			{
				bSpell |= Drop.Item == Spell && Drop.Tier == EItemTier::E_Tier;
			}
		}
		TestTrue(TEXT("a crystal in the zone's tiers"), bCrystal);
		TestTrue(TEXT("the spell at its own tier"), bSpell);
		FTrialDrop Piece;
		Piece.Item = NewObject<UWeaponData>(GetTransientPackage());
		Piece.Tier = EItemTier::D_Tier;
		TestTrue(TEXT("a piece arrives empty"), WoRTrialPoolLook::GetDroppedLine(TrialDrops::MakeView(Piece)).ToString().EndsWith(TEXT("empty")));
		TestFalse(TEXT("nothing to give: unset"), TrialDrops::Pick(NoPool, NoUses, ESpellElement::Fire, FTrialTierRange{EItemTier::C_Tier, EItemTier::F_Tier}, Roll).IsSet());
	});
}

#endif
