// StatStoneSpec.cpp
// B-353 (D-142): one StatStone kind with a sub-stat field. Every old single-stat stone loads as a StatStone with its
// sub-stat, and its bonus (attached percent, name) is unchanged. Damage, Ability, Healing and the pool stones stay.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/StatStone.h"
#include "Equipment/FRuntimeAttachedItem.h"

namespace
{
	const EItemTier STONE_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::C_Tier, EItemTier::S_Tier};
	const ECrystalType STONE_TEST_KEPT[] = {ECrystalType::DamageStone, ECrystalType::AbilityStone, ECrystalType::HealingStone,
											ECrystalType::MaxHPStone, ECrystalType::MaxEPStone};
	constexpr int32 STONE_TEST_STACK = 2;
	/** D-142: Defense, Crit, Turn Speed, Status, Efficiency, Spell Damage, Resistance, Spell / Action Speed, Luck, Reflex. */
	constexpr int32 STONE_TEST_RETIRED_KINDS = 11;
	constexpr float STONE_TEST_TOLERANCE = 0.0001f;
}

BEGIN_DEFINE_SPEC(FStatStoneSpec, "WoR.Crystals.StatStone", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static FRuntimeAttachedItem StoneOn(const FCrystalId &Id)
	{
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		Stone.Crystal.Id = Id;
		return Stone;
	}
END_DEFINE_SPEC(FStatStoneSpec)

void FStatStoneSpec::Define()
{
	It("loads every old single-stat stone as a StatStone with its sub-stat, bonus unchanged", [this]()
	{
		int32 Retired = 0;
		for (const FStoneRow &Row : StoneRowTable::ROWS)
		{
			if (!StatStone::IsRetiredKind(Row.Type))
			{
				continue;
			}
			++Retired;
			for (const EItemTier Tier : STONE_TEST_TIERS)
			{
				const FCrystalId Old(Row.Type, Tier);
				const FCrystalId New = StatStone::Normalize(Old);
				const FString What = FString::Printf(TEXT("%s (tier %d)"), Row.Label, static_cast<int32>(Tier));
				TestEqual(What + TEXT(": kind"), New.Type, ECrystalType::StatStone);
				TestEqual(What + TEXT(": sub-stat"), New.StoneStat, Row.Stat);
				TestEqual(What + TEXT(": tier"), New.Tier, Tier);
				TestNearlyEqual(What + TEXT(": attached bonus"),
								CrystalEffectTable::GetAttachedStonePercent(StoneOn(New), Row.Stat),
								CrystalEffectTable::GetAttachedStonePercent(StoneOn(Old), Row.Stat), STONE_TEST_TOLERANCE);
				TestEqual(What + TEXT(": name"), ItemIdentity::GetDisplayName(New), ItemIdentity::GetDisplayName(Old));
			}
		}
		TestTrue(TEXT("the eleven single-stat kinds are retired"), Retired == STONE_TEST_RETIRED_KINDS);
	});

	It("keeps the Damage, Ability, Healing and pool stones as they are", [this]()
	{
		for (const ECrystalType Kept : STONE_TEST_KEPT)
		{
			const FCrystalId Id(Kept, EItemTier::F_Tier);
			TestTrue(FString::Printf(TEXT("kind %d unchanged"), static_cast<int32>(Kept)), StatStone::Normalize(Id) == Id);
		}
	});

	It("stacks an old stone as its StatStone in a character's crystals", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("Test world created"), TestWorld.Create())) return;
		AActor *Owner = TestWorld.SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Owner"), Owner)) return;
		UCrystalInventoryComponent *Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		const FCrystalId Old(ECrystalType::LuckStone, EItemTier::F_Tier);
		TestTrue(TEXT("added"), Crystals->AddCount(Old, STONE_TEST_STACK));
		TestEqual(TEXT("held as a Luck StatStone"), Crystals->GetCount(StatStone::Make(ESubStat::Luck, EItemTier::F_Tier)), STONE_TEST_STACK);
		TestEqual(TEXT("and still found by the old id"), Crystals->GetCount(Old), STONE_TEST_STACK);
	});
}

#endif
