// VendorScreenModel.cpp

#include "UI/Menus/VendorScreenModel.h"

#include "Character/CharacterDataComponent.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Currency/EconomyService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemSlotUpgrade.h"
#include "Loadout/CarriedGearState.h"
#include "Loadout/FLoadoutSlot.h"
#include "Memory/CurseData.h"
#include "Memory/CurseSubsystem.h"
#include "Merchant/MerchantInteractable.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/CurseClearWidget.h"
#include "UI/Menus/InventoryScreenModel.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRVendorScreen"

namespace VendorScreenBuild
{
	/** The place's currency, then Gear Essence: the first two of the game menu's chips. */
	constexpr int32 ChipCount = 2;

	UEconomyService *EconomyOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
	}

	/** "260 Gold, 10 Pillar Essence": the money line always, an essence when charged, as a purchase pays them. */
	FText CostText(const FPurchaseCost &Cost, ECurrencyType Place, bool bMoneyOnly = false)
	{
		TArray<FText> Parts;
		for (const ECurrencyType Balance : PurchaseCostBalances::CHARGED)
		{
			const int32 Amount = Cost.Get(Balance);
			if (Balance == ECurrencyType::Prisms || (Amount > 0 && !bMoneyOnly))
			{
				Parts.Add(FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber(Amount),
										WoRGameNames::GetCurrencyName(PurchaseCostBalances::PaidFrom(Balance, Place))));
			}
		}
		return FText::Join(LOCTEXT("CostSeparator", ", "), Parts);
	}

	bool SameStockLine(const FMerchantStockEntry &A, const FMerchantStockEntry &B)
	{
		return A.IsAssetEntry() ? A.Asset == B.Asset : B.IsCrystalEntry() && A.Crystal == B.Crystal;
	}

	TArray<FMerchantStockEntry> AllStock(const TArray<FVendorCartLine> &Cart)
	{
		TArray<FMerchantStockEntry> Stock;
		for (const FVendorCartLine &Line : Cart)
		{
			Stock.Add(Line.Stock);
		}
		return Stock;
	}

	TArray<FMerchantStockEntry> StockFor(const TArray<FVendorCartLine> &Cart, FName For)
	{
		TArray<FMerchantStockEntry> Stock;
		for (const FVendorCartLine &Line : Cart)
		{
			if (Line.For == For)
			{
				Stock.Add(Line.Stock);
			}
		}
		return Stock;
	}

	FText KindWithElement(const FText &Kind, ESpellElement Element)
	{
		return Element == ESpellElement::None ? Kind : FText::Format(LOCTEXT("KindAndElement", "{0} · {1}"), Kind, WoRGameNames::GetElementName(Element));
	}

	/** "+14 Power vs Iron Sword": Stock against the weapon Member's worn loadout leads with. Empty when either has
	 *  no Power, or Member isn't in this level. */
	FText ComparePower(UWorld *World, const UWeaponData &Stock, FName Member)
	{
		const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return FText::GetEmpty();
		}
		const FCombatLoadout &Loadout = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		const UWeaponData *Worn = Loadout.PrimarySlotType == EPrimarySlotType::Weapon ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
		if (!Worn || Worn->Power <= 0 || Stock.Power <= 0)
		{
			return FText::GetEmpty();
		}
		FNumberFormattingOptions Signed;
		Signed.AlwaysSign = true;
		return FText::Format(LOCTEXT("ComparePower", "{0} Power vs {1}"), FText::AsNumber(Stock.Power - Worn->Power, &Signed),
							 FText::FromString(Worn->GetDisplayName()));
	}

	/** How a stock line reads, in the words the loadout and inventory screens use for the same thing. */
	FVendorScreenRow MakeStockRow(UWorld *World, const FMerchantStockEntry &Entry, FName Member)
	{
		FVendorScreenRow Line;
		Line.Stock = Entry;
		UPrimaryDataAsset *Asset = Entry.Asset;
		if (Entry.IsCrystalEntry())
		{
			WoRInventoryVerbs::FInventoryRow Stack;
			Stack.Item = Entry.Crystal;
			Stack.Count = Entry.Count;
			const FInventoryScreenTile Tile = WoRInventoryScreen::BuildStackTile(Stack);
			Line.Name = Tile.Name;
			Line.Tier = Tile.Tier;
			Line.KindLine = Tile.KindLine;
			Line.Element = Tile.Element;
			Line.Note = Tile.Note;
		}
		else if (const USkillDataBase *Skill = Cast<USkillDataBase>(Asset))
		{
			// TODO: skill and evolution names are FString assets; FText with B-127 (D-061).
			const USpellData *Spell = Cast<USpellData>(Skill);
			Line.Name = FText::FromString(Skill->Name);
			Line.Tier = WoRGameNames::GetTierLetter(Skill->Tier);
			Line.Element = Spell ? Spell->Element : ESpellElement::None;
			Line.KindLine = Spell ? KindWithElement(LOCTEXT("KindSpell", "Spell"), Line.Element) : LOCTEXT("KindAbility", "Ability");
			Line.Note = FText::FromString(Skill->Description);
		}
		else if (const UEvolutionItemData *Evolution = Cast<UEvolutionItemData>(Asset))
		{
			Line.Name = FText::FromString(Evolution->ItemName);
			Line.Tier = WoRGameNames::GetTierLetter(Evolution->Tier);
			Line.Element = Evolution->GetAssociatedElement();
			Line.KindLine = KindWithElement(LOCTEXT("KindEvolution", "Evolution"), Line.Element);
			Line.Note = FText::FromString(Evolution->Description);
		}
		else
		{
			// A weapon or an adapter, as bought: bare, with the weapon's own abilities.
			FLoadoutSlot Slot;
			UWeaponData *Weapon = Cast<UWeaponData>(Asset);
			if (Weapon)
			{
				Slot.Gear = FGearInstance::FromWeaponEntry(FWeaponInventoryEntry::CreateFromWeapon(Weapon));
				for (UAbilityData *Ability : Weapon->PresetAbilities)
				{
					Slot.AssignedAbilities.Add(Ability);
				}
			}
			else if (URingData *Adapter = Cast<URingData>(Asset))
			{
				Slot.Gear = FGearInstance::FromRingEntry(FRingInventoryEntry::CreateFromRing(Adapter));
			}
			if (Slot.IsEmpty())
			{
				Line.Name = FText::FromString(GetNameSafe(Asset)); // not a sellable type: the purchase's refusal greys it
				return Line;
			}
			const FLoadoutScreenRow Gear = WoRLoadoutScreen::BuildGearRow(Slot);
			Line.Name = Gear.Name;
			Line.Tier = Gear.Tier;
			Line.KindLine = Gear.KindLine;
			Line.Element = Slot.Gear.AttachedItem.GetElement();
			Line.Details = Gear.Details;
			const FText Compared = Weapon ? ComparePower(World, *Weapon, Member) : FText::GetEmpty();
			if (!Compared.IsEmpty())
			{
				Line.Details.Add({LOCTEXT("DetailCompared", "Compared"), Compared});
			}
		}
		if (Entry.Count > 1)
		{
			Line.KindLine = FText::Format(LOCTEXT("KindAndBundle", "{0} · x{1}"), Line.KindLine, FText::AsNumber(Entry.Count));
		}
		return Line;
	}

	/** Why Payer can't put Entry in Cart for Member: the purchase's own refusal for that member's lines with Entry
	 *  added, then the wallet against the whole cart. Empty when it can. */
	FText BuyRefusal(UEconomyService &Economy, AActor *Payer, const FMerchantStockEntry &Entry, FName Member, const TArray<FVendorCartLine> &Cart)
	{
		TArray<FMerchantStockEntry> Theirs = StockFor(Cart, Member);
		Theirs.Add(Entry);
		const FText Refusal = Economy.GetPurchaseRefusal(Payer, Member, Theirs);
		if (!Refusal.IsEmpty())
		{
			return Refusal;
		}
		TArray<FMerchantStockEntry> Everything = AllStock(Cart);
		Everything.Add(Entry);
		return UEconomyService::GetAffordRefusal(Payer, Economy.PreviewCartCost(Everything));
	}

	/** The inventory screen's tile for the same piece or stack, so both screens use the same words for it. */
	const FInventoryScreenTile *FindTile(const TArray<FInventoryScreenTile> &Tiles, const WoRVendorServices::FVendorRow &Row)
	{
		return Tiles.FindByPredicate([&Row](const FInventoryScreenTile &Tile)
		{
			return Row.IsGear() ? Tile.Row.GearID == Row.GearID : !Tile.Row.IsGear() && Tile.Row.Item == Row.Item;
		});
	}
}

