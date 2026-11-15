// WornGear.cpp
// D-221 stage 1 (B-505): the worn-gear record and its two conversions to and from the pawn's FCombatLoadout.

#include "Loadout/WornGear.h"
#include "Loadout/FSavedLoadout.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Pool/PoolSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

TArray<FGuid> FWornGear::GetPieceIDs() const
{
	TArray<FGuid> IDs;
	for (const FLoadoutSlot &Entry : Equipment)
	{
		IDs.Add(Entry.GearID);
		IDs.Add(Entry.OffHandID);
	}
	IDs.Add(EvolutionID);
	IDs.Append(Apparel);
	IDs.RemoveAll([](const FGuid &ID) { return !ID.IsValid(); });
	return IDs;
}

bool FWornGear::SameAs(const FWornGear &Other) const
{
	const auto SameEntry = [](const FLoadoutSlot &A, const FLoadoutSlot &B)
	{ return A.GearID == B.GearID && A.OffHandID == B.OffHandID && A.AssignedAbilities == B.AssignedAbilities; };
	const auto SamePool = [](const FBDElementSpellPool &A, const FBDElementSpellPool &B)
	{ return A.Element == B.Element && A.Spells == B.Spells; };
	if (Equipment.Num() != Other.Equipment.Num() || BDSpellPools.Num() != Other.BDSpellPools.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Equipment.Num(); ++Index)
	{
		if (!SameEntry(Equipment[Index], Other.Equipment[Index]))
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < BDSpellPools.Num(); ++Index)
	{
		if (!SamePool(BDSpellPools[Index], Other.BDSpellPools[Index]))
		{
			return false;
		}
	}
	return ActiveGearID == Other.ActiveGearID && EvolutionID == Other.EvolutionID && EvolutionSpells == Other.EvolutionSpells &&
		   Apparel == Other.Apparel && InnateSpells == Other.InnateSpells && WheelPick == Other.WheelPick;
}

FWornGear FCombatLoadout::ToWornGear(ECharacterClass CharacterClass) const
{
	using namespace WornGearLayout;
	FWornGear Worn;
	Worn.Equipment.SetNum(FIRST_ADAPTER_INDEX);
	if (HasPrimaryWeapon())
	{
		Worn.Equipment[PRIMARY_INDEX] = FLoadoutSlot::FromWeaponLoadoutEntry(PrimaryWeapon);
	}
	else if (HasPrimaryRing())
	{
		Worn.Equipment[PRIMARY_INDEX] = FLoadoutSlot::FromRingLoadoutEntry(PrimaryRing);
	}
	if (HasSecondaryWeapon())
	{
		Worn.Equipment[SECONDARY_INDEX] = FLoadoutSlot::FromWeaponLoadoutEntry(SecondaryWeapon);
	}
	for (const FRingLoadoutEntry &Ring : RingLoadout)
	{
		if (Ring.IsValid())
		{
			Worn.Equipment.Add(FLoadoutSlot::FromRingLoadoutEntry(Ring));
		}
	}

	// shortcut: a Resonator's active adapter (ActiveRingIndex) isn't recorded, as no map kept it before either; stage 2
	// makes ActiveGearID the one ACTIVE entry for adapters too.
	// The weapon drawn (ComputeActiveSlotIndices' rule, with a non-Generic's hidden weapon as none).
	const FGuid PrimaryID = Worn.Equipment[PRIMARY_INDEX].GearID;
	const FGuid SecondaryID = Worn.Equipment[SECONDARY_INDEX].GearID;
	if (CharacterClass == ECharacterClass::Generic)
	{
		Worn.ActiveGearID = HasPrimaryWeapon() && bShowPrimary ? PrimaryID : SecondaryID;
	}
	else if (HasPrimaryWeapon() && bShowPrimary)
	{
		Worn.ActiveGearID = PrimaryID;
	}

	if (IsEvolutionLoadout())
	{
		Worn.EvolutionID = PrimaryEvolutionInstance;
	}
	Worn.EvolutionSpells.Append(EvolutionSpells);
	for (const FApparelEntry &Piece : Apparel)
	{
		Worn.Apparel.Add(Piece.PersistentID);
	}
	Worn.InnateSpells.Append(InnateSpells);
	Worn.BDSpellPools = BDSpellPools;
	return Worn;
}

