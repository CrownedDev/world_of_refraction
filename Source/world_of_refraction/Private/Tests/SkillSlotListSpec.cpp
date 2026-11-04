// SkillSlotListSpec.cpp
// B-351 (D-145, changes CL-20): one skill-slot list per weapon. Slots by tier (F 1, E 2, D 3, C 4, B 5, A 6, S 6);
// built-ins pre-fill it; the player replaces any slot and can restore the built-in; an Ability Stone adds slots.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/SkillSlotList.h"
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
	const FCrystalId SLOT_TEST_ABILITY_STONE(ECrystalType::AbilityStone, EItemTier::F_Tier);
	constexpr int32 SLOT_TEST_FIRST = 0;
	constexpr int32 SLOT_TEST_SECOND = 1;
}

BEGIN_DEFINE_SPEC(FSkillSlotListSpec, "WoR.Loadout.SkillSlotList", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FWeaponLoadoutEntry Entry;
	UAbilityData *BuiltIn = nullptr;
	UAbilityData *Chosen = nullptr;

	static FRuntimeAttachedItem AbilityStone()
	{
		FRuntimeAttachedItem Stone;
		Stone.Kind = EAttachedItemKind::AugmentStone;
		Stone.Crystal.Id = SLOT_TEST_ABILITY_STONE;
		return Stone;
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
			TestEqual(FString::Printf(TEXT("tier %d"), static_cast<int32>(Row.Tier)), SkillSlotList::SlotCount(Row.Tier, FRuntimeAttachedItem()), Row.Slots);
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

	It("adds slots for an Ability Stone in the stone socket", [this]()
	{
		TestFalse(TEXT("no second slot without the stone"), Entry.ReplaceSkill(SLOT_TEST_SECOND, Chosen));
		Entry.WeaponEntry.StoneSocket = AbilityStone();
		const int32 StoneSlots = CrystalEffectTable::GetAttachmentSlotsForTier(SLOT_TEST_ABILITY_STONE);
		TestTrue(TEXT("the stone grants slots"), StoneSlots > 0);
		TestEqual(TEXT("tier slots plus the stone's"), Entry.GetSkillSlotCount(), 1 + StoneSlots);
		TestTrue(TEXT("the stone's slot takes a skill"), Entry.ReplaceSkill(SLOT_TEST_SECOND, Chosen));
		const TArray<UAbilityData *> Offered = Entry.GetSkillSlotAbilities();
		TestTrue(TEXT("both offered in one list"), Offered.Contains(BuiltIn) && Offered.Contains(Chosen));
	});
}

#endif
