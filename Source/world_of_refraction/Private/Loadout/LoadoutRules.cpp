// LoadoutRules.cpp

#include "Loadout/LoadoutRules.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/SpellPoolConstants.h"
#include "Inventory/InventoryConstants.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"

namespace
{
	/** Unset checks pass: the adapter couldn't check them. */
	template <typename TArg>
	bool Passes(const TFunction<bool(TArg)> &Check, TArg Arg)
	{
		return !Check || Check(Arg);
	}

	struct FFindingSink
	{
		TArray<FInvalidSlotFinding> Findings;

		void Add(ELoadoutSlotType Type, int32 SlotIndex, bool bClearable, const FString &Reason, int32 SubIndex = -1)
		{
			FInvalidSlotFinding Finding;
			Finding.SlotType = Type;
			Finding.SlotIndex = SlotIndex;
			Finding.SubIndex = SubIndex;
			Finding.bClearable = bClearable;
			Finding.Reason = Reason;
			Findings.Add(MoveTemp(Finding));
		}
	};

	/** Element coherence for a ring's customisable spells against its crystal.
	 *  Shared by the Resonator ring loadout and the Generic/Caster primary ring. */
	void CheckRingSpellCoherence(FFindingSink &Sink, const FRingInventoryEntry &Ring, int32 RingLoadoutIndex)
	{
		if (!Ring.Ring)
		{
			return;
		}
		for (int32 j = 0; j < Ring.AssignedSpells.Num(); ++j)
		{
			const USpellData *Spell = Ring.AssignedSpells[j];
			if (!Spell)
			{
				continue;
			}
			if (!Ring.HasCrystal())
			{
				Sink.Add(ELoadoutSlotType::RingSpell, RingLoadoutIndex, true,
						 FString::Printf(TEXT("Ring spell '%s' requires a crystal but the ring has none"), *Spell->Name), j);
				continue;
			}
			const ESpellElement RingElement = Ring.GetElement();
			if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, RingElement))
			{
				Sink.Add(ELoadoutSlotType::RingSpell, RingLoadoutIndex, true,
						 FString::Printf(TEXT("Ring spell '%s' element %s does not match ring crystal element %s"),
										 *Spell->Name, *UEnum::GetValueAsString(Spell->Element), *UEnum::GetValueAsString(RingElement)),
						 j);
			}
		}
	}

	/** Ability-slot counts (previously checked in the editor only). */
	void CheckAbilityCounts(FFindingSink &Sink, const FWeaponLoadoutEntry &Weapon, const TCHAR *Label)
	{
		if (Weapon.AssignedAbilities.Num() > LoadoutConstants::MAX_WEAPON_ABILITIES)
		{
			Sink.Add(ELoadoutSlotType::AbilityOverflow, -1, false,
					 FString::Printf(TEXT("%s weapon has too many abilities (%d/%d)"), Label,
									 Weapon.AssignedAbilities.Num(), LoadoutConstants::MAX_WEAPON_ABILITIES));
		}
		if (Weapon.AssignedAugmentStoneAbilities.Num() > LoadoutConstants::MAX_AUGMENTSTONE_ABILITIES)
		{
			Sink.Add(ELoadoutSlotType::AbilityOverflow, -1, false,
					 FString::Printf(TEXT("%s weapon has too many augment stone abilities (%d/%d)"), Label,
									 Weapon.AssignedAugmentStoneAbilities.Num(), LoadoutConstants::MAX_AUGMENTSTONE_ABILITIES));
		}
	}

	void CheckPrimaryWeapon(FFindingSink &Sink, const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context)
	{
		const FWeaponLoadoutEntry &Weapon = Loadout.PrimaryWeapon;
		if (!Passes<const UWeaponData *>(Context.OwnsWeapon, Weapon.WeaponEntry.Weapon))
		{
			Sink.Add(ELoadoutSlotType::PrimaryWeapon, -1, true, TEXT("Primary weapon not in inventory"));
		}

		const UWeaponData *EquippedWeapon = Weapon.WeaponEntry.Weapon;
		for (int32 i = 0; i < Weapon.AssignedAbilities.Num(); ++i)
		{
			const UAbilityData *Ability = Weapon.AssignedAbilities[i];
			if (!Ability)
			{
				continue;
			}
			if (!Passes<const UAbilityData *>(Context.KnowsAbility, Ability))
			{
				Sink.Add(ELoadoutSlotType::WeaponAbility, i, true, FString::Printf(TEXT("Ability '%s' not learned"), *Ability->Name));
				continue;
			}
			// Weapon-type coherence: ability must match the equipped weapon type.
			if (Ability->RequiredWeaponType != EquippedWeapon->WeaponType)
			{
				Sink.Add(ELoadoutSlotType::WeaponAbility, i, true,
						 FString::Printf(TEXT("Ability '%s' requires weapon type %s, equipped weapon is %s"),
										 *Ability->Name, *UEnum::GetValueAsString(Ability->RequiredWeaponType),
										 *UEnum::GetValueAsString(EquippedWeapon->WeaponType)));
				continue;
			}
			if (!Ability->AllowsWieldMode(EquippedWeapon->WieldMode))
			{
				Sink.Add(ELoadoutSlotType::WeaponAbility, i, true,
						 FString::Printf(TEXT("Ability '%s' requires wield mode %s, equipped weapon is %s"),
										 *Ability->Name, *UEnum::GetValueAsString(Ability->RequiredWieldMode),
										 *UEnum::GetValueAsString(EquippedWeapon->WieldMode)));
			}
		}

		for (int32 i = 0; i < Weapon.AssignedSpells.Num(); ++i)
		{
			const USpellData *Spell = Weapon.AssignedSpells[i];
			if (!Spell)
			{
				continue;
			}
			if (!Passes<const USpellData *>(Context.KnowsSpell, Spell))
			{
				Sink.Add(ELoadoutSlotType::WeaponSpell, i, true, FString::Printf(TEXT("Spell '%s' not learned"), *Spell->Name));
				continue;
			}
			// Element coherence: weapon-sourced spell must match the weapon crystal.
			if (!Weapon.WeaponEntry.HasCrystal())
			{
				Sink.Add(ELoadoutSlotType::WeaponSpell, i, true,
						 FString::Printf(TEXT("Weapon spell '%s' requires a crystal but equipped weapon has none"), *Spell->Name));
				continue;
			}
			const ESpellElement CrystalElement = Weapon.WeaponEntry.GetElement();
			if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, CrystalElement))
			{
				Sink.Add(ELoadoutSlotType::WeaponSpell, i, true,
						 FString::Printf(TEXT("Weapon spell '%s' element %s does not match weapon crystal element %s"),
										 *Spell->Name, *UEnum::GetValueAsString(Spell->Element),
										 *UEnum::GetValueAsString(CrystalElement)));
			}
		}

		CheckAbilityCounts(Sink, Weapon, TEXT("Primary"));
	}

	int32 PrimarySlotCost(const FCombatLoadout &Loadout)
	{
		switch (Loadout.PrimarySlotType)
		{
		case EPrimarySlotType::Weapon:
			// The fight's cost: the actual attached crystal (bare 1 / crystal 2 / evolution 3).
			return Loadout.PrimaryWeapon.WeaponEntry.GetSlotCost();
		case EPrimarySlotType::Evolution:
			return LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST;
		case EPrimarySlotType::None:
		case EPrimarySlotType::Ring:
		default:
			return 0;
		}
	}

	void CheckCasterSpells(FFindingSink &Sink, const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context)
	{
		if (Context.bBrokenDarkness)
		{
			// BD pool count + element-match + shared weight budget are owned by
			// ValidateBDSpellLoadout. Reported, not cleared (BD pool entries are never auto-cleared).
			for (const FString &Err : FCombatLoadout::ValidateBDSpellLoadout(
					 Loadout.InnateSpells, Loadout.BDSpellPools, Context.SpellDiscount, /*bCheckWeight=*/true))
			{
				Sink.Add(ELoadoutSlotType::BDPoolSpell, -1, false, Err);
			}
			return;
		}

		for (int32 i = 0; i < Loadout.InnateSpells.Num(); ++i)
		{
			const USpellData *Spell = Loadout.InnateSpells[i];
			if (!Spell)
			{
				continue;
			}
			if (!Passes<const USpellData *>(Context.KnowsSpell, Spell))
			{
				Sink.Add(ELoadoutSlotType::InnateSpell, i, true, FString::Printf(TEXT("Innate spell '%s' not learned"), *Spell->Name));
				continue;
			}
			if (!Passes<ESpellElement>(Context.CanCastElement, Spell->Element))
			{
				Sink.Add(ELoadoutSlotType::InnateSpell, i, true,
						 FString::Printf(TEXT("Innate spell '%s' element not castable by this character"), *Spell->Name));
			}
		}

		// Per-school count cap: each school's spell count must fit one pool.
		TMap<ESpellSchool, int32> PerSchoolCount;
		for (const USpellData *Spell : Loadout.InnateSpells)
		{
			if (Spell)
			{
				PerSchoolCount.FindOrAdd(Spell->School)++;
			}
		}
		for (const TPair<ESpellSchool, int32> &Pair : PerSchoolCount)
		{
			if (Pair.Value > SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL)
			{
				Sink.Add(ELoadoutSlotType::InnateSpell, -1, false,
						 FString::Printf(TEXT("Too many %s innate spells (%d/%d)"), *UEnum::GetValueAsString(Pair.Key),
										 Pair.Value, SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL));
			}
		}

		// Weight budget: effective per-spell costs (tier - mastery discount) must fit the budget.
		int32 Used = 0;
		for (const USpellData *Spell : Loadout.InnateSpells)
		{
			if (Spell)
			{
				Used += SpellPoolConstants::SpellSlotEffectiveCost(Spell->Tier, Context.SpellDiscount);
			}
		}
		if (Used > SpellPoolConstants::INNATE_SPELL_BUDGET)
		{
			Sink.Add(ELoadoutSlotType::InnateSpell, -1, false,
					 FString::Printf(TEXT("Innate spell budget exceeded: %d/%d points used (discount -%d)"),
									 Used, SpellPoolConstants::INNATE_SPELL_BUDGET, Context.SpellDiscount));
		}
	}

	struct FUtilityCrystalBand
	{
		const TCHAR *Name;
		EItemTier LowTier;
		EItemTier HighTier;
		int32 Cap;
	};

	const FUtilityCrystalBand UtilityCrystalBands[] = {
		{TEXT("S"), EItemTier::S_Tier, EItemTier::S_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER},
		{TEXT("A/B"), EItemTier::B_Tier, EItemTier::A_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_A_B_TIER},
		{TEXT("C/D"), EItemTier::D_Tier, EItemTier::C_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_C_D_TIER},
		{TEXT("E/F"), EItemTier::F_Tier, EItemTier::E_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_E_F_TIER},
	};

	/** D-072/D-077: utility crystals in item slots, per crystal, by combined tier band. Report-only
	 *  for now (B-159 drops the excess on load). */
	void CheckUtilityCrystalTierCaps(FFindingSink &Sink, const FCombatLoadout &Loadout)
	{
		for (const FUtilityCrystalBand &Band : UtilityCrystalBands)
		{
			int32 Count = 0;
			for (const FItemLoadoutSlot &Slot : Loadout.ItemSlots)
			{
				const bool bInBand = Slot.CrystalId.Tier >= Band.LowTier && Slot.CrystalId.Tier <= Band.HighTier;
				if (bInBand && CrystalTypeHelpers::IsGemType(Slot.CrystalId.Type) && Slot.Quantity > 0)
				{
					Count += Slot.Quantity;
				}
			}
			if (Count > Band.Cap)
			{
				Sink.Add(ELoadoutSlotType::ItemSlot, -1, false,
						 FString::Printf(TEXT("Too many %s-tier utility crystals: %d (max %d)"), Band.Name, Count, Band.Cap));
			}
		}
	}

	void CheckItems(FFindingSink &Sink, const FCombatLoadout &Loadout)
	{
		// Quantity range (previously checked in the editor only).
		for (int32 i = 0; i < Loadout.ItemSlots.Num(); ++i)
		{
			const int32 Quantity = Loadout.ItemSlots[i].Quantity;
			if (Quantity < 0 || Quantity > InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT)
			{
				Sink.Add(ELoadoutSlotType::ItemSlot, i, false,
						 FString::Printf(TEXT("Equipped item slot %d Quantity %d out of range [0, %d]"),
										 i, Quantity, InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT));
			}
		}
		// Duplicate crystal types are cleared: keep the first of each type.
		if (Loadout.HasDuplicateItemTypes())
		{
			Sink.Add(ELoadoutSlotType::DuplicateItems, -1, true, TEXT("Duplicate item types in loadout"));
		}
		CheckUtilityCrystalTierCaps(Sink, Loadout);
	}
}

