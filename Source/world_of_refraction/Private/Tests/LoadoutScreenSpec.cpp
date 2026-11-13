// LoadoutScreenSpec.cpp
// B-440a (D-181, D-180): the loadout screen's gear page model. One row per weapon and adapter with its two sockets
// and its cost as "frame + attachment", the weapon in hand marked only when there are two, the budget by class,
// apparel outside the budget with its empty slots, and the name tables the rows read. The screen is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "WoRGameNames.h"

namespace
{
	const FCrystalId LOADOUT_SCREEN_TEST_GEM(ECrystalType::Garnet, EItemTier::B_Tier);
	constexpr int32 LOADOUT_SCREEN_TEST_DURABILITY = 40;
	constexpr int32 LOADOUT_SCREEN_TEST_GEAR_SOCKETS = 2;
	constexpr int32 LOADOUT_SCREEN_TEST_FIRST = 0;
	constexpr int32 LOADOUT_SCREEN_TEST_SECOND = 1;
}

BEGIN_DEFINE_SPEC(FLoadoutScreenSpec, "WoR.UI.LoadoutScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static FRuntimeAttachedItem Gem()
	{
		FRuntimeAttachedItem Item;
		Item.Kind = EAttachedItemKind::Crystal;
		Item.Crystal = FCrystalAttachment(LOADOUT_SCREEN_TEST_GEM, LOADOUT_SCREEN_TEST_DURABILITY);
		return Item;
	}

	static FWeaponLoadoutEntry Weapon(EWeaponType Type, EWeaponWieldMode Wield = EWeaponWieldMode::Single)
	{
		UWeaponData *Data = NewObject<UWeaponData>(GetTransientPackage());
		Data->WeaponType = Type;
		Data->WieldMode = Wield;
		FWeaponLoadoutEntry Entry;
		Entry.WeaponEntry.Weapon = Data;
		return Entry;
	}

	/** A Generic with two weapons, the first in hand. */
	static FCombatLoadout TwoWeapons()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Weapon(EWeaponType::Sword, EWeaponWieldMode::OffHandShield);
		Loadout.PrimaryWeapon.WeaponEntry.AttachedItem = Gem();
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon = Weapon(EWeaponType::Hammer);
		Loadout.bShowPrimary = true;
		return Loadout;
	}
END_DEFINE_SPEC(FLoadoutScreenSpec)

