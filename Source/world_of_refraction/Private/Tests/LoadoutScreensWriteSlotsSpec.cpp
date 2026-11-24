// LoadoutScreensWriteSlotsSpec.cpp
// B-309d2: the loadout screens put, swap and clear pieces in FCombatLoadout::Slots by place (EditSlots) and the old
// fields are rebuilt from them. After each screen action the slots and the old fields agree (what
// wor.Loadout.CompareSlots checks), a piece keeps its picks, and an adapter added after an empty place keeps the gap.
// Unproven here: the socket and skill edits through WoRLoadoutScreen::FindRowGear (B-309d2b) and the inventory writers (B-309d3).

#if WITH_DEV_AUTOMATION_TESTS

#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/WornGear.h"
#include "Misc/AutomationTest.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2Model.h"

namespace
{
	FWeaponInventoryEntry OwnedWeapon()
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(NewObject<UWeaponData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	FRingInventoryEntry OwnedAdapter()
	{
		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(NewObject<URingData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	/** The slots hold exactly what a fresh mirror of the old fields holds, place by place. */
	bool InStep(const FCombatLoadout &Loadout, ECharacterClass Class)
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

	FLoadoutV2Draft WeaponDraft(const FWeaponInventoryEntry &MainHand)
	{
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Weapon;
		Draft.MainHand = MainHand;
		return Draft;
	}

	FLoadoutV2Draft AdapterDraft(const FRingInventoryEntry &Adapter)
	{
		FLoadoutV2Draft Draft;
		Draft.Kind = ELoadoutV2DraftKind::Adapter;
		Draft.Adapter = Adapter;
		return Draft;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutScreensWriteSlotsSpec, "WoR.Loadout.ScreensWriteSlots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutScreensWriteSlotsSpec)

void FLoadoutScreensWriteSlotsSpec::Define()
{
	using namespace WornGearLayout;

	It("puts a slot in its place in place order, replaces what is there and takes it out", [this]()
	{
		FCombatLoadout Loadout;
		FLoadoutSlot Adapter = FLoadoutSlot::FromRingLoadoutEntry(FRingLoadoutEntry());
		Loadout.PutSlotAt(FIRST_ADAPTER_INDEX + 1, Adapter);
		Loadout.PutSlotAt(PRIMARY_INDEX, Adapter);
		Loadout.PutSlotAt(SECONDARY_INDEX, Adapter);
		TestTrue(TEXT("kept in place order"), Loadout.Slots.Num() == 3 && Loadout.Slots[0].Place == PRIMARY_INDEX && Loadout.Slots[1].Place == SECONDARY_INDEX &&
												  Loadout.Slots[2].Place == FIRST_ADAPTER_INDEX + 1);
		Loadout.PutSlotAt(SECONDARY_INDEX, Adapter);
		TestEqual(TEXT("a second put replaces"), Loadout.Slots.Num(), 3);
		TestTrue(TEXT("taken out"), Loadout.ClearSlotAt(SECONDARY_INDEX) && !Loadout.FindSlotAt(SECONDARY_INDEX));
		TestFalse(TEXT("an empty place has nothing to take out"), Loadout.ClearSlotAt(SECONDARY_INDEX));
	});

	It("adds a Generic's two weapons and its adapter through the slots, in step after each (loadout v2)", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Generic;
		FCombatLoadout Loadout;
		const FWeaponInventoryEntry First = OwnedWeapon();
		const FWeaponInventoryEntry Second = OwnedWeapon();
		FText Refusal;
		TestTrue(TEXT("the first weapon"), WoRLoadoutV2::ApplyDraft(Loadout, Class, WeaponDraft(First), Refusal));
		TestTrue(TEXT("in the primary, in step"), Loadout.PrimaryWeapon.WeaponEntry.PersistentID == First.PersistentID &&
													  Loadout.PrimarySlotType == EPrimarySlotType::Weapon && InStep(Loadout, Class));
		TestTrue(TEXT("the second weapon"), WoRLoadoutV2::ApplyDraft(Loadout, Class, WeaponDraft(Second), Refusal));
		TestTrue(TEXT("in the secondary, in step"), Loadout.SecondaryWeapon.WeaponEntry.PersistentID == Second.PersistentID &&
														Loadout.SecondarySlotType == ESecondarySlotType::Weapon && InStep(Loadout, Class));
		const FRingInventoryEntry Adapter = OwnedAdapter();
		TestTrue(TEXT("the adapter"), WoRLoadoutV2::ApplyDraft(Loadout, Class, AdapterDraft(Adapter), Refusal));
		TestTrue(TEXT("in the adapter loadout, in step"), Loadout.RingLoadout.Num() == 1 && Loadout.RingLoadout[0].RingEntry.PersistentID == Adapter.PersistentID &&
															  InStep(Loadout, Class));

		TestEqual(TEXT("everything but the first entry comes off"), WoRLoadoutV2::RemoveAllGear(Loadout, Class), 2);
		TestTrue(TEXT("only the first weapon left, in step"), Loadout.PrimaryWeapon.WeaponEntry.PersistentID == First.PersistentID && !Loadout.SecondaryWeapon.IsValid() &&
																  Loadout.SecondarySlotType == ESecondarySlotType::None && Loadout.RingLoadout.IsEmpty() && InStep(Loadout, Class));
	});

	It("adds a Resonator's adapter after an empty place, keeping the gap (loadout v2)", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Resonator;
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
		Loadout.RingLoadout = {FRingLoadoutEntry(), FRingLoadoutEntry()};
		Loadout.RingLoadout[0].RingEntry = OwnedAdapter();
		const FRingInventoryEntry Added = OwnedAdapter();
		FText Refusal;
		TestTrue(TEXT("added"), WoRLoadoutV2::ApplyDraft(Loadout, Class, AdapterDraft(Added), Refusal));
		TestTrue(TEXT("after the empty place, which stays"), Loadout.RingLoadout.Num() == 3 && !Loadout.RingLoadout[1].IsValid() &&
																 Loadout.RingLoadout[2].RingEntry.PersistentID == Added.PersistentID);
		TestTrue(TEXT("the evolution primary kept, in step"), Loadout.PrimarySlotType == EPrimarySlotType::Evolution && InStep(Loadout, Class));
	});

	It("keeps a weapon's skill picks when its sockets change, and starts a new main hand without them (loadout v2)", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Generic;
		UAbilityData *Pick = NewObject<UAbilityData>(GetTransientPackage());
		FCombatLoadout Loadout;
		FText Refusal;
		const FWeaponInventoryEntry Sword = OwnedWeapon();
		WoRLoadoutV2::ApplyDraft(Loadout, Class, WeaponDraft(Sword), Refusal);
		Loadout.PrimaryWeapon.WeaponEntry.SkillPicks = {Pick};
		Loadout.PrimaryWeapon.AssignedAbilities = {Pick};
		FLoadoutV2Entry Entry;
		Entry.Kind = ELoadoutV2EntryKind::Weapon;
		Entry.Place = ELoadoutV2Place::Primary;

		TestTrue(TEXT("the same sword again"), WoRLoadoutV2::ReplaceEntry(Loadout, Class, Entry, WeaponDraft(Loadout.PrimaryWeapon.WeaponEntry), Refusal));
		TestTrue(TEXT("its pick stays, in step"), Loadout.PrimaryWeapon.AssignedAbilities.Num() == 1 && Loadout.PrimaryWeapon.AssignedAbilities[0] == Pick &&
													  Loadout.PrimaryWeapon.WeaponEntry.SkillPicks.Contains(Pick) && InStep(Loadout, Class));
		const FWeaponInventoryEntry Axe = OwnedWeapon();
		TestTrue(TEXT("another main hand"), WoRLoadoutV2::ReplaceEntry(Loadout, Class, Entry, WeaponDraft(Axe), Refusal));
		TestTrue(TEXT("the axe, without the sword's pick, in step"), Loadout.PrimaryWeapon.WeaponEntry.PersistentID == Axe.PersistentID &&
																		  !Loadout.PrimaryWeapon.AssignedAbilities.Contains(Pick) && InStep(Loadout, Class));
	});

	It("puts a weapon over an evolution primary and moves a Generic's second weapon up with its picks (gear screen)", [this]()
	{
		const ECharacterClass Class = ECharacterClass::Generic;
		UAbilityData *Pick = NewObject<UAbilityData>(GetTransientPackage());
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
		Loadout.PrimaryEvolutionInstance = FGuid::NewGuid();
		FLoadoutScreenRow Primary;
		Primary.Kind = ELoadoutScreenRowKind::EvolutionSeat;
		Primary.Place = ELoadoutScreenPlace::Primary;
		FLoadoutScreenChoice Choice;
		Choice.Weapon.WeaponEntry = OwnedWeapon();
		TestTrue(TEXT("put"), WoRLoadoutScreen::PutGearChoice(Loadout, Class, Primary, Choice));
		TestTrue(TEXT("a weapon primary, the evolution seat emptied, in step"), Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
																					 !Loadout.PrimaryEvolutionInstance.IsValid() && Loadout.PrimaryWeapon.IsValid() && InStep(Loadout, Class));

		FLoadoutScreenRow Secondary;
		Secondary.Kind = ELoadoutScreenRowKind::Weapon;
		Secondary.Place = ELoadoutScreenPlace::Secondary;
		FLoadoutScreenChoice Second;
		Second.Weapon.WeaponEntry = OwnedWeapon();
		Second.Weapon.WeaponEntry.SkillPicks = {Pick};
		TestTrue(TEXT("a second weapon"), WoRLoadoutScreen::PutGearChoice(Loadout, Class, Secondary, Second));
		Loadout.bShowPrimary = false;

		FLoadoutScreenRow Worn = WoRLoadoutScreen::BuildGearPage(Loadout, Class).Rows[0];
		FText Refusal;
		TestTrue(TEXT("the primary comes off"), WoRLoadoutScreen::TakeOffGear(Loadout, Class, Worn, Refusal));
		TestTrue(TEXT("the second weapon moved up with its pick, in hand, in step"),
				 Loadout.PrimaryWeapon.WeaponEntry.PersistentID == Second.Weapon.WeaponEntry.PersistentID && Loadout.PrimaryWeapon.WeaponEntry.SkillPicks.Contains(Pick) &&
					 !Loadout.SecondaryWeapon.IsValid() && Loadout.SecondarySlotType == ESecondarySlotType::None && Loadout.bShowPrimary && InStep(Loadout, Class));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