TArray<FInvalidSlotFinding> FLoadoutRules::CollectFindings(const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context)
{
	FFindingSink Sink;

	if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
	{
		CheckPrimaryWeapon(Sink, Loadout, Context);
	}

	if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
	{
		if (!Passes<const URingData *>(Context.OwnsRing, Loadout.PrimaryRing.RingEntry.Ring))
		{
			Sink.Add(ELoadoutSlotType::PrimaryRing, -1, true, TEXT("Primary ring not in inventory"));
		}
		CheckRingSpellCoherence(Sink, Loadout.PrimaryRing.RingEntry, -1);
	}

	if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
	{
		if (!Passes<const UEvolutionItemData *>(Context.OwnsEvolution, Loadout.PrimaryEvolution.Item))
		{
			Sink.Add(ELoadoutSlotType::PrimaryEvolution, -1, true, TEXT("Primary evolution crystal not in inventory"));
		}
		// Cap keys on the evolution's own tier (D-073: the fight's cap, F 1 ... A/S 6).
		const int32 EvoCap = CrystalEffectTable::SlotsForContainerTier(Loadout.PrimaryEvolution.Tier);
		if (Loadout.EvolutionSpells.Num() > EvoCap)
		{
			Sink.Add(ELoadoutSlotType::EvolutionOverflow, -1, true,
					 FString::Printf(TEXT("Too many evolution spells (%d/%d)"), Loadout.EvolutionSpells.Num(), EvoCap));
		}
	}

	if (Context.CharacterClass == ECharacterClass::Generic &&
		Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
	{
		if (!Passes<const UWeaponData *>(Context.OwnsWeapon, Loadout.SecondaryWeapon.WeaponEntry.Weapon))
		{
			Sink.Add(ELoadoutSlotType::SecondaryWeapon, -1, true, TEXT("Secondary weapon not in inventory"));
		}
		CheckAbilityCounts(Sink, Loadout.SecondaryWeapon, TEXT("Secondary"));
	}

	if (Context.CharacterClass == ECharacterClass::Resonator)
	{
		int32 RingCost = 0;
		for (int32 i = 0; i < Loadout.RingLoadout.Num(); ++i)
		{
			const FRingLoadoutEntry &RingEntry = Loadout.RingLoadout[i];
			if (!RingEntry.IsValid())
			{
				continue;
			}
			if (!Passes<const URingData *>(Context.OwnsRing, RingEntry.RingEntry.Ring))
			{
				Sink.Add(ELoadoutSlotType::ResonatorRing, i, true, TEXT("Ring in loadout not in inventory"));
			}
			CheckRingSpellCoherence(Sink, RingEntry.RingEntry, i);
			RingCost += InventoryConstants::GetRingSlotCost(RingEntry.IsEvolved());
		}

		// Unified budget: primary slot cost + ring slot costs must fit one budget.
		const int32 PrimaryCost = PrimarySlotCost(Loadout);
		const int32 TotalCost = PrimaryCost + RingCost;
		if (TotalCost > LoadoutConstants::LOADOUT_TOTAL_BUDGET)
		{
			Sink.Add(ELoadoutSlotType::RingOverflow, -1, true,
					 FString::Printf(TEXT("Loadout exceeds budget: primary (cost %d) + rings (cost %d) = %d, max is %d"),
									 PrimaryCost, RingCost, TotalCost, LoadoutConstants::LOADOUT_TOTAL_BUDGET));
		}
	}

	if (Context.CharacterClass == ECharacterClass::Caster)
	{
		CheckCasterSpells(Sink, Loadout, Context);
	}

	CheckItems(Sink, Loadout);
	return MoveTemp(Sink.Findings);
}

TArray<FString> FLoadoutRules::CollectAuthoredShapeErrors(const FSavedLoadout &Saved)
{
	TArray<FString> Errors;

	switch (Saved.PrimarySlotType)
	{
	case EPrimarySlotType::Weapon:
		if (!Saved.PrimaryWeapon)
		{
			Errors.Add(TEXT("Primary slot set to Weapon but no weapon assigned"));
		}
		break;
	case EPrimarySlotType::Ring:
		if (Saved.RequiredClass == ECharacterClass::Resonator)
		{
			Errors.Add(TEXT("Resonator cannot use Ring as primary slot"));
		}
		else if (!Saved.PrimaryRing)
		{
			Errors.Add(TEXT("Primary slot set to Ring but no ring assigned"));
		}
		break;
	case EPrimarySlotType::Evolution:
		if (!Saved.PrimaryEvolution)
		{
			Errors.Add(TEXT("Primary slot set to Evolution but no evolution crystal assigned"));
		}
		break;
	case EPrimarySlotType::None:
	default:
		break;
	}

	if (Saved.RequiredClass == ECharacterClass::Generic &&
		Saved.SecondarySlotType == ESecondarySlotType::Weapon && !Saved.SecondaryWeapon)
	{
		Errors.Add(TEXT("Secondary slot set to Weapon but no weapon assigned"));
	}

	if (Saved.EquippedItems.Num() > InventoryConstants::MAX_ITEM_LOADOUT_SLOTS)
	{
		Errors.Add(FString::Printf(TEXT("Too many equipped items (%d/%d)"),
								   Saved.EquippedItems.Num(), InventoryConstants::MAX_ITEM_LOADOUT_SLOTS));
	}

	for (int32 i = 0; i < Saved.EquippedRings.Num(); ++i)
	{
		if (!Saved.EquippedRings[i].Ring)
		{
			Errors.Add(FString::Printf(TEXT("Null ring in slot %d"), i));
		}
	}
	return Errors;
}

TArray<FString> FLoadoutRules::ToReasons(const TArray<FInvalidSlotFinding> &Findings)
{
	TArray<FString> Reasons;
	Reasons.Reserve(Findings.Num());
	for (const FInvalidSlotFinding &Finding : Findings)
	{
		Reasons.Add(Finding.Reason);
	}
	return Reasons;
}
