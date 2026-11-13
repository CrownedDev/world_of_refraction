// SkillSlotListSpec.cpp
// B-351 (D-145, changes CL-20): one skill-slot list per weapon. Slots by tier (F 1, E 2, D 3, C 4, B 5, A 6, S 6);
// built-ins pre-fill it; the player replaces any slot and can restore the built-in. B-437 (D-175): a stone adds no
// slots; a weapon has its tier's slots plus its crystal's, 12 at most.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/SkillSlotList.h"
#include "Loadout/WeaponSpellRules.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	struct FSlotsByTier
	{
		EItemTier Tier;
		int32 Slots;
	};
	/** CL-20 / D-145: F 1 .. A 6, S 6. */
	const FSlotsByTier SLOT_TEST_TIERS[] = {
		{EItemTier::F_Tier, 1}, {EItemTier::E_Tier, 2}, {EItemTier::D_Tier, 3}, {EItemTier::C_Tier, 4},
		{EItemTier::B_Tier, 5}, {EItemTier::A_Tier, 6}, {EItemTier::S_Tier, 6}};
	/** D-175: no stone adds a slot, whatever its kind. */
	const ECrystalType SLOT_TEST_STONES[] = {ECrystalType::DamageStone, ECrystalType::StatStone, ECrystalType::MaxHPStone,
											 ECrystalType::DurabilityStone, ECrystalType::HealingStone};
	const FCrystalId SLOT_TEST_S_GEM(ECrystalType::Garnet, EItemTier::S_Tier);
	constexpr int32 SLOT_TEST_GEM_DURABILITY = 5;
	/** D-175: an S weapon's 6 slots plus an S gem's 6. */
	constexpr int32 SLOT_TEST_MAX_SLOTS = 12;
	constexpr int32 SLOT_TEST_F_SLOTS = 1;
	constexpr int32 SLOT_TEST_FIRST = 0;
	constexpr int32 SLOT_TEST_SECOND = 1;
}

BEGIN_DEFINE_SPEC(FSkillSlotListSpec, "WoR.Loadout.SkillSlotList", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FWeaponLoadoutEntry Entry;
	UAbilityData *BuiltIn = nullptr;
	UAbilityData *Chosen = nullptr;

	static FRuntimeAttachedItem StoneOf(ECrystalType Type)
	{
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		Stone.Crystal.Id = FCrystalId(Type, EItemTier::S_Tier);
		return Stone;
	}

	static FRuntimeAttachedItem SGem()
	{
		FRuntimeAttachedItem Gem;
		Gem.Kind = EAttachedItemKind::Crystal;
		Gem.Crystal = FCrystalAttachment(SLOT_TEST_S_GEM, SLOT_TEST_GEM_DURABILITY);
		return Gem;
	}
END_DEFINE_SPEC(FSkillSlotListSpec)

void FSkillSlotListSpec::Define()
{
	BeforeEach([this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		BuiltIn = NewObject<UAbilityData>(GetTransientPackage());
		Chosen = NewObject<UAbilityData>(GetTransientPackage());
		Weapon->PresetAbilities = {BuiltIn};
		Entry = FWeaponLoadoutEntry();
		Entry.WeaponEntry.Weapon = Weapon;
		Entry.WeaponEntry.Tier = EItemTier::F_Tier;
	});

	It("sizes the list by tier, F 1 to S 6", [this]()
	{
		for (const FSlotsByTier &Row : SLOT_TEST_TIERS)
		{
			Entry.WeaponEntry.Tier = Row.Tier;
			TestEqual(FString::Printf(TEXT("tier %d"), static_cast<int32>(Row.Tier)), Entry.GetSkillSlotCount(), Row.Slots);
		}
	});

	It("pre-fills the list with the built-ins", [this]()
	{
		const TArray<UAbilityData *> Slots = Entry.GetSkillSlots();
		TestEqual(TEXT("one slot at F"), Slots.Num(), 1);
		TestTrue(TEXT("the built-in in it"), Slots.Num() > 0 && Slots[SLOT_TEST_FIRST] == BuiltIn);
	});

	It("replaces a built-in, then restores it", [this]()
	{
		TestTrue(TEXT("replaced"), Entry.ReplaceSkill(SLOT_TEST_FIRST, Chosen));
		TestTrue(TEXT("the list is active"), Entry.WeaponEntry.bSkillSlotsActive);
		TestTrue(TEXT("the pick shows"), Entry.GetSkillSlots()[SLOT_TEST_FIRST] == Chosen);
		TestTrue(TEXT("the built-in is still on the weapon"), Entry.WeaponEntry.Weapon->PresetAbilities.Contains(BuiltIn));
		TestTrue(TEXT("restored"), Entry.RestoreSkill(SLOT_TEST_FIRST));
		TestTrue(TEXT("the built-in is back"), Entry.GetSkillSlots()[SLOT_TEST_FIRST] == BuiltIn);
		TestFalse(TEXT("nothing left to restore"), Entry.RestoreSkill(SLOT_TEST_FIRST));
		TestEqual(TEXT("offered: the built-in"), Entry.GetSkillSlotAbilities().Num(), 1);
	});

	It("counts the tier's slots plus the crystal's with any stone attached, 12 at most (B-437, D-175)", [this]()
	{
		WeaponSpellRules::FScopedWeaponSpells Rule(false); // the D-027 default, whatever the console says
		for (const ECrystalType Stone : SLOT_TEST_STONES)
		{
			Entry.WeaponEntry.Tier = EItemTier::F_Tier;
			Entry.WeaponEntry.AttachedItem = FRuntimeAttachedItem();
			Entry.WeaponEntry.StoneSocket = StoneOf(Stone);
			const FString Kind = FString::Printf(TEXT("stone kind %d"), static_cast<int32>(Stone));
			TestEqual(Kind + TEXT(": the stone adds no slot"), Entry.GetSkillSlotCount(), SLOT_TEST_F_SLOTS);
			TestFalse(Kind + TEXT(": no second slot to fill"), Entry.ReplaceSkill(SLOT_TEST_SECOND, Chosen));

			Entry.WeaponEntry.Tier = EItemTier::S_Tier;
			Entry.WeaponEntry.AttachedItem = SGem();
			TestEqual(Kind + TEXT(": an S weapon with an S gem"), Entry.GetSkillSlotCount(), SLOT_TEST_MAX_SLOTS);
		}
	});

	It("reaches the loadout's ability list, which combat and the AI read (B-351c)", [this]()
	{
		Entry.WeaponEntry.Tier = EItemTier::E_Tier; // two slots
		TestTrue(TEXT("the second slot takes a skill"), Entry.ReplaceSkill(SLOT_TEST_SECOND, Chosen));
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Entry;
		const TArray<UAbilityData *> Abilities = Loadout.GetAllAbilities();
		TestTrue(TEXT("the built-in"), Abilities.Contains(BuiltIn));
		TestTrue(TEXT("the pick in the second slot"), Abilities.Contains(Chosen));
	});
}

#endif
