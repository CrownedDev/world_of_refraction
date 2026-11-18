// TrialDuplicates.cpp

#include "Trial/TrialDuplicates.h"

#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Trial/TrialDrops.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Trial/DuplicateDropWidget.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialDuplicates"

namespace
{
	template <class TSubsystem>
	TSubsystem *DuplicatesSubsystem(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	const TCHAR *DuplicateChoiceName(EDuplicateChoice Choice)
	{
		switch (Choice)
		{
		case EDuplicateChoice::Replace: return TEXT("Replace");
		case EDuplicateChoice::Convert: return TEXT("Convert");
		default: return TEXT("Add");
		}
	}
}

FGuid TrialDuplicates::FindHeldWeapon(const UInventoryComponent &Inventory, const UWeaponData *Weapon, const FGuid &ExceptID)
{
	for (int32 Index = 0; Weapon && Index < Inventory.GetWeaponCount(); ++Index)
	{
		const FWeaponInventoryEntry Entry = Inventory.GetWeaponAt(Index);
		if (Entry.Weapon == Weapon && Entry.PersistentID.IsValid() && Entry.PersistentID != ExceptID)
		{
			return Entry.PersistentID;
		}
	}
	return FGuid();
}

bool TrialDuplicates::MakeView(const UPoolSubsystem &Pool, const FTrialDuplicate &Duplicate, FDuplicateDropView &OutView)
{
	const FWeaponInventoryEntry *New = Pool.FindWeaponInPool(Duplicate.NewID);
	const FWeaponInventoryEntry *Held = Pool.FindWeaponInPool(Duplicate.HeldID);
	if (!New || !Held || !New->Weapon)
	{
		return false;
	}
	FTrialDrop Drop;
	Drop.Item = New->Weapon;
	Drop.Tier = New->Tier;
	OutView.Drop = TrialDrops::MakeView(Drop);
	OutView.HeldTier = Held->Tier;
	OutView.EssenceValue = EconomyYield::GetLevelingEssenceYieldForTier(New->Tier);
	return true;
}

FText TrialDuplicates::Resolve(UWorld *World, const FTrialDuplicate &Duplicate, EDuplicateChoice Choice)
{
	UE_LOG(LogWoR, Log, TEXT("[TrialDuplicates] %s: %s (new %s, held %s)"), *Duplicate.CharacterKey.ToString(), DuplicateChoiceName(Choice),
		   *Duplicate.NewID.ToString(), *Duplicate.HeldID.ToString());
	if (Choice == EDuplicateChoice::Add || !Duplicate.IsSet())
	{
		return FText::GetEmpty(); // the drop is in the inventory already
	}
	UInventoryComponent *Inventory = WoRGive::FindMemberInventory(World, Duplicate.CharacterKey);
	AActor *Owner = Inventory ? Inventory->GetOwner() : nullptr;
	if (!Owner)
	{
		return LOCTEXT("NotHere", "That character isn't here to do it.");
	}
	if (Choice == EDuplicateChoice::Convert)
	{
		// The one place essence is made on a run (D-225 over D-177's vendor-only dismantle): the dropped copy only.
		UEconomyService *Economy = DuplicatesSubsystem<UEconomyService>(World);
		return Economy && Economy->DismantleWeapon(Owner, Duplicate.NewID) ? FText::GetEmpty() : LOCTEXT("NotConverted", "It couldn't be converted.");
	}
	// As UEconomyService::ResolveDuplicate: the kept copy takes the other's places first, so no primary is left empty (D-116).
	const bool bActiveHeld = Inventory->GetLoadoutsUsing(Duplicate.HeldID).Contains(Inventory->ActiveLoadoutIndex);
	if (Inventory->MoveLoadoutSlots(Duplicate.HeldID, Duplicate.NewID) > 0 && bActiveHeld)
	{
		if (ULoadoutComponent *Loadout = Owner->FindComponentByClass<ULoadoutComponent>())
		{
			Loadout->OnLoadoutChanged.Broadcast(Inventory->ActiveLoadoutIndex);
			Loadout->NotifyActiveWeaponChanged();
		}
	}
	// The replaced one stays in the inventory, unworn: taking gear apart is the Blacksmith's, in the hub (D-177).
	return FText::GetEmpty();
}

int32 TrialDuplicates::ConvertKnownSkill(AActor *Receiver, const USkillDataBase *Skill)
{
	UCurrencyComponent *Wallet = Receiver && Skill ? CurrencyWallet::FindFor(Receiver) : nullptr;
	const int32 Essence = Skill ? EconomyYield::GetLevelingEssenceYieldForTier(Skill->Tier) : 0;
	if (!Wallet || !Wallet->AddSkillEssence(Essence))
	{
		return 0;
	}
	UE_LOG(LogWoR, Log, TEXT("[TrialDuplicates] %s already knows %s: %d Skill essence instead (Q-570)"), *GetNameSafe(Receiver), *Skill->GetName(), Essence);
	return Essence;
}

FText TrialDuplicates::GetConvertedLine(const FTrialDropView &Drop, int32 Essence)
{
	return FText::Format(LOCTEXT("DroppedKnownSkill", "Dropped: {0} ({1}): already known, {2} Skill essence"), Drop.ItemName,
						 WoRGameNames::GetTierLetter(Drop.Tier), Essence);
}

bool TrialDuplicates::OpenNextPrompt(APlayerController *PC)
{
	UWorld *World = PC ? PC->GetWorld() : nullptr;
	UTrialRunSubsystem *TrialRun = DuplicatesSubsystem<UTrialRunSubsystem>(World);
	const UPoolSubsystem *Pool = DuplicatesSubsystem<UPoolSubsystem>(World);
	// On a run only: the hub's controller opens nothing (a list left by a run that ended is cleared at the next entry).
	while (TrialRun && Pool && Pool->IsRunLayerActive() && !TrialRun->GetPendingDuplicates().IsEmpty())
	{
		const FTrialDuplicate Duplicate = TrialRun->GetPendingDuplicates()[0];
		FDuplicateDropView View;
		if (!MakeView(*Pool, Duplicate, View))
		{
			FTrialDuplicate Gone;
			TrialRun->TakePendingDuplicate(Gone); // a piece is gone: nothing to choose
			continue;
		}
		TWeakObjectPtr<APlayerController> WeakPC(PC);
		const UDuplicateDropWidget *Prompt = UDuplicateDropWidget::Open(PC, View, [WeakPC, Duplicate](EDuplicateChoice Choice)
		{
			APlayerController *Chooser = WeakPC.Get();
			if (!Chooser)
			{
				return; // the level is going: both are kept (Q-565)
			}
			const FText Refusal = Resolve(Chooser->GetWorld(), Duplicate, Choice);
			UE_CLOG(!Refusal.IsEmpty(), LogWoR, Warning, TEXT("[TrialDuplicates] %s; both are kept"), *Refusal.ToString());
			// Next tick: this prompt is still closing, and the next one must not be pushed under it.
			Chooser->GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Chooser, [WeakPC]() { OpenNextPrompt(WeakPC.Get()); }));
		});
		if (!Prompt)
		{
			return false; // no root layout here: it stays first in line (wor.Trial.NextDuplicate)
		}
		FTrialDuplicate Shown;
		TrialRun->TakePendingDuplicate(Shown);
		return true;
	}
	return false;
}

