// SpellSlotListSpec.cpp
// B-351d part 1 (D-145, Q-351d): one spell-slot list per adapter crystal. The ring's presets pre-fill it; the player
// replaces any slot and can restore the built-in; once edited it is the list every GetAllSpells reader gets.
// B-411 (D-027): weapons give no spells, so the weapon crystal's cases went with the weapon spell path.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	/** An E-tier gem: two spell slots (Q-147, CL-25). */
	const FCrystalId SPELL_SLOT_TEST_GEM(ECrystalType::Garnet, EItemTier::E_Tier);
	constexpr int32 SPELL_SLOT_TEST_FIRST = 0;
	/** Review: an intact gem (durability 0 reads as broken, which gives no spells). */
	constexpr int32 SPELL_SLOT_TEST_DURABILITY = 5;
}

BEGIN_DEFINE_SPEC(FSpellSlotListSpec, "WoR.Loadout.SpellSlotList", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FRingLoadoutEntry Adapter;
	USpellData *BuiltIn = nullptr;
	USpellData *Chosen = nullptr;

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
		Adapter = FRingLoadoutEntry();
		URingData *Ring = NewObject<URingData>(GetTransientPackage());
		Ring->PresetSpells = {BuiltIn};
		Adapter.RingEntry.Ring = Ring;
		Adapter.RingEntry.AttachedItem = Gem();
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
