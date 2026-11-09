// CurseSubsystem.cpp

#include "Memory/CurseSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/CurseData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Memory/MemoryUnlock.h"
#include "Party/Party.h"
#include "Currency/CurrencyComponent.h"
#include "Party/PartySessionSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "Curse"

void UCurseSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (UMemoryJournalSubsystem *Journal = Collection.InitializeDependency<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.AddDynamic(this, &UCurseSubsystem::HandleMemoryRecorded);
	}
}

void UCurseSubsystem::Deinitialize()
{
	if (UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.RemoveAll(this);
	}
	Super::Deinitialize();
}

bool UCurseSubsystem::AddCurse(FName CurseId)
{
	if (CurseId.IsNone() || HeldCurses.Contains(CurseId))
	{
		return false;
	}
	HeldCurses.Add(CurseId);
	bPenaltyDirty = true;
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s now held%s (%d)"), *CurseId.ToString(), LiftedCurses.Contains(CurseId) ? TEXT(" again") : TEXT(""),
		   HeldCurses.Num());
	return true;
}

bool UCurseSubsystem::RemoveCurse(FName CurseId)
{
	if (HeldCurses.Remove(CurseId) == 0)
	{
		return false;
	}
	LiftedCurses.Add(CurseId);
	bPenaltyDirty = true;
	LiftedThisEvent.Add(CurseId);
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s lifted (%d left)"), *CurseId.ToString(), HeldCurses.Num());
	return true;
}

bool UCurseSubsystem::ClearCurse(FName CurseId)
{
	if (!RemoveCurse(CurseId))
	{
		return false;
	}
	// Merge fix: the same-event guard is for a cure a memory event gives (RemoveCurse reward), not the player's own clear.
	LiftedThisEvent.Remove(CurseId);
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s cleared by the player (it comes back if its giver's conditions are met again)"),
		   *CurseId.ToString());
	return true;
}

bool UCurseSubsystem::PayToClearCurse(FName CurseId, UCurrencyComponent *Wallet, FText &OutRefusal)
{
	if (!HasCurse(CurseId))
	{
		OutRefusal = LOCTEXT("NotHeld", "You don't hold that curse.");
		return false;
	}
	if (!EnsureCurseAssetsGathered())
	{
		// Review: the asset (its tier) isn't known yet; never price an S-tier curse at the defaults.
		OutRefusal = LOCTEXT("StillLoading", "The curse records are still loading. Try again in a moment.");
		return false;
	}
	const UCurseData *Curse = FindCurseAsset(CurseId);
	const UCurseData *Priced = Curse ? Curse : GetDefault<UCurseData>(); // Q-390: no asset prices as the defaults
	const ECurrencyType Currency = Priced->GetClearCurrency();
	const int32 Price = Priced->GetClearPrice();
	if (!Wallet || !Wallet->CanAfford(Currency, Price))
	{
		OutRefusal = FText::Format(LOCTEXT("Short", "Clearing it costs {0} {1}; you have {2}."), FText::AsNumber(Price),
								   WoRGameNames::GetCurrencyName(Currency), FText::AsNumber(Wallet ? Wallet->GetBalance(Currency) : 0));
		return false;
	}
	if (!Wallet->Spend(Currency, Price))
	{
		OutRefusal = LOCTEXT("SpendRefused", "The payment didn't go through.");
		UE_LOG(LogWoR, Warning, TEXT("[Curse] %s: the clear's spend of %d was refused (no server authority?)"), *CurseId.ToString(), Price);
		return false;
	}
	ClearCurse(CurseId);
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s cleared for %d %s"), *CurseId.ToString(), Price,
		   *WoRGameNames::GetCurrencyName(Currency).ToString());
	return true;
}

const UCurseData *UCurseSubsystem::FindCurseAsset(FName CurseId) const
{
	EnsureCurseAssetsGathered();
	for (const UCurseData *Curse : CurseAssets)
	{
		if (Curse && Curse->CurseId == CurseId)
		{
			return Curse;
		}
	}
	return nullptr;
}