void FLoadoutScreenSpec::Define()
{
	It("lists a Generic's two weapons with their sockets, costs and the one in hand", [this]()
	{
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(TwoWeapons(), ECharacterClass::Generic);
		if (!TestTrue(TEXT("two weapon rows first"), Page.Rows.Num() >= 2 && Page.FirstApparelRow == 2))
		{
			return;
		}
		const FLoadoutScreenRow &Sword = Page.Rows[LOADOUT_SCREEN_TEST_FIRST];
		const FLoadoutScreenRow &Hammer = Page.Rows[LOADOUT_SCREEN_TEST_SECOND];
		TestTrue(TEXT("a weapon row"), Sword.Kind == ELoadoutScreenRowKind::Weapon);
		TestTrue(TEXT("the first is in hand"), Sword.bActive);
		TestFalse(TEXT("the second is not"), Hammer.bActive);
		TestFalse(TEXT("two weapons: the row says only one gives its abilities"), Sword.Note.IsEmpty());

		TestEqual(TEXT("two sockets"), Sword.Sockets.Num(), LOADOUT_SCREEN_TEST_GEAR_SOCKETS);
		TestFalse(TEXT("the crystal socket is filled"), Sword.Sockets[LOADOUT_SCREEN_TEST_FIRST].bEmpty);
		TestTrue(TEXT("named by the name table"), Sword.Sockets[LOADOUT_SCREEN_TEST_FIRST].Name.EqualTo(WoRGameNames::GetCrystalTypeName(ECrystalType::Garnet)));
		TestTrue(TEXT("the stone socket is empty (D-180: it stays)"), Sword.Sockets[LOADOUT_SCREEN_TEST_SECOND].bEmpty);
		TestTrue(TEXT("the bare hammer's sockets are empty"), Hammer.Sockets[LOADOUT_SCREEN_TEST_FIRST].bEmpty);

		TestEqual(TEXT("weapon + crystal"), Sword.Cost, LoadoutConstants::WEAPON_FRAME_COST + LoadoutConstants::CRYSTAL_ATTACHMENT_COST);
		TestEqual(TEXT("bare weapon"), Hammer.Cost, LoadoutConstants::WEAPON_FRAME_COST);
		TestTrue(TEXT("the sum is spelled out"), Sword.CostLine.ToString().Contains(TEXT("+")));
		TestFalse(TEXT("a bare weapon has no sum"), Hammer.CostLine.ToString().Contains(TEXT("+")));
		TestEqual(TEXT("spent"), Page.Spent, Sword.Cost + Hammer.Cost);
		TestEqual(TEXT("the Generic's budget"), Page.Budget, LoadoutClassTable::GENERIC.Budget);
	});

	It("puts the wield mode in the kind line, and nothing for a single weapon (D-180)", [this]()
	{
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(TwoWeapons(), ECharacterClass::Generic);
		if (!TestTrue(TEXT("two rows"), Page.Rows.Num() >= 2))
		{
			return;
		}
		const FString Shield = WoRGameNames::GetWieldModeName(EWeaponWieldMode::OffHandShield).ToString();
		TestFalse(TEXT("a shield has a wield word"), Shield.IsEmpty());
		TestTrue(TEXT("sword and shield"), Page.Rows[LOADOUT_SCREEN_TEST_FIRST].KindLine.ToString().Contains(Shield));
		TestTrue(TEXT("its type"), Page.Rows[LOADOUT_SCREEN_TEST_FIRST].KindLine.ToString().Contains(WoRGameNames::GetWeaponTypeName(EWeaponType::Sword).ToString()));
		TestTrue(TEXT("a single weapon has none"), WoRGameNames::GetWieldModeName(EWeaponWieldMode::Single).IsEmpty());
	});

	It("marks no weapon as in hand when there is only one", [this]()
	{
		FCombatLoadout Loadout = TwoWeapons();
		Loadout.SecondarySlotType = ESecondarySlotType::None;
		Loadout.SecondaryWeapon = FWeaponLoadoutEntry();
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		if (TestEqual(TEXT("one weapon row"), Page.FirstApparelRow, 1))
		{
			TestFalse(TEXT("no ACTIVE mark"), Page.Rows[LOADOUT_SCREEN_TEST_FIRST].bActive);
			TestTrue(TEXT("no two-weapon note"), Page.Rows[LOADOUT_SCREEN_TEST_FIRST].Note.IsEmpty());
		}
	});

	It("lists a Resonator's adapters after its weapon, on the Resonator's budget", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Weapon(EWeaponType::Staff);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FRingLoadoutEntry Adapter;
			Adapter.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
			Adapter.RingEntry.AttachedItem = Gem();
			Loadout.RingLoadout.Add(Adapter);
		}
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Resonator);
		if (!TestEqual(TEXT("a weapon and two adapters"), Page.FirstApparelRow, 3))
		{
			return;
		}
		TestTrue(TEXT("the weapon first"), Page.Rows[0].Kind == ELoadoutScreenRowKind::Weapon);
		TestTrue(TEXT("then an adapter"), Page.Rows[1].Kind == ELoadoutScreenRowKind::Adapter);
		TestTrue(TEXT("one adapter is the one in use"), Page.Rows[1].bActive != Page.Rows[2].bActive);
		TestEqual(TEXT("the Resonator's budget"), Page.Budget, LoadoutClassTable::RESONATOR.Budget);
		TestEqual(TEXT("spent is the rows' costs"), Page.Spent, Page.Rows[0].Cost + Page.Rows[1].Cost + Page.Rows[2].Cost);
	});

	It("shows apparel outside the budget and fills the rest with empty slots", [this]()
	{
		FCombatLoadout Loadout = TwoWeapons();
		const FLoadoutScreenPage Bare = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		FApparelEntry Coat;
		Coat.Apparel = NewObject<UApparelData>(GetTransientPackage());
		Coat.AttachedItem = Gem();
		Loadout.Apparel.Add(Coat);
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		if (!TestEqual(TEXT("two apparel rows after the gear"), Page.Rows.Num(), Page.FirstApparelRow + ApparelRules::MAX_APPAREL_SLOTS))
		{
			return;
		}
		const FLoadoutScreenRow &Worn = Page.Rows[Page.FirstApparelRow];
		TestTrue(TEXT("the worn piece"), Worn.Kind == ELoadoutScreenRowKind::Apparel);
		TestFalse(TEXT("outside the budget"), Worn.bCountsToBudget);
		TestFalse(TEXT("its socket holds the crystal"), Worn.Sockets.Num() != 1 || Worn.Sockets[0].bEmpty);
		TestTrue(TEXT("then an empty slot"), Page.Rows.Last().Kind == ELoadoutScreenRowKind::EmptyApparel);
		TestEqual(TEXT("apparel costs nothing"), Page.Spent, Bare.Spent);
	});

	It("names every crystal kind, weapon kind and tier", [this]()
	{
		const UEnum *Crystals = StaticEnum<ECrystalType>();
		for (int32 Index = 0; Index < Crystals->NumEnums() - 1; ++Index) // the last is _MAX
		{
			const ECrystalType Type = static_cast<ECrystalType>(Crystals->GetValueByIndex(Index));
			TestEqual(*FString::Printf(TEXT("%s is named unless it is None"), *Crystals->GetNameStringByIndex(Index)),
					  WoRGameNames::GetCrystalTypeName(Type).IsEmpty(), Type == ECrystalType::None);
		}
		const UEnum *Weapons = StaticEnum<EWeaponType>();
		for (int32 Index = 0; Index < Weapons->NumEnums() - 1; ++Index)
		{
			TestFalse(*FString::Printf(TEXT("%s is named"), *Weapons->GetNameStringByIndex(Index)),
					  WoRGameNames::GetWeaponTypeName(static_cast<EWeaponType>(Weapons->GetValueByIndex(Index))).IsEmpty());
		}
		const UEnum *Tiers = StaticEnum<EItemTier>();
		for (int32 Index = 0; Index < Tiers->NumEnums() - 1; ++Index)
		{
			TestFalse(*FString::Printf(TEXT("%s has a letter"), *Tiers->GetNameStringByIndex(Index)),
					  WoRGameNames::GetTierLetter(static_cast<EItemTier>(Tiers->GetValueByIndex(Index))).IsEmpty());
		}
	});

	It("titles a loadout with its place among the saved ones", [this]()
	{
		const FString Title = WoRLoadoutScreen::FormatLoadoutTitle(TEXT("Bruiser"), 2, 5).ToString();
		TestTrue(TEXT("name"), Title.Contains(TEXT("Bruiser")));
		TestTrue(TEXT("place"), Title.Contains(TEXT("2")) && Title.Contains(TEXT("5")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
