// LoadoutScreenSpec.cpp
// B-440a (D-181, D-180): the loadout screen's gear page model. One row per weapon and adapter with its two sockets
// and its cost as "frame + attachment", the weapon in hand marked only when there are two, the budget by class,
// apparel outside the budget with its empty slots, and the name tables the rows read. The screen is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/LoadoutRules.h"
#include "Blueprint/UserWidget.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutScreenWidget.h"
#include "WoRGameNames.h"
#include "WoRTestWorld.h"

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
		Entry.WeaponEntry.PersistentID = FGuid::NewGuid(); // owned (B-440b's picker skips pieces without one)
		return Entry;
	}

	static FRingLoadoutEntry Adapter()
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	static const FLoadoutScreenRow *FindRow(const FLoadoutScreenPage &Page, ELoadoutScreenRowKind Kind)
	{
		return Page.Rows.FindByPredicate([Kind](const FLoadoutScreenRow &Row) { return Row.Kind == Kind; });
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
		if (TestEqual(TEXT("one weapon row, then the free weapon place (B-440b)"), Page.FirstApparelRow, 2))
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
		if (!TestEqual(TEXT("a weapon, two adapters and the free adapter place (B-440b)"), Page.FirstApparelRow, 4))
		{
			return;
		}
		TestTrue(TEXT("the weapon first"), Page.Rows[0].Kind == ELoadoutScreenRowKind::Weapon);
		TestTrue(TEXT("then an adapter"), Page.Rows[1].Kind == ELoadoutScreenRowKind::Adapter);
		TestTrue(TEXT("one adapter is the one in use"), Page.Rows[1].bActive != Page.Rows[2].bActive);
		TestEqual(TEXT("the Resonator's budget"), Page.Budget, LoadoutClassTable::RESONATOR.Budget);
		TestEqual(TEXT("spent is the rows' costs"), Page.Spent, Page.Rows[0].Cost + Page.Rows[1].Cost + Page.Rows[2].Cost);
	});

	It("charges the evolution seat whenever the slot is an evolution's, as the budget check does", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution; // no item: a stale seat still takes its budget
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		if (TestEqual(TEXT("the seat row, then the free weapon place (B-440b)"), Page.FirstApparelRow, 2))
		{
			TestTrue(TEXT("an evolution seat"), Page.Rows[LOADOUT_SCREEN_TEST_FIRST].Kind == ELoadoutScreenRowKind::EvolutionSeat);
			TestFalse(TEXT("named even when the item is missing"), Page.Rows[LOADOUT_SCREEN_TEST_FIRST].Name.IsEmpty());
		}
		TestEqual(TEXT("spent as NonAdapterCost charges it"), Page.Spent, FLoadoutRules::NonAdapterCost(Loadout, ECharacterClass::Generic));
		TestEqual(TEXT("an evolution alone"), Page.Spent, LoadoutConstants::EVOLUTION_ALONE_SLOT_COST);
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

	It("opens with no party, says so, and closes on Back (the screen, headless)", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("world"), TestWorld.Create()))
		{
			return;
		}
		ULoadoutScreenWidget *Screen = CreateWidget<ULoadoutScreenWidget>(TestWorld.GetWorld(), ULoadoutScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestTrue(TEXT("activated"), Screen->IsActivated());
		TestFalse(TEXT("no party: the page says why it is empty"), Screen->GetPage().Refusal.IsEmpty());
		Screen->MoveSelection(1);
		Screen->MoveMember(1);
		TestEqual(TEXT("nothing to move to"), Screen->GetSelectedRow(), 0);
		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
	});

	It("titles a loadout with its place among the saved ones", [this]()
	{
		const FString Title = WoRLoadoutScreen::FormatLoadoutTitle(TEXT("Bruiser"), 2, 5).ToString();
		TestTrue(TEXT("name"), Title.Contains(TEXT("Bruiser")));
		TestTrue(TEXT("place"), Title.Contains(TEXT("2")) && Title.Contains(TEXT("5")));
	});

	It("gives each gear row its place, and each free place a row (B-440b)", [this]()
	{
		FCombatLoadout Generic = TwoWeapons();
		Generic.SecondarySlotType = ESecondarySlotType::None;
		Generic.SecondaryWeapon = FWeaponLoadoutEntry();
		const FLoadoutScreenPage One = WoRLoadoutScreen::BuildGearPage(Generic, ECharacterClass::Generic);
		if (TestTrue(TEXT("a weapon and a free place"), One.Rows.Num() > 1))
		{
			TestTrue(TEXT("the weapon is the primary"), One.Rows[0].Place == ELoadoutScreenPlace::Primary);
			TestEqual(TEXT("its owned piece"), One.Rows[0].GearID, Generic.PrimaryWeapon.WeaponEntry.PersistentID);
			TestTrue(TEXT("then the free second weapon"), One.Rows[1].Kind == ELoadoutScreenRowKind::EmptyGear && One.Rows[1].Place == ELoadoutScreenPlace::Secondary);
			TestFalse(TEXT("a free place takes no budget"), One.Rows[1].bCountsToBudget);
		}
		TestNull(TEXT("two weapons: no free place"), FindRow(WoRLoadoutScreen::BuildGearPage(TwoWeapons(), ECharacterClass::Generic), ELoadoutScreenRowKind::EmptyGear));

		FCombatLoadout Resonator;
		Resonator.PrimarySlotType = EPrimarySlotType::Weapon;
		Resonator.PrimaryWeapon = Weapon(EWeaponType::Staff);
		Resonator.RingLoadout = {Adapter(), Adapter()};
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Resonator, ECharacterClass::Resonator);
		if (TestEqual(TEXT("weapon, two adapters, a free adapter"), Page.FirstApparelRow, 4))
		{
			TestTrue(TEXT("the second adapter's place"), Page.Rows[2].Place == ELoadoutScreenPlace::Adapter && Page.Rows[2].AdapterIndex == 1);
			TestTrue(TEXT("a new adapter"), Page.Rows[3].Place == ELoadoutScreenPlace::Adapter && Page.Rows[3].AdapterIndex == INDEX_NONE);
		}
	});

	It("offers the owned gear a place takes, with the budget after it, and keeps refused lines (B-440b)", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Evolution; // the seat: 3 of the Generic's 5
		const FLoadoutScreenPage Before = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		const FLoadoutScreenRow *Free = FindRow(Before, ELoadoutScreenRowKind::EmptyGear);
		if (!TestNotNull(TEXT("the free weapon place"), Free))
		{
			return;
		}
		const FWeaponInventoryEntry Bare = Weapon(EWeaponType::Sword).WeaponEntry;
		FWeaponInventoryEntry Evolved = Weapon(EWeaponType::Hammer).WeaponEntry;
		Evolved.AttachedItem.Kind = EAttachedItemKind::Evolution;
		const FWeaponInventoryEntry Worn = Weapon(EWeaponType::Spear).WeaponEntry;
		const FRingInventoryEntry Ring = Adapter().RingEntry;
		const TArray<FLoadoutScreenChoice> Choices =
			WoRLoadoutScreen::BuildGearChoices(Loadout, ECharacterClass::Generic, *Free, {Bare, Evolved, Worn}, {Ring}, {Worn.PersistentID});
		if (!TestEqual(TEXT("the three weapons; a second weapon place takes no adapter"), Choices.Num(), 3))
		{
			return;
		}
		TestEqual(TEXT("bare: seat 3 + weapon 1"), Choices[0].NewSpent, LoadoutConstants::EVOLUTION_ALONE_SLOT_COST + LoadoutConstants::WEAPON_FRAME_COST);
		TestTrue(TEXT("it fits"), Choices[0].Refusal.IsEmpty());
		TestTrue(TEXT("an evolved weapon is over the budget"), Choices[1].NewSpent > Before.Budget && !Choices[1].Refusal.IsEmpty());
		TestFalse(TEXT("another member wears the third"), Choices[2].Refusal.IsEmpty());
	});

	It("puts the other weapon in hand, swaps a piece in, and takes one off but never the last primary (B-440b)", [this]()
	{
		FCombatLoadout Loadout = TwoWeapons();
		const FGuid Hammer = Loadout.SecondaryWeapon.WeaponEntry.PersistentID;
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		const FWeaponInventoryEntry Spare = Weapon(EWeaponType::Spear).WeaponEntry;
		const TArray<FLoadoutScreenChoice> Choices = WoRLoadoutScreen::BuildGearChoices(Loadout, ECharacterClass::Generic, Page.Rows[LOADOUT_SCREEN_TEST_SECOND],
																					   {Loadout.PrimaryWeapon.WeaponEntry, Loadout.SecondaryWeapon.WeaponEntry, Spare}, {}, {});
		if (!TestEqual(TEXT("put in hand, then the spare (the two worn ones aren't offered)"), Choices.Num(), 2))
		{
			return;
		}
		TestTrue(TEXT("put in hand first"), Choices[0].bMakeActive);
		FCombatLoadout InHand = Loadout;
		TestTrue(TEXT("put in hand"), WoRLoadoutScreen::PutGearChoice(InHand, Page.Rows[LOADOUT_SCREEN_TEST_SECOND], Choices[0]));
		TestTrue(TEXT("the hammer is in hand"), WoRLoadoutScreen::BuildGearPage(InHand, ECharacterClass::Generic).Rows[LOADOUT_SCREEN_TEST_SECOND].bActive);

		FCombatLoadout Swapped = Loadout;
		TestTrue(TEXT("swapped"), WoRLoadoutScreen::PutGearChoice(Swapped, Page.Rows[LOADOUT_SCREEN_TEST_SECOND], Choices[1]));
		TestEqual(TEXT("the spare is the second weapon"), Swapped.SecondaryWeapon.WeaponEntry.PersistentID, Spare.PersistentID);

		FText Refusal;
		FCombatLoadout Off = Loadout;
		TestTrue(TEXT("the primary comes off"), WoRLoadoutScreen::TakeOffGear(Off, ECharacterClass::Generic, Page.Rows[LOADOUT_SCREEN_TEST_FIRST], Refusal));
		TestEqual(TEXT("the hammer moved up into it (D-116)"), Off.PrimaryWeapon.WeaponEntry.PersistentID, Hammer);
		TestTrue(TEXT("no second weapon"), Off.SecondarySlotType == ESecondarySlotType::None);
		const FLoadoutScreenRow Last = WoRLoadoutScreen::BuildGearPage(Off, ECharacterClass::Generic).Rows[LOADOUT_SCREEN_TEST_FIRST];
		TestFalse(TEXT("the last primary stays"), WoRLoadoutScreen::TakeOffGear(Off, ECharacterClass::Generic, Last, Refusal));
		TestFalse(TEXT("with the reason"), Refusal.IsEmpty());
		TestEqual(TEXT("unchanged"), Off.PrimaryWeapon.WeaponEntry.PersistentID, Hammer);
	});

	It("offers carried gems, stones and owned evolutions for an empty socket, with the budget after and the refusals (B-440c)", [this]()
	{
		FCombatLoadout Loadout = TwoWeapons(); // sword + Garnet (2) and a bare hammer (1): 3 of 5
		Loadout.PrimaryWeapon.WeaponEntry.AttachedItem.Crystal.Id = FCrystalId(ECrystalType::Garnet, EItemTier::S_Tier); // the one S-tier
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		const FLoadoutScreenRow &Hammer = Page.Rows[LOADOUT_SCREEN_TEST_SECOND];
		const FCrystalId Citrine(ECrystalType::Citrine, EItemTier::C_Tier);
		const FCrystalId Onyx(ECrystalType::Onyx, EItemTier::S_Tier);
		const FCrystalId Stone(ECrystalType::DamageStone, EItemTier::F_Tier);
		const TMap<FCrystalId, int32> Carried = {{Citrine, 2}, {Onyx, 1}, {Stone, 1}};
		FEvolutionInventoryEntry Free;
		Free.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		Free.InstanceID = FGuid::NewGuid();
		FEvolutionInventoryEntry Used = Free;
		Used.InstanceID = FGuid::NewGuid();

		const TArray<FLoadoutScreenChoice> Crystal = WoRLoadoutScreen::BuildSocketChoices(Loadout, ECharacterClass::Generic, Hammer, LoadoutScreenSockets::CRYSTAL,
																						 Carried, {Free, Used}, {Used.InstanceID});
		if (!TestEqual(TEXT("two gems (highest tier first), two evolutions; no stone"), Crystal.Num(), 4))
		{
			return;
		}
		TestTrue(TEXT("the S gem first"), Crystal[0].Crystal == Onyx);
		TestFalse(TEXT("a second S-tier is refused (one per loadout)"), Crystal[0].Refusal.IsEmpty());
		TestTrue(TEXT("then the C gem"), Crystal[1].Crystal == Citrine);
		TestEqual(TEXT("a gem on the bare hammer: 3 + 1"), Crystal[1].NewSpent, Page.Spent + LoadoutConstants::CRYSTAL_ATTACHMENT_COST);
		TestTrue(TEXT("it fits"), Crystal[1].Refusal.IsEmpty());
		TestTrue(TEXT("an evolution"), Crystal[2].EvolutionID == Free.InstanceID && Crystal[2].Refusal.IsEmpty());
		TestFalse(TEXT("one in use is refused"), Crystal[3].Refusal.IsEmpty());

		const TArray<FLoadoutScreenChoice> Stones = WoRLoadoutScreen::BuildSocketChoices(Loadout, ECharacterClass::Generic, Hammer, LoadoutScreenSockets::STONE,
																						Carried, {Free}, {});
		TestTrue(TEXT("the stone socket takes the stone only"), Stones.Num() == 1 && Stones[0].Crystal == Stone);
		TestEqual(TEXT("a filled socket offers nothing"),
				  WoRLoadoutScreen::BuildSocketChoices(Loadout, ECharacterClass::Generic, Page.Rows[LOADOUT_SCREEN_TEST_FIRST], LoadoutScreenSockets::CRYSTAL,
													   Carried, {Free}, {}).Num(), 0);
	});

	It("says what X takes off a socket before it does it (B-440c, Q-440c)", [this]()
	{
		FCombatLoadout Loadout = TwoWeapons();
		Loadout.SecondaryWeapon.WeaponEntry.AttachedItem.Kind = EAttachedItemKind::Evolution;
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Generic);
		FText Refusal;
		TestFalse(TEXT("a gem: both sockets come off, destroyed"),
				  WoRLoadoutScreen::GetTakeOffSocketWarning(Page.Rows[LOADOUT_SCREEN_TEST_FIRST], LoadoutScreenSockets::CRYSTAL, Refusal).IsEmpty());
		TestTrue(TEXT("an empty socket: nothing to say"),
				 WoRLoadoutScreen::GetTakeOffSocketWarning(Page.Rows[LOADOUT_SCREEN_TEST_FIRST], LoadoutScreenSockets::STONE, Refusal).IsEmpty());
		TestFalse(TEXT("but a reason"), Refusal.IsEmpty());
		TestFalse(TEXT("an evolution goes back"),
				  WoRLoadoutScreen::GetTakeOffSocketWarning(Page.Rows[LOADOUT_SCREEN_TEST_SECOND], LoadoutScreenSockets::CRYSTAL, Refusal).IsEmpty());
	});

	It("adds a Resonator's adapter in the free place and takes one off (B-440b)", [this]()
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon = Weapon(EWeaponType::Staff);
		Loadout.RingLoadout = {Adapter()};
		const FLoadoutScreenPage Page = WoRLoadoutScreen::BuildGearPage(Loadout, ECharacterClass::Resonator);
		const FLoadoutScreenRow *Free = FindRow(Page, ELoadoutScreenRowKind::EmptyGear);
		const FRingInventoryEntry Spare = Adapter().RingEntry;
		const TArray<FLoadoutScreenChoice> Choices =
			Free ? WoRLoadoutScreen::BuildGearChoices(Loadout, ECharacterClass::Resonator, *Free, {Weapon(EWeaponType::Sword).WeaponEntry}, {Spare}, {})
				 : TArray<FLoadoutScreenChoice>();
		if (!TestEqual(TEXT("only the adapter: an adapter place takes no weapon"), Choices.Num(), 1))
		{
			return;
		}
		FCombatLoadout Added = Loadout;
		TestTrue(TEXT("added"), WoRLoadoutScreen::PutGearChoice(Added, *Free, Choices[0]));
		TestEqual(TEXT("two adapters"), Added.RingLoadout.Num(), 2);

		FText Refusal;
		TestTrue(TEXT("the first adapter comes off"), WoRLoadoutScreen::TakeOffGear(Added, ECharacterClass::Resonator, Page.Rows[LOADOUT_SCREEN_TEST_SECOND], Refusal));
		TestEqual(TEXT("the spare stays"), Added.RingLoadout.Num() == 1 ? Added.RingLoadout[0].RingEntry.PersistentID : FGuid(), Spare.PersistentID);
		TestFalse(TEXT("a free place has nothing to take off"), WoRLoadoutScreen::TakeOffGear(Added, ECharacterClass::Resonator, *Free, Refusal));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