int32 UCurseSubsystem::ApplyRewards(const TArray<FMemoryReward> &Rewards)
{
	int32 Changed = 0;
	for (const FMemoryReward &Reward : Rewards)
	{
		if (Reward.Kind == EMemoryRewardKind::Curse)
		{
			Changed += AddCurse(Reward.RewardId) ? 1 : 0;
		}
		else if (Reward.Kind == EMemoryRewardKind::RemoveCurse)
		{
			Changed += RemoveCurse(Reward.RewardId) ? 1 : 0;
		}
	}
	return Changed;
}

int32 UCurseSubsystem::RegainFrom(const TArray<FMemoryReward> &Rewards)
{
	int32 Regained = 0;
	for (const FMemoryReward &Reward : Rewards)
	{
		if (Reward.Kind == EMemoryRewardKind::Curse && !LiftedThisEvent.Contains(Reward.RewardId) && AddCurse(Reward.RewardId))
		{
			++Regained;
		}
	}
	return Regained;
}

bool UCurseSubsystem::CouldRegain(const TArray<FMemoryReward> &Rewards) const
{
	return Rewards.ContainsByPredicate([this](const FMemoryReward &Reward)
	{
		return Reward.Kind == EMemoryRewardKind::Curse && !Reward.RewardId.IsNone() && !HeldCurses.Contains(Reward.RewardId) &&
			   !LiftedThisEvent.Contains(Reward.RewardId);
	});
}

int32 UCurseSubsystem::CheckRegain(const FMemoryEvent &Event, const TArray<UMemoryData *> &Candidates)
{
	const UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>();
	int32 Regained = 0;
	for (const UMemoryData *Memory : Candidates)
	{
		if (Memory && Journal && Journal->HasMemory(Memory) && CouldRegain(Memory->Rewards) && MemoryConditions::AreMet(*Memory, Event, Journal))
		{
			const int32 Back = RegainFrom(Memory->Rewards);
			Regained += Back;
			UE_LOG(LogWoR, Log, TEXT("[Curse] %s met again: %d curse(s) back"), *Memory->GetMemoryId().ToString(), Back);
		}
	}
	return Regained;
}

void UCurseSubsystem::HandleMemoryRecorded(UMemoryData *Memory, EMemorySource /*Source*/)
{
	if (Memory)
	{
		ApplyRewards(*Memory);
	}
}

FSavedCurses UCurseSubsystem::Capture() const
{
	FSavedCurses Saved;
	Saved.Held = HeldCurses.Array();
	Saved.Held.Sort([](const FName &A, const FName &B) { return A.LexicalLess(B); });
	Saved.Lifted = LiftedCurses.Array();
	Saved.Lifted.Sort([](const FName &A, const FName &B) { return A.LexicalLess(B); });
	return Saved;
}

void UCurseSubsystem::Restore(const FSavedCurses &Saved)
{
	HeldCurses.Reset();
	bPenaltyDirty = true;
	LiftedCurses.Reset();
	LiftedCurses.Append(Saved.Lifted);
	for (const FName &CurseId : Saved.Held)
	{
		if (!CurseId.IsNone())
		{
			HeldCurses.Add(CurseId);
		}
	}
}

FString UCurseSubsystem::GetCursesString() const
{
	TArray<FString> Lifted;
	for (const FName &CurseId : LiftedCurses)
	{
		if (!HeldCurses.Contains(CurseId))
		{
			Lifted.Add(CurseId.ToString());
		}
	}
	Lifted.Sort();
	const FString LiftedText = Lifted.IsEmpty() ? FString() : FString::Printf(TEXT("; lifted or cleared (can come back): %s"), *FString::Join(Lifted, TEXT(", ")));
	if (HeldCurses.IsEmpty())
	{
		return TEXT("no curses held") + LiftedText;
	}
	TArray<FString> Names;
	for (const FName &CurseId : HeldCurses)
	{
		Names.Add(CurseId.ToString());
	}
	Names.Sort();
	return FString::Printf(TEXT("curses held: %s"), *FString::Join(Names, TEXT(", "))) + LiftedText;
}

