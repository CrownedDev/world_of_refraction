// FormMiniBossSpec.cpp
// B-279d (CharacterForms §1b): an encounter can field a character form as its mini-boss. The form's
// character leads the team within the three-enemy cap, the fight is at least a mini-boss, and the
// character fights with its authored loadout.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Combat/Encounter/EncounterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryData.h"
#include "Loadout/LoadoutComponent.h"

BEGIN_DEFINE_SPEC(FFormMiniBossSpec, "WoR.Character.FormMiniBoss",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterFormData *Form = nullptr;
	UWeaponData *Sword = nullptr;
	UEncounterData *Encounter = nullptr;
END_DEFINE_SPEC(FFormMiniBossSpec)

void FFormMiniBossSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());

		Sword = NewObject<UWeaponData>(GetTransientPackage());
		UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Inventory->Weapons.Add(Sword);
		FSavedLoadout Authored;
		Authored.LoadoutName = TEXT("Authored");
		Authored.RequiredClass = ECharacterClass::Generic;
		Authored.PrimarySlotType = EPrimarySlotType::Weapon;
		Authored.PrimaryWeapon = Sword;
		Inventory->SavedLoadouts = {Authored};

		UCharacterData *Character = FWoRTestWorld::NewCharacterData();
		Character->CharacterClass = ECharacterClass::Generic;
		Character->Inventory = Inventory;
		Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = Character;

		Encounter = NewObject<UEncounterData>(GetTransientPackage());
		for (int32 Index = 0; Index < EncounterConstants::ENCOUNTER_MAX_ENEMIES; ++Index)
		{
			Encounter->Enemies.Add(FWoRTestWorld::NewCharacterData());
		}
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Form = nullptr;
		Sword = nullptr;
		Encounter = nullptr;
	});

	It("puts the form's character first, within three, and makes the fight a mini-boss", [this]()
	{
		TestEqual(TEXT("no form: the placed rank"), UEncounterData::GetFightRank(Encounter, EEncounterRank::Normal, Encounter->GetEnemyRoster()), EEncounterRank::Normal);
		Encounter->MiniBossForm = Form;
		Encounter->Enemies.Add(Form->Character);
		const TArray<UCharacterData *> Roster = Encounter->GetEnemyRoster();
		TestEqual(TEXT("still three"), Roster.Num(), EncounterConstants::ENCOUNTER_MAX_ENEMIES);
		TestTrue(TEXT("the form's character leads"), Roster.Num() > 0 && Roster[0] == Form->Character);
		TestEqual(TEXT("listed too, it still fights once"), Roster.FilterByPredicate([this](const UCharacterData *Data) { return Data == Form->Character; }).Num(), 1);
		TestEqual(TEXT("raised to mini-boss"), UEncounterData::GetFightRank(Encounter, EEncounterRank::Normal, Roster), EEncounterRank::MiniBoss);
		TestEqual(TEXT("a boss stays a boss"), UEncounterData::GetFightRank(Encounter, EEncounterRank::Boss, Roster), EEncounterRank::Boss);
		TestEqual(TEXT("an elite becomes a mini-boss (B-224's severity, not the enum order)"),
				  UEncounterData::GetFightRank(Encounter, EEncounterRank::Elite, Roster), EEncounterRank::MiniBoss);
		TestEqual(TEXT("left out of a caught fight: not raised"), UEncounterData::GetFightRank(Encounter, EEncounterRank::Normal, {Roster.Last()}), EEncounterRank::Normal);
	});

	It("fights with its authored loadout", [this]()
	{
		Encounter->MiniBossForm = Form;
		UCharacterData *MiniBoss = Encounter->GetEnemyRoster()[0];
		// As UCharacterDataComponent does for a spawned combatant: the inventory from the authored asset, then the loadout.
		AActor *Actor = TestWorld->SpawnCombatant(MiniBoss, /*bAIControlled=*/true);
		UInventoryComponent *Inventory = Actor ? FWoRTestWorld::AddComponent<UInventoryComponent>(Actor) : nullptr;
		if (!TestNotNull(TEXT("spawned with an inventory"), Inventory))
		{
			return;
		}
		Inventory->InitializeFromCharacterData(MiniBoss);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor)->InitializeFromCharacterData(MiniBoss, Inventory);
		if (
			!TestTrue(TEXT("an active loadout"), Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return;
		}
		const FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TestTrue(TEXT("the authored primary"), Active.PrimaryWeapon.WeaponEntry.Weapon == Sword);
	});
}

#endif
