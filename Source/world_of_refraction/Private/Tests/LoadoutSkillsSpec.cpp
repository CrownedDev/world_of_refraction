// LoadoutSkillsSpec.cpp
// B-440d: the loadout screen's Skills page model. A Refractor's innate spells by school on the points budget (6 slots a
// school), the choices a slot offers with the points after and the refusal over the points, an innate spell put in and
// taken out, and a weapon's skill slots: a built-in stays until replaced, X brings it back. The screen is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/SpellPoolConstants.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Menus/LoadoutSkillsModel.h"

namespace
{
	constexpr int32 LOADOUT_SKILLS_TEST_NO_DISCOUNT = 0;
	constexpr int32 LOADOUT_SKILLS_TEST_DISCOUNT = 1;
}

BEGIN_DEFINE_SPEC(FLoadoutSkillsSpec, "WoR.UI.LoadoutSkills", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static USpellData *Spell(ESpellSchool School, EItemTier Tier)
	{
		USpellData *Data = NewObject<USpellData>(GetTransientPackage());
		Data->School = School;
		Data->Tier = Tier;
		return Data;
	}

	static UAbilityData *Ability()
	{
		return NewObject<UAbilityData>(GetTransientPackage());
	}

	/** A Refractor with a sword and two Destruction spells (C and D tier). */
	static FCombatLoadout Refractor(USpellData *First, USpellData *Second)
	{
		FCombatLoadout Loadout;
		Loadout.InnateSpells = {First, Second};
		return Loadout;
	}

	/** A Generic's C-tier sword with one built-in ability. */
	static FCombatLoadout Swordsman(UAbilityData *BuiltIn)
	{
		UWeaponData *Data = NewObject<UWeaponData>(GetTransientPackage());
		Data->WeaponType = EWeaponType::Sword;
		Data->PresetAbilities = {BuiltIn};
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry.Weapon = Data;
		Loadout.PrimaryWeapon.WeaponEntry.Tier = EItemTier::C_Tier;
		Loadout.PrimaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		Loadout.bShowPrimary = true;
		return Loadout;
	}
END_DEFINE_SPEC(FLoadoutSkillsSpec)

void FLoadoutSkillsSpec::Define()
{
	It("shows a Refractor's innate spells by school, six slots a school, on the points budget less the discount", [this]()
	{
		USpellData *Bolt = Spell(ESpellSchool::Destruction, EItemTier::C_Tier);
		USpellData *Lance = Spell(ESpellSchool::Destruction, EItemTier::D_Tier);
		USpellData *Ward = Spell(ESpellSchool::Enhancement, EItemTier::F_Tier);
		const FLoadoutSkillsPage Page = WoRLoadoutSkills::BuildSkillsPage(Refractor(Bolt, Lance), ECharacterClass::Caster, LOADOUT_SKILLS_TEST_DISCOUNT, {Ward});
		if (!TestEqual(TEXT("a tab per school held: the two in use, and the learned one"), Page.Tabs.Num(), 2))
		{
			return;
		}
		const FLoadoutSkillTab &Destruction = Page.Tabs[0];
		TestTrue(TEXT("schools in order"), Destruction.School == ESpellSchool::Destruction && Page.Tabs[1].School == ESpellSchool::Enhancement);
		TestEqual(TEXT("two filled"), Destruction.Filled, 2);
		TestEqual(TEXT("six slots"), Destruction.Rows.Num(), SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL);
		TestTrue(TEXT("the rest are empty"), Destruction.Rows.Last().Kind == ELoadoutScreenRowKind::EmptySkill);
		TestEqual(TEXT("the learned school has none in yet"), Page.Tabs[1].Filled, 0);

		const int32 BoltPoints = SpellPoolConstants::SpellSlotEffectiveCost(EItemTier::C_Tier, LOADOUT_SKILLS_TEST_DISCOUNT);
		const int32 LancePoints = SpellPoolConstants::SpellSlotEffectiveCost(EItemTier::D_Tier, LOADOUT_SKILLS_TEST_DISCOUNT);
		TestEqual(TEXT("a slot shows its points, discounted"), Destruction.Rows[0].Cost, BoltPoints);
		TestEqual(TEXT("points spent"), Page.Points, BoltPoints + LancePoints);
		TestEqual(TEXT("the innate budget"), Page.MaxPoints, SpellPoolConstants::INNATE_SPELL_BUDGET);

		const FLoadoutSkillsPage Generic = WoRLoadoutSkills::BuildSkillsPage(Refractor(Bolt, Lance), ECharacterClass::Generic, LOADOUT_SKILLS_TEST_NO_DISCOUNT, {Ward});
		TestEqual(TEXT("a Generic has no innate lists or points"), Generic.MaxPoints, 0);
		TestEqual(TEXT("and no gear here, so no tabs"), Generic.Tabs.Num(), 0);
	});

	It("offers the school's learned spells not already in, with the points after, and refuses over the points", [this]()
	{
		USpellData *Bolt = Spell(ESpellSchool::Destruction, EItemTier::C_Tier);
		USpellData *Lance = Spell(ESpellSchool::Destruction, EItemTier::D_Tier);
		USpellData *Nova = Spell(ESpellSchool::Destruction, EItemTier::S_Tier);
		USpellData *Ward = Spell(ESpellSchool::Enhancement, EItemTier::F_Tier);
		FLoadoutSkillsPage Page = WoRLoadoutSkills::BuildSkillsPage(Refractor(Bolt, Lance), ECharacterClass::Caster, LOADOUT_SKILLS_TEST_NO_DISCOUNT, {Bolt, Nova, Ward});
		const FLoadoutScreenRow Empty = Page.Tabs[0].Rows.Last();
		TArray<FLoadoutScreenChoice> Choices = WoRLoadoutSkills::BuildSkillChoices(Page, 0, Empty, {}, {Bolt, Nova, Ward}, {});
		if (!TestEqual(TEXT("only Nova: Bolt is in, Ward is another school"), Choices.Num(), 1))
		{
			return;
		}
		TestTrue(TEXT("the line carries its spell"), Choices[0].Row.Skill == Nova);
		const int32 After = Page.Points + SpellPoolConstants::SpellSlotBaseCost(EItemTier::S_Tier);
		TestEqual(TEXT("the points after"), Choices[0].NewSpent, After);
		TestTrue(TEXT("allowed while it fits"), Choices[0].Refusal.IsEmpty() == (After <= Page.MaxPoints));

		// B-629: All learned adds the one in the list already, ticked and refused.
		const TArray<FLoadoutScreenChoice> All = WoRLoadoutSkills::BuildSkillChoices(Page, 0, Empty, {}, {Bolt, Nova, Ward}, {}, /*bAllLearned=*/true);
		if (TestEqual(TEXT("all learned: Bolt and Nova (Ward is still another school)"), All.Num(), 2))
		{
			TestTrue(TEXT("Bolt ticked, refused"), All[0].Row.Skill == Bolt && All[0].bSlotted && !All[0].Refusal.IsEmpty());
			TestTrue(TEXT("Nova as before"), All[1].Row.Skill == Nova && !All[1].bSlotted);
		}

		Page.MaxPoints = Page.Points + 1; // nearly full
		Choices = WoRLoadoutSkills::BuildSkillChoices(Page, 0, Empty, {}, {Nova}, {});
		TestFalse(TEXT("over the points: refused, and still listed"), Choices.Num() == 1 && Choices[0].Refusal.IsEmpty());
	});

	It("puts an innate spell in an empty slot and takes one out", [this]()
	{
		USpellData *Bolt = Spell(ESpellSchool::Destruction, EItemTier::C_Tier);
		USpellData *Lance = Spell(ESpellSchool::Destruction, EItemTier::D_Tier);
		USpellData *Nova = Spell(ESpellSchool::Destruction, EItemTier::F_Tier);
		FCombatLoadout Loadout = Refractor(Bolt, Lance);
		const FLoadoutSkillsPage Page = WoRLoadoutSkills::BuildSkillsPage(Loadout, ECharacterClass::Caster, LOADOUT_SKILLS_TEST_NO_DISCOUNT, {});
		const FLoadoutSkillTab &Tab = Page.Tabs[0];
		FText Refusal;
		TestTrue(TEXT("put in"), WoRLoadoutSkills::PutSkill(Loadout, ECharacterClass::Caster, Tab, Tab.Rows.Last(), Nova, Refusal));
		TestTrue(TEXT("now in the innate list"), Loadout.InnateSpells.Contains(Nova));
		TestTrue(TEXT("take Bolt out"), WoRLoadoutSkills::PutSkill(Loadout, ECharacterClass::Caster, Tab, Tab.Rows[0], nullptr, Refusal));
		TestFalse(TEXT("Bolt is out"), Loadout.InnateSpells.Contains(Bolt));
		TestFalse(TEXT("an empty slot has nothing to take out"), WoRLoadoutSkills::PutSkill(Loadout, ECharacterClass::Caster, Tab, Tab.Rows.Last(), nullptr, Refusal));
		TestFalse(TEXT("and says so"), Refusal.IsEmpty());
	});

	It("lists a weapon's skill slots, empty until filled (B-518: no built-ins), and X empties one", [this]()
	{
		UAbilityData *Slash = Ability();
		UAbilityData *Cleave = Ability();
		FCombatLoadout Loadout = Swordsman(Slash);
		const FLoadoutSkillsPage Page = WoRLoadoutSkills::BuildSkillsPage(Loadout, ECharacterClass::Generic, LOADOUT_SKILLS_TEST_NO_DISCOUNT, {});
		if (!TestEqual(TEXT("one tab: the sword"), Page.Tabs.Num(), 1))
		{
			return;
		}
		const FLoadoutSkillTab &Tab = Page.Tabs[0];
		TestTrue(TEXT("a weapon's list"), Tab.List == ELoadoutSkillList::WeaponSkills);
		TestEqual(TEXT("a row per slot"), Tab.Rows.Num(), Loadout.PrimaryWeapon.GetSkillSlotCount());
		TestTrue(TEXT("the weapon's built-in doesn't fill the first"), Tab.Rows[0].Skill == nullptr);

		const TArray<FLoadoutScreenChoice> Choices = WoRLoadoutSkills::BuildSkillChoices(Page, 0, Tab.Rows[0], {Cleave}, {}, {});
		TestTrue(TEXT("offers only the learned one"), Choices.Num() == 1 && Choices[0].Row.Skill == Cleave);

		// B-629: All learned lists an axe skill too, locked with what it needs; Fits this slot leaves it out.
		UAbilityData *Chop = Ability();
		Chop->RequiredWeaponType = EWeaponType::Axe;
		TestEqual(TEXT("fits: the axe skill left out"), WoRLoadoutSkills::BuildSkillChoices(Page, 0, Tab.Rows[0], {Cleave, Chop}, {}, {}).Num(), 1);
		const TArray<FLoadoutScreenChoice> All = WoRLoadoutSkills::BuildSkillChoices(Page, 0, Tab.Rows[0], {Cleave, Chop}, {}, {}, /*bAllLearned=*/true);
		if (TestEqual(TEXT("all learned: both"), All.Num(), 2))
		{
			TestTrue(TEXT("the sword skill free"), All[0].Row.Skill == Cleave && All[0].Refusal.IsEmpty() && !All[0].bSlotted);
			TestTrue(TEXT("the axe skill locked, with the reason"), All[1].Row.Skill == Chop && !All[1].Refusal.IsEmpty() && !All[1].bSlotted);
		}

		FText Refusal;
		TestFalse(TEXT("X on an empty slot: nothing to take off"), WoRLoadoutSkills::PutSkill(Loadout, ECharacterClass::Generic, Tab, Tab.Rows[0], nullptr, Refusal));
		TestTrue(TEXT("put it in"), WoRLoadoutSkills::PutSkill(Loadout, ECharacterClass::Generic, Tab, Tab.Rows[0], Cleave, Refusal));
		const FLoadoutSkillsPage After = WoRLoadoutSkills::BuildSkillsPage(Loadout, ECharacterClass::Generic, LOADOUT_SKILLS_TEST_NO_DISCOUNT, {});
		TestTrue(TEXT("the pick is in, marked a pick"), After.Tabs[0].Rows[0].Skill == Cleave && After.Tabs[0].Rows[0].bPick);
		TestTrue(TEXT("X takes it off"), WoRLoadoutSkills::PutSkill(Loadout, ECharacterClass::Generic, After.Tabs[0], After.Tabs[0].Rows[0], nullptr, Refusal));
		TestTrue(TEXT("the slot is empty, Slash doesn't come back"), Loadout.PrimaryWeapon.GetSkillSlots()[0] == nullptr);
	});
}

#endif