TArray<EVendorScreenTab> WoRVendorScreen::GetTabs(EMerchantType Vendor)
{
	switch (Vendor)
	{
	case EMerchantType::Blacksmith:
		return {EVendorScreenTab::Buy, EVendorScreenTab::Sell, EVendorScreenTab::Dismantle};
	case EMerchantType::Jeweler:
		return {EVendorScreenTab::Buy, EVendorScreenTab::Sell, EVendorScreenTab::ItemSlots};
	case EMerchantType::Spiritualist:
		return {EVendorScreenTab::Buy,	  EVendorScreenTab::Sell,	   EVendorScreenTab::Repair,
				EVendorScreenTab::Curses, EVendorScreenTab::Dismantle, EVendorScreenTab::Merge};
	default:
		return {EVendorScreenTab::Buy}; // UIScreens #15: the shop is every vendor's; D-193 names the three with services
	}
}

FText WoRVendorScreen::GetTabName(EVendorScreenTab Tab)
{
	switch (Tab)
	{
	case EVendorScreenTab::Buy:
		return LOCTEXT("TabBuy", "Buy");
	case EVendorScreenTab::Sell:
		return WoRGameNames::GetVendorServiceName(EVendorService::Sell);
	case EVendorScreenTab::Dismantle:
		return WoRGameNames::GetVendorServiceName(EVendorService::Dismantle);
	case EVendorScreenTab::ItemSlots:
		return LOCTEXT("TabItemSlots", "Item slots");
	case EVendorScreenTab::Repair:
		return LOCTEXT("TabRepair", "Repair");
	case EVendorScreenTab::Curses:
		return LOCTEXT("TabCurses", "Curses");
	case EVendorScreenTab::Merge:
		return LOCTEXT("TabMerge", "Merge");
	}
	return FText::GetEmpty();
}

bool WoRVendorScreen::GetService(EVendorScreenTab Tab, EVendorService &OutService)
{
	if (Tab != EVendorScreenTab::Sell && Tab != EVendorScreenTab::Dismantle)
	{
		return false;
	}
	OutService = Tab == EVendorScreenTab::Sell ? EVendorService::Sell : EVendorService::Dismantle;
	return true;
}

bool WoRVendorScreen::IsBuilt(EVendorScreenTab Tab)
{
	return true; // B-456d: every tab D-193 names is on the screen
}

FText WoRVendorScreen::GetVendorLine(EMerchantType Vendor)
{
	switch (Vendor)
	{
	case EMerchantType::Blacksmith:
		return LOCTEXT("LineBlacksmith", "weapons, augment stones");
	case EMerchantType::Jeweler:
		return LOCTEXT("LineJeweler", "adapters, item slots");
	case EMerchantType::Spiritualist:
		return LOCTEXT("LineSpiritualist", "evolutions, crystals, repairs");
	default:
		return FText::GetEmpty();
	}
}

