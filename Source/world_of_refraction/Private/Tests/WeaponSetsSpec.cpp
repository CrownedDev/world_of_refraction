// WeaponSetsSpec.cpp
// B-477 (D-209, D-210, D-211): a Generic's weapon sets. Two weapons of the same type, both pairable, make a set; a
// pair of different types, or with a single-only weapon, or the same piece twice, can't; a dual-only weapon is refused
// alone. A pair has one skill list sized by the average of its two tiers (rounded down) and pre-filled with both
// weapons' built-ins, takes dual-blade abilities, sums both weapons' stats and resistances in the fight, and goes
// through the authored loadout and back. The loadout checks refuse a bad set and clear it to the main hand alone.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutRules.h"
#include "Loadout/WeaponSetRules.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	constexpr int32 WEAPON_SETS_TEST_DEFENSE = 4;
	constexpr int32 WEAPON_SETS_TEST_OFF_HAND_DEFENSE = 3;
	constexpr int32 WEAPON_SETS_B_AND_C_SLOTS = 4; // (5 + 4) / 2, rounded down (D-211)
	constexpr int32 WEAPON_SETS_S_AND_F_SLOTS = 3; // (6 + 1) / 2

	UWeaponData *WeaponSetsTestWeapon(EWeaponType Type, EWeaponHandling Handling, UAbilityData *BuiltIn = nullptr)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponType = Type;
		Weapon->Handling = Handling;
		if (BuiltIn)
		{
			Weapon->PresetAbilities.Add(BuiltIn);
		}
		return Weapon;
	}

	FWeaponInventoryEntry WeaponSetsTestHand(UWeaponData *Weapon, EItemTier Tier = EItemTier::F_Tier)
	{
		FWeaponInventoryEntry Hand;
		Hand.Weapon = Weapon;
		Hand.Tier = Tier;
		Hand.PersistentID = FGuid::NewGuid();
		return Hand;
	}

	FWeaponLoadoutEntry WeaponSetsTestSet(const FWeaponInventoryEntry &Main, const FWeaponInventoryEntry &OffHand = FWeaponInventoryEntry())
	{
		FWeaponLoadoutEntry Set;
		Set.WeaponEntry = Main;
		Set.OffHandEntry = OffHand;
		return Set;
	}

	UAbilityData *WeaponSetsTestAbility(EWeaponType Type, EWeaponWieldMode RequiredWield)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Weapon Sets Test Ability");
		Ability->RequiredWeaponType = Type;
		Ability->RequiredWieldMode = RequiredWield;
		return Ability;
	}

	bool WeaponSetsHasFinding(const TArray<FInvalidSlotFinding> &Findings, ELoadoutSlotType Type)
	{
		return Findings.ContainsByPredicate([Type](const FInvalidSlotFinding &F) { return F.SlotType == Type && F.bClearable; });
	}

	TArray<FInvalidSlotFinding> WeaponSetsFindings(const FCombatLoadout &Loadout, ECharacterClass Class)
	{
		FLoadoutRuleContext Context;
		Context.CharacterClass = Class;
		return FLoadoutRules::CollectFindings(Loadout, Context);
	}

	FCombatLoadout WeaponSetsPrimary(const FWeaponLoadoutEntry &Set)
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Set;
		return Loadout;
	}
}

BEGIN_DEFINE_SPEC(FWeaponSetsSpec, "WoR.Loadout.WeaponSets", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FWeaponSetsSpec)

