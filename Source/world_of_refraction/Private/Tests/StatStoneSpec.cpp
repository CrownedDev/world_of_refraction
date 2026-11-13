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
	const EItemTier STAT_STONE_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::C_Tier, EItemTier::S_Tier};
	const ECrystalType STONE_TEST_KEPT[] = {ECrystalType::DamageStone, ECrystalType::HealingStone,
											ECrystalType::MaxHPStone, ECrystalType::MaxEPStone};
	constexpr int32 STONE_TEST_STACK = 2;
	/** D-142: Defense, Crit, Turn Speed, Status, Efficiency, Spell Damage, Resistance, Spell / Action Speed, Luck, Reflex. */
	constexpr int32 STONE_TEST_RETIRED_KINDS = 11;
	constexpr float STAT_STONE_TEST_TOLERANCE = 0.0001f;
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
			for (const EItemTier Tier : STAT_STONE_TEST_TIERS)
			{
				const FCrystalId Old(Row.Type, Tier);
				const FCrystalId New = StatStone::Normalize(Old);
				const FString What = FString::Printf(TEXT("%s (tier %d)"), Row.Label, static_cast<int32>(Tier));
				TestEqual(What + TEXT(": kind"), New.Type, ECrystalType::StatStone);
				TestEqual(What + TEXT(": sub-stat"), New.StoneStat, Row.Stat);
				TestEqual(What + TEXT(": tier"), New.Tier, Tier);
				TestNearlyEqual(What + TEXT(": attached bonus"),
								CrystalEffectTable::GetAttachedStonePercent(StoneOn(New), Row.Stat),
								CrystalEffectTable::GetAttachedStonePercent(StoneOn(Old), Row.Stat), STAT_STONE_TEST_TOLERANCE);
				TestEqual(What + TEXT(": name"), ItemIdentity::GetDisplayName(New), ItemIdentity::GetDisplayName(Old));
			}
		}
		TestTrue(TEXT("the eleven single-stat kinds are retired"), Retired == STONE_TEST_RETIRED_KINDS);
	});

	It("clears a stray sub-stat on a non-StatStone and leaves an inert StatStone without a row (review)", [this]()
	{
		const FCrystalId Stray(ECrystalType::Garnet, EItemTier::C_Tier, ESubStat::Defense);
		TestTrue(TEXT("a gem never carries a sub-stat"), StatStone::Normalize(Stray) == FCrystalId(ECrystalType::Garnet, EItemTier::C_Tier));
		TestEqual(TEXT("a StatStone with no sub-stat reads no row"), StatStone::RowType(FCrystalId(ECrystalType::StatStone, EItemTier::F_Tier)), ECrystalType::StatStone);
		TestEqual(TEXT("a stat stone grants a Broken Darkness character no energy, like the kinds it replaces"),
				  CrystalEffectTable::GetBrokenDarknessEnergyPercent(StatStone::Make(ESubStat::Defense, EItemTier::S_Tier)), 0.0f, STAT_STONE_TEST_TOLERANCE);
	});

	It("grants a Broken Darkness character energy for a gem only, never for any stone (B-353b)", [this]()
	{
		const UEnum *Kinds = StaticEnum<ECrystalType>();
		if (!TestNotNull(TEXT("ECrystalType"), Kinds)) return;
		for (int32 Index = 0; Index < Kinds->NumEnums() - 1; ++Index)
		{
			const ECrystalType Type = static_cast<ECrystalType>(Kinds->GetValueByIndex(Index));
			const float Percent = CrystalEffectTable::GetBrokenDarknessEnergyPercent(FCrystalId(Type, EItemTier::S_Tier));
			if (Type == ECrystalType::None || CrystalTypeHelpers::IsAugmentStoneType(Type))
			{
				TestEqual(Kinds->GetNameStringByIndex(Index) + TEXT(": no energy"), Percent, 0.0f, STAT_STONE_TEST_TOLERANCE);
			}
			else
			{
				TestTrue(Kinds->GetNameStringByIndex(Index) + TEXT(": a gem grants energy"), Percent > 0.0f);
			}
		}
	});

	It("keeps the Damage, Healing and pool stones as they are", [this]()
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