FVendorScreenPage WoRVendorScreen::BuildPage(UWorld *World, EMerchantType Vendor, EVendorScreenTab Tab, FName Member)
{
	FVendorScreenPage Page;
	EVendorService Service;
	if (Tab == EVendorScreenTab::Buy)
	{
		return BuildBuyPage(World, nullptr, nullptr, Member, {}); // no vendor to stock it: "Nothing for sale."
	}
	if (Tab == EVendorScreenTab::ItemSlots)
	{
		return BuildItemSlotsPage(World, nullptr); // no one to pay: every row says so
	}
	if (Tab == EVendorScreenTab::Repair)
	{
		return BuildRepairPage(World, nullptr);
	}
	if (Tab == EVendorScreenTab::Curses)
	{
		return BuildCursesPage(World, nullptr);
	}
	if (Tab == EVendorScreenTab::Merge)
	{
		return BuildMergePage(World, Member, {});
	}
	if (!GetService(Tab, Service))
	{
		Page.EmptyNote = LOCTEXT("NotBuilt", "This service isn't on this screen yet.");
		return Page;
	}
	TArray<FInventoryScreenTile> Tiles = WoRInventoryScreen::BuildPage(World, Member, EInventoryScreenTab::Gear).Tiles;
	Tiles.Append(WoRInventoryScreen::BuildPage(World, Member, EInventoryScreenTab::Pouch).Tiles);
	for (const WoRVendorServices::FVendorRow &Row : WoRVendorServices::GatherRows(World, Vendor, Service, Member))
	{
		FVendorScreenRow &Line = Page.Rows.AddDefaulted_GetRef();
		Line.Row = Row;
		Line.Name = Row.Label; // an evolution's label carries its tier already
		Line.KindLine = Row.IsEvolution() ? LOCTEXT("KindEvolution", "Evolution") : FText::GetEmpty();
		if (const FInventoryScreenTile *Tile = Row.IsEvolution() ? nullptr : VendorScreenBuild::FindTile(Tiles, Row))
		{
			Line.Name = Tile->Name;
			Line.Tier = Tile->Tier;
			Line.KindLine = Tile->KindLine;
			Line.Element = Tile->Element;
		}
		Line.Pay = WoRVendorServices::GetPayText(World, Service, Row);
		Line.Refusal = WoRVendorServices::GetRefusal(World, Vendor, Service, Member, Row);
	}
	if (Page.Rows.IsEmpty())
	{
		Page.EmptyNote = Service == EVendorService::Sell ? LOCTEXT("EmptySell", "Nothing of theirs to sell.")
														 : LOCTEXT("EmptyDismantle", "Nothing of theirs to dismantle.");
	}
	if (Service == EVendorService::Dismantle)
	{
		Page.Rule = LOCTEXT("RuleDismantle", "Dismantling is done in the hub, not during a run. What is dismantled is gone until you find it again.");
	}
	return Page;
}

FVendorScreenPage WoRVendorScreen::BuildBuyPage(UWorld *World, const UMerchantData *Merchant, AActor *Payer, FName Member, const TArray<FVendorCartLine> &Cart)
{
	using namespace VendorScreenBuild;

	FVendorScreenPage Page;
	UEconomyService *Economy = EconomyOf(World);
	if (Merchant && Economy)
	{
		const ECurrencyType Place = UEconomyService::GetPlaceCurrency(World);
		// B-179: a trial vendor shows this run's rolled stock; at home the full list.
		const UTrialRunSubsystem *Run = World->GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
		const TArray<FMerchantStockEntry> Stock = Run ? Run->GetMerchantStock(Merchant) : Merchant->Stock;
		for (const FMerchantStockEntry &Entry : Stock)
		{
			if (!Entry.IsAssetEntry() && !Entry.IsCrystalEntry())
			{
				continue; // an empty line isn't for sale (B-437b's leftover lines)
			}
			FVendorScreenRow &Line = Page.Rows.Add_GetRef(MakeStockRow(World, Entry, Member));
			const FPurchaseCost Cost = Economy->PreviewCartCost({Entry});
			Line.Pay = CostText(Cost, Place);
			const FText Money = CostText(Cost, Place, /*bMoneyOnly=*/true);
			Line.PayMoney = Money.EqualTo(Line.Pay) ? FText::GetEmpty() : Money;
			for (const FVendorCartLine &InCart : Cart)
			{
				Line.Row.Count += SameStockLine(InCart.Stock, Entry) ? InCart.Stock.Count : 0;
			}
			Line.Refusal = BuyRefusal(*Economy, Payer, Entry, Member, Cart);
		}
	}
	if (Page.Rows.IsEmpty())
	{
		Page.EmptyNote = LOCTEXT("EmptyBuy", "Nothing for sale.");
	}
	return Page;
}

FVendorScreenPage WoRVendorScreen::BuildItemSlotsPage(UWorld *World, AActor *Payer)
{
	FVendorScreenPage Page;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	const UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
	const auto PriceText = [](int32 SlotCount)
	{
		return FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber(ItemSlotUpgrade::GetPrice(SlotCount)),
							 WoRGameNames::GetCurrencyName(ItemSlotUpgrade::GetCurrency()));
	};
	for (const FLoadoutScreenMember &Member : Pool ? WoRLoadoutScreen::BuildMembers(World) : TArray<FLoadoutScreenMember>())
	{
		FVendorScreenRow &Line = Page.Rows.AddDefaulted_GetRef();
		Line.Member = Member.Key;
		Line.Name = Member.Name;
		Line.Slots = Pool->GetItemSlotCount(Member.Key);
		Line.SlotsMax = InventoryConstants::ITEM_SLOTS_MAX;
		Line.KindLine = FText::Format(LOCTEXT("SlotsOf", "{0} of {1}"), FText::AsNumber(Line.Slots), FText::AsNumber(Line.SlotsMax));
		Line.Refusal = ItemSlotUpgrade::GetRefusal(World, *Pool, Member.Key, Wallet);
		if (!ItemSlotUpgrade::CanGrow(Line.Slots))
		{
			Line.Pay = LOCTEXT("SlotsFull", "full");
			continue;
		}
		Line.Pay = PriceText(Line.Slots);
		Line.Note = FText::Format(LOCTEXT("SlotsAfter", "{0}'s pouch will hold {1} kinds of item."), Member.Name, FText::AsNumber(Line.Slots + 1));
		if (ItemSlotUpgrade::CanGrow(Line.Slots + 1))
		{
			Line.Details.Add({LOCTEXT("DetailNextSlot", "Next slot after this"), PriceText(Line.Slots + 1)});
		}
	}
	if (Page.Rows.IsEmpty())
	{
		Page.EmptyNote = LOCTEXT("EmptyItemSlots", "No one in the party to buy a slot for.");
	}
	Page.Intro = FText::Format(LOCTEXT("IntroItemSlots", "Each slot lets a character's pouch hold one more kind of item ({0} to start, {1} at most)."),
							   FText::AsNumber(InventoryConstants::ITEM_SLOTS_START), FText::AsNumber(InventoryConstants::ITEM_SLOTS_MAX));
	Page.Rule = LOCTEXT("RuleItemSlots", "Item slots are sold in the hub, not during a run.");
	return Page;
}

