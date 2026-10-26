// MemoryFragment.cpp

#include "Memory/MemoryFragment.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "MemoryFragment"

namespace MemoryFragmentConstants
{
	const FVector FRAGMENT_SCALE(0.3f);
}

AMemoryFragment::AMemoryFragment()
{
	FeatureMesh->SetRelativeScale3D(MemoryFragmentConstants::FRAGMENT_SCALE);
}

void AMemoryFragment::BeginPlay()
{
	Super::BeginPlay();
	// The trial level reloads after each fight; a fragment taken earlier this run stays gone.
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	if (TrialRun && TrialRun->HasFound(GetFeatureId()))
	{
		RemoveFromMap();
	}
}

void AMemoryFragment::Interact(APawn * /*Pawn*/)
{
	UTrialRunSubsystem *TrialRun = GetTrialRun();
	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!TrialRun || !Journal || TrialRun->HasFound(GetFeatureId()))
	{
		return;
	}
	if (!Memory)
	{
		UE_LOG(LogWoR, Warning, TEXT("[MemoryFragment] %s has no Memory set - nothing to pick up"), *GetName());
		return;
	}
	TrialRun->CollectMemory(Memory);
	Journal->RecordMemory(Memory);
	TrialRun->RecordFound(GetFeatureId(), LOCTEXT("FragmentFound", "A memory fragment"));
	ShowMessage(LOCTEXT("FragmentMessage", "You found a memory fragment."));
	RemoveFromMap();
}

FString AMemoryFragment::GetHintText() const
{
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	return TrialRun && TrialRun->HasFound(GetFeatureId())
			   ? FString()
			   : LOCTEXT("FragmentHint", "Press E to pick up the memory fragment").ToString();
}

void AMemoryFragment::RemoveFromMap()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	RefreshHint();
}

#undef LOCTEXT_NAMESPACE
