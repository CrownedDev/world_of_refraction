// LoadoutSlotsTruthSpec.cpp
// B-309d (piece 1): the slots can be the truth. FCombatLoadout::RebuildLegacyFromSlots rebuilds the old fields from
// Slots by each slot's place with no loss (old fields -> slots -> old fields gives the same loadout, the mirror in
// step), keeps an empty adapter place and the active adapter, and follows a slot taken out (the type fields with it).
// Unproven here: the writers moving onto the slots (B-309d's later pieces).

#if WITH_DEV_AUTOMATION_TESTS

#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/WornGear.h"
#include "Misc/AutomationTest.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 STAT_POOL = 7;
	constexpr int32 ACTIVE_RING = 2;

	FWeaponInventoryEntry OwnedWeapon(UWeaponData *Weapon)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Quality = EItemQuality::A_Quality;
		Entry.StatPool = STAT_POOL;
		Entry.StatMaxPool = STAT_POOL + 1;
		Entry.ResistancePool = STAT_POOL + 2;
		Entry.ResistanceMaxPool = STAT_POOL + 3;
		Entry.bPrebuilt = true;
		Entry.bCrystalRemovable = true;
		Entry.bSkillSlotsActive = true;
		Entry.bSpellSlotsActive = true;
		return Entry;
	}

	FRingLoadoutEntry OwnedRing(URingData *Ring)
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry = FRingInventoryEntry::CreateFromRing(Ring);
		Entry.RingEntry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	bool SamePiece(const FWeaponInventoryEntry &A, const FWeaponInventoryEntry &B)
	{
		return A.Weapon == B.Weapon && A.PersistentID == B.PersistentID && A.InstanceID == B.InstanceID && A.Tier == B.Tier &&
			   A.Quality == B.Quality && A.StatPool == B.StatPool && A.StatMaxPool == B.StatMaxPool && A.ResistancePool == B.ResistancePool &&
			   A.ResistanceMaxPool == B.ResistanceMaxPool && A.bPrebuilt == B.bPrebuilt && A.bCrystalRemovable == B.bCrystalRemovable &&
			   A.bSkillSlotsActive == B.bSkillSlotsActive && A.bSpellSlotsActive == B.bSpellSlotsActive && A.SkillPicks == B.SkillPicks &&
			   A.SpellPicks == B.SpellPicks && A.AssignedSpells == B.AssignedSpells;
	}

	bool SameWeaponEntry(const FWeaponLoadoutEntry &A, const FWeaponLoadoutEntry &B)
	{
		return SamePiece(A.WeaponEntry, B.WeaponEntry) && SamePiece(A.OffHandEntry, B.OffHandEntry) && A.AssignedAbilities == B.AssignedAbilities &&
			   A.AssignedSpells == B.AssignedSpells && A.OverrideAttack == B.OverrideAttack && A.bUseWeaponParryAnimation == B.bUseWeaponParryAnimation;
	}

	bool SameRingEntry(const FRingLoadoutEntry &A, const FRingLoadoutEntry &B)
	{
		return A.RingEntry.Ring == B.RingEntry.Ring && A.RingEntry.PersistentID == B.RingEntry.PersistentID && A.RingEntry.SpellPicks == B.RingEntry.SpellPicks;
	}

	/** Old fields -> slots, the old fields wiped, then rebuilt from the slots alone. */
	FCombatLoadout ThroughTheSlots(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FCombatLoadout Mirrored = Loadout;
		Mirrored.BuildSlotsFromOldFields(Class);
		FCombatLoadout Back = Mirrored;
		Back.PrimaryWeapon = FWeaponLoadoutEntry();
		Back.SecondaryWeapon = FWeaponLoadoutEntry();
		Back.PrimaryRing = FRingLoadoutEntry();
		Back.RingLoadout.Reset();
		Back.RebuildLegacyFromSlots(Class);
		return Back;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutSlotsTruthSpec, "WoR.Loadout.SlotsTruth", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutSlotsTruthSpec)