FText WoRVendorScreen::GetItemSlotConfirmText(const FVendorScreenRow &Row)
{
	return FText::Format(LOCTEXT("ConfirmItemSlot", "Buy an item slot for {0} for {1}?"), Row.Name, Row.Pay);
}

FText WoRVendorScreen::BuyItemSlot(UWorld *World, AActor *Payer, const FVendorScreenRow &Row)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	FText Refusal = LOCTEXT("PurchaseFailed", "The purchase didn't go through.");
	return Pool && ItemSlotUpgrade::TryBuy(World, *Pool, Row.Member, CurrencyWallet::FindFor(Payer), Refusal) ? FText::GetEmpty() : Refusal;
}

namespace VendorScreenSpiritualist
{
	UParty *PartyOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		return PartySession ? PartySession->GetLocalParty() : nullptr;
	}

	UCurseSubsystem *CursesOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCurseSubsystem>() : nullptr;
	}

	FText MoneyText(int32 Amount, const UObject *Context)
	{
		return FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber(Amount), WoRGameNames::GetCurrencyName(UEconomyService::GetPlaceCurrency(Context)));
	}

	FText MemberName(const UWorld *World, const FPartyMember &Member)
	{
		const FName Key = UPoolSubsystem::GetCharacterKey(Member);
		for (const FLoadoutScreenMember &Shown : WoRLoadoutScreen::BuildMembers(World))
		{
			if (Shown.Key == Key)
			{
				return Shown.Name;
			}
		}
		return FText::FromName(Key);
	}

	/** A repair row's piece: its name, tier and element, and how worn it is. */
	void DescribePiece(const FCarriedGearState &Gear, const GearRepair::FWornPiece &Piece, FVendorScreenRow &Line, int32 &OutCurrent, int32 &OutMax)
	{
		if (Piece.AttachmentIndex == INDEX_NONE)
		{
			const FEvolutionAttachment &Evolution = Gear.PrimaryEvolution;
			// TODO: evolution names are FString assets; FText with B-127 (D-061).
			Line.Name = FText::FromString(Evolution.Item ? Evolution.Item->ItemName : FString());
			Line.Tier = WoRGameNames::GetTierLetter(Evolution.Tier);
			Line.Element = Evolution.Item ? Evolution.Item->GetAssociatedElement() : ESpellElement::None;
			OutCurrent = Evolution.CurrentDurability;
			OutMax = Evolution.Item ? Evolution.Item->MaxDurability : 0;
			return;
		}
		const FCarriedAttachment &Carried = Gear.Attachments[Piece.AttachmentIndex];
		const FRuntimeAttachedItem &Attached = Carried.Attached;
		if (Attached.IsEvolution())
		{
			Line.Name = FText::FromString(Attached.Evolution.Item ? Attached.Evolution.Item->ItemName : FString());
			Line.Tier = WoRGameNames::GetTierLetter(Attached.Evolution.Tier);
		}
		else if (Attached.IsFusion())
		{
			const FCrystalId &A = Attached.Fusion.Id.HalfA;
			const FCrystalId &B = Attached.Fusion.Id.HalfB;
			Line.Name = FText::Format(LOCTEXT("FusionName", "{0} + {1}"), WoRGameNames::GetCrystalTypeName(A.Type), WoRGameNames::GetCrystalTypeName(B.Type));
			Line.Tier = WoRGameNames::GetTierLetter(FMath::Max(A.Tier, B.Tier));
		}
		else
		{
			Line.Name = WoRGameNames::GetCrystalTypeName(Attached.Crystal.Id.Type);
			Line.Tier = WoRGameNames::GetTierLetter(Attached.Crystal.Id.Tier);
		}
		Line.Element = Attached.GetElement();
		OutCurrent = Attached.GetCurrentDurability();
		OutMax = Attached.GetMaxDurability();
	}

	/** "F x4 · E x2 · D x1": what Inventory holds of Type, lowest tier first. */
	FText HoldingsText(const UCrystalInventoryComponent &Inventory, ECrystalType Type)
	{
		TArray<FText> Parts;
		for (int32 Tier = 0; Tier < FCrystalMergePlan::TIER_COUNT; ++Tier)
		{
			if (const int32 Count = Inventory.GetCount(FCrystalId(Type, TierHelpers::GetTierFromValue(Tier))))
			{
				Parts.Add(FText::Format(LOCTEXT("TierCount", "{0} x{1}"), WoRGameNames::GetTierLetter(TierHelpers::GetTierFromValue(Tier)), FText::AsNumber(Count)));
			}
		}
		return FText::Join(LOCTEXT("HoldingsSeparator", " · "), Parts);
	}

	/** "F x4, E x2" for the plan's uses; "nothing" for none. */
	FText TiersText(const TArray<EItemTier> &Tiers)
	{
		TMap<EItemTier, int32> Counts;
		for (const EItemTier Tier : Tiers)
		{
			++Counts.FindOrAdd(Tier);
		}
		Counts.KeySort([](EItemTier A, EItemTier B) { return A < B; });
		TArray<FText> Parts;
		for (const TPair<EItemTier, int32> &Count : Counts)
		{
			Parts.Add(FText::Format(LOCTEXT("TierCount", "{0} x{1}"), WoRGameNames::GetTierLetter(Count.Key), FText::AsNumber(Count.Value)));
		}
		return Parts.IsEmpty() ? LOCTEXT("Nothing", "nothing") : FText::Join(LOCTEXT("TiersSeparator", ", "), Parts);
	}

	TArray<EItemTier> UsesOf(const FCrystalMergePlan &Plan)
	{
		TArray<EItemTier> Uses;
		for (int32 Tier = 0; Tier < FCrystalMergePlan::TIER_COUNT; ++Tier)
		{
			for (int32 Count = 0; Count < Plan.Uses[Tier]; ++Count)
			{
				Uses.Add(TierHelpers::GetTierFromValue(Tier));
			}
		}
		return Uses;
	}

	/** The highest tier Owner holds enough Type for (its plan uses something), else E. */
	EItemTier DefaultMergeTarget(AActor *Owner, ECrystalType Type)
	{
		EItemTier Best = EItemTier::E_Tier;
		for (int32 Tier = TierHelpers::GetTierValue(EItemTier::E_Tier); Tier < FCrystalMergePlan::TIER_COUNT; ++Tier)
		{
			FCrystalMergePlan Plan;
			UEconomyService::GetMergeRefusal(Owner, Type, TierHelpers::GetTierFromValue(Tier), &Plan);
			if (!UsesOf(Plan).IsEmpty())
			{
				Best = TierHelpers::GetTierFromValue(Tier);
			}
		}
		return Best;
	}

	AActor *MemberOwner(UWorld *World, FName Member)
	{
		const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
		return Inventory ? Inventory->GetOwner() : nullptr;
	}
}

