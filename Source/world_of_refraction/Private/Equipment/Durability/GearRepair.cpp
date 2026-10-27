// GearRepair.cpp

#include "Equipment/Durability/GearRepair.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "WoRLog.h"

namespace
{
	int32 EvolutionMax(const FEvolutionAttachment &Evolution)
	{
		return Evolution.Item ? Evolution.Item->MaxDurability : 0;
	}

	/** An unbreakable evolution never wears, and RepairBetweenCombats refuses it. */
	bool IsUnbreakable(const FEvolutionAttachment &Evolution)
	{
		return Evolution.Item && Evolution.Item->Breakability == EBreakability::Unbreakable;
	}

	EItemTier AttachmentTier(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEvolution())
		{
			return Item.Evolution.Tier;
		}
		if (Item.IsFusion())
		{
			return FMath::Max(Item.Fusion.Id.HalfA.Tier, Item.Fusion.Id.HalfB.Tier);
		}
		return Item.Crystal.Id.Tier;
	}

	bool CanRepair(const FRuntimeAttachedItem &Item)
	{
		return !Item.IsEmpty() && (!Item.IsFusion() || Item.Fusion.HasGemHalf()) &&
			   (!Item.IsEvolution() || !IsUnbreakable(Item.Evolution));
	}

	/** Repairs every carried item to full; returns how many gained durability. */
	int32 RepairAll(UParty &Party)
	{
		int32 Repaired = 0;
		for (FPartyMember &Member : Party.Members)
		{
			if (!Member.CarriedGear.bCarried)
			{
				continue;
			}
			for (FCarriedAttachment &Carried : Member.CarriedGear.Attachments)
			{
				if (GearRepair::GetRepairCost(Carried.Attached) > 0)
				{
					Repaired += Carried.Attached.RepairBetweenCombats(Carried.Attached.GetMaxDurability()) > 0 ? 1 : 0;
				}
			}
			FEvolutionAttachment &Evolution = Member.CarriedGear.PrimaryEvolution;
			if (Member.CarriedGear.bHasPrimaryEvolution && GearRepair::GetRepairCost(Evolution) > 0)
			{
				Repaired += Evolution.RepairBetweenCombats(EvolutionMax(Evolution)) > 0 ? 1 : 0;
			}
		}
		return Repaired;
	}

	UParty *LocalPartyOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		return PartySession ? PartySession->GetLocalParty() : nullptr;
	}

	UCurrencyComponent *LocalWalletOf(const UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const AWoRPlayerState *PS = PC && PC->IsLocalController() ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
		return PS ? PS->GetWallet() : nullptr;
	}

	FAutoConsoleCommandWithWorld GRepairQuoteCmd(
		TEXT("wor.Party.RepairQuote"),
		TEXT("DEBUG: the Gold a rest-spot repair of the party's carried gear costs (B-178)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const UParty *Party = LocalPartyOf(World);
			const UCurrencyComponent *Wallet = LocalWalletOf(World);
			UE_LOG(LogWoR, Display, TEXT("%s\n  The wallet has %d Gold"),
				   Party ? *GearRepair::GetRepairQuoteString(*Party) : TEXT("[GearRepair] No local party"),
				   Wallet ? Wallet->GetBalance(ECurrencyType::Gold) : 0);
		}));

	FAutoConsoleCommandWithWorld GRepairCmd(
		TEXT("wor.Party.Repair"),
		TEXT("DEBUG: pay Gold and repair the party's carried gear to full, as at a rest spot (B-178)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			UParty *Party = LocalPartyOf(World);
			UCurrencyComponent *Wallet = LocalWalletOf(World);
			int32 Spent = 0;
			if (!Party || !Wallet || !GearRepair::RepairParty(*Party, *Wallet, Spent))
			{
				UE_LOG(LogWoR, Display, TEXT("[GearRepair] wor.Party.Repair: nothing repaired (no party or wallet, nothing worn, or not enough Gold)"));
			}
		}));
}

int32 GearRepair::GetGoldForTier(EItemTier Tier)
{
	const int32 Index = FMath::Clamp(static_cast<int32>(Tier), 0, static_cast<int32>(UE_ARRAY_COUNT(GOLD_BY_TIER)) - 1);
	return GOLD_BY_TIER[Index];
}

