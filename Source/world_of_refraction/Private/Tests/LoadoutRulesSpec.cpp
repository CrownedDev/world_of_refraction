// LoadoutRulesSpec.cpp
// One loadout rule set (B-028, D-073 = A): the editor check now gives the fight's answers, BD rules
// follow the character, the cleaner-only checks are reported, and unset context checks pass.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WeaponSpellRules.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/SpellPoolConstants.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Rings/RingData.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Character/CharacterData.h"
#include "Inventory/InventoryData.h"

namespace
{
	/** S-tier innate spells: over the innate budget at full cost, under it with the full discount. */
	constexpr int32 LOADOUT_RULES_TEST_HEAVY_INNATE_SPELLS = 4;

	/** S-tier spells in each of a BD character's two pools: together over the BD budget. */
	constexpr int32 LOADOUT_RULES_TEST_HEAVY_BD_POOL_SPELLS = 6;

	/** An F-tier evolution holds 1 spell in the fight; the old editor check allowed 6. */
	constexpr int32 LOADOUT_RULES_TEST_EVOLUTION_SPELLS = 3;

	bool AnyReasonContains(const TArray<FString> &Reasons, const TCHAR *Text)
	{
		return Reasons.ContainsByPredicate([Text](const FString &Reason) { return Reason.Contains(Text); });
	}

	USpellData *NewSpell(ESpellElement Element)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Element = Element;
		Spell->Name = TEXT("Test Spell");
		return Spell;
	}

	FCombatLoadout LoadoutWithItems(std::initializer_list<FItemLoadoutSlot> Slots)
	{
		FCombatLoadout Loadout;
		Loadout.ItemSlots.Append(Slots.begin(), Slots.size());
		return Loadout;
	}

	FItemLoadoutSlot ItemSlot(ECrystalType Type, EItemTier Tier, int32 Quantity)
	{
		FItemLoadoutSlot Slot;
		Slot.CrystalId = FCrystalId(Type, Tier);
		Slot.Quantity = Quantity;
		return Slot;
	}

	/** B-354: an equipped stack big enough to have been over D-072's A/B band (2). */
	constexpr int32 SEVERAL_A_CRYSTALS = 3;

	FLoadoutRuleContext ContextFor(ECharacterClass Class)
	{
		FLoadoutRuleContext Context;
		Context.CharacterClass = Class;
		return Context;
	}

	int32 CountSTierFindings(const FCombatLoadout &Loadout, ECharacterClass Class = ECharacterClass::Generic)
	{
		int32 Count = 0;
		for (const FInvalidSlotFinding &Finding : FLoadoutRules::CollectFindings(Loadout, ContextFor(Class)))
		{
			Count += Finding.SlotType == ELoadoutSlotType::EquippedSTierCrystal ? 1 : 0;
		}
		return Count;
	}

	int32 CountTierCapFindings(const FCombatLoadout &Loadout)
	{
		int32 Count = 0;
		for (const FInvalidSlotFinding &Finding : FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext()))
		{
			Count += Finding.SlotType == ELoadoutSlotType::UtilityCrystalTierCap ? 1 : 0;
		}
		return Count;
	}

	FCombatLoadout SwordLoadoutWithAbility(EWeaponType AbilityWeaponType)
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->WeaponType = EWeaponType::Sword;
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Test Ability");
		Ability->RequiredWeaponType = AbilityWeaponType;
		Ability->RequiredWieldMode = Sword->WieldMode;

		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Loadout.PrimaryWeapon.AssignedAbilities.Add(Ability);
		return Loadout;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutRulesSpec, "WoR.Loadout.Rules",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutRulesSpec)