void FWeaponSetsSpec::Define()
{
	It("pairs two pairable weapons of the same type, and only those (D-209)", [this]()
	{
		using WeaponSetRules::EPairRefusal;
		const FWeaponInventoryEntry SwordA = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual));
		const FWeaponInventoryEntry SwordB = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::DualOnly));
		const FWeaponInventoryEntry SingleSword = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOnly));
		const FWeaponInventoryEntry Dagger = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Dagger, EWeaponHandling::SingleOrDual));
		TestTrue(TEXT("two swords pair"), WeaponSetRules::CheckPair(SwordA, SwordB) == EPairRefusal::None);
		TestTrue(TEXT("a sword and a dagger don't"), WeaponSetRules::CheckPair(SwordA, Dagger) == EPairRefusal::DifferentTypes);
		TestTrue(TEXT("a single-only sword doesn't"), WeaponSetRules::CheckPair(SwordA, SingleSword) == EPairRefusal::NotPairable);
		TestTrue(TEXT("nor the same piece twice"), WeaponSetRules::CheckPair(SwordA, SwordA) == EPairRefusal::SamePiece);
		TestFalse(TEXT("each refusal has words"), WeaponSetRules::GetPairRefusalText(EPairRefusal::DifferentTypes).IsEmpty());
	});

	It("refuses a dual-only weapon alone, and lets it stand in a pair (D-209)", [this]()
	{
		const FWeaponInventoryEntry DualOnly = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::DualOnly));
		const FWeaponInventoryEntry Other = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::DualOnly));
		TestFalse(TEXT("alone: refused"), WeaponSetRules::CanStandAlone(WeaponSetsTestSet(DualOnly)));
		TestTrue(TEXT("in a pair: fine"), WeaponSetRules::CanStandAlone(WeaponSetsTestSet(DualOnly, Other)));
		TestTrue(TEXT("the loadout check drops it (as the primary weapon)"),
				 WeaponSetsHasFinding(WeaponSetsFindings(WeaponSetsPrimary(WeaponSetsTestSet(DualOnly)), ECharacterClass::Generic), ELoadoutSlotType::PrimaryWeapon));
	});

	It("sizes a pair's one skill list by the average of its two tiers, rounded down (D-211)", [this]()
	{
		TestEqual(TEXT("B with C gives 4"), WeaponSetRules::GetPairTierSlots(EItemTier::B_Tier, EItemTier::C_Tier), WEAPON_SETS_B_AND_C_SLOTS);
		TestEqual(TEXT("S with F gives 3"), WeaponSetRules::GetPairTierSlots(EItemTier::S_Tier, EItemTier::F_Tier), WEAPON_SETS_S_AND_F_SLOTS);
		UWeaponData *Sword = WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual);
		const FWeaponLoadoutEntry Pair = WeaponSetsTestSet(WeaponSetsTestHand(Sword, EItemTier::B_Tier), WeaponSetsTestHand(Sword, EItemTier::C_Tier));
		TestEqual(TEXT("one list for the pair, no crystal"), Pair.GetSkillSlotCount(), WEAPON_SETS_B_AND_C_SLOTS);
		TestEqual(TEXT("as many slots in the list"), Pair.GetSkillSlots().Num(), WEAPON_SETS_B_AND_C_SLOTS);
		const FWeaponLoadoutEntry Alone = WeaponSetsTestSet(WeaponSetsTestHand(Sword, EItemTier::B_Tier));
		TestEqual(TEXT("a weapon alone keeps its tier's slots"), Alone.GetSkillSlotCount(), WeaponSetRules::GetPairTierSlots(EItemTier::B_Tier, EItemTier::B_Tier));
	});

	It("pre-fills a pair's list with both weapons' built-ins, the main hand's first, once each (D-210)", [this]()
	{
		UAbilityData *Slash = WeaponSetsTestAbility(EWeaponType::Sword, EWeaponWieldMode::Single);
		UAbilityData *Cross = WeaponSetsTestAbility(EWeaponType::Sword, EWeaponWieldMode::Dual);
		UWeaponData *Main = WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual, Slash);
		UWeaponData *Off = WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual, Cross);
		Off->PresetAbilities.Add(Slash); // shared with the main hand: listed once
		const FWeaponLoadoutEntry Pair = WeaponSetsTestSet(WeaponSetsTestHand(Main, EItemTier::S_Tier), WeaponSetsTestHand(Off, EItemTier::S_Tier));
		const TArray<UAbilityData *> BuiltIns = WeaponSetRules::GetSetBuiltIns(Pair);
		if (TestEqual(TEXT("two built-ins"), BuiltIns.Num(), 2))
		{
			TestTrue(TEXT("the main hand's first"), BuiltIns[0] == Slash && BuiltIns[1] == Cross);
		}
		const TArray<UAbilityData *> Slots = Pair.GetSkillSlots();
		TestTrue(TEXT("both pre-fill the list"), Slots.Contains(Slash) && Slots.Contains(Cross));
	});

	It("lets a pair hold dual-blade abilities of its type; a weapon alone can't (D-210)", [this]()
	{
		UAbilityData *Cross = WeaponSetsTestAbility(EWeaponType::Sword, EWeaponWieldMode::Dual);
		UWeaponData *Sword = WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual);
		FWeaponLoadoutEntry Pair = WeaponSetsTestSet(WeaponSetsTestHand(Sword), WeaponSetsTestHand(Sword));
		Pair.AssignedAbilities.Add(Cross);
		FWeaponLoadoutEntry Alone = WeaponSetsTestSet(WeaponSetsTestHand(Sword));
		Alone.AssignedAbilities.Add(Cross);
		TestFalse(TEXT("the pair takes it"),
				  WeaponSetsHasFinding(WeaponSetsFindings(WeaponSetsPrimary(Pair), ECharacterClass::Generic), ELoadoutSlotType::WeaponAbility));
		TestTrue(TEXT("a sword alone doesn't"),
				 WeaponSetsHasFinding(WeaponSetsFindings(WeaponSetsPrimary(Alone), ECharacterClass::Generic), ELoadoutSlotType::WeaponAbility));
	});

	It("refuses a bad pair, and any pair off a Generic, clearing the off hand (D-209)", [this]()
	{
		const FWeaponInventoryEntry Sword = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual));
		const FWeaponInventoryEntry Dagger = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Dagger, EWeaponHandling::SingleOrDual));
		const FWeaponInventoryEntry Sword2 = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual));
		TestTrue(TEXT("sword + dagger: the off hand is refused"),
				 WeaponSetsHasFinding(WeaponSetsFindings(WeaponSetsPrimary(WeaponSetsTestSet(Sword, Dagger)), ECharacterClass::Generic), ELoadoutSlotType::OffHandWeapon));
		TestFalse(TEXT("two swords on a Generic: fine"),
				  WeaponSetsHasFinding(WeaponSetsFindings(WeaponSetsPrimary(WeaponSetsTestSet(Sword, Sword2)), ECharacterClass::Generic), ELoadoutSlotType::OffHandWeapon));
		TestTrue(TEXT("two swords on a Refractor: refused"),
				 WeaponSetsHasFinding(WeaponSetsFindings(WeaponSetsPrimary(WeaponSetsTestSet(Sword, Sword2)), ECharacterClass::Caster), ELoadoutSlotType::OffHandWeapon));

		FCombatLoadout Secondary;
		Secondary.SecondarySlotType = ESecondarySlotType::Weapon;
		Secondary.SecondaryWeapon = WeaponSetsTestSet(Sword, Dagger);
		TestTrue(TEXT("the secondary set's finding says so"),
				 WeaponSetsFindings(Secondary, ECharacterClass::Generic).ContainsByPredicate([](const FInvalidSlotFinding &F)
																							  { return F.SlotType == ELoadoutSlotType::OffHandWeapon && F.SubIndex == LoadoutAbilityOverflowIndex::SECONDARY_WEAPON; }));
	});

	It("costs nothing for the off hand: the gear is free and the sockets are the main hand's (D-206, D-210)", [this]()
	{
		const FWeaponInventoryEntry Sword = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual));
		FWeaponInventoryEntry Sword2 = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual));
		TestEqual(TEXT("a bare pair costs 0"), FLoadoutRules::NonAdapterCost(WeaponSetsPrimary(WeaponSetsTestSet(Sword, Sword2)), ECharacterClass::Generic), 0);
	});

	It("keeps each set's off hand through the authored loadout and back", [this]()
	{
		UWeaponData *Sword = WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual);
		FCombatLoadout Loadout = WeaponSetsPrimary(WeaponSetsTestSet(WeaponSetsTestHand(Sword), WeaponSetsTestHand(Sword)));
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = WeaponSetsTestSet(WeaponSetsTestHand(Sword), WeaponSetsTestHand(Sword));
		const FSavedLoadout Saved = Loadout.ToSavedLoadout(ECharacterClass::Generic);
		TestTrue(TEXT("saved: both off hands"), Saved.PrimaryOffHandWeapon == Sword && Saved.SecondaryOffHandWeapon == Sword);
		TestEqual(TEXT("saved: by owned GUID"), Saved.PrimaryOffHandInstance, Loadout.PrimaryWeapon.OffHandEntry.PersistentID);
		const FCombatLoadout Back = FCombatLoadout::CreateFromSavedLoadout(Saved);
		TestTrue(TEXT("rebuilt: both sets are pairs"), Back.PrimaryWeapon.HasOffHand() && Back.SecondaryWeapon.HasOffHand());
	});

	Describe("in a fight", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
		});

		AfterEach([this]() { TestWorld.Reset(); });

		It("sums both weapons' stats for a Generic's pair, only the main hand's for a bad pair (D-210)", [this]()
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = ECharacterClass::Generic;
			UWeaponData *Sword = WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual);
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Sword);
			if (!TestNotNull(TEXT("Loadout"), Loadout))
			{
				return;
			}
			UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimaryWeapon.WeaponEntry.StatBonus.BonusDefense = WEAPON_SETS_TEST_DEFENSE;
			Active.PrimaryWeapon.OffHandEntry = WeaponSetsTestHand(WeaponSetsTestWeapon(EWeaponType::Sword, EWeaponHandling::SingleOrDual));
			Active.PrimaryWeapon.OffHandEntry.StatBonus.BonusDefense = WEAPON_SETS_TEST_OFF_HAND_DEFENSE;
			TestTrue(TEXT("a pair"), Loadout->IsWeaponSetPaired(Active.PrimaryWeapon));
			TestEqual(TEXT("both weapons' stats"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense,
					  WEAPON_SETS_TEST_DEFENSE + WEAPON_SETS_TEST_OFF_HAND_DEFENSE);

			Active.PrimaryWeapon.OffHandEntry.Weapon = WeaponSetsTestWeapon(EWeaponType::Dagger, EWeaponHandling::SingleOrDual);
			TestFalse(TEXT("a sword and a dagger are no pair"), Loadout->IsWeaponSetPaired(Active.PrimaryWeapon));
			TestEqual(TEXT("only the main hand's stats"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense, WEAPON_SETS_TEST_DEFENSE);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