FVendorScreenPage WoRVendorScreen::BuildRepairPage(UWorld *World, AActor *Payer)
{
	using namespace VendorScreenSpiritualist;

	FVendorScreenPage Page;
	const UParty *Party = PartyOf(World);
	const UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
	for (const GearRepair::FWornPiece &Piece : Party ? GearRepair::GatherWornPieces(*Party) : TArray<GearRepair::FWornPiece>())
	{
		const FCarriedGearState &Gear = Party->Members[Piece.MemberIndex].CarriedGear;
		FVendorScreenRow &Line = Page.Rows.AddDefaulted_GetRef();
		Line.Piece = Piece;
		int32 Current = 0;
		int32 Max = 0;
		DescribePiece(Gear, Piece, Line, Current, Max);
		const UEquipmentDataBase *Holder = Piece.AttachmentIndex == INDEX_NONE ? nullptr : Cast<UEquipmentDataBase>(Gear.Attachments[Piece.AttachmentIndex].Holder);
		const FText Where = Holder ? FText::FromString(Holder->GetDisplayName()) : LOCTEXT("PrimarySeat", "Evolution seat");
		Line.KindLine = FText::Format(LOCTEXT("OwnerAndHolder", "{0} · {1}"), MemberName(World, Party->Members[Piece.MemberIndex]), Where);
		Line.bStateIsBad = Current <= 0;
		Line.State = Line.bStateIsBad ? LOCTEXT("Broken", "broken") : FText::AsPercent(Max > 0 ? static_cast<double>(Current) / Max : 0.0);
		Line.Pay = MoneyText(Piece.Cost, World);
		Line.Details.Add({LOCTEXT("DetailDurability", "Durability"), FText::Format(LOCTEXT("DurabilityOf", "{0} of {1}"), FText::AsNumber(Current), FText::AsNumber(Max))});
		Line.Refusal = GearRepair::GetRefusal(Wallet, Piece.Cost);
	}
	if (Page.Rows.IsEmpty())
	{
		Page.EmptyNote = LOCTEXT("EmptyRepair", "Nothing is worn.");
	}
	const int32 AllCost = Party ? GearRepair::GetPartyRepairCost(*Party) : 0;
	Page.AllPay = MoneyText(AllCost, World);
	Page.AllRefusal = GearRepair::GetRefusal(Wallet, AllCost);
	Page.Rule = LOCTEXT("RuleRepair", "Gear wears in a trial's fights. A repair brings it back to full; when the run ends it comes back whole anyway.");
	return Page;
}

FVendorScreenPage WoRVendorScreen::BuildCursesPage(UWorld *World, AActor *Payer)
{
	using namespace VendorScreenSpiritualist;

	FVendorScreenPage Page;
	const UCurseSubsystem *Curses = CursesOf(World);
	TArray<FName> Held = Curses ? Curses->GetHeldCurses().Array() : TArray<FName>();
	Held.Sort([](const FName &A, const FName &B) { return A.LexicalLess(B); });
	const bool bHere = UCurseClearWidget::CanClearHere(World);
	const UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
	for (const FName CurseId : Held)
	{
		const UCurseData *Curse = Curses->FindCurseAsset(CurseId);
		const UCurseData *Priced = Curse ? Curse : GetDefault<UCurseData>(); // Q-390: no asset prices as the defaults
		FVendorScreenRow &Line = Page.Rows.AddDefaulted_GetRef();
		Line.CurseId = CurseId;
		Line.Name = Curse && !Curse->Title.IsEmpty() ? Curse->Title : FText::FromName(CurseId);
		Line.Tier = WoRGameNames::GetTierLetter(Priced->Tier);
		Line.KindLine = Curse && !Curse->Description.IsEmpty() ? Curse->Description : LOCTEXT("CurseHeld", "A curse you hold");
		Line.Pay = FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber(Priced->GetClearPrice()), WoRGameNames::GetCurrencyName(Priced->GetClearCurrency()));
		Line.Note = LOCTEXT("CurseNote", "It is cleared. It may come back.");
		Line.Refusal = bHere ? Curses->GetClearRefusal(CurseId, Wallet) : LOCTEXT("CursesInRun", "Curses can be cleared in the hub, not during a run.");
	}
	if (Page.Rows.IsEmpty())
	{
		Page.EmptyNote = LOCTEXT("EmptyCurses", "You hold no curses.");
	}
	Page.Rule = LOCTEXT("RuleCurses", "Curses can be cleared in the hub, not during a run.");
	return Page;
}

