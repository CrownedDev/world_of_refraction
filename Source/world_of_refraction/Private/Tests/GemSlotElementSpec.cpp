// GemSlotElementSpec.cpp
// B-410 (Q-098, Crown 2026-11-12): the ability slots a weapon's gem adds (B-098) take only an ability with no element,
// a Generic one or one of the gem's element; the weapon's tier slots take any. A wrong pick is refused with a reason,
// and a pick the gem no longer takes (the crystal changed) is a clearable validation finding.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WeaponSpellRules.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	/** An E-tier gem: it adds ability slots (B-098). */
	const FCrystalId GEM_SLOT_TEST_GEM(ECrystalType::Garnet, EItemTier::E_Tier);
	constexpr int32 GEM_SLOT_TEST_DURABILITY = 5;
	constexpr int32 GEM_SLOT_TEST_TIER_SLOT = 0;

	FRuntimeAttachedItem GemSlotTestGem()
	{
		FRuntimeAttachedItem Socket;
		Socket.Kind = EAttachedItemKind::Crystal;
		Socket.Crystal = FCrystalAttachment(GEM_SLOT_TEST_GEM, GEM_SLOT_TEST_DURABILITY);
		return Socket;
	}

	/** An element that is neither the gem's nor None / Generic. */
	ESpellElement OtherElementThan(ESpellElement GemElement)
	{
		return GemElement == ESpellElement::Water ? ESpellElement::Fire : ESpellElement::Water;
	}

	UAbilityData *MakeAbility(ESpellElement Element)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Gem Slot Test Ability");
		Ability->Element = Element;
		return Ability;
	}

	int32 CountGemSkillFindings(const TArray<FInvalidSlotFinding> &Findings)
	{
		return Findings.FilterByPredicate([](const FInvalidSlotFinding &Finding)
		{ return Finding.SlotType == ELoadoutSlotType::WeaponGemSkill; }).Num();
	}
}

BEGIN_DEFINE_SPEC(FGemSlotElementSpec, "WoR.Loadout.GemSlotElement", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<WeaponSpellRules::FScopedWeaponSpells> Rule;
	FWeaponLoadoutEntry Weapon;
	int32 GemSlot = INDEX_NONE;
	ESpellElement GemElement = ESpellElement::None;
END_DEFINE_SPEC(FGemSlotElementSpec)

void FGemSlotElementSpec::Define()
{
	BeforeEach([this]()
	{
		Rule = MakeUnique<WeaponSpellRules::FScopedWeaponSpells>(false); // the B-098 default: gem slots are ability slots
		Weapon = FWeaponLoadoutEntry();
		Weapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon.WeaponEntry.AttachedItem = GemSlotTestGem();
		GemSlot = Weapon.GetTierSkillSlotCount();
		GemElement = Weapon.WeaponEntry.GetAttachedItem().GetElement();
	});
	AfterEach([this]()
	{
		Weapon = FWeaponLoadoutEntry();
		Rule.Reset();
	});

	It("knows which slots the gem adds", [this]()
	{
		TestTrue(TEXT("the gem adds a slot"), Weapon.GetSkillSlotCount() > GemSlot);
		TestTrue(TEXT("the first one after the tier's is a gem slot"), Weapon.IsGemSkillSlot(GemSlot));
		TestFalse(TEXT("a tier slot is not"), Weapon.IsGemSkillSlot(GEM_SLOT_TEST_TIER_SLOT));
		TestTrue(TEXT("the test gem has a real element"), GemElement != ESpellElement::None && GemElement != ESpellElement::Generic);
	});

	It("refuses a wrong-element ability in a gem slot with the reason, accepts it in a tier slot", [this]()
	{
		UAbilityData *Wrong = MakeAbility(OtherElementThan(GemElement));
		FText Reason;
		TestFalse(TEXT("refused in the gem slot"), Weapon.CanHoldSkill(GemSlot, Wrong, &Reason));
		TestFalse(TEXT("with a reason"), Reason.IsEmpty());
		TestFalse(TEXT("ReplaceSkill refuses too"), Weapon.ReplaceSkill(GemSlot, Wrong));
		TestTrue(TEXT("accepted in a tier slot"), Weapon.ReplaceSkill(GEM_SLOT_TEST_TIER_SLOT, Wrong));
	});

	It("accepts None, Generic and the gem's own element in a gem slot", [this]()
	{
		TestTrue(TEXT("no element"), Weapon.ReplaceSkill(GemSlot, MakeAbility(ESpellElement::None)));
		TestTrue(TEXT("Generic"), Weapon.ReplaceSkill(GemSlot, MakeAbility(ESpellElement::Generic)));
		TestTrue(TEXT("the gem's element"), Weapon.ReplaceSkill(GemSlot, MakeAbility(GemElement)));
		TestTrue(TEXT("a new ability defaults to Generic"), NewObject<UAbilityData>(GetTransientPackage())->Element == ESpellElement::Generic);
	});

	It("flags a pick the gem no longer takes, clearably", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Weapon;
		TestTrue(TEXT("a matching pick"), Loadout.PrimaryWeapon.ReplaceSkill(GemSlot, MakeAbility(GemElement)));
		TestEqual(TEXT("clean"), CountGemSkillFindings(FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext())), 0);

		Loadout.PrimaryWeapon.WeaponEntry.SkillPicks[GemSlot] = MakeAbility(OtherElementThan(GemElement)); // the gem changed under it
		const TArray<FInvalidSlotFinding> Findings = FLoadoutRules::CollectFindings(Loadout, FLoadoutRuleContext());
		TestEqual(TEXT("one gem slot finding"), CountGemSkillFindings(Findings), 1);
		for (const FInvalidSlotFinding &Finding : Findings)
		{
			if (Finding.SlotType == ELoadoutSlotType::WeaponGemSkill)
			{
				TestEqual(TEXT("on that slot"), Finding.SlotIndex, GemSlot);
				TestTrue(TEXT("clearable"), Finding.bClearable);
			}
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