void FLoadoutRulesSpec::Define()
{
	Describe("the editor check", [this]()
	{
		It("caps evolution spells by the evolution's tier, as the fight does", [this]()
		{
			UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
			Evolution->Tier = EItemTier::F_Tier;
			FSavedLoadout Saved;
			Saved.RequiredClass = ECharacterClass::Generic;
			Saved.PrimarySlotType = EPrimarySlotType::Evolution;
			Saved.PrimaryEvolution = Evolution;
			for (int32 i = 0; i < LOADOUT_RULES_TEST_EVOLUTION_SPELLS; ++i)
			{
				Saved.EvolutionSpells.Add(FSpellRef(NewSpell(ESpellElement::Fire)));
			}

			TestTrue(TEXT("now flagged"), AnyReasonContains(Saved.GetValidationErrors(), TEXT("Too many evolution spells")));
			TestFalse(TEXT("the old check passed it"), AnyReasonContains(Saved.GetValidationErrorsLegacy(), TEXT("Too many evolution spells")));
		});

		It("lets an evolution carry exactly its tier's spell count (D-039)", [this]()
		{
			UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
			Evolution->Tier = EItemTier::C_Tier;
			FSavedLoadout Saved;
			Saved.RequiredClass = ECharacterClass::Generic;
			Saved.PrimarySlotType = EPrimarySlotType::Evolution;
			Saved.PrimaryEvolution = Evolution;
			const int32 TierCount = CrystalEffectTable::SlotsForContainerTier(Evolution->Tier);
			for (int32 i = 0; i < TierCount; ++i)
			{
				Saved.EvolutionSpells.Add(FSpellRef(NewSpell(ESpellElement::Fire)));
			}
			TestFalse(TEXT("at the count: passes"), AnyReasonContains(Saved.GetValidationErrors(), TEXT("Too many evolution spells")));
			Saved.EvolutionSpells.Add(FSpellRef(NewSpell(ESpellElement::Fire)));
			TestTrue(TEXT("one over: refused"), AnyReasonContains(Saved.GetValidationErrors(), TEXT("Too many evolution spells")));
		});

		It("caps an evolution attached to a weapon or ring by its own tier, not the holder's (Q-147 = B)", [this]()
		{
			FRuntimeAttachedItem Attached;
			Attached.Kind = EAttachedItemKind::Evolution;
			Attached.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
			for (const EItemTier Tier : {EItemTier::F_Tier, EItemTier::S_Tier})
			{
				Attached.Evolution.Tier = Tier;
				TestEqual(TEXT("weapon: the evolution's tier count"),
						  CrystalEffectTable::ResolveSpellSlotCap(Attached, LoadoutConstants::MAX_SPELL_SLOTS),
						  CrystalEffectTable::SlotsForContainerTier(Tier));
				TestEqual(TEXT("ring: the evolution's tier count"),
						  CrystalEffectTable::ResolveSpellSlotCap(Attached, LoadoutConstants::MAX_RING_SPELLS),
						  CrystalEffectTable::SlotsForContainerTier(Tier));
			}
			TestTrue(TEXT("an F evolution holds fewer than the weapon ceiling"),
					 CrystalEffectTable::SlotsForContainerTier(EItemTier::F_Tier) < LoadoutConstants::MAX_SPELL_SLOTS);
		});

		It("lists only an F evolution's spell count on an S weapon and an S ring (Q-147 = B)", [this]()
		{
			FRuntimeAttachedItem Attached;
			Attached.Kind = EAttachedItemKind::Evolution;
			Attached.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
			Attached.Evolution.Tier = EItemTier::F_Tier;
			Attached.Evolution.CurrentDurability = 1; // intact: a broken one offers no spells (D-083)
			const int32 FCount = CrystalEffectTable::SlotsForContainerTier(EItemTier::F_Tier);
			TestTrue(TEXT("the S holder alone would hold more"), CrystalEffectTable::SlotsForContainerTier(EItemTier::S_Tier) > FCount);

			FWeaponLoadoutEntry Weapon;
			Weapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Weapon.WeaponEntry.Tier = EItemTier::S_Tier;
			Weapon.WeaponEntry.AttachedItem = Attached;
			FRingLoadoutEntry Ring;
			Ring.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Ring.RingEntry.Tier = EItemTier::S_Tier;
			Ring.RingEntry.AttachedItem = Attached;
			for (int32 i = 0; i < LoadoutConstants::MAX_SPELL_SLOTS; ++i)
			{
				Weapon.AssignedSpells.Add(NewSpell(ESpellElement::Fire));
				Ring.RingEntry.AssignedSpells.Add(NewSpell(ESpellElement::Fire));
			}

			WeaponSpellRules::FScopedWeaponSpells OldWeaponSpells(true); // B-098: the old weapon-spell path, until the sweep
			TestEqual(FString::Printf(TEXT("weapon lists the evolution's count: got %d"), Weapon.GetAllSpells().Num()), Weapon.GetAllSpells().Num(), FCount);
			TestEqual(FString::Printf(TEXT("ring lists the evolution's count: got %d"), Ring.GetAllSpells().Num()), Ring.GetAllSpells().Num(), FCount);
			TestEqual(FString::Printf(TEXT("ring offers the evolution's count to fill: got %d"), Ring.GetCustomizableSpellCount()), Ring.GetCustomizableSpellCount(), FCount);
		});

		It("uses BD rules when the character is BD, whatever pools are authored", [this]()
		{
			FSavedLoadout Saved;
			Saved.RequiredClass = ECharacterClass::Caster;
			Saved.InnateSpells.Add(FSpellRef(NewSpell(ESpellElement::Fire)));

			TestTrue(TEXT("BD character: a Fire spell in the Darkness pool is flagged"), Saved.GetValidationErrors(true).Num() > 0);
			TestEqual(TEXT("ordinary character: fine"), Saved.GetValidationErrors(false).Num(), 0);
		});

		It("weighs spells with the owning character's authored discount (D-080 = D)", [this]()
		{
			FSavedLoadout Saved;
			Saved.RequiredClass = ECharacterClass::Caster;
			for (int32 i = 0; i < LOADOUT_RULES_TEST_HEAVY_INNATE_SPELLS; ++i)
			{
				USpellData *Spell = NewSpell(ESpellElement::Fire);
				Spell->Tier = EItemTier::S_Tier;
				Saved.InnateSpells.Add(FSpellRef(Spell));
			}
			UCharacterData *Master = NewObject<UCharacterData>(GetTransientPackage());
			Master->WorldMindLevel = SpellPoolConstants::SPELL_DISCOUNT_MIND_THRESHOLD;
			Master->WorldBodyLevel = SpellPoolConstants::SPELL_DISCOUNT_BODY_THRESHOLD;
			Master->WorldSpiritLevel = SpellPoolConstants::SPELL_DISCOUNT_SPIRIT_THRESHOLD;

			TestTrue(TEXT("no character: over the budget"), AnyReasonContains(Saved.GetValidationErrors(false), TEXT("Innate spell budget exceeded")));
			TestFalse(TEXT("a high-stat owner: fits"), AnyReasonContains(Saved.GetValidationErrors(false, Master->GetAuthoredSpellDiscount()), TEXT("Innate spell budget exceeded")));
		});

		It("warns, not errors, about the spell budget for an inventory on its own (Q-161 = D)", [this]()
		{
			UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
			FSavedLoadout Heavy;
			Heavy.RequiredClass = ECharacterClass::Caster;
			for (int32 i = 0; i < LOADOUT_RULES_TEST_HEAVY_INNATE_SPELLS; ++i)
			{
				USpellData *Spell = NewSpell(ESpellElement::Fire);
				Spell->Tier = EItemTier::S_Tier;
				Heavy.InnateSpells.Add(FSpellRef(Spell));
				Inventory->Spells.Add(Spell);
			}
			FSavedLoadout Broken;
			Broken.RequiredClass = ECharacterClass::Generic;
			Broken.PrimarySlotType = EPrimarySlotType::Weapon;
			Inventory->SavedLoadouts = {Heavy, Broken};

			TArray<FString> Errors;
			TArray<FString> Warnings;
			Inventory->GetStandaloneValidation(Errors, Warnings);
			TestTrue(TEXT("budget is a warning"), AnyReasonContains(Warnings, TEXT("Innate spell budget exceeded")));
			TestFalse(TEXT("budget is no error"), AnyReasonContains(Errors, TEXT("Innate spell budget exceeded")));
			TestTrue(TEXT("other problems stay errors"), AnyReasonContains(Errors, TEXT("no weapon assigned")));
			TestFalse(TEXT("and are not repeated as warnings"), AnyReasonContains(Warnings, TEXT("no weapon assigned")));
		});

		It("warns, not errors, about the Broken Darkness spell budget on its own too", [this]()
		{
			UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
			FSavedLoadout Heavy;
			Heavy.RequiredClass = ECharacterClass::Caster;
			FSavedBDElementSpellPool FirePool;
			FirePool.Element = ESpellElement::Fire;
			for (int32 i = 0; i < LOADOUT_RULES_TEST_HEAVY_BD_POOL_SPELLS; ++i)
			{
				USpellData *Dark = NewSpell(ESpellElement::Darkness);
				USpellData *Fire = NewSpell(ESpellElement::Fire);
				Dark->Tier = EItemTier::S_Tier;
				Fire->Tier = EItemTier::S_Tier;
				Heavy.InnateSpells.Add(FSpellRef(Dark));
				FirePool.Spells.Add(FSpellRef(Fire));
				Inventory->Spells.Append({Dark, Fire});
			}
			Heavy.BDSpellPools = {FirePool};
			Inventory->SavedLoadouts = {Heavy};

			TArray<FString> Errors;
			TArray<FString> Warnings;
			Inventory->GetStandaloneValidation(Errors, Warnings);
			TestTrue(TEXT("BD budget is a warning"), AnyReasonContains(Warnings, TEXT("Broken Darkness spell budget exceeded")));
			TestFalse(TEXT("BD budget is no error"), AnyReasonContains(Errors, TEXT("Broken Darkness spell budget exceeded")));
		});

		It("still reports what only an authored loadout can get wrong", [this]()
		{
			FSavedLoadout Saved;
			Saved.RequiredClass = ECharacterClass::Generic;
			Saved.PrimarySlotType = EPrimarySlotType::Weapon;
			TestTrue(TEXT("weapon slot with no weapon"), AnyReasonContains(Saved.GetValidationErrors(), TEXT("no weapon assigned")));
		});
	});

	Describe("the shared rules", [this]()
	{
		It("report an ability for the wrong weapon type (once a cleaner-only check)", [this]()
		{
			const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectFindings(SwordLoadoutWithAbility(EWeaponType::Spear), FLoadoutRuleContext());
			TestEqual(TEXT("one finding"), Findings.Num(), 1);
			if (Findings.Num() == 1)
			{
				TestEqual(TEXT("on the weapon ability"), Findings[0].SlotType, ELoadoutSlotType::WeaponAbility);
				TestTrue(TEXT("clearable"), Findings[0].bClearable);
			}
		});

		It("pass a coherent loadout, and check ownership only when the context can", [this]()
		{
			const FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			TestEqual(TEXT("no context checks: clean"), FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext()).Num(), 0);

			FLoadoutRuleContext Owning;
			Owning.OwnsWeapon = [](const UWeaponData *) { return false; };
			Owning.KnowsAbility = [](const UAbilityData *) { return false; };
			const TArray<FString> Reasons = FLoadoutRules::ToReasons(FLoadoutRules::CollectFindings(Loadout, Owning));
			TestTrue(TEXT("weapon not owned"), AnyReasonContains(Reasons, TEXT("Primary weapon not in inventory")));
			TestTrue(TEXT("ability not learned"), AnyReasonContains(Reasons, TEXT("not learned")));
		});
	});

	Describe("the spell cap (B-196)", [this]()
	{
		// An S holder with an intact F evolution: the fight keeps the F count and drops the rest.
		auto FEvolution = []()
		{
			FRuntimeAttachedItem Attached;
			Attached.Kind = EAttachedItemKind::Evolution;
			Attached.Evolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
			Attached.Evolution.Tier = EItemTier::F_Tier;
			Attached.Evolution.CurrentDurability = 1;
			return Attached;
		};
		auto CountOverCap = [](const FCombatLoadout &Loadout, ELoadoutSlotType Type)
		{
			int32 Count = 0;
			for (const FInvalidSlotFinding &Finding : FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext()))
			{
				Count += Finding.SlotType == Type && Finding.bClearable && Finding.Reason.Contains(TEXT("over the holder's spell cap")) ? 1 : 0;
			}
			return Count;
		};
		const int32 FCount = CrystalEffectTable::SlotsForContainerTier(EItemTier::F_Tier);

		It("reports each weapon spell over the cap, none at the cap", [this, FEvolution, CountOverCap, FCount]()
		{
			FCombatLoadout Loadout;
			Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
			Loadout.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Loadout.PrimaryWeapon.WeaponEntry.Tier = EItemTier::S_Tier;
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem = FEvolution();
			for (int32 i = 0; i < FCount; ++i)
			{
				Loadout.PrimaryWeapon.AssignedSpells.Add(NewSpell(ESpellElement::Fire));
			}
			TestEqual(TEXT("at the cap: no finding"), CountOverCap(Loadout, ELoadoutSlotType::WeaponSpell), 0);
			for (int32 i = FCount; i < LoadoutConstants::MAX_SPELL_SLOTS; ++i)
			{
				Loadout.PrimaryWeapon.AssignedSpells.Add(NewSpell(ESpellElement::Fire));
			}
			TestEqual(TEXT("one finding per spell the fight drops"), CountOverCap(Loadout, ELoadoutSlotType::WeaponSpell),
					  LoadoutConstants::MAX_SPELL_SLOTS - FCount);
		});

		It("doesn't flag a loadout spell that only replaces one of the item's own (the fight keeps it)", [this, FEvolution, CountOverCap, FCount]()
		{
			FCombatLoadout Loadout;
			Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
			FWeaponInventoryEntry &Entry = Loadout.PrimaryWeapon.WeaponEntry;
			Entry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Entry.Tier = EItemTier::S_Tier;
			Entry.AttachedItem = FEvolution();
			for (int32 i = 0; i <= FCount; ++i)
			{
				Entry.AssignedSpells.Add(NewSpell(ESpellElement::Fire)); // the item's own: one over the cap
			}
			Loadout.PrimaryWeapon.AssignedSpells.Add(NewSpell(ESpellElement::Fire)); // replaces the first
			TestEqual(TEXT("no clearable finding"), CountOverCap(Loadout, ELoadoutSlotType::WeaponSpell), 0);
			const TArray<FString> Reasons = FLoadoutRules::ToReasons(FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext()));
			TestTrue(TEXT("the item's own spells are reported"), AnyReasonContains(Reasons, TEXT("of the item's own spells")));
		});

		It("reports each primary ring spell over the cap", [this, FEvolution, CountOverCap, FCount]()
		{
			FCombatLoadout Loadout;
			Loadout.PrimarySlotType = EPrimarySlotType::Ring;
			Loadout.PrimaryRing.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Loadout.PrimaryRing.RingEntry.Tier = EItemTier::S_Tier;
			Loadout.PrimaryRing.RingEntry.AttachedItem = FEvolution();
			for (int32 i = 0; i < LoadoutConstants::MAX_RING_SPELLS; ++i)
			{
				Loadout.PrimaryRing.RingEntry.AssignedSpells.Add(NewSpell(ESpellElement::Fire));
			}
			TestEqual(TEXT("one finding per spell the fight drops"), CountOverCap(Loadout, ELoadoutSlotType::RingSpell),
					  LoadoutConstants::MAX_RING_SPELLS - FCount);
		});
	});

	Describe("one S-tier crystal per battle loadout (B-354, D-143)", [this]()
	{
		It("allows one S crystal and several A crystals", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			Loadout.ItemSlots.Add(ItemSlot(ECrystalType::Sapphire, EItemTier::A_Tier, SEVERAL_A_CRYSTALS));
			Loadout.ItemSlots.Add(ItemSlot(ECrystalType::Citrine, EItemTier::A_Tier, SEVERAL_A_CRYSTALS));
			TestEqual(TEXT("one S equipped"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS);
			TestEqual(TEXT("no S finding"), CountSTierFindings(Loadout), 0);
			TestEqual(TEXT("the D-072 bands are gone"), CountTierCapFindings(Loadout), 0);
		});

		It("refuses a second S crystal, and clears one from the item slots", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			Loadout.ItemSlots.Add(ItemSlot(ECrystalType::Sapphire, EItemTier::S_Tier, 1));
			TestEqual(TEXT("a second S is reported"), CountSTierFindings(Loadout), 1);
			FLoadoutRules::TrimSTierItemCrystals(Loadout, ECharacterClass::Generic);
			TestEqual(TEXT("the item slot's S gem is cleared"), CountSTierFindings(Loadout), 0);
			TestTrue(TEXT("the attached S crystal stays"),
					 Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id.Tier == EItemTier::S_Tier);
		});

		It("counts S attachments on active gear (augment stone, fusion half), not S stones in item slots", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::AugmentStone;
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::DamageStone, EItemTier::S_Tier);
			Loadout.ItemSlots.Add(ItemSlot(ECrystalType::DamageStone, EItemTier::S_Tier, SEVERAL_A_CRYSTALS));
			TestEqual(TEXT("an attached S stone counts; S stones in item slots don't"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 1);

			Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
			Loadout.SecondaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Fusion;
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Fusion.Id.HalfA = FCrystalId(ECrystalType::Garnet, EItemTier::A_Tier);
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Fusion.Id.HalfB = FCrystalId(ECrystalType::Sapphire, EItemTier::S_Tier);
			TestEqual(TEXT("a fusion with an S half counts"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 2);
			const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext());
			const FInvalidSlotFinding *Found = Findings.FindByPredicate([](const FInvalidSlotFinding &F) { return F.SlotType == ELoadoutSlotType::EquippedSTierCrystal; });
			if (!TestNotNull(TEXT("two S attachments are reported"), Found)) return;
			TestFalse(TEXT("not clearable: no S gem in the item slots"), Found->bClearable);
		});

		It("counts an S stone in the stone socket beside an S gem (B-351, D-141)", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			Loadout.PrimaryWeapon.WeaponEntry.StoneSocket.Kind = EAttachedItemKind::AugmentStone;
			Loadout.PrimaryWeapon.WeaponEntry.StoneSocket.Crystal.Id = FCrystalId(ECrystalType::DamageStone, EItemTier::S_Tier);
			TestEqual(TEXT("an S gem and an S stone on one weapon are two"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 2);
			TestEqual(TEXT("two S: reported"), CountSTierFindings(Loadout), 1);
			Loadout.PrimaryWeapon.WeaponEntry.StoneSocket.Crystal.Id = FCrystalId(ECrystalType::DamageStone, EItemTier::A_Tier);
			TestEqual(TEXT("an A stone doesn't count"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 1);
		});

		It("counts the evolution seat and an evolution on the secondary weapon", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.PrimarySlotType = EPrimarySlotType::Evolution;
			Loadout.PrimaryEvolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
			Loadout.PrimaryEvolution.Tier = EItemTier::S_Tier;
			TestEqual(TEXT("the S evolution seat counts (the stale primary weapon doesn't)"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 1);
			Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
			Loadout.SecondaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Evolution;
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Evolution.Tier = EItemTier::S_Tier;
			TestEqual(TEXT("an S evolution on gear counts too"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 2);
			TestEqual(TEXT("two S: reported"), CountSTierFindings(Loadout), 1);
		});

		It("doesn't count a stale entry in an inactive slot, or the primary ring twice", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.SecondarySlotType = ESecondarySlotType::None;
			Loadout.SecondaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			TestEqual(TEXT("an inactive secondary doesn't count"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 0);

			FRingLoadoutEntry Adapter;
			Adapter.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Adapter.RingEntry.PersistentID = FGuid::NewGuid();
			Adapter.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Adapter.RingEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Sapphire, EItemTier::S_Tier);
			Loadout.PrimarySlotType = EPrimarySlotType::Ring;
			Loadout.PrimaryRing = Adapter;
			Loadout.RingLoadout = {Adapter};
			TestEqual(TEXT("the primary ring in the ring loadout counts once"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Resonator), 1);
		});

		It("ignores a stale S on a slot the class doesn't fight with (a Caster's secondary, a Generic's ring loadout)", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
			Loadout.SecondaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			TestEqual(TEXT("a Generic fights with its secondary"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 1);
			TestEqual(TEXT("a Caster doesn't"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Caster), 0);

			FRingLoadoutEntry Adapter;
			Adapter.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Adapter.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Adapter.RingEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Sapphire, EItemTier::S_Tier);
			Loadout.RingLoadout = {Adapter};
			Loadout.ItemSlots.Add(ItemSlot(ECrystalType::Citrine, EItemTier::S_Tier, 1));
			TestEqual(TEXT("a Generic: the secondary and the gem, not the ring loadout"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), 2);
			TestEqual(TEXT("a Resonator: the ring loadout and the gem, not the secondary"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Resonator), 2);
			TestEqual(TEXT("a Caster: the gem alone, no finding"), CountSTierFindings(Loadout, ECharacterClass::Caster), 0);
			TestEqual(TEXT("a Generic: two S, reported"), CountSTierFindings(Loadout, ECharacterClass::Generic), 1);

			FLoadoutRules::TrimSTierItemCrystals(Loadout, ECharacterClass::Caster);
			TestEqual(TEXT("a Caster's stale secondary leaves the gem its room"), Loadout.ItemSlots[0].Quantity, 1);
			FLoadoutRules::TrimSTierItemCrystals(Loadout, ECharacterClass::Generic);
			TestEqual(TEXT("a Generic's secondary takes it"), Loadout.ItemSlots[0].Quantity, 0);
		});

		It("trims every S gem from the item slots when attachments already use the cap", [this]()
		{
			FCombatLoadout Loadout = SwordLoadoutWithAbility(EWeaponType::Sword);
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			Loadout.ItemSlots.Add(ItemSlot(ECrystalType::Sapphire, EItemTier::S_Tier, SEVERAL_A_CRYSTALS));
			FLoadoutRules::TrimSTierItemCrystals(Loadout, ECharacterClass::Generic);
			TestEqual(TEXT("no S gem left in the item slots"), Loadout.ItemSlots[0].Quantity, 0);
			TestEqual(TEXT("within the cap"), FLoadoutRules::CountEquippedSTierCrystals(Loadout, ECharacterClass::Generic), LoadoutConstants::MAX_EQUIPPED_S_TIER_CRYSTALS);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