EItemTier WoRVendorScreen::StepMergeTarget(EItemTier Current, int32 Delta)
{
	const int32 First = TierHelpers::GetTierValue(EItemTier::E_Tier); // F is the floor, never a target
	const int32 Count = FCrystalMergePlan::TIER_COUNT - First;
	const int32 Index = FMath::Clamp(TierHelpers::GetTierValue(Current), First, FCrystalMergePlan::TIER_COUNT - 1) - First;
	return TierHelpers::GetTierFromValue(First + ((Index + Delta) % Count + Count) % Count);
}

FVendorScreenPage WoRVendorScreen::BuildMergePage(UWorld *World, FName Member, const TMap<ECrystalType, EItemTier> &Targets)
{
	using namespace VendorScreenSpiritualist;

	FVendorScreenPage Page;
	AActor *Owner = MemberOwner(World, Member);
	const UCrystalInventoryComponent *Crystals = Owner ? Owner->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
	TArray<ECrystalType> Kinds;
	for (const WoRInventoryVerbs::FInventoryRow &Owned : Crystals ? WoRInventoryVerbs::GatherRows(World, Member) : TArray<WoRInventoryVerbs::FInventoryRow>())
	{
		// A stat stone keeps its sub-stat apart from its type, so it isn't merged here; chest stacks aren't the member's.
		if (!Owned.IsGear() && !Owned.bInChest && Owned.Item.Type != ECrystalType::None && !CrystalTypeHelpers::IsStatStone(Owned.Item.Type))
		{
			Kinds.AddUnique(Owned.Item.Type);
		}
	}
	for (const ECrystalType Type : Kinds)
	{
		const EItemTier *Chosen = Targets.Find(Type);
		const EItemTier Target = Chosen ? *Chosen : DefaultMergeTarget(Owner, Type);
		FCrystalMergePlan Plan;
		FVendorScreenRow &Line = Page.Rows.AddDefaulted_GetRef();
		Line.Refusal = UEconomyService::GetMergeRefusal(Owner, Type, Target, &Plan);
		Line.Row.Item = FCrystalId(Type, Target);
		Line.Name = WoRGameNames::GetCrystalTypeName(Type);
		Line.Tier = WoRGameNames::GetTierLetter(Target);
		Line.KindLine = HoldingsText(*Crystals, Type);
		Line.Element = ItemIdentity::GetElement(Line.Row.Item);
		Line.Pay = FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber(Plan.Price), WoRGameNames::GetCurrencyName(Plan.Pays));
		const TArray<EItemTier> Uses = UsesOf(Plan);
		if (!Uses.IsEmpty())
		{
			Line.Details.Add({LOCTEXT("DetailUses", "Uses"), TiersText(Uses)});
			Line.Details.Add({LOCTEXT("DetailMakes", "Makes"), TiersText({Target})});
			Line.Details.Add({LOCTEXT("DetailChange", "Change back"), TiersText(Plan.Change)});
		}
	}
	if (Page.Rows.IsEmpty())
	{
		Page.EmptyNote = LOCTEXT("EmptyMerge", "No crystals of theirs to merge.");
	}
	Page.Intro = LOCTEXT("IntroMerge", "Left / Right picks the tier to make.");
	Page.Rule = LOCTEXT("RuleMerge", "Anything over the target comes back as the fewest crystals of the same kind.");
	return Page;
}

FText WoRVendorScreen::GetConfirmText(EVendorScreenTab Tab, const FVendorScreenRow &Row)
{
	switch (Tab)
	{
	case EVendorScreenTab::Repair:
		return FText::Format(LOCTEXT("ConfirmRepair", "Repair {0} for {1}?"), Row.Name, Row.Pay);
	case EVendorScreenTab::Curses:
		return FText::Format(LOCTEXT("ConfirmCurse", "Clear {0} for {1}? It may come back."), Row.Name, Row.Pay);
	case EVendorScreenTab::Merge:
		return FText::Format(LOCTEXT("ConfirmMerge", "Merge into {0} ({1}) for {2}?"), Row.Name, Row.Tier, Row.Pay);
	default:
		return FText::GetEmpty();
	}
}

FText WoRVendorScreen::RunService(UWorld *World, AActor *Payer, FName Member, EVendorScreenTab Tab, const FVendorScreenRow &Row)
{
	using namespace VendorScreenSpiritualist;

	if (!Row.Refusal.IsEmpty())
	{
		return Row.Refusal;
	}
	const FText Failed = LOCTEXT("ServiceDidntGoThrough", "It didn't go through.");
	if (Tab == EVendorScreenTab::Repair)
	{
		UParty *Party = PartyOf(World);
		UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
		int32 Spent = 0;
		return Party && Wallet && GearRepair::RepairPiece(*Party, Row.Piece, *Wallet, Spent) ? FText::GetEmpty() : Failed;
	}
	if (Tab == EVendorScreenTab::Curses)
	{
		UCurseSubsystem *Curses = CursesOf(World);
		if (!Curses || !UCurseClearWidget::CanClearHere(World))
		{
			return LOCTEXT("CursesInRun", "Curses can be cleared in the hub, not during a run.");
		}
		FText Refusal;
		return Curses->PayToClearCurse(Row.CurseId, CurrencyWallet::FindFor(Payer), Refusal) ? FText::GetEmpty() : Refusal;
	}
	if (Tab == EVendorScreenTab::Merge)
	{
		AActor *Owner = MemberOwner(World, Member);
		UEconomyService *Economy = VendorScreenBuild::EconomyOf(World);
		const FText Refusal = UEconomyService::GetMergeRefusal(Owner, Row.Row.Item.Type, Row.Row.Item.Tier);
		if (!Refusal.IsEmpty())
		{
			return Refusal;
		}
		return Economy && Economy->MergeCrystals(Owner, Row.Row.Item.Type, Row.Row.Item.Tier) ? FText::GetEmpty() : Failed;
	}
	return Failed;
}

