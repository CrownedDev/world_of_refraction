// SpellSlotListSpec.cpp
// B-351d part 1 (D-145, Q-351d): one spell-slot list per crystal. B-518 (D-224): nothing pre-fills it (the crystal's
// spells and the ring's presets don't play); the player fills any slot and can empty it; it is the list every
// GetAllSpells reader gets; the list goes with the crystal.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Loadout/WeaponSpellRules.h"
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
	/** B-098: these cases prove the old weapon-spell path, kept until the sweep (wor.Loadout.WeaponSpells 1). */
	TUniquePtr<WeaponSpellRules::FScopedWeaponSpells> OldWeaponSpells;
END_DEFINE_SPEC(FSpellSlotListSpec)

void FSpellSlotListSpec::Define()
{
	BeforeEach([this]() { OldWeaponSpells = MakeUnique<WeaponSpellRules::FScopedWeaponSpells>(true); });
	AfterEach([this]() { OldWeaponSpells.Reset(); });

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

	It("starts a weapon crystal's list empty, sized by the gem (B-518: its spells don't pre-fill it)", [this]()
	{
		const TArray<USpellData *> Slots = Weapon.GetSpellSlots();
		TestEqual(TEXT("the gem's slots"), Slots.Num(), Weapon.GetSpellSlotCap());
		TestTrue(TEXT("the first is empty"), Slots.IsValidIndex(SPELL_SLOT_TEST_FIRST) && Slots[SPELL_SLOT_TEST_FIRST] == nullptr);
		TestEqual(TEXT("nothing plays"), Weapon.GetAllSpells().Num(), 0);
	});

	It("fills a weapon crystal's slot, then empties it; GetAllSpells follows", [this]()
	{
		Weapon.AssignedSpells = {OldOverride}; // the old loadout-level override: never plays (B-518)
		TestFalse(TEXT("the old override doesn't play"), Weapon.GetAllSpells().Contains(OldOverride));
		TestTrue(TEXT("filled"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
		TestTrue(TEXT("the pick plays"), Weapon.GetAllSpells().Contains(Chosen));
		TestTrue(TEXT("emptied"), Weapon.RestoreSpell(SPELL_SLOT_TEST_FIRST));
		TestEqual(TEXT("nothing plays, no built-in back"), Weapon.GetAllSpells().Num(), 0);
		TestFalse(TEXT("nothing left to empty"), Weapon.RestoreSpell(SPELL_SLOT_TEST_FIRST));
		TestFalse(TEXT("past the cap: refused"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_PAST_CAP, Chosen));
	});

	It("an empty slot takes a spell too", [this]()
	{
		TestTrue(TEXT("second slot"), Weapon.ReplaceSpell(SPELL_SLOT_TEST_SECOND, Chosen));
		const TArray<USpellData *> Spells = Weapon.GetAllSpells();
		TestTrue(TEXT("only the pick"), Spells.Num() == 1 && Spells.Contains(Chosen));
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

	It("starts an adapter's list empty (not from the ring's presets); a locked ring is an ordinary one (B-518, Q7)", [this]()
	{
		const TArray<USpellData *> Slots = Adapter.GetSpellSlots();
		TestTrue(TEXT("the first is empty"), Slots.IsValidIndex(SPELL_SLOT_TEST_FIRST) && Slots[SPELL_SLOT_TEST_FIRST] == nullptr);
		TestTrue(TEXT("filled"), Adapter.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
		TestTrue(TEXT("the pick plays"), Adapter.GetAllSpells().Contains(Chosen));
		TestTrue(TEXT("emptied"), Adapter.RestoreSpell(SPELL_SLOT_TEST_FIRST));
		TestFalse(TEXT("no preset back"), Adapter.GetAllSpells().Contains(BuiltIn));
		Adapter.RingEntry.Ring->bSpellsLocked = true;
		TestTrue(TEXT("locked: still takes a pick"), Adapter.ReplaceSpell(SPELL_SLOT_TEST_FIRST, Chosen));
		TestTrue(TEXT("and plays it"), Adapter.GetAllSpells().Contains(Chosen));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
