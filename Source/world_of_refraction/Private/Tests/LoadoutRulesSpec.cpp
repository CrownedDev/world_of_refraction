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

namespace
{
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

		It("uses BD rules when the character is BD, whatever pools are authored", [this]()
		{
			FSavedLoadout Saved;
			Saved.RequiredClass = ECharacterClass::Caster;
			Saved.InnateSpells.Add(FSpellRef(NewSpell(ESpellElement::Fire)));

			TestTrue(TEXT("BD character: a Fire spell in the Darkness pool is flagged"), Saved.GetValidationErrors(true).Num() > 0);
			TestEqual(TEXT("ordinary character: fine"), Saved.GetValidationErrors(false).Num(), 0);
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