FText WoRVendorScreen::GetDoneText(EVendorScreenTab Tab, const FVendorScreenRow &Row)
{
	switch (Tab)
	{
	case EVendorScreenTab::Repair:
		return FText::Format(LOCTEXT("DoneRepair", "{0} is repaired."), Row.Name);
	case EVendorScreenTab::Curses:
		return FText::Format(LOCTEXT("DoneCurse", "{0} is cleared. It may come back."), Row.Name);
	case EVendorScreenTab::Merge:
		return FText::Format(LOCTEXT("DoneMerge", "Made {0} ({1})."), Row.Name, Row.Tier);
	default:
		return FText::GetEmpty();
	}
}

FText WoRVendorScreen::GetRepairAllConfirmText(const FVendorScreenPage &Page)
{
	return FText::Format(LOCTEXT("ConfirmRepairAll", "Repair everything for {0}?"), Page.AllPay);
}

FText WoRVendorScreen::RepairAll(UWorld *World, AActor *Payer)
{
	UParty *Party = VendorScreenSpiritualist::PartyOf(World);
	UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
	const FText Refusal = GearRepair::GetRefusal(Wallet, Party ? GearRepair::GetPartyRepairCost(*Party) : 0);
	if (!Refusal.IsEmpty())
	{
		return Refusal;
	}
	int32 Spent = 0;
	return GearRepair::RepairParty(*Party, *Wallet, Spent) ? FText::GetEmpty() : LOCTEXT("ServiceDidntGoThrough", "It didn't go through.");
}

FText WoRVendorScreen::AddToCart(AActor *Payer, const FVendorScreenRow &Row, const FLoadoutScreenMember &Member, TArray<FVendorCartLine> &Cart)
{
	if (!Row.Refusal.IsEmpty())
	{
		return Row.Refusal;
	}
	// A purchase honours Count for gear and crystals alone; a skill or an evolution is one per line.
	const UObject *Asset = Row.Stock.Asset;
	const bool bStacks = Row.Stock.IsCrystalEntry() || (Asset && (Asset->IsA<UWeaponData>() || Asset->IsA<URingData>()));
	const int32 Units = bStacks ? FMath::Max(1, Row.Stock.Count) : 1;
	FVendorCartLine *Line = !bStacks ? nullptr : Cart.FindByPredicate([&Row, &Member](const FVendorCartLine &InCart)
	{
		return InCart.For == Member.Key && VendorScreenBuild::SameStockLine(InCart.Stock, Row.Stock);
	});
	if (Line)
	{
		Line->Stock.Count += Units;
		return FText::GetEmpty();
	}
	const FName PayerKey = UPoolSubsystem::GetCharacterKey(Payer ? Payer->FindComponentByClass<UCharacterDataComponent>() : nullptr);
	FVendorCartLine &Added = Cart.AddDefaulted_GetRef();
	Added.Stock = Row.Stock;
	Added.Stock.Count = Units;
	Added.For = Member.Key;
	Added.ForName = Member.Key == PayerKey ? WoRGameNames::GetYourselfWord() : Member.Name;
	Added.Name = Row.Name;
	return FText::GetEmpty();
}

bool WoRVendorScreen::RemoveFromCart(const FVendorScreenRow &Row, FName Member, TArray<FVendorCartLine> &Cart)
{
	const auto IsLine = [&Row](const FVendorCartLine &InCart) { return VendorScreenBuild::SameStockLine(InCart.Stock, Row.Stock); };
	int32 Index = Cart.IndexOfByPredicate([&IsLine, Member](const FVendorCartLine &InCart) { return InCart.For == Member && IsLine(InCart); });
	if (Index == INDEX_NONE)
	{
		Index = Cart.FindLastByPredicate(IsLine);
	}
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Cart[Index].Stock.Count -= FMath::Max(1, Row.Stock.Count);
	if (Cart[Index].Stock.Count <= 0)
	{
		Cart.RemoveAt(Index);
	}
	return true;
}

FVendorCartSummary WoRVendorScreen::BuildCartSummary(UWorld *World, AActor *Payer, const TArray<FVendorCartLine> &Cart)
{
	FVendorCartSummary Summary;
	const UEconomyService *Economy = VendorScreenBuild::EconomyOf(World);
	if (!Economy || Cart.IsEmpty())
	{
		return Summary;
	}
	const ECurrencyType Place = UEconomyService::GetPlaceCurrency(World);
	const FPurchaseCost Cost = Economy->PreviewCartCost(VendorScreenBuild::AllStock(Cart));
	const UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
	Summary.Total = VendorScreenBuild::CostText(Cost, Place);
	Summary.After = FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber((Wallet ? Wallet->GetBalance(Place) : 0) - Cost.Prisms),
								  WoRGameNames::GetCurrencyName(Place));
	Summary.Refusal = UEconomyService::GetAffordRefusal(Payer, Cost);
	return Summary;
}

FText WoRVendorScreen::GetCartLineText(UWorld *World, const FVendorCartLine &Line)
{
	const UEconomyService *Economy = VendorScreenBuild::EconomyOf(World);
	const FText Price = Economy ? VendorScreenBuild::CostText(Economy->PreviewCartCost({Line.Stock}), UEconomyService::GetPlaceCurrency(World)) : FText::GetEmpty();
	const FText What = Line.Stock.Count > 1 ? FText::Format(LOCTEXT("CartCount", "{0} x{1}"), Line.Name, FText::AsNumber(Line.Stock.Count)) : Line.Name;
	return FText::Format(LOCTEXT("CartLine", "{0}, for {1}: {2}"), What, Line.ForName, Price);
}