FString TrialDuplicates::GetDuplicatesString(const UTrialRunSubsystem &TrialRun, const UPoolSubsystem *Pool)
{
	FString Out = FString::Printf(TEXT("[TrialDuplicates] %d waiting"), TrialRun.GetPendingDuplicates().Num());
	for (const FTrialDuplicate &Duplicate : TrialRun.GetPendingDuplicates())
	{
		FDuplicateDropView View;
		const bool bBoth = Pool && MakeView(*Pool, Duplicate, View);
		Out += FString::Printf(TEXT("\n  %s: %s"), *Duplicate.CharacterKey.ToString(),
							   bBoth ? *WoRTrialPoolLook::GetDuplicateQuestion(View).ToString() : TEXT("a piece is gone"));
	}
	return Out;
}

static FAutoConsoleCommandWithWorld GPrintTrialDuplicatesCommand(
	TEXT("wor.Trial.PrintDuplicates"),
	TEXT("DEBUG (B-570): the dropped duplicates waiting for their choice."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (const UTrialRunSubsystem *TrialRun = DuplicatesSubsystem<UTrialRunSubsystem>(World))
		{
			UE_LOG(LogWoR, Display, TEXT("%s"), *TrialDuplicates::GetDuplicatesString(*TrialRun, DuplicatesSubsystem<UPoolSubsystem>(World)));
		}
	}));

static FAutoConsoleCommandWithWorld GNextTrialDuplicateCommand(
	TEXT("wor.Trial.NextDuplicate"),
	TEXT("PLACEHOLDER UI (B-570): opens the prompt for the next dropped duplicate (it opens by itself on the trial map)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!TrialDuplicates::OpenNextPrompt(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoR, Display, TEXT("[TrialDuplicates] none waiting, or no root layout here"));
		}
	}));

#undef LOCTEXT_NAMESPACE
