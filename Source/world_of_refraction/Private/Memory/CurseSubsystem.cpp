// CurseSubsystem.cpp

#include "Memory/CurseSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Memory/CurseData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Memory/MemoryUnlock.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "WoRLog.h"

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
	if (CurseId.IsNone() || HeldCurses.Contains(CurseId) || LiftedCurses.Contains(CurseId))
	{
		return false;
	}
	HeldCurses.Add(CurseId);
	bPenaltyDirty = true;
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s now held (%d)"), *CurseId.ToString(), HeldCurses.Num());
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
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s lifted (%d left)"), *CurseId.ToString(), HeldCurses.Num());
	return true;
}

int32 UCurseSubsystem::ApplyRewards(const UMemoryData &Memory)
{
	int32 Changed = 0;
	for (const FMemoryReward &Reward : Memory.Rewards)
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
	if (HeldCurses.IsEmpty())
	{
		return TEXT("no curses held");
	}
	TArray<FString> Names;
	for (const FName &CurseId : HeldCurses)
	{
		Names.Add(CurseId.ToString());
	}
	Names.Sort();
	return FString::Printf(TEXT("curses held: %s"), *FString::Join(Names, TEXT(", ")));
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

TArray<const UCurseData *> UCurseSubsystem::GetHeldCurseAssets() const
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