FText WoRVendorScreen::PurchaseCart(UWorld *World, AActor *Payer, TArray<FVendorCartLine> &Cart)
{
	UEconomyService *Economy = VendorScreenBuild::EconomyOf(World);
	if (!Economy || Cart.IsEmpty())
	{
		return LOCTEXT("CartEmpty", "The cart is empty.");
	}
	const FText Short = BuildCartSummary(World, Payer, Cart).Refusal;
	if (!Short.IsEmpty())
	{
		return Short;
	}
	// shortcut: one receiver's lines are one all-or-nothing purchase, not the whole cart: a later receiver's refusal
	// leaves the earlier ones bought (their lines have left the cart). Upgrade when PurchaseFor takes mixed receivers.
	while (!Cart.IsEmpty())
	{
		const FName For = Cart[0].For;
		const FText ForName = Cart[0].ForName;
		const TArray<FMerchantStockEntry> Theirs = VendorScreenBuild::StockFor(Cart, For);
		FText Refusal = Economy->GetPurchaseRefusal(Payer, For, Theirs);
		if (Refusal.IsEmpty() && !Economy->PurchaseFor(Payer, For, Theirs))
		{
			Refusal = LOCTEXT("PurchaseFailed", "The purchase didn't go through.");
		}
		if (!Refusal.IsEmpty())
		{
			return FText::Format(LOCTEXT("PurchaseRefusedFor", "For {0}: {1}"), ForName, Refusal);
		}
		Cart.RemoveAll([For](const FVendorCartLine &Line) { return Line.For == For; });
	}
	return FText::GetEmpty();
}

const UMerchantData *WoRVendorScreen::FindMerchant(const UWorld *World, EMerchantType Vendor)
{
	if (World)
	{
		for (TActorIterator<AMerchantInteractable> It(World); It; ++It)
		{
			if (It->Merchant && It->Merchant->MerchantType == Vendor)
			{
				return It->Merchant;
			}
		}
	}
	return nullptr;
}

int32 WoRVendorScreen::GetTakeCount(const FVendorScreenRow &Row, bool bWholeStack)
{
	return Row.Row.IsGear() || Row.Row.IsEvolution() || !bWholeStack ? 1 : FMath::Max(1, Row.Row.Count);
}

TArray<FPauseMenuChip> WoRVendorScreen::BuildChips(const UWorld *World)
{
	TArray<FPauseMenuChip> Chips = WoRPauseMenu::BuildHeader(World).Chips;
	Chips.SetNum(FMath::Min(Chips.Num(), VendorScreenBuild::ChipCount));
	return Chips;
}

FString WoRVendorScreen::GetVendorScreenString(UWorld *World, EMerchantType Vendor, AActor *Payer)
{
	const UMerchantData *Merchant = FindMerchant(World, Vendor);
	FString Out = FString::Printf(TEXT("[VendorScreen] %s (%s)"), *WoRGameNames::GetMerchantTypeName(Vendor).ToString(), *GetVendorLine(Vendor).ToString());
	for (const FPauseMenuChip &Chip : BuildChips(World))
	{
		Out += FString::Printf(TEXT("  %s %s"), *Chip.Value.ToString(), *Chip.Label.ToString());
	}
	for (const EVendorScreenTab Tab : GetTabs(Vendor))
	{
		if (!IsBuilt(Tab))
		{
			Out += FString::Printf(TEXT("\n  %s: not built yet"), *GetTabName(Tab).ToString());
			continue;
		}
		const auto AddRows = [&Out, Tab](const FText &Whose, const FVendorScreenPage &Page)
		{
			Out += FString::Printf(TEXT("\n  %s, %s: %s"), *GetTabName(Tab).ToString(), *Whose.ToString(), *Page.EmptyNote.ToString());
			for (const FVendorScreenRow &Row : Page.Rows)
			{
				Out += FString::Printf(TEXT("\n    %s (%s) x%d, %s: %s"), *Row.Name.ToString(), *Row.Tier.ToString(), Row.Row.Count, *Row.KindLine.ToString(),
									   Row.Refusal.IsEmpty() ? *Row.Pay.ToString() : *FString::Printf(TEXT("no: %s"), *Row.Refusal.ToString()));
			}
		};
		if (Tab == EVendorScreenTab::ItemSlots || Tab == EVendorScreenTab::Repair || Tab == EVendorScreenTab::Curses)
		{
			// Their rows name the members, or (curses) are the player's.
			AddRows(LOCTEXT("WholeParty", "the party"), Tab == EVendorScreenTab::ItemSlots ? BuildItemSlotsPage(World, Payer)
														: Tab == EVendorScreenTab::Repair  ? BuildRepairPage(World, Payer)
																						   : BuildCursesPage(World, Payer));
			continue;
		}
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			AddRows(Member.Name, Tab == EVendorScreenTab::Buy ? BuildBuyPage(World, Merchant, Payer, Member.Key, {}) : BuildPage(World, Vendor, Tab, Member.Key));
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GVendorScreenPrintCommand(
	TEXT("wor.UI.PrintVendor"),
	TEXT("DEBUG (B-456a to B-456d): what the vendor screen would show for the Blacksmith, the Jeweler and the Spiritualist: tabs, rows (Buy: the stock of the vendor in this level; Item slots: each member's pouch; Repair: each worn piece; Curses: each held curse; Merge: each crystal kind at its default tier), pay or refusal."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		for (const EMerchantType Vendor : {EMerchantType::Blacksmith, EMerchantType::Jeweler, EMerchantType::Spiritualist})
		{
			UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRVendorScreen::GetVendorScreenString(World, Vendor, PC ? PC->GetPawn() : nullptr));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
