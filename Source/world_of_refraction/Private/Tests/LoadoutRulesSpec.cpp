// LoadoutRulesSpec.cpp
// One loadout rule set (B-028, D-073 = A): the editor check now gives the fight's answers, BD rules
// follow the character, the cleaner-only checks are reported, and unset context checks pass.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/SpellPoolConstants.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
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

	Describe("utility crystal tier caps (D-072, D-077)", [this]()
	{
		It("allow each band at its cap", [this]()
		{
			const FCombatLoadout AtCaps = LoadoutWithItems({
				ItemSlot(ECrystalType::Garnet, EItemTier::S_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER),
				ItemSlot(ECrystalType::Sapphire, EItemTier::A_Tier, 1),
				ItemSlot(ECrystalType::Citrine, EItemTier::B_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_A_B_TIER - 1),
				ItemSlot(ECrystalType::Emerald, EItemTier::C_Tier, 1),
				ItemSlot(ECrystalType::Amber, EItemTier::D_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_C_D_TIER - 1),
				ItemSlot(ECrystalType::Opal, EItemTier::E_Tier, 1),
				ItemSlot(ECrystalType::Onyx, EItemTier::F_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_E_F_TIER - 1),
			});
			TestEqual(TEXT("no cap findings"), CountTierCapFindings(AtCaps), 0);
		});

		It("report a band one over its cap, counting each crystal in a stack and combining the band", [this]()
		{
			TestEqual(TEXT("S stack of 2"), CountTierCapFindings(LoadoutWithItems({
				ItemSlot(ECrystalType::Garnet, EItemTier::S_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER + 1)})), 1);
			TestEqual(TEXT("A + B over 2"), CountTierCapFindings(LoadoutWithItems({
				ItemSlot(ECrystalType::Sapphire, EItemTier::A_Tier, 1),
				ItemSlot(ECrystalType::Citrine, EItemTier::B_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_A_B_TIER)})), 1);
			TestEqual(TEXT("C + D over 3"), CountTierCapFindings(LoadoutWithItems({
				ItemSlot(ECrystalType::Emerald, EItemTier::C_Tier, 2),
				ItemSlot(ECrystalType::Amber, EItemTier::D_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_C_D_TIER - 1)})), 1);
			TestEqual(TEXT("E + F over 4"), CountTierCapFindings(LoadoutWithItems({
				ItemSlot(ECrystalType::Opal, EItemTier::E_Tier, 2),
				ItemSlot(ECrystalType::Onyx, EItemTier::F_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_E_F_TIER - 1)})), 1);
		});

		It("trim the last slots in an over-cap band down to the cap", [this]()
		{
			FCombatLoadout Loadout = LoadoutWithItems({
				ItemSlot(ECrystalType::Sapphire, EItemTier::A_Tier, 1),
				ItemSlot(ECrystalType::Opal, EItemTier::S_Tier, 1),
				ItemSlot(ECrystalType::Citrine, EItemTier::B_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_A_B_TIER),
			});
			FLoadoutRules::TrimUtilityCrystalsToTierCaps(Loadout);
			TestEqual(TEXT("within the caps now"), CountTierCapFindings(Loadout), 0);
			TestEqual(TEXT("first A/B slot kept"), Loadout.ItemSlots[0].Quantity, 1);
			TestEqual(TEXT("S slot untouched"), Loadout.ItemSlots[1].Quantity, 1);
			TestEqual(TEXT("last A/B slot trimmed"), Loadout.ItemSlots[2].Quantity, LoadoutConstants::MAX_UTILITY_CRYSTALS_A_B_TIER - 1);
		});

		It("ignore utility stones and attached crystals", [this]()
		{
			TestEqual(TEXT("S-tier stones aren't capped"), CountTierCapFindings(LoadoutWithItems({
				ItemSlot(ECrystalType::DamageStone, EItemTier::S_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER + 1)})), 0);

			FCombatLoadout Attached = SwordLoadoutWithAbility(EWeaponType::Sword);
			Attached.PrimaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
			Attached.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier);
			Attached.ItemSlots.Add(ItemSlot(ECrystalType::Sapphire, EItemTier::S_Tier, LoadoutConstants::MAX_UTILITY_CRYSTALS_S_TIER));
			TestEqual(TEXT("an attached S crystal doesn't use the item cap"), CountTierCapFindings(Attached), 0);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
