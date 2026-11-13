// VendorServices.cpp

#include "Shop/VendorServices.h"

#include "Currency/CurrencyTypes.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryVerbs.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRVendorServices"

namespace VendorServicesBuild
{
	UPoolSubsystem *PoolOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	UEconomyService *EconomyOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
	}

	/** D-193: the Spiritualist's Dismantle is for evolutions; the Blacksmith's for gear and crystals. */
	bool TakesEvolutions(EMerchantType Vendor, EVendorService Service)
	{
		return Service == EVendorService::Dismantle && Vendor == EMerchantType::Spiritualist;
	}

	FText AmountOf(int32 Amount, ECurrencyType Currency)
	{
		return FText::Format(LOCTEXT("Amount", "{0} {1}"), FText::AsNumber(Amount), WoRGameNames::GetCurrencyName(Currency));
	}
}

bool WoRVendorServices::IsInRun(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return (Pool && Pool->IsRunLayerActive()) || (Run && Run->IsRunActive());
}

bool WoRVendorServices::Offers(EMerchantType Vendor, EVendorService Service)
{
	switch (Vendor)
	{
	case EMerchantType::Blacksmith:
	case EMerchantType::Spiritualist:
		return true; // Sell and Dismantle
	case EMerchantType::Jeweler:
		return Service == EVendorService::Sell;
	default:
		return false;
	}
}