int32 GearRepair::GetRepairCost(const FRuntimeAttachedItem &Item)
{
	if (!CanRepair(Item))
	{
		return 0;
	}
	return GetRepairGold(AttachmentTier(Item), Item.GetCurrentDurability(), Item.GetMaxDurability());
}

int32 GearRepair::GetRepairGold(EItemTier Tier, int32 Current, int32 Max)
{
	const int32 Missing = Max - FMath::Clamp(Current, 0, Max);
	if (Missing <= 0)
	{
		return 0;
	}
	return FMath::DivideAndRoundUp(GetGoldForTier(Tier) * Missing, Max);
}

int32 GearRepair::GetRepairCost(const FEvolutionAttachment &Evolution)
{
	if (!Evolution.Item || IsUnbreakable(Evolution))
	{
		return 0;
	}
	return GetRepairGold(Evolution.Tier, Evolution.CurrentDurability, EvolutionMax(Evolution));
}

FString GearRepair::GetRepairQuoteString(const UParty &Party)
{
	FString Out = TEXT("[GearRepair] Quote");
	for (int32 MemberIndex = 0; MemberIndex < Party.Members.Num(); ++MemberIndex)
	{
		const FCarriedGearState &Gear = Party.Members[MemberIndex].CarriedGear;
		if (!Gear.bCarried)
		{
			Out += FString::Printf(TEXT("\n  [%d] nothing carried (authored gear, unworn)"), MemberIndex);
			continue;
		}
		for (const FCarriedAttachment &Carried : Gear.Attachments)
		{
			if (const int32 Gold = GetRepairCost(Carried.Attached))
			{
				Out += FString::Printf(TEXT("\n  [%d] %s on %s, tier %s, %d/%d: %d Gold"), MemberIndex,
									   *UEnum::GetValueAsString(Carried.Attached.Kind), *GetNameSafe(Carried.Holder),
									   *ItemIdentity::GetTierLetter(AttachmentTier(Carried.Attached)),
									   Carried.Attached.GetCurrentDurability(), Carried.Attached.GetMaxDurability(), Gold);
			}
		}
		if (const int32 Gold = Gear.bHasPrimaryEvolution ? GetRepairCost(Gear.PrimaryEvolution) : 0)
		{
			Out += FString::Printf(TEXT("\n  [%d] primary evolution %s, tier %s, %d/%d: %d Gold"), MemberIndex,
								   *GetNameSafe(Gear.PrimaryEvolution.Item), *ItemIdentity::GetTierLetter(Gear.PrimaryEvolution.Tier),
								   Gear.PrimaryEvolution.CurrentDurability, EvolutionMax(Gear.PrimaryEvolution), Gold);
		}
	}
	Out += FString::Printf(TEXT("\n  Total: %d Gold"), GetPartyRepairCost(Party));
	return Out;
}

int32 GearRepair::GetPartyRepairCost(const UParty &Party)
{
	int32 Gold = 0;
	for (const FPartyMember &Member : Party.Members)
	{
		if (!Member.CarriedGear.bCarried)
		{
			continue;
		}
		for (const FCarriedAttachment &Carried : Member.CarriedGear.Attachments)
		{
			Gold += GetRepairCost(Carried.Attached);
		}
		if (Member.CarriedGear.bHasPrimaryEvolution)
		{
			Gold += GetRepairCost(Member.CarriedGear.PrimaryEvolution);
		}
	}
	return Gold;
}

bool GearRepair::RepairParty(UParty &Party, UCurrencyComponent &Wallet, int32 &OutGoldSpent)
{
	OutGoldSpent = 0;
	const int32 Cost = GetPartyRepairCost(Party);
	if (Cost <= 0 || !Wallet.Spend(ECurrencyType::Gold, Cost))
	{
		return false;
	}
	OutGoldSpent = Cost;
	const int32 Repaired = RepairAll(Party);
	UE_LOG(LogWoR, Log, TEXT("[GearRepair] Repaired %d item(s) for %d Gold"), Repaired, Cost);
	return true;
}
