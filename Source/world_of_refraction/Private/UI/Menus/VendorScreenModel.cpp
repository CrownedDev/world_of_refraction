// VendorScreenModel.cpp

#include "UI/Menus/VendorScreenModel.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Menus/InventoryScreenModel.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRVendorScreen"

namespace VendorScreenBuild
{
	/** The place's currency, then Gear Essence: the first two of the game menu's chips. */
	constexpr int32 ChipCount = 2;

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
		return {};
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
	EVendorService Service;
	return GetService(Tab, Service); // TODO: B-456b onwards: Buy, then the Jeweler's and the Spiritualist's services
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

FString WoRVendorScreen::GetVendorScreenString(UWorld *World, EMerchantType Vendor)
{
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
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			const FVendorScreenPage Page = BuildPage(World, Vendor, Tab, Member.Key);
			Out += FString::Printf(TEXT("\n  %s, %s: %s"), *GetTabName(Tab).ToString(), *Member.Name.ToString(), *Page.EmptyNote.ToString());
			for (const FVendorScreenRow &Row : Page.Rows)
			{
				Out += FString::Printf(TEXT("\n    %s (%s) x%d, %s: %s"), *Row.Name.ToString(), *Row.Tier.ToString(), Row.Row.Count, *Row.KindLine.ToString(),
									   Row.Refusal.IsEmpty() ? *Row.Pay.ToString() : *FString::Printf(TEXT("no: %s"), *Row.Refusal.ToString()));
			}
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GVendorScreenPrintCommand(
	TEXT("wor.UI.PrintVendor"),
	TEXT("DEBUG (B-456a): what the vendor screen would show for the Blacksmith, the Jeweler and the Spiritualist: tabs, rows, pay or refusal."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		for (const EMerchantType Vendor : {EMerchantType::Blacksmith, EMerchantType::Jeweler, EMerchantType::Spiritualist})
		{
			UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRVendorScreen::GetVendorScreenString(World, Vendor));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