TArray<WoRVendorServices::FVendorRow> WoRVendorServices::GatherRows(UWorld *World, EMerchantType Vendor, EVendorService Service, FName Member)
{
	TArray<FVendorRow> Rows;
	const UPoolSubsystem *Pool = VendorServicesBuild::PoolOf(World);
	if (!Pool || Member.IsNone() || !Offers(Vendor, Service))
	{
		return Rows;
	}
	if (VendorServicesBuild::TakesEvolutions(Vendor, Service))
	{
		const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
		const AActor *Owner = Inventory ? Inventory->GetOwner() : nullptr;
		const UEvolutionInventoryComponent *Evolutions = Owner ? Owner->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
		if (Evolutions)
		{
			for (const FEvolutionInventoryEntry &Entry : Evolutions->Entries)
			{
				if (!Entry.Item)
				{
					continue;
				}
				FVendorRow Row;
				Row.EvolutionID = Entry.InstanceID;
				Row.Tier = Entry.Tier;
				Row.Count = 1;
				Row.Label = FText::Format(LOCTEXT("EvolutionLabel", "{0} ({1})"), FText::FromString(Entry.Item->ItemName), WoRGameNames::GetTierLetter(Entry.Tier));
				Rows.Add(MoveTemp(Row));
			}
		}
		return Rows;
	}
	// The member's own gear and pouch stacks, as the inventory lists them (the chest's rows aren't a vendor's).
	for (const WoRInventoryVerbs::FInventoryRow &Owned : WoRInventoryVerbs::GatherRows(World, Member))
	{
		if (Owned.bInChest)
		{
			continue;
		}
		FVendorRow Row;
		Row.GearID = Owned.GearID;
		Row.Item = Owned.Item;
		Row.Count = Owned.Count;
		Row.Label = Owned.Label;
		Row.Tier = Owned.Item.Tier;
		if (Owned.IsGear())
		{
			const FWeaponInventoryEntry *Weapon = Pool->FindWeaponInPool(Owned.GearID);
			const FRingInventoryEntry *Adapter = Weapon ? nullptr : Pool->FindRingInPool(Owned.GearID);
			Row.Tier = Weapon ? Weapon->Tier : Adapter ? Adapter->Tier : EItemTier::F_Tier;
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

FText WoRVendorServices::GetRefusal(UWorld *World, EMerchantType Vendor, EVendorService Service, FName Member, const FVendorRow &Row)
{
	const UPoolSubsystem *Pool = VendorServicesBuild::PoolOf(World);
	if (!Pool || Member.IsNone())
	{
		return LOCTEXT("NoStore", "No character's things to act on");
	}
	if (!Offers(Vendor, Service))
	{
		return FText::Format(LOCTEXT("NotOffered", "The {0} doesn't do that"), WoRGameNames::GetMerchantTypeName(Vendor));
	}
	const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
	if (!Inventory)
	{
		return LOCTEXT("NotHere", "That character isn't here to do it");
	}
	if (Service == EVendorService::Dismantle && IsInRun(World))
	{
		return LOCTEXT("DismantleInHub", "Dismantling is done in the hub, not during a run"); // D-193
	}
	if (Row.IsEvolution() != VendorServicesBuild::TakesEvolutions(Vendor, Service))
	{
		return FText::Format(LOCTEXT("WrongVendor", "The {0} doesn't take that"), WoRGameNames::GetMerchantTypeName(Vendor));
	}
	if (Row.IsEvolution() && Inventory->IsEvolutionInUse(Row.EvolutionID))
	{
		// A loadout's primary, or attached to a weapon, an adapter or worn apparel: it would be torn out unasked.
		return LOCTEXT("EvolutionInUse", "It's in use (a loadout's primary, or attached to gear). Take it off first");
	}
	if (Row.IsGear())
	{
		if (Inventory->IsPrimaryInAnyLoadout(Row.GearID))
		{
			return LOCTEXT("PrimaryStays", "This is a loadout's primary. Swap in another primary first");
		}
		if (Pool->IsRunLayerActive() && !Pool->IsRunOnly(Row.GearID))
		{
			return LOCTEXT("HomeOnRun", "Gear from home can't go during a run; it comes back when the run ends");
		}
	}
	return FText::GetEmpty();
}

FText WoRVendorServices::GetPayText(UWorld *World, EVendorService Service, const FVendorRow &Row)
{
	if (Service == EVendorService::Sell)
	{
		return VendorServicesBuild::AmountOf(UEconomyService::GetSellPriceForTier(Row.Tier), UEconomyService::GetPlaceCurrency(World));
	}
	if (Row.IsEvolution())
	{
		// Elemental essence, or Gear for an evolution with no element (ER-30): the amount is the same either way.
		return FText::Format(LOCTEXT("EvolutionYield", "{0} essence"), FText::AsNumber(EconomyYield::GetLevelingEssenceYieldForTier(Row.Tier)));
	}
	return Row.IsGear() ? VendorServicesBuild::AmountOf(EconomyYield::GetLevelingEssenceYieldForTier(Row.Tier), ECurrencyType::GearEssence)
						: VendorServicesBuild::AmountOf(EconomyYield::GetTypedEssenceYieldForTier(Row.Tier), EconomyYield::GetCrystalEssence(Row.Item.Type));
}

FText WoRVendorServices::GetConfirmText(UWorld *World, EVendorService Service, FName Member, const FVendorRow &Row)
{
	const FText Pay = GetPayText(World, Service, Row);
	const FText Ask = Service == EVendorService::Sell
						  ? FText::Format(LOCTEXT("ConfirmSell", "Sell {0} for {1}?"), Row.Label, Pay)
						  : FText::Format(LOCTEXT("ConfirmDismantle", "Dismantle {0} for {1}? It is gone until you find it again."), Row.Label, Pay);
	if (!Row.IsGear())
	{
		return Ask;
	}
	// What sits in the piece goes with it, and (D-016) a worn piece leaves its loadouts: both are said first.
	const UPoolSubsystem *Pool = VendorServicesBuild::PoolOf(World);
	const FWeaponInventoryEntry *Weapon = Pool ? Pool->FindWeaponInPool(Row.GearID) : nullptr;
	const FRingInventoryEntry *Adapter = Pool && !Weapon ? Pool->FindRingInPool(Row.GearID) : nullptr;
	const bool bHoldsSomething = Weapon   ? !Weapon->AttachedItem.IsEmpty() || !Weapon->StoneSocket.IsEmpty()
								 : Adapter ? !Adapter->AttachedItem.IsEmpty() || !Adapter->StoneSocket.IsEmpty()
										   : false;
	const UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
	FText Warning = Inventory ? UEconomyService::GetDismantleWarning(Inventory->GetOwner(), Row.GearID) : FText::GetEmpty();
	if (bHoldsSomething)
	{
		const FText GoesToo = LOCTEXT("SocketsGoToo", "What is socketed in it goes with it.");
		Warning = Warning.IsEmpty() ? GoesToo : FText::Format(LOCTEXT("TwoWarnings", "{0} {1}"), Warning, GoesToo);
	}
	return Warning.IsEmpty() ? Ask : FText::Format(LOCTEXT("ConfirmWorn", "{0} {1}"), Warning, Ask);
}

FText WoRVendorServices::Run(UWorld *World, EMerchantType Vendor, EVendorService Service, FName Member, const FVendorRow &Row, int32 Count)
{
	const FText Refusal = GetRefusal(World, Vendor, Service, Member, Row);
	if (!Refusal.IsEmpty())
	{
		return Refusal;
	}
	const UPoolSubsystem *Pool = VendorServicesBuild::PoolOf(World);
	UEconomyService *Economy = VendorServicesBuild::EconomyOf(World);
	UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Member);
	AActor *Owner = Inventory ? Inventory->GetOwner() : nullptr;
	if (!Owner || !Economy)
	{
		return LOCTEXT("NotHere", "That character isn't here to do it");
	}
	const int32 Moving = FMath::Clamp(Count, 1, FMath::Max(1, Row.Count));
	bool bDone = false;
	if (Service == EVendorService::Sell)
	{
		bDone = Row.IsGear() ? Economy->SellGear(Owner, Row.GearID) : Economy->SellCrystal(Owner, Row.Item, Moving);
	}
	else if (Row.IsEvolution())
	{
		bDone = Economy->DismantleEvolution(Owner, Row.EvolutionID);
	}
	else if (Row.IsGear())
	{
		bDone = Pool->FindRingInPool(Row.GearID) ? Economy->DismantleRing(Owner, Row.GearID) : Economy->DismantleWeapon(Owner, Row.GearID);
	}
	else
	{
		bDone = Economy->DismantleCrystal(Owner, Row.Item, Moving);
	}
	// A refusal the checks above don't foresee (in a fight, an evolution in use) is logged by the economy with its reason.
	return bDone ? FText::GetEmpty()
				 : FText::Format(LOCTEXT("ServiceFailed", "Couldn't {0} {1} (see the log)"), WoRGameNames::GetVendorServiceName(Service), Row.Label);
}

FString WoRVendorServices::GetRowsString(UWorld *World, EMerchantType Vendor, FName Member)
{
	FString Out = FString::Printf(TEXT("[Vendor] %s, for %s:"), *WoRGameNames::GetMerchantTypeName(Vendor).ToString(), *Member.ToString());
	for (const EVendorService Service : {EVendorService::Sell, EVendorService::Dismantle})
	{
		if (!Offers(Vendor, Service))
		{
			continue;
		}
		Out += FString::Printf(TEXT("\n  %s:"), *WoRGameNames::GetVendorServiceName(Service).ToString());
		for (const FVendorRow &Row : GatherRows(World, Vendor, Service, Member))
		{
			const FText Refusal = GetRefusal(World, Vendor, Service, Member, Row);
			Out += FString::Printf(TEXT("\n    %s: %s"), *Row.Label.ToString(),
								   Refusal.IsEmpty() ? *GetPayText(World, Service, Row).ToString() : *FString::Printf(TEXT("refused (%s)"), *Refusal.ToString()));
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GVendorPrintServicesCommand(
	TEXT("wor.Vendor.PrintServices"),
	TEXT("DEBUG (B-445): for each vendor that sells or dismantles, what it would take from the party leader and what it gives."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
		if (!Party || Party->Members.Num() == 0)
		{
			UE_LOG(LogWoREconomy, Display, TEXT("[Vendor] no party"));
			return;
		}
		const FName Leader = UPoolSubsystem::GetCharacterKey(Party->Members[0]);
		for (const EMerchantType Vendor : {EMerchantType::Blacksmith, EMerchantType::Jeweler, EMerchantType::Spiritualist})
		{
			UE_LOG(LogWoREconomy, Display, TEXT("%s"), *WoRVendorServices::GetRowsString(World, Vendor, Leader));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