void FLoadoutSlotsTruthSpec::Define()
{
	It("rebuilds a Generic's two weapon sets and its adapter from the slots with no loss", [this]()
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Dagger = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Bow = NewObject<UWeaponData>(GetTransientPackage());
		URingData *Adapter = NewObject<URingData>(GetTransientPackage());
		UAbilityData *Pick = NewObject<UAbilityData>(GetTransientPackage());
		UAbilityData *Merge = NewObject<UAbilityData>(GetTransientPackage());
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = OwnedWeapon(Sword);
		Loadout.PrimaryWeapon.WeaponEntry.SkillPicks = {nullptr, Pick}; // an empty skill slot kept
		Loadout.PrimaryWeapon.WeaponEntry.SpellPicks = {Spell};
		Loadout.PrimaryWeapon.WeaponEntry.AssignedSpells = {Spell};
		Loadout.PrimaryWeapon.AssignedSpells = {Spell};
		Loadout.PrimaryWeapon.OffHandEntry = OwnedWeapon(Dagger);
		Loadout.PrimaryWeapon.AssignedAbilities = {Merge};
		Loadout.PrimaryWeapon.OverrideAttack = Pick;
		Loadout.PrimaryWeapon.bUseWeaponParryAnimation = false;
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon.WeaponEntry = OwnedWeapon(Bow);
		Loadout.SecondaryWeapon.AssignedSpells = {Spell};
		Loadout.RingLoadout = {OwnedRing(Adapter)};
		Loadout.RingLoadout[0].RingEntry.SpellPicks = {Spell};
		Loadout.bShowPrimary = false;

		const FCombatLoadout Back = ThroughTheSlots(Loadout, ECharacterClass::Generic);
		TestTrue(TEXT("the primary set: sword, dagger, picks, merge list, attack, parry flag"), SameWeaponEntry(Back.PrimaryWeapon, Loadout.PrimaryWeapon));
		TestTrue(TEXT("the secondary bow with its spell list"), SameWeaponEntry(Back.SecondaryWeapon, Loadout.SecondaryWeapon));
		TestTrue(TEXT("the adapter with its picks"), Back.RingLoadout.Num() == 1 && SameRingEntry(Back.RingLoadout[0], Loadout.RingLoadout[0]));
		TestTrue(TEXT("both types kept"), Back.PrimarySlotType == EPrimarySlotType::Weapon && Back.SecondarySlotType == ESecondarySlotType::Weapon);
		FCombatLoadout Mirrored = Loadout;
		Mirrored.BuildSlotsFromOldFields(ECharacterClass::Generic);
		TestEqual(TEXT("the bow still in hand"), Back.ActiveWeaponSlot, Mirrored.ActiveWeaponSlot);
		TestEqual(TEXT("the adapter still in use"), Back.ActiveAdapterSlot, Mirrored.ActiveAdapterSlot);
		using namespace WornGearLayout;
		TestTrue(TEXT("each slot in its place"), Mirrored.Slots.Num() == 3 && Mirrored.Slots[0].Place == PRIMARY_INDEX &&
													 Mirrored.Slots[1].Place == SECONDARY_INDEX && Mirrored.Slots[2].Place == FIRST_ADAPTER_INDEX);
		FCombatLoadout Remirrored = Back;
		Remirrored.BuildSlotsFromOldFields(ECharacterClass::Generic);
		bool bSameSlots = Remirrored.Slots.Num() == Mirrored.Slots.Num();
		for (int32 Index = 0; bSameSlots && Index < Mirrored.Slots.Num(); ++Index)
		{
			const FLoadoutSlot &A = Remirrored.Slots[Index];
			const FLoadoutSlot &B = Mirrored.Slots[Index];
			bSameSlots = A.Place == B.Place && A.GearID == B.GearID && A.OffHandID == B.OffHandID && A.Gear.Asset == B.Gear.Asset &&
						 A.Gear.Kind == B.Gear.Kind && A.AssignedAbilities == B.AssignedAbilities &&
						 A.SpellOverrides == B.SpellOverrides;
		}
		TestTrue(TEXT("the rebuilt fields mirror into the same slots, in the same places"), bSameSlots);
	});

	It("keeps a Resonator's empty adapter place, its active adapter and its evolution primary", [this]()
	{
		URingData *First = NewObject<URingData>(GetTransientPackage());
		URingData *Third = NewObject<URingData>(GetTransientPackage());
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
		Loadout.RingLoadout = {OwnedRing(First), FRingLoadoutEntry(), OwnedRing(Third)};
		Loadout.ActiveRingIndex = ACTIVE_RING;

		const FCombatLoadout Back = ThroughTheSlots(Loadout, ECharacterClass::Resonator);
		TestTrue(TEXT("three places, the middle one empty"), Back.RingLoadout.Num() == 3 && SameRingEntry(Back.RingLoadout[0], Loadout.RingLoadout[0]) &&
																 !Back.RingLoadout[1].IsValid() && SameRingEntry(Back.RingLoadout[2], Loadout.RingLoadout[2]));
		TestTrue(TEXT("the evolution primary keeps its type"), Back.PrimarySlotType == EPrimarySlotType::Evolution);
		TestTrue(TEXT("the third adapter is still the one in use"),
				 Back.Slots.IsValidIndex(Back.ActiveAdapterSlot) && Back.Slots[Back.ActiveAdapterSlot].GearID == Loadout.RingLoadout[ACTIVE_RING].RingEntry.PersistentID);
	});

	It("rebuilds a Caster's adapter primary, and follows a slot taken out", [this]()
	{
		URingData *Adapter = NewObject<URingData>(GetTransientPackage());
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Ring;
		Loadout.PrimaryRing = OwnedRing(Adapter);
		const FCombatLoadout Back = ThroughTheSlots(Loadout, ECharacterClass::Caster);
		TestTrue(TEXT("the adapter primary"), Back.PrimarySlotType == EPrimarySlotType::Ring && SameRingEntry(Back.PrimaryRing, Loadout.PrimaryRing));

		FCombatLoadout Emptied = Back;
		Emptied.Slots.Reset(); // a writer takes the adapter out of its slot
		Emptied.RebuildLegacyFromSlots(ECharacterClass::Caster);
		TestTrue(TEXT("the old field follows, its type stays (an emptied adapter primary reads as none)"),
				 !Emptied.PrimaryRing.IsValid() && Emptied.PrimarySlotType == EPrimarySlotType::Ring && Emptied.ActiveAdapterSlot == INDEX_NONE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
