// WeaponFollowsSkillSpec.cpp
// B-474 (D-207): "Switch to the weapon of the skill you use". A Generic with two weapon entries lists the other
// weapon's skills; using one of them makes that weapon active (as the menu's switch does, ET-19); the weapon in hand's
// own skill, or a skill neither weapon has, switches nothing. The setting is on by default, and Switch Weapon sits on
// L3 (keyboard 6), no longer D-pad Left.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/CombatPlayerController.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Save/WoRSaveTypes.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Combat/FCombatCapabilities.h"

namespace
{
	UAbilityData *WeaponFollowsTestAbility(const TCHAR *Name)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = Name;
		return Ability;
	}

	UWeaponData *WeaponFollowsTestWeapon(UAbilityData *Ability)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->PresetAbilities.Add(Ability);
		return Weapon;
	}
}

BEGIN_DEFINE_SPEC(FWeaponFollowsSkillSpec, "WoR.Loadout.WeaponFollowsSkill", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	UAbilityData *SwordSkill = nullptr;
	UAbilityData *HammerSkill = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Hammer = nullptr;
END_DEFINE_SPEC(FWeaponFollowsSkillSpec)

void FWeaponFollowsSkillSpec::Define()
{
	It("is on by default, and Switch Weapon is on L3 / 6 (D-207)", [this]()
	{
		TestTrue(TEXT("the setting starts on"), FSavedSettings().bSwitchToSkillWeapon);
		const FCombatMenuKey *Switch = ACombatPlayerController::GetMenuKeys().FindByPredicate([](const FCombatMenuKey &Key)
																								 { return Key.Category == EPieMenuCategory::SwitchWeapon; });
		if (TestNotNull(TEXT("the switch key"), Switch))
		{
			TestTrue(TEXT("L3"), Switch->Gamepad == EKeys::Gamepad_LeftThumbstick);
			TestTrue(TEXT("keyboard 6"), Switch->Keyboard == EKeys::Six);
		}
		TestFalse(TEXT("D-pad Left is free"), ACombatPlayerController::GetMenuKeys().ContainsByPredicate([](const FCombatMenuKey &Key)
																										{ return Key.Gamepad == EKeys::Gamepad_DPad_Left; }));
	});

	Describe("a Generic with two weapons", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
			SwordSkill = WeaponFollowsTestAbility(TEXT("Slash"));
			HammerSkill = WeaponFollowsTestAbility(TEXT("Smash"));
			Sword = WeaponFollowsTestWeapon(SwordSkill);
			Hammer = WeaponFollowsTestWeapon(HammerSkill);
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = ECharacterClass::Generic;
			Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Sword);
			if (Loadout)
			{
				UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
				FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
				Active.PrimarySlotType = EPrimarySlotType::Weapon;
				Active.bShowPrimary = true;
				Active.SecondarySlotType = ESecondarySlotType::Weapon;
				Active.SecondaryWeapon.WeaponEntry.Weapon = Hammer;
				Active.SecondaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
			}
		});

		AfterEach([this]()
		{
			Loadout = nullptr;
			TestWorld.Reset();
		});

		It("lists the other weapon's skills, and the capabilities carry them", [this]()
		{
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			TestTrue(TEXT("the sword is in hand"), Loadout->GetActiveWeapon() == Sword);
			TestTrue(TEXT("the hammer's skill is the other weapon's"), Loadout->GetOtherWeaponSkills().Contains(HammerSkill));
			TestFalse(TEXT("not the sword's"), Loadout->GetOtherWeaponSkills().Contains(SwordSkill));
			const FCombatCapabilities Capabilities = FCombatCapabilities::BuildFrom(Loadout, ECharacterClass::Generic, FLinearColor::White, [](int32) { return FLinearColor::White; });
			TestTrue(TEXT("the menu's data has it"), Capabilities.OtherWeaponAbilities.Contains(HammerSkill));
		});

		It("makes the other weapon active when its skill is used (D-207)", [this]()
		{
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			TestTrue(TEXT("switched"), Loadout->SwitchToWeaponOfSkill(HammerSkill));
			TestTrue(TEXT("the hammer is in hand"), Loadout->GetActiveWeapon() == Hammer);
			TestTrue(TEXT("and now the sword's skill is the other one"), Loadout->GetOtherWeaponSkills().Contains(SwordSkill));
		});

		It("undoes the switch when the skill's action is refused", [this]()
		{
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			TestTrue(TEXT("switched"), Loadout->SwitchToWeaponOfSkill(HammerSkill));
			Loadout->RevertSkillWeaponSwitch();
			TestTrue(TEXT("the sword is back in hand"), Loadout->GetActiveWeapon() == Sword);
			TestTrue(TEXT("the turn's switch is unused"), Loadout->GetTurnSwitchRefusedReason().IsEmpty());
		});

		It("switches nothing for the weapon in hand's own skill or a skill neither has", [this]()
		{
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			TestFalse(TEXT("the sword's own skill"), Loadout->SwitchToWeaponOfSkill(SwordSkill));
			TestFalse(TEXT("a stranger's skill"), Loadout->SwitchToWeaponOfSkill(WeaponFollowsTestAbility(TEXT("Other"))));
			TestTrue(TEXT("the sword stays in hand"), Loadout->GetActiveWeapon() == Sword);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
