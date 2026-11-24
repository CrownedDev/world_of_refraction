// InventoryWritesSlotsSpec.cpp
// B-309d3: the inventory's and the loadout component's writers change FCombatLoadout::Slots by place and the old fields
// are rebuilt from them: UInventoryComponent::StripFromLoadout (an off hand alone, a piece's slot, the adapter loadout
// closing up as before), and FCombatLoadout::EditGearEntries (the walker behind the gear sync and the invalid-slot
// clears), which edits a slot's entry in its place and an old entry no slot mirrors (a Caster's adapters) where it is.
// Unproven here: RefillActivePrimary and the run preset need a spawned member (WoR.Inventory.AlwaysPrimary, WoR.Pool).

#if WITH_DEV_AUTOMATION_TESTS

#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/WornGear.h"
#include "Misc/AutomationTest.h"

namespace
{
	FWeaponInventoryEntry WritesOwnedWeapon()
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	FRingInventoryEntry WritesOwnedAdapter()
	{
		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(NewObject<URingData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	/** The slots hold exactly what a fresh mirror of the old fields holds, place by place. */
	bool WritesInStep(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FCombatLoadout Fresh = Loadout;
		Fresh.RebuildSlotsFromLegacy(Class);
		if (Fresh.Slots.Num() != Loadout.Slots.Num() || Fresh.ActiveWeaponSlot != Loadout.ActiveWeaponSlot || Fresh.ActiveAdapterSlot != Loadout.ActiveAdapterSlot)
		{
			return false;
		}
		for (int32 Index = 0; Index < Fresh.Slots.Num(); ++Index)
		{
			const FLoadoutSlot &A = Fresh.Slots[Index];
			const FLoadoutSlot &B = Loadout.Slots[Index];
			if (A.Place != B.Place || A.GearID != B.GearID || A.OffHandID != B.OffHandID || A.LegacyOrigin != B.LegacyOrigin || A.LegacyIndex != B.LegacyIndex)
			{
				return false;
			}
		}
		return true;
	}
}

BEGIN_DEFINE_SPEC(FInventoryWritesSlotsSpec, "WoR.Loadout.InventoryWritesSlots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventoryWritesSlotsSpec)

void FInventoryWritesSlotsSpec::Define()
{
	using namespace WornGearLayout;

	It("strips a Resonator's middle adapter through the slots: the list closes up and the same ring stays active", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Resonator;
		const FRingInventoryEntry First = WritesOwnedAdapter();
		const FRingInventoryEntry Middle = WritesOwnedAdapter();
		const FRingInventoryEntry Last = WritesOwnedAdapter();
		FCombatLoadout Loadout;
		for (const FRingInventoryEntry *Adapter : {&First, &Middle, &Last})
		{
			Loadout.RingLoadout.AddDefaulted_GetRef().RingEntry = *Adapter;
		}
		Loadout.ActiveRingIndex = 2;
		Loadout.RebuildSlotsFromLegacy(Class);

		TestTrue(TEXT("stripped"), UInventoryComponent::StripFromLoadout(Loadout, Middle.PersistentID, Class));
		TestTrue(TEXT("the list closes up"), Loadout.RingLoadout.Num() == 2 && Loadout.RingLoadout[0].RingEntry.PersistentID == First.PersistentID &&
												 Loadout.RingLoadout[1].RingEntry.PersistentID == Last.PersistentID);
		TestEqual(TEXT("the same ring stays active"), Loadout.ActiveRingIndex, 1);
		const FLoadoutSlot *Moved = Loadout.FindSlotAt(FIRST_ADAPTER_INDEX + 1);
		TestTrue(TEXT("the last adapter's slot moved up a place"), Moved && Moved->GearID == Last.PersistentID && !Loadout.FindSlotAt(FIRST_ADAPTER_INDEX + 2));
		TestTrue(TEXT("in step"), WritesInStep(Loadout, Class));
		TestFalse(TEXT("a piece not there strips nothing"), UInventoryComponent::StripFromLoadout(Loadout, Middle.PersistentID, Class));
	});

	It("strips a Generic's off hand alone, then its main hand with the Weapon type, through the slots", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Generic;
		const FWeaponInventoryEntry MainHand = WritesOwnedWeapon();
		const FWeaponInventoryEntry OffHand = WritesOwnedWeapon();
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = MainHand;
		Loadout.PrimaryWeapon.OffHandEntry = OffHand;
		Loadout.RebuildSlotsFromLegacy(Class);

		TestTrue(TEXT("the off hand stripped"), UInventoryComponent::StripFromLoadout(Loadout, OffHand.PersistentID, Class));
		const FLoadoutSlot *Primary = Loadout.FindSlotAt(PRIMARY_INDEX);
		TestTrue(TEXT("the main hand stays, alone"), Primary && Primary->GearID == MainHand.PersistentID && !Primary->OffHandID.IsValid() &&
														 Loadout.PrimaryWeapon.WeaponEntry.PersistentID == MainHand.PersistentID &&
														 !Loadout.PrimaryWeapon.OffHandEntry.PersistentID.IsValid());
		TestTrue(TEXT("in step after the off hand"), WritesInStep(Loadout, Class));

		TestTrue(TEXT("the main hand stripped"), UInventoryComponent::StripFromLoadout(Loadout, MainHand.PersistentID, Class));
		TestTrue(TEXT("no primary, type None"), !Loadout.FindSlotAt(PRIMARY_INDEX) && !Loadout.PrimaryWeapon.IsValid() &&
												   Loadout.PrimarySlotType == EPrimarySlotType::None);
		TestTrue(TEXT("in step after the main hand"), WritesInStep(Loadout, Class));
	});

	It("edits each gear entry in its place: a slot's in the slot, a Caster's adapter (no slot) in its old entry", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Caster;
		const FWeaponInventoryEntry Weapon = WritesOwnedWeapon();
		const FRingInventoryEntry Adapter = WritesOwnedAdapter();
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = Weapon;
		Loadout.RingLoadout.AddDefaulted_GetRef().RingEntry = Adapter;
		Loadout.RebuildSlotsFromLegacy(Class);
		TestEqual(TEXT("only the weapon is a slot"), Loadout.Slots.Num(), 1);

		const FGuid NewWeaponID = FGuid::NewGuid();
		const FGuid NewAdapterID = FGuid::NewGuid();
		int32 WeaponPlace = INDEX_NONE;
		int32 AdapterPlace = INDEX_NONE;
		Loadout.EditGearEntries(Class, [&](int32 Place, FWeaponLoadoutEntry *Entry, FRingLoadoutEntry *Ring)
		{
			if (Entry && Entry->WeaponEntry.PersistentID == Weapon.PersistentID)
			{
				WeaponPlace = Place;
				Entry->WeaponEntry.PersistentID = NewWeaponID;
			}
			if (Ring && Ring->RingEntry.PersistentID == Adapter.PersistentID)
			{
				AdapterPlace = Place;
				Ring->RingEntry.PersistentID = NewAdapterID;
			}
		});
		TestEqual(TEXT("the weapon visited in the primary place"), WeaponPlace, PRIMARY_INDEX);
		TestEqual(TEXT("the adapter visited in its place"), AdapterPlace, FIRST_ADAPTER_INDEX);
		const FLoadoutSlot *Primary = Loadout.FindSlotAt(PRIMARY_INDEX);
		TestTrue(TEXT("the weapon's edit in the slot and the old entry"), Primary && Primary->GearID == NewWeaponID &&
																			  Loadout.PrimaryWeapon.WeaponEntry.PersistentID == NewWeaponID);
		TestTrue(TEXT("the adapter's edit kept in its old entry"), Loadout.RingLoadout.Num() == 1 && Loadout.RingLoadout[0].RingEntry.PersistentID == NewAdapterID);
		TestTrue(TEXT("in step"), WritesInStep(Loadout, Class));

		TestTrue(TEXT("the Caster's adapter stripped"), UInventoryComponent::StripFromLoadout(Loadout, NewAdapterID, Class));
		TestTrue(TEXT("its adapter loadout is empty, the weapon stays"), Loadout.RingLoadout.IsEmpty() && Loadout.PrimaryWeapon.WeaponEntry.PersistentID == NewWeaponID);
	});

