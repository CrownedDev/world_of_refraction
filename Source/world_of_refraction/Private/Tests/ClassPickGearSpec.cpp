// ClassPickGearSpec.cpp
// B-556 (D-228, one-player stage 4): the picks set the starting gear and the trial kit. UClassDefaultLoadoutData::
// MakePickedGear lays the picked pieces over a class kit (or alone, for the starting gear); GetPickedCrystals gives a
// Resonator its crystals; WoRPlayerBuild::MakeInventory puts both and the class's innate spells in the player's inventory.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/PlayerBuild.h"
#include "Character/PlayerPicks.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Loadout/WornGear.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Menus/NewGameScreenModel.h"

namespace
{
	UWeaponData *MakeWeapon(EWeaponType Type)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponType = Type;
		return Weapon;
	}
}

BEGIN_DEFINE_SPEC(FClassPickGearSpec, "WoR.Loadout.ClassPicks", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UClassDefaultLoadoutData *Table = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Shield = nullptr;
	UWeaponData *Axe = nullptr;
	UWeaponData *BaseWeapon = nullptr;
	URingData *BaseRing = nullptr;

	FPlayerPicks MakePicks(ECharacterClass Class)
	{
		FPlayerPicks Picks;
		Picks.Name = TEXT("Ash");
		Picks.Class = Class;
		return Picks;
	}
END_DEFINE_SPEC(FClassPickGearSpec)

void FClassPickGearSpec::Define()
{
	BeforeEach([this]()
	{
		Table = NewObject<UClassDefaultLoadoutData>(GetTransientPackage());
		Sword = MakeWeapon(EWeaponType::Sword);
		Shield = MakeWeapon(EWeaponType::Shield);
		Axe = MakeWeapon(EWeaponType::Axe);
		Table->PickWeapons.Add(EWeaponType::Sword, Sword);
		Table->PickWeapons.Add(EWeaponType::Shield, Shield);
		Table->PickWeapons.Add(EWeaponType::Axe, Axe);
		Table->PickAdapter = NewObject<URingData>(GetTransientPackage());
		BaseWeapon = MakeWeapon(EWeaponType::Spear);
		BaseRing = NewObject<URingData>(GetTransientPackage());
	});

	It("gives a Generic its two weapons: a shield in the off hand, two types in both weapon places", [this]()
	{
		FPlayerPicks Picks = MakePicks(ECharacterClass::Generic);
		Picks.Weapons = {EWeaponType::Shield, EWeaponType::Sword}; // picked shield first: it still goes to the off hand
		const FClassDefaultLoadout ShieldSet = Table->MakePickedGear(Picks, nullptr);
		TestTrue(TEXT("sword in hand, shield in the off hand"), ShieldSet.Pieces.IsValidIndex(WornGearLayout::SECONDARY_INDEX) &&
				 ShieldSet.Pieces[WornGearLayout::PRIMARY_INDEX].Piece == Sword && ShieldSet.Pieces[WornGearLayout::PRIMARY_INDEX].OffHand == Shield &&
				 !ShieldSet.Pieces[WornGearLayout::SECONDARY_INDEX].Piece);

		Picks.Weapons = {EWeaponType::Sword, EWeaponType::Axe};
		const FClassDefaultLoadout TwoWeapons = Table->MakePickedGear(Picks, nullptr);
		TestTrue(TEXT("sword first, axe second"), TwoWeapons.Pieces.IsValidIndex(WornGearLayout::SECONDARY_INDEX) &&
				 TwoWeapons.Pieces[WornGearLayout::PRIMARY_INDEX].Piece == Sword && !TwoWeapons.Pieces[WornGearLayout::PRIMARY_INDEX].OffHand &&
				 TwoWeapons.Pieces[WornGearLayout::SECONDARY_INDEX].Piece == Axe);
		TestEqual(TEXT("the first weapon is the active one"), TwoWeapons.ActiveIndex, WornGearLayout::PRIMARY_INDEX);
	});

	It("lays the picks over the class kit for a trial: the weapon places are the picks', the rest stays", [this]()
	{
		FClassDefaultLoadout Base;
		Base.Pieces.SetNum(WornGearLayout::FIRST_ADAPTER_INDEX + 1);
		Base.Pieces[WornGearLayout::PRIMARY_INDEX].Piece = BaseWeapon;
		Base.Pieces[WornGearLayout::PRIMARY_INDEX].Crystal.Type = ECrystalType::Garnet;
		Base.Pieces[WornGearLayout::SECONDARY_INDEX].Piece = BaseWeapon;
		Base.Pieces[WornGearLayout::FIRST_ADAPTER_INDEX].Piece = BaseRing;
		FPlayerPicks Picks = MakePicks(ECharacterClass::Generic);
		Picks.Weapons = {EWeaponType::Sword, EWeaponType::Shield};
		const FClassDefaultLoadout Kit = Table->MakePickedGear(Picks, &Base);
		TestTrue(TEXT("the picked set replaces the kit's weapons"),
				 Kit.Pieces[WornGearLayout::PRIMARY_INDEX].Piece == Sword && !Kit.Pieces[WornGearLayout::SECONDARY_INDEX].Piece);
		TestEqual(TEXT("the kit's crystal on the weapon place stays"), Kit.Pieces[WornGearLayout::PRIMARY_INDEX].Crystal.Type, ECrystalType::Garnet);
		TestTrue(TEXT("the kit's adapter stays"), Kit.Pieces[WornGearLayout::FIRST_ADAPTER_INDEX].Piece == BaseRing);

		const FClassDefaultLoadout Refractor = Table->MakePickedGear(MakePicks(ECharacterClass::Caster), &Base);
		TestTrue(TEXT("a Refractor's kit is the class kit alone"), Refractor.Pieces.Num() == Base.Pieces.Num() &&
				 Refractor.Pieces[WornGearLayout::PRIMARY_INDEX].Piece == BaseWeapon);
	});

	It("gives a Resonator two empty adapters and 3 F-tier crystals of each picked type", [this]()
	{
		FPlayerPicks Picks = MakePicks(ECharacterClass::Resonator);
		Picks.Crystals = {ECrystalType::Garnet, ECrystalType::Sapphire};
		const FClassDefaultLoadout Gear = Table->MakePickedGear(Picks, nullptr);
		TestEqual(TEXT("two adapter places after the weapon places"), Gear.Pieces.Num(), WornGearLayout::FIRST_ADAPTER_INDEX + ClassPickRules::PICK_ADAPTER_COUNT);
		TestTrue(TEXT("both the pick adapter, no crystal"), Gear.Pieces.Last().Piece == Table->PickAdapter && Gear.Pieces.Last().Crystal.Type == ECrystalType::None);
		TestEqual(TEXT("the first adapter is active"), Gear.ActiveIndex, WornGearLayout::FIRST_ADAPTER_INDEX);
		const TMap<FCrystalId, int32> Crystals = UClassDefaultLoadoutData::GetPickedCrystals(Picks);
		TestEqual(TEXT("Garnet"), Crystals.FindRef(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier)), WoRNewGame::STARTING_CRYSTALS_PER_TYPE);
		TestEqual(TEXT("Sapphire"), Crystals.FindRef(FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier)), WoRNewGame::STARTING_CRYSTALS_PER_TYPE);
		TestTrue(TEXT("a Generic gets no crystals"), UClassDefaultLoadoutData::GetPickedCrystals(MakePicks(ECharacterClass::Generic)).IsEmpty());
	});

	It("puts the picked gear, the crystals and the class's innate spells in the built player's inventory", [this]()
	{
		FPlayerPicks Picks = MakePicks(ECharacterClass::Resonator);
		Picks.Crystals = {ECrystalType::Garnet, ECrystalType::Sapphire};
		FClassElementNumbers Numbers;
		Numbers.InnateSpells.Add(NewObject<USpellData>(GetTransientPackage()));
		const UInventoryData *Inventory = WoRPlayerBuild::MakeInventory(Picks, *Table, &Numbers);
		if (!TestNotNull(TEXT("inventory"), Inventory)) return;
		TestEqual(TEXT("the adapters are its starting gear"), Inventory->StartingGear.Pieces.Num(), WornGearLayout::FIRST_ADAPTER_INDEX + ClassPickRules::PICK_ADAPTER_COUNT);
		TestEqual(TEXT("the crystals are its stock"), Inventory->CrystalStock.Num(), 2);
		TestTrue(TEXT("the innate spell is in its one loadout"), Inventory->SavedLoadouts.Num() == 1 &&
				 Inventory->SavedLoadouts[0].InnateSpells.Num() == 1 && Inventory->SavedLoadouts[0].InnateSpells[0].Spell == Numbers.InnateSpells[0]);
		TestTrue(TEXT("no other gear to bank"), Inventory->Weapons.IsEmpty() && Inventory->Rings.IsEmpty() && !Inventory->StartingEvolution);
	});
}

#endif