FCombatLoadout FCombatLoadout::CreateFromWornGear(const FWornGear &Worn, ECharacterClass CharacterClass,
												  const UInventoryComponent *OwnedInventory,
												  const UEvolutionInventoryComponent *OwnedEvolutions, bool bFallbackShowPrimary)
{
	using namespace WornGearLayout;
	const auto Weapon = [OwnedInventory](const FGuid &ID) -> UWeaponData *
	{
		const FWeaponInventoryEntry *Owned = ID.IsValid() && OwnedInventory
			? OwnedInventory->Weapons.FindByPredicate([&ID](const FWeaponInventoryEntry &E) { return E.PersistentID == ID; })
			: nullptr;
		return Owned ? Owned->Weapon : nullptr;
	};
	const auto Ring = [OwnedInventory](const FGuid &ID) -> URingData *
	{
		const FRingInventoryEntry *Owned = ID.IsValid() && OwnedInventory
			? OwnedInventory->Rings.FindByPredicate([&ID](const FRingInventoryEntry &E) { return E.PersistentID == ID; })
			: nullptr;
		return Owned ? Owned->Ring : nullptr;
	};
	const auto Refs = [](const TArray<TObjectPtr<USpellData>> &Spells)
	{
		TArray<FSpellRef> Out;
		for (USpellData *Spell : Spells)
		{
			Out.Emplace(Spell);
		}
		return Out;
	};

	FSavedLoadout Saved;
	Saved.LoadoutName = TEXT("Worn");
	Saved.RequiredClass = CharacterClass;
	const FLoadoutSlot *Primary = Worn.GetEntry(PRIMARY_INDEX);
	const FEvolutionInventoryEntry *Evolution = Worn.EvolutionID.IsValid() && OwnedEvolutions
		? OwnedEvolutions->Entries.FindByPredicate([&Worn](const FEvolutionInventoryEntry &E) { return E.InstanceID == Worn.EvolutionID; })
		: nullptr;
	if (UWeaponData *Main = Primary ? Weapon(Primary->GearID) : nullptr)
	{
		Saved.PrimarySlotType = EPrimarySlotType::Weapon;
		Saved.PrimaryWeapon = Main;
		Saved.PrimaryWeaponInstance = Primary->GearID;
		Saved.PrimaryOffHandWeapon = Weapon(Primary->OffHandID);
		Saved.PrimaryOffHandInstance = Saved.PrimaryOffHandWeapon ? Primary->OffHandID : FGuid();
		Saved.PrimaryWeaponAbilities.Append(Primary->AssignedAbilities);
	}
	else if (URingData *Adapter = Primary ? Ring(Primary->GearID) : nullptr)
	{
		Saved.PrimarySlotType = EPrimarySlotType::Ring;
		Saved.PrimaryRing = Adapter;
		Saved.PrimaryRingInstance = Primary->GearID;
	}
	else if (Evolution)
	{
		Saved.PrimarySlotType = EPrimarySlotType::Evolution;
		Saved.PrimaryEvolution = Evolution->Item;
		Saved.PrimaryEvolutionInstance = Evolution->InstanceID;
	}
	const FLoadoutSlot *Secondary = Worn.GetEntry(SECONDARY_INDEX);
	if (UWeaponData *Second = Secondary ? Weapon(Secondary->GearID) : nullptr)
	{
		Saved.SecondarySlotType = ESecondarySlotType::Weapon;
		Saved.SecondaryWeapon = Second;
		Saved.SecondaryWeaponInstance = Secondary->GearID;
		Saved.SecondaryOffHandWeapon = Weapon(Secondary->OffHandID);
		Saved.SecondaryOffHandInstance = Saved.SecondaryOffHandWeapon ? Secondary->OffHandID : FGuid();
		Saved.SecondaryWeaponAbilities.Append(Secondary->AssignedAbilities);
	}
	for (int32 Index = FIRST_ADAPTER_INDEX; Index < Worn.Equipment.Num(); ++Index)
	{
		if (URingData *Adapter = Ring(Worn.Equipment[Index].GearID))
		{
			FResonatorRingSlot &Slot = Saved.EquippedRings.AddDefaulted_GetRef();
			Slot.Ring = Adapter;
			Slot.RingInstance = Worn.Equipment[Index].GearID; // its spells ride on the owned piece
		}
	}

	const UGameInstance *GI = OwnedInventory && OwnedInventory->GetWorld() ? OwnedInventory->GetWorld()->GetGameInstance() : nullptr;
	const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	for (const FGuid &ID : Worn.Apparel)
	{
		if (const FApparelEntry *Piece = Pool ? Pool->FindApparelInPool(ID) : nullptr)
		{
			Saved.Apparel.Add(Piece->Apparel);
			Saved.ApparelInstances.Add(ID);
		}
	}
	Saved.EvolutionSpells = Refs(Worn.EvolutionSpells);
	Saved.InnateSpells = Refs(Worn.InnateSpells);
	for (const FBDElementSpellPool &In : Worn.BDSpellPools)
	{
		FSavedBDElementSpellPool &Out = Saved.BDSpellPools.AddDefaulted_GetRef();
		Out.Element = In.Element;
		for (USpellData *Spell : In.Spells)
		{
			Out.Spells.Emplace(Spell);
		}
	}

	// Lossless against ToWornGear: a weapon primary is shown when it is the ACTIVE entry.
	const bool bWeaponPrimary = Saved.PrimarySlotType == EPrimarySlotType::Weapon;
	Saved.bShowPrimary = bWeaponPrimary ? Worn.ActiveGearID.IsValid() && Worn.ActiveGearID == Saved.PrimaryWeaponInstance : bFallbackShowPrimary;
	return CreateFromSavedLoadout(Saved, OwnedInventory, OwnedEvolutions);
}