	It("strips a Caster's adapter the same through the Generic default class (the callers with no class)", [this]()
	{
		const FWeaponInventoryEntry Weapon = WritesOwnedWeapon();
		const FRingInventoryEntry First = WritesOwnedAdapter();
		const FRingInventoryEntry Second = WritesOwnedAdapter();
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = Weapon;
		Loadout.RingLoadout.AddDefaulted_GetRef().RingEntry = First;
		Loadout.RingLoadout.AddDefaulted_GetRef().RingEntry = Second;
		Loadout.RebuildSlotsFromLegacy(ECharacterClass::Caster);
		FCombatLoadout AsCaster = Loadout;

		TestTrue(TEXT("stripped by default"), UInventoryComponent::StripFromLoadout(Loadout, First.PersistentID));
		TestTrue(TEXT("stripped as a Caster"), UInventoryComponent::StripFromLoadout(AsCaster, First.PersistentID, ECharacterClass::Caster));
		for (const FCombatLoadout *Result : {&Loadout, &AsCaster})
		{
			TestTrue(TEXT("the second adapter moved up, the weapon stays"),
					 Result->RingLoadout.Num() == 1 && Result->RingLoadout[0].RingEntry.PersistentID == Second.PersistentID &&
						 Result->PrimaryWeapon.WeaponEntry.PersistentID == Weapon.PersistentID && Result->PrimarySlotType == EPrimarySlotType::Weapon);
		}
	});
}

#endif
