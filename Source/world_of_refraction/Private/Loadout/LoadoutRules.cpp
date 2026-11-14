// LoadoutRules.cpp

#include "Loadout/LoadoutRules.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/SpellPoolConstants.h"
#include "Loadout/WeaponSetRules.h"
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
						 FString::Printf(TEXT("Adapter spell '%s' requires a crystal but the adapter has none"), *Spell->Name), j);
				continue;
			}
			const ESpellElement RingElement = Ring.GetElement();
			if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, RingElement))
			{
				Sink.Add(ELoadoutSlotType::RingSpell, RingLoadoutIndex, true,
						 FString::Printf(TEXT("Adapter spell '%s' element %s does not match adapter crystal element %s"),
										 *Spell->Name, *UEnum::GetValueAsString(Spell->Element), *UEnum::GetValueAsString(RingElement)),
						 j);
			}
		}
	}

	/** B-196: GetAllSpells silently drops the spells over the holder's cap (Q-147) in a fight.
	 *  Assigned spells first replace the item's own (preset) slots and only the rest are appended,
	 *  so only appended ones (the last assigned) can be cleared to get under the cap; anything
	 *  still over is the item's own spells (one non-clearable finding). Assigned is what the merge
	 *  reads (empty for a ring whose crystal can't provide spells). WeaponSub (weapon spells only): -1 primary,
	 *  SECONDARY_WEAPON for a Generic's secondary (B-304). */
	void CheckSpellCap(FFindingSink &Sink, ELoadoutSlotType Type, int32 SlotIndex, bool bSpellIsSubIndex,
					   const TArray<USpellData *> &Assigned, int32 PresetCount, int32 UncappedCount, int32 Cap, int32 WeaponSub = -1)
	{
		int32 OverCap = FMath::Max(0, UncappedCount - Cap);
		int32 Clearable = FMath::Min(OverCap, FMath::Max(0, UncappedCount - PresetCount));
		for (int32 i = Assigned.Num() - 1; i >= 0 && Clearable > 0; --i)
		{
			if (!Assigned[i])
			{
				continue;
			}
			const FString Reason = FString::Printf(TEXT("Spell '%s' is over the holder's spell cap (%d)"), *Assigned[i]->Name, Cap);
			Sink.Add(Type, bSpellIsSubIndex ? SlotIndex : i, true, Reason, bSpellIsSubIndex ? i : WeaponSub);
			--OverCap;
			--Clearable;
		}
		if (OverCap > 0)
		{
			// The rest are the item's own spells (the inventory entry), not this loadout's.
			Sink.Add(Type, SlotIndex, false,
					 FString::Printf(TEXT("%d of the item's own spells are over the holder's spell cap (%d)"), OverCap, Cap));
		}
	}

	void CheckRingSpellCap(FFindingSink &Sink, const FRingLoadoutEntry &Ring, int32 RingLoadoutIndex)
	{
		if (Ring.GetSpellsOverCap() == 0)
		{
			return; // also a locked ring: its presets are never capped
		}
		CheckSpellCap(Sink, ELoadoutSlotType::RingSpell, RingLoadoutIndex, /*bSpellIsSubIndex=*/true, Ring.RingEntry.GetSpells(),
					  Ring.RingEntry.Ring ? Ring.RingEntry.Ring->PresetSpells.Num() : 0, Ring.GetAllSpellsUncapped().Num(),
					  Ring.GetSpellSlotCap());
	}

	/** Assigned abilities, not the empty entries a drop leaves behind. */
	int32 CountAssigned(const TArray<UAbilityData *> &Abilities)
	{
		int32 Count = 0;
		for (const UAbilityData *Ability : Abilities)
		{
			Count += Ability ? 1 : 0;
		}
		return Count;
	}

	/** Ability-slot counts (previously checked in the editor only). */
	void CheckAbilityCounts(FFindingSink &Sink, const FWeaponLoadoutEntry &Weapon, const TCHAR *Label, int32 WeaponIndex)
	{
		const int32 Abilities = CountAssigned(Weapon.AssignedAbilities);
		if (Abilities > LoadoutConstants::MAX_WEAPON_ABILITIES)
		{
			Sink.Add(ELoadoutSlotType::AbilityOverflow, WeaponIndex, true,
					 FString::Printf(TEXT("%s weapon has too many abilities (%d/%d)"), Label,
									 Abilities, LoadoutConstants::MAX_WEAPON_ABILITIES),
					 LoadoutAbilityOverflowIndex::ABILITIES);
		}
	}

	/** B-304 (D-110): one check for both of a Generic's weapon slots: ownership, spells (element and cap) and ability
	 *  counts; the ability type / wield checks are the primary's only (see below). A secondary weapon's findings carry
	 *  SubIndex SECONDARY_WEAPON so ClearInvalidSlots clears the right weapon. */
	void CheckWeapon(FFindingSink &Sink, const FWeaponLoadoutEntry &Weapon, const FLoadoutRuleContext &Context, bool bSecondary)
	{
		const int32 WeaponSub = bSecondary ? LoadoutAbilityOverflowIndex::SECONDARY_WEAPON : -1;
		const ELoadoutSlotType WeaponSlotType = bSecondary ? ELoadoutSlotType::SecondaryWeapon : ELoadoutSlotType::PrimaryWeapon;
		if (!Passes<const UWeaponData *>(Context.OwnsWeapon, Weapon.WeaponEntry.Weapon))
		{
			Sink.Add(WeaponSlotType, -1, true, bSecondary ? TEXT("Secondary weapon not in inventory") : TEXT("Primary weapon not in inventory"));
		}

		// B-477 (D-209): the weapon set. Only a Generic holds a pair, only an allowed pair, and a DualOnly weapon never alone.
		if (Weapon.HasOffHand())
		{
			const WeaponSetRules::EPairRefusal Refusal = WeaponSetRules::CheckPair(Weapon.WeaponEntry, Weapon.OffHandEntry);
			if (Context.CharacterClass != ECharacterClass::Generic)
			{
				Sink.Add(ELoadoutSlotType::OffHandWeapon, -1, true, TEXT("Only a Generic can dual wield"), WeaponSub);
			}
			else if (Refusal != WeaponSetRules::EPairRefusal::None)
			{
				Sink.Add(ELoadoutSlotType::OffHandWeapon, -1, true,
						 FString::Printf(TEXT("Off-hand weapon '%s' can't pair: %s"), *GetNameSafe(Weapon.OffHandEntry.Weapon),
										 *WeaponSetRules::GetPairRefusalText(Refusal).ToString()),
						 WeaponSub);
			}
			else if (!Passes<const UWeaponData *>(Context.OwnsWeapon, Weapon.OffHandEntry.Weapon))
			{
				Sink.Add(ELoadoutSlotType::OffHandWeapon, -1, true, TEXT("Off-hand weapon not in inventory"), WeaponSub);
			}
		}
		else if (!WeaponSetRules::CanStandAlone(Weapon))
		{
			Sink.Add(WeaponSlotType, -1, true,
					 FString::Printf(TEXT("Weapon '%s' can only be dual wielded"), *GetNameSafe(Weapon.WeaponEntry.Weapon)));
		}
		// D-210: a valid pair takes single-blade and dual-blade abilities of its type.
		const EWeaponWieldMode SetWieldMode = Context.CharacterClass == ECharacterClass::Generic ? WeaponSetRules::GetSetWieldMode(Weapon)
																								  : (Weapon.WeaponEntry.Weapon ? Weapon.WeaponEntry.Weapon->WieldMode : EWeaponWieldMode::Single);

		// The ability checks stay the primary's for now: a secondary's authored abilities were never type-checked, so
		// checking them would newly refuse authored Generic loadouts (B-304 review). The row's spell checks apply.
		const UWeaponData *EquippedWeapon = Weapon.WeaponEntry.Weapon;
		for (int32 i = 0; !bSecondary && i < Weapon.AssignedAbilities.Num(); ++i)
		{
			const UAbilityData *Ability = Weapon.AssignedAbilities[i];
			if (!Ability)
			{
				continue;
			}
			if (!Passes<const UAbilityData *>(Context.KnowsAbility, Ability))
			{
				Sink.Add(ELoadoutSlotType::WeaponAbility, i, true, FString::Printf(TEXT("Ability '%s' not learned"), *Ability->Name), WeaponSub);
				continue;
			}
			// Weapon-type coherence: ability must match the equipped weapon type.
			if (Ability->RequiredWeaponType != EquippedWeapon->WeaponType)
			{
				Sink.Add(ELoadoutSlotType::WeaponAbility, i, true,
						 FString::Printf(TEXT("Ability '%s' requires weapon type %s, equipped weapon is %s"),
										 *Ability->Name, *UEnum::GetValueAsString(Ability->RequiredWeaponType),
										 *UEnum::GetValueAsString(EquippedWeapon->WeaponType)), WeaponSub);
				continue;
			}
			if (!Ability->AllowsWieldMode(SetWieldMode))
			{
				Sink.Add(ELoadoutSlotType::WeaponAbility, i, true,
						 FString::Printf(TEXT("Ability '%s' requires wield mode %s, equipped weapon is %s"),
										 *Ability->Name, *UEnum::GetValueAsString(Ability->RequiredWieldMode),
										 *UEnum::GetValueAsString(SetWieldMode)), WeaponSub);
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
				Sink.Add(ELoadoutSlotType::WeaponSpell, i, true, FString::Printf(TEXT("Spell '%s' not learned"), *Spell->Name), WeaponSub);
				continue;
			}
			// Element coherence: weapon-sourced spell must match the weapon crystal.
			if (!Weapon.WeaponEntry.HasCrystal())
			{
				Sink.Add(ELoadoutSlotType::WeaponSpell, i, true,
						 FString::Printf(TEXT("Weapon spell '%s' requires a crystal but equipped weapon has none"), *Spell->Name), WeaponSub);
				continue;
			}
			const ESpellElement CrystalElement = Weapon.WeaponEntry.GetElement();
			if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, CrystalElement))
			{
				Sink.Add(ELoadoutSlotType::WeaponSpell, i, true,
						 FString::Printf(TEXT("Weapon spell '%s' element %s does not match weapon crystal element %s"),
										 *Spell->Name, *UEnum::GetValueAsString(Spell->Element),
										 *UEnum::GetValueAsString(CrystalElement)), WeaponSub);
			}
		}

		CheckSpellCap(Sink, ELoadoutSlotType::WeaponSpell, -1, /*bSpellIsSubIndex=*/false, Weapon.AssignedSpells,
					  Weapon.WeaponEntry.GetSpells().Num(), Weapon.GetAllSpellsUncapped().Num(), Weapon.GetSpellSlotCap(), WeaponSub);
		CheckAbilityCounts(Sink, Weapon, bSecondary ? TEXT("Secondary") : TEXT("Primary"),
						   bSecondary ? LoadoutAbilityOverflowIndex::SECONDARY_WEAPON : LoadoutAbilityOverflowIndex::PRIMARY_WEAPON);
	}

	void CheckCasterSpells(FFindingSink &Sink, const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context)
	{
		if (Context.bBrokenDarkness)
		{
			// BD pool count + element-match + shared weight budget are owned by
			// ValidateBDSpellLoadout. Reported, not cleared (BD pool entries are never auto-cleared).
			for (const FString &Err : FCombatLoadout::ValidateBDSpellLoadout(
					 Loadout.InnateSpells, Loadout.BDSpellPools, Context.SpellDiscount, /*bCheckWeight=*/Context.bCheckSpellBudget))
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
				PerSchoolCount.FindOrAdd(Spell->GetSchool())++; // B-356: a retired school counts as its mapped one
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
		if (Context.bCheckSpellBudget && Used > SpellPoolConstants::INNATE_SPELL_BUDGET)
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
	constexpr int32 UTILITY_BAND_COUNT = static_cast<int32>(UE_ARRAY_COUNT(UtilityCrystalBands));

	bool CountsTowardBand(const FItemLoadoutSlot &Slot, const FUtilityCrystalBand &Band)
	{
		return Slot.Quantity > 0 && CrystalTypeHelpers::IsGemType(Slot.CrystalId.Type) &&
			   Slot.CrystalId.Tier >= Band.LowTier && Slot.CrystalId.Tier <= Band.HighTier;
	}

	bool IsSTierAttachment(const FRuntimeAttachedItem &Attached)
	{
		switch (Attached.Kind)
		{
		case EAttachedItemKind::Crystal:
		case EAttachedItemKind::AugmentStone:
			return Attached.Crystal.Id.Tier == EItemTier::S_Tier;
		case EAttachedItemKind::Fusion:
			return Attached.Fusion.Id.HalfA.Tier == EItemTier::S_Tier || Attached.Fusion.Id.HalfB.Tier == EItemTier::S_Tier;
		case EAttachedItemKind::Evolution:
			return Attached.Evolution.Tier == EItemTier::S_Tier;
		default:
			return false;
		}
	}

	bool IsSTierItemGem(const FItemLoadoutSlot &Slot)
	{
		return Slot.Quantity > 0 && CrystalTypeHelpers::IsGemType(Slot.CrystalId.Type) && Slot.CrystalId.Tier == EItemTier::S_Tier;
	}

	/** The same adapter in the primary ring slot and the ring loadout (by owned ID, else by asset for unlinked copies). */
	bool IsSameAdapter(const FRingInventoryEntry &A, const FRingInventoryEntry &B)
	{
		return A.PersistentID.IsValid() || B.PersistentID.IsValid() ? A.PersistentID == B.PersistentID : A.Ring == B.Ring;
	}

	/** S attachments on the slots the fight reads, gated as CollectFindings gates them: the primary by its slot type (a
	 *  weapon with a Weapon or None type, as UInventoryComponent::LoadoutHasPrimary reads it; the ring; the evolution
	 *  seat), a Generic's secondary weapon with a Weapon type, and each adapter in a Resonator's ring loadout (the primary
	 *  ring counted once when it is also there). A stale entry in an inactive slot, or in a slot the class doesn't
	 *  fight with, doesn't count. */
	int32 CountAttachedSTier(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		int32 Count = 0;
		// B-351 (D-141): both sockets count, the element socket and the stone socket.
		const auto CountEntry = [&Count](const FRuntimeAttachedItem &Attached, const FRuntimeAttachedItem &Stone)
		{
			Count += IsSTierAttachment(Attached) ? 1 : 0;
			Count += IsSTierAttachment(Stone) ? 1 : 0;
		};
		const FWeaponInventoryEntry &Primary = Loadout.PrimaryWeapon.WeaponEntry;
		const FRingInventoryEntry &PrimaryRing = Loadout.PrimaryRing.RingEntry;
		const bool bWeaponPrimary = Loadout.PrimarySlotType == EPrimarySlotType::Weapon || Loadout.PrimarySlotType == EPrimarySlotType::None;
		const bool bRingPrimary = Loadout.PrimarySlotType == EPrimarySlotType::Ring && PrimaryRing.Ring;
		if (bWeaponPrimary && Primary.Weapon)
		{
			CountEntry(Primary.AttachedItem, Primary.StoneSocket);
		}
		if (bRingPrimary)
		{
			CountEntry(PrimaryRing.AttachedItem, PrimaryRing.StoneSocket);
		}
		if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
		{
			Count += Loadout.PrimaryEvolution.Tier == EItemTier::S_Tier ? 1 : 0;
		}
		const FWeaponInventoryEntry &Secondary = Loadout.SecondaryWeapon.WeaponEntry;
		if (Class == ECharacterClass::Generic && Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Secondary.Weapon)
		{
			CountEntry(Secondary.AttachedItem, Secondary.StoneSocket);
		}
		if (Class == ECharacterClass::Resonator)
		{
			for (const FRingLoadoutEntry &Ring : Loadout.RingLoadout)
			{
				if (Ring.RingEntry.Ring && !(bRingPrimary && IsSameAdapter(Ring.RingEntry, PrimaryRing)))
				{
					CountEntry(Ring.RingEntry.AttachedItem, Ring.RingEntry.StoneSocket);
				}
			}
		}
		return Count;
	}

	/** B-354 (D-143): one S-tier crystal equipped per battle loadout. */
	void CheckEquippedSTierCrystals(FFindingSink &Sink, const FCombatLoadout &Loadout, ECharacterClass Class, bool bCountItemSlots)
	{
		const int32 Count = FLoadoutRules::CountEquippedSTierCrystals(Loadout, Class, bCountItemSlots);
		if (Count > LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS)
		{
			// Only the item slots' gems can be cleared; an S attachment too many stays reported for the player to change.
			const bool bItemGems = bCountItemSlots && Loadout.ItemSlots.ContainsByPredicate([](const FItemLoadoutSlot &Slot) { return IsSTierItemGem(Slot); });
			Sink.Add(ELoadoutSlotType::EquippedSTierCrystal, INDEX_NONE, bItemGems,
					 FString::Printf(TEXT("Too many S-tier crystals equipped: %d (max %d)"), Count, LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS));
		}
	}

	/** OBSOLETE (B-354, D-143): D-072/D-077 utility crystals in item slots, per crystal, by combined tier band. */
	[[maybe_unused]] void CheckUtilityCrystalTierCaps(FFindingSink &Sink, const FCombatLoadout &Loadout)
	{
		for (const FUtilityCrystalBand &Band : UtilityCrystalBands)
		{
			int32 Count = 0;
			for (const FItemLoadoutSlot &Slot : Loadout.ItemSlots)
			{
				Count += CountsTowardBand(Slot, Band) ? Slot.Quantity : 0;
			}
			if (Count > Band.Cap)
			{
				Sink.Add(ELoadoutSlotType::UtilityCrystalTierCap, INDEX_NONE, true,
						 FString::Printf(TEXT("Too many %s-tier utility crystals: %d (max %d)"), Band.Name, Count, Band.Cap));
			}
		}
	}

	void CheckItems(FFindingSink &Sink, const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context)
	{
		if (Context.bItemsInPouch)
		{
			// B-313b (D-111): a pouch member's item slots hold no items; only its attached S crystals count.
			CheckEquippedSTierCrystals(Sink, Loadout, Context.CharacterClass, /*bCountItemSlots=*/false);
			return;
		}
		// Quantity range (previously checked in the editor only).
		for (int32 i = 0; i < Loadout.ItemSlots.Num(); ++i)
		{
			const int32 Quantity = Loadout.ItemSlots[i].Quantity;
			if (Quantity < 0 || Quantity > InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT)
			{
				Sink.Add(ELoadoutSlotType::ItemSlot, i, true,
						 FString::Printf(TEXT("Equipped item slot %d Quantity %d out of range [0, %d]"),
										 i, Quantity, InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT));
			}
		}
		// Duplicate crystal types are cleared: keep the first of each type.
		if (Loadout.HasDuplicateItemTypes())
		{
			Sink.Add(ELoadoutSlotType::DuplicateItems, -1, true, TEXT("Duplicate item types in loadout"));
		}
		CheckEquippedSTierCrystals(Sink, Loadout, Context.CharacterClass, /*bCountItemSlots=*/true); // B-354 (D-143)
	}
}

TArray<FInvalidSlotFinding> FLoadoutRules::CollectFindings(const FCombatLoadout &Loadout, const FLoadoutRuleContext &Context)
{
	FFindingSink Sink;

	if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
	{
		CheckWeapon(Sink, Loadout.PrimaryWeapon, Context, /*bSecondary=*/false);
	}

	if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
	{
		if (!Passes<const URingData *>(Context.OwnsRing, Loadout.PrimaryRing.RingEntry.Ring))
		{
			Sink.Add(ELoadoutSlotType::PrimaryRing, -1, true, TEXT("Primary adapter not in inventory"));
		}
		CheckRingSpellCoherence(Sink, Loadout.PrimaryRing.RingEntry, -1);
		CheckRingSpellCap(Sink, Loadout.PrimaryRing, -1);
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
		// B-304 (D-110): the secondary gets the primary's spell checks (spells, spell cap), not only ownership and
		// counts; CheckWeapon still skips the abilities' weapon type and wield mode for it.
		CheckWeapon(Sink, Loadout.SecondaryWeapon, Context, /*bSecondary=*/true);
	}

	if (Context.CharacterClass == ECharacterClass::Resonator)
	{
		for (int32 i = 0; i < Loadout.RingLoadout.Num(); ++i)
		{
			const FRingLoadoutEntry &RingEntry = Loadout.RingLoadout[i];
			if (!RingEntry.IsValid())
			{
				continue;
			}
			if (!Passes<const URingData *>(Context.OwnsRing, RingEntry.RingEntry.Ring))
			{
				Sink.Add(ELoadoutSlotType::ResonatorRing, i, true, TEXT("Adapter in loadout not in inventory"));
			}
			CheckRingSpellCoherence(Sink, RingEntry.RingEntry, i);
			CheckRingSpellCap(Sink, RingEntry, i);
		}
	}

	// D-110 (B-303): every class's budget and limits, from one table over the unified slots.
	Sink.Findings.Append(CollectClassLimitFindings(Loadout, Context.CharacterClass));

	if (Context.CharacterClass == ECharacterClass::Caster)
	{
		CheckCasterSpells(Sink, Loadout, Context);
	}

	CheckItems(Sink, Loadout, Context);
	return MoveTemp(Sink.Findings);
}

int32 FLoadoutRules::AttachmentCost(const FRuntimeAttachedItem &Attached)
{
	switch (Attached.Kind)
	{
	case EAttachedItemKind::None:
		return 0;
	case EAttachedItemKind::Evolution:
		return LoadoutConstants::LOADOUT_EVOLUTION_COST;
	case EAttachedItemKind::AugmentStone:
		return LoadoutConstants::LOADOUT_STONE_COST;
	case EAttachedItemKind::Crystal:
	case EAttachedItemKind::Fusion:
	default:
		return LoadoutConstants::LOADOUT_CRYSTAL_COST;
	}
}

int32 FLoadoutRules::SlotCost(const FLoadoutSlot &Slot)
{
	if (Slot.IsEmpty())
	{
		return 0;
	}
	// D-206: the gear itself is free (LOADOUT_GEAR_COST); each socket's contents cost.
	return LoadoutConstants::LOADOUT_GEAR_COST + AttachmentCost(Slot.Gear.AttachedItem) + AttachmentCost(Slot.Gear.StoneSocket);
}

int32 FLoadoutRules::NonAdapterCost(const FCombatLoadout &Loadout, ECharacterClass Class)
{
	FCombatLoadout Mirror = Loadout;
	Mirror.RebuildSlotsFromLegacy(Class);
	int32 Cost = Mirror.PrimarySlotType == EPrimarySlotType::Evolution ? LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST : 0;
	for (const FLoadoutSlot &Slot : Mirror.Slots)
	{
		Cost += Slot.Kind() == EGearKind::Weapon ? SlotCost(Slot) : 0;
	}
	return Cost;
}

TArray<FInvalidSlotFinding> FLoadoutRules::CollectClassLimitFindings(const FCombatLoadout &Loadout, ECharacterClass Class)
{
	FCombatLoadout Mirror = Loadout;
	Mirror.RebuildSlotsFromLegacy(Class);
	const FLoadoutClassLimits Limits = LoadoutClassTable::Get(Class);

	const bool bEvolutionSeat = Mirror.PrimarySlotType == EPrimarySlotType::Evolution;
	int32 GearCost = bEvolutionSeat ? LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST : 0;
	int32 AdapterCost = 0;
	int32 Weapons = 0;
	int32 Adapters = 0;
	for (const FLoadoutSlot &Slot : Mirror.Slots)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}
		if (Slot.Kind() == EGearKind::Adapter)
		{
			++Adapters;
			AdapterCost += SlotCost(Slot);
		}
		else
		{
			++Weapons;
			GearCost += SlotCost(Slot);
		}
	}

	TArray<FInvalidSlotFinding> Findings;
	const auto Add = [&Findings](ELoadoutSlotType Type, bool bClearable, const FString &Reason)
	{
		FInvalidSlotFinding &Finding = Findings.AddDefaulted_GetRef();
		Finding.SlotType = Type;
		Finding.SlotIndex = -1;
		Finding.bClearable = bClearable;
		Finding.Reason = Reason;
	};

	// The Resonator's budget finding keeps its shape and words (ClearInvalidSlots drops adapters for it). A Generic's
	// is cleared by dropping the secondary weapon (a saved loadout from before D-206 could carry more). B-472 (D-206):
	// a Refractor's one item can go over 2 only with a stone beside an evolution, so its finding drops that stone.
	const bool bResonator = Class == ECharacterClass::Resonator;
	const bool bGenericSecondary = Class == ECharacterClass::Generic && Mirror.SecondarySlotType == ESecondarySlotType::Weapon &&
								   Mirror.SecondaryWeapon.IsValid();
	const bool bRefractorStone = Class == ECharacterClass::Caster && Mirror.Slots.ContainsByPredicate([](const FLoadoutSlot &Slot)
																									 { return !Slot.IsEmpty() && !Slot.Gear.StoneSocket.IsEmpty(); });
	const int32 TotalCost = GearCost + AdapterCost;
	if (TotalCost > Limits.Budget)
	{
		const ELoadoutSlotType Clears = bResonator		   ? ELoadoutSlotType::RingOverflow
										: bGenericSecondary ? ELoadoutSlotType::SecondaryWeapon
										: bRefractorStone	? ELoadoutSlotType::PrimaryStone
															: ELoadoutSlotType::None;
		Add(Clears, Clears != ELoadoutSlotType::None,
			FString::Printf(TEXT("Loadout exceeds budget: primary (cost %d) + adapters (cost %d) = %d, max is %d"), GearCost,
							AdapterCost, TotalCost, Limits.Budget));
	}
	const int32 Items = Weapons + Adapters + (bEvolutionSeat ? 1 : 0);
	if (Limits.MaxItems != LoadoutClassTable::NO_LIMIT && Items > Limits.MaxItems)
	{
		Add(ELoadoutSlotType::None, false, FString::Printf(TEXT("Too many items for the class (%d/%d)"), Items, Limits.MaxItems));
	}
	if (Limits.MaxWeapons != LoadoutClassTable::NO_LIMIT && Weapons > Limits.MaxWeapons)
	{
		Add(ELoadoutSlotType::None, false, FString::Printf(TEXT("Too many weapons for the class (%d/%d)"), Weapons, Limits.MaxWeapons));
	}
	if (Limits.MaxAdapters != LoadoutClassTable::NO_LIMIT && Adapters > Limits.MaxAdapters)
	{
		// B-472 (D-206): a Resonator's extra adapters are dropped like its over-budget ones (RingOverflow; one clear
		// drops both, so the count finding is clearable only when the budget one isn't already there).
		const bool bBudgetClears = bResonator && TotalCost > Limits.Budget;
		Add(bResonator ? ELoadoutSlotType::RingOverflow : ELoadoutSlotType::None, bResonator && !bBudgetClears,
			FString::Printf(TEXT("Too many adapters for the class (%d/%d)"), Adapters, Limits.MaxAdapters));
	}
	return Findings;
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
			Errors.Add(TEXT("Resonator cannot use an Adapter as primary slot"));
		}
		else if (!Saved.PrimaryRing)
		{
			Errors.Add(TEXT("Primary slot set to Adapter but no adapter assigned"));
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

int32 FLoadoutRules::CountEquippedSTierCrystals(const FCombatLoadout &Loadout, ECharacterClass Class, bool bCountItemSlots)
{
	int32 Count = CountAttachedSTier(Loadout, Class);
	for (const FItemLoadoutSlot &Slot : Loadout.ItemSlots)
	{
		Count += bCountItemSlots && IsSTierItemGem(Slot) ? Slot.Quantity : 0;
	}
	return Count;
}

void FLoadoutRules::TrimSTierItemCrystals(FCombatLoadout &Loadout, ECharacterClass Class)
{
	int32 Remaining = FMath::Max(0, LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS - CountAttachedSTier(Loadout, Class));
	for (FItemLoadoutSlot &Slot : Loadout.ItemSlots)
	{
		if (!IsSTierItemGem(Slot))
		{
			continue;
		}
		const int32 Kept = FMath::Min(Slot.Quantity, Remaining);
		Remaining -= Kept;
		Slot.Quantity = Kept;
		if (Kept == 0)
		{
			Slot = FItemLoadoutSlot();
		}
	}
}

void FLoadoutRules::TrimUtilityCrystalsToTierCaps(FCombatLoadout &Loadout)
{
	for (const FUtilityCrystalBand &Band : UtilityCrystalBands)
	{
		int32 Remaining = Band.Cap;
		for (FItemLoadoutSlot &Slot : Loadout.ItemSlots)
		{
			if (!CountsTowardBand(Slot, Band))
			{
				continue;
			}
			const int32 Kept = FMath::Min(Slot.Quantity, Remaining);
			Remaining -= Kept;
			Slot.Quantity = Kept;
			if (Kept == 0)
			{
				Slot = FItemLoadoutSlot();
			}
		}
	}
}

int32 FLoadoutRules::GetUtilityBandIndex(const FCrystalId &Id)
{
	if (!CrystalTypeHelpers::IsGemType(Id.Type))
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < UTILITY_BAND_COUNT; ++i)
	{
		if (Id.Tier >= UtilityCrystalBands[i].LowTier && Id.Tier <= UtilityCrystalBands[i].HighTier)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 FLoadoutRules::GetUtilityBandCap(int32 BandIndex)
{
	return BandIndex >= 0 && BandIndex < UTILITY_BAND_COUNT ? UtilityCrystalBands[BandIndex].Cap : 0;
}
