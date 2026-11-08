// SpellSlotListSpec.cpp
// B-351d part 1 (D-145, Q-351d): one spell-slot list per crystal. The crystal's spells (a weapon) or the ring's presets
// (an adapter) pre-fill it; the player replaces any slot and can restore the built-in; once edited it is the list
// every GetAllSpells reader gets; until then the old position merge plays; the list goes with the crystal.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	/** An E-tier gem: two spell slots (Q-147, CL-25). */
	const FCrystalId SPELL_SLOT_TEST_GEM(ECrystalType::Garnet, EItemTier::E_Tier);
	constexpr int32 SPELL_SLOT_TEST_FIRST = 0;
	constexpr int32 SPELL_SLOT_TEST_SECOND = 1;
	constexpr int32 SPELL_SLOT_TEST_PAST_CAP = 5;
	/** Review: an intact gem (durability 0 reads as broken, which gives no spells). */
	constexpr int32 SPELL_SLOT_TEST_DURABILITY = 5;
}

BEGIN_DEFINE_SPEC(FSpellSlotListSpec, "WoR.Loadout.SpellSlotList", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FWeaponLoadoutEntry Weapon;
	FRingLoadoutEntry Adapter;
	USpellData *BuiltIn = nullptr;
	USpellData *Chosen = nullptr;
	USpellData *OldOverride = nullptr;

	static FRuntimeAttachedItem Gem()
	{
		FRuntimeAttachedItem Socket;
		Socket.Kind = EAttachedItemKind::Crystal;
		Socket.Crystal = FCrystalAttachment(SPELL_SLOT_TEST_GEM, SPELL_SLOT_TEST_DURABILITY);
		return Socket;
	}
END_DEFINE_SPEC(FSpellSlotListSpec)

void FSpellSlotListSpec::Define()
{
	BeforeEach([this]()
	{
		BuiltIn = NewObject<USpellData>(GetTransientPackage());
		Chosen = NewObject<USpellData>(GetTransientPackage());
		OldOverride = NewObject<USpellData>(GetTransientPackage());
		Weapon = FWeaponLoadoutEntry();
		Weapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon.WeaponEntry.AttachedItem = Gem();
		Weapon.WeaponEntry.AssignedSpells = {BuiltIn};
		Adapter = FRingLoadoutEntry();
		URingData *Ring = NewObject<URingData>(GetTransientPackage());
		Ring->PresetSpells = {BuiltIn};
		Adapter.RingEntry.Ring = Ring;
		Adapter.RingEntry.AttachedItem = Gem();
	});

	It("pre-fills a weapon crystal's list with its spells, sized by the gem", [this]()
	{
		const TArray<USpellData *> Slots = Weapon.GetSpellSlots();
		TestEqual(TEXT("the gem's slots"), Slots.Num(), Weapon.GetSpellSlotCap());
		TestTrue(TEXT("the built-in first"), Slots.IsValidIndex(SPELL_SLOT_TEST_FIRST) && Slots[SPELL_SLOT_TEST_FIRST] == BuiltIn);
	});

	It("replaces a weapon crystal's built-in, then restores it; GetAllSpells follows", [this]()
	{
		Weapon.AssignedSpells = {OldOverride}; // the old loadout-level override: plays until the list is edited
		TestTrue(TEXT("old merge before an edit"), Weapon.GetAllSpells().Contains(OldOverride));
		TestTrue(TEXT("replaced"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
		TestTrue(TEXT("active"), Weapon.WeaponEntry.bSpellSlotsActive);
		TestTrue(TEXT("the pick plays"), Weapon.GetAllSpells().Contains(Chosen));
		TestFalse(TEXT("the old override doesn't"), Weapon.GetAllSpells().Contains(OldOverride));
		TestTrue(TEXT("restored"), Weapon.RestoreSpell(SPELL_SLOT_TEST_FIRST));
		TestTrue(TEXT("the built-in is back"), Weapon.GetAllSpells().Contains(BuiltIn));
		TestFalse(TEXT("nothing left to restore"), Weapon.RestoreSpell(SPELL_SLOT_TEST_FIRST));
		TestFalse(TEXT("past the cap: refused"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_PAST_CAP, Chosen));
		TestTrue(TEXT("debug says active"), Weapon.GetSpellSlotString().Contains(TEXT("active")));
	});

	It("an empty slot takes a spell too", [this]()
	{
		TestTrue(TEXT("second slot"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_SECOND, Chosen));
		const TArray<USpellData *> Spells = Weapon.GetAllSpells();
		TestTrue(TEXT("both"), Spells.Contains(BuiltIn) && Spells.Contains(Chosen));
	});

	It("without a crystal there is no list, and removing it clears the picks", [this]()
	{
		TestTrue(TEXT("picked"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
		Weapon.WeaponEntry.RemoveCrystal();
		TestEqual(TEXT("no slots"), Weapon.GetSpellSlots().Num(), 0);
		TestEqual(TEXT("picks gone"), Weapon.WeaponEntry.SpellPicks.Num(), 0);
		TestFalse(TEXT("inactive"), Weapon.WeaponEntry.bSpellSlotsActive);
		TestFalse(TEXT("nothing to replace"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
	});

	It("an adapter's list starts from the ring's presets; a locked ring can't be edited", [this]()
	{
		const TArray<USpellData *> Slots = Adapter.GetSpellSlots();
		TestTrue(TEXT("the preset first"), Slots.IsValidIndex(SPELL_SLOT_TEST_FIRST) && Slots[SPELL_SLOT_TEST_FIRST] == BuiltIn);
		TestTrue(TEXT("replaced"), Adapter.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
		TestTrue(TEXT("the pick plays"), Adapter.GetAllSpells().Contains(Chosen));
		TestTrue(TEXT("restored"), Adapter.RestoreSpell(SPELL_SLOT_TEST_FIRST));
		TestTrue(TEXT("the preset is back"), Adapter.GetAllSpells().Contains(BuiltIn));
		Adapter.RingEntry.Ring->bSpellsLocked = true;
		TestFalse(TEXT("locked: refused"), Adapter.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
