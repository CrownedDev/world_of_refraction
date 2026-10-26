// MemoryFragment.cpp

#include "Memory/MemoryFragment.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
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
	constexpr float PICKUP_MESSAGE_SECONDS = 4.0f;
	constexpr int32 PICKUP_MESSAGE_KEY = -1; // a new line each time
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
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(MemoryFragmentConstants::PICKUP_MESSAGE_KEY,
										 MemoryFragmentConstants::PICKUP_MESSAGE_SECONDS, FColor::Yellow,
										 LOCTEXT("FragmentMessage", "You found a memory fragment.").ToString());
	}
	RemoveFromMap();
}

FString AMemoryFragment::GetHintText() const
{
	return LOCTEXT("FragmentHint", "Press E to pick up the memory fragment").ToString();
}

void AMemoryFragment::RemoveFromMap()
{
	SetActorHiddenInGame(true);
	// Ends the overlap, which clears the hint.
	SetActorEnableCollision(false);
}

#undef LOCTEXT_NAMESPACE