void UCurseSubsystem::SetCurseAssets(const TArray<UCurseData *> &Curses)
{
	CurseAssets.Reset();
	for (UCurseData *Curse : Curses)
	{
		CurseAssets.Add(Curse);
	}
	bGatheredCurses = true;
	bPenaltyDirty = true;
}

bool UCurseSubsystem::EnsureCurseAssetsGathered() const
{
	if (!bGatheredCurses)
	{
		// Review: not while the registry is still scanning (it would keep a partial list for good).
		const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		if (!Registry.IsLoadingAssets())
		{
			const_cast<UCurseSubsystem *>(this)->SetCurseAssets(UCurseData::GatherAll()); // once
		}
	}
	return bGatheredCurses;
}

TArray<const UCurseData *> UCurseSubsystem::GetHeldCurseAssets() const
{
	EnsureCurseAssetsGathered();
	TArray<const UCurseData *> Held;
	for (const UCurseData *Curse : CurseAssets)
	{
		if (Curse && HeldCurses.Contains(Curse->CurseId))
		{
			Held.Add(Curse);
		}
	}
	return Held;
}

void UCurseSubsystem::RebuildPenalty() const
{
	CachedStatPenalty = FEquipmentStatBonus();
	CachedResistancePenalty = FResistanceBonus();
	for (const UCurseData *Curse : GetHeldCurseAssets())
	{
		CachedStatPenalty.Accumulate(Curse->StatPenalty);
		CachedResistancePenalty.Accumulate(Curse->ResistancePenalty);
	}
	bPenaltyDirty = !bGatheredCurses; // try again once the registry has loaded
}

FEquipmentStatBonus UCurseSubsystem::GetStatPenalty() const
{
	if (HeldCurses.IsEmpty())
	{
		return FEquipmentStatBonus();
	}
	if (bPenaltyDirty)
	{
		RebuildPenalty();
	}
	return CachedStatPenalty;
}

FResistanceBonus UCurseSubsystem::GetResistancePenalty() const
{
	if (HeldCurses.IsEmpty())
	{
		return FResistanceBonus();
	}
	if (bPenaltyDirty)
	{
		RebuildPenalty();
	}
	return CachedResistancePenalty;
}

bool UCurseSubsystem::IsCursedHere(const UCharacterData *Character) const
{
	if (!Character || HeldCurses.IsEmpty() || MemoryUnlock::FindRemotePlayerOf(GetGameInstance(), Character))
	{
		return false;
	}
	const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	return Party && Party->Members.ContainsByPredicate([Character](const FPartyMember &Member) { return Member.CharacterData == Character; });
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GCurseClearCommand(
	TEXT("wor.Curse.Clear"),
	TEXT("Q-378: clear a held curse as the player would (wor.Curse.Clear <curse id>)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		UCurseSubsystem *Curses = GameInstance ? GameInstance->GetSubsystem<UCurseSubsystem>() : nullptr;
		if (!Curses || Args.IsEmpty())
		{
			UE_LOG(LogWoR, Warning, TEXT("[Curse] usage: wor.Curse.Clear <curse id>"));
			return;
		}
		const bool bCleared = Curses->ClearCurse(FName(*Args[0]));
		UE_LOG(LogWoR, Display, TEXT("[Curse] %s: %s"), *Args[0], bCleared ? TEXT("cleared") : TEXT("not held"));
	}));

static FAutoConsoleCommandWithWorld GCursePrintCommand(
	TEXT("wor.Curse.Print"),
	TEXT("B-378: the curses this player holds."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UCurseSubsystem *Curses = GameInstance ? GameInstance->GetSubsystem<UCurseSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("[Curse] %s"), Curses ? *Curses->GetCursesString() : TEXT("no curse subsystem"));
		if (Curses)
		{
			const FEquipmentStatBonus P = Curses->GetStatPenalty();
			UE_LOG(LogWoR, Display, TEXT("[Curse] penalty on your characters: HP %+d, Defence %+d"), P.BonusMaxHP, P.BonusDefense);
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
