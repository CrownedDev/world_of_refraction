// MemoryFragment.cpp

#include "Memory/MemoryFragment.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryUnlock.h"
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
	// The trial level reloads after each fight; a fragment taken earlier this run stays gone, and
	// one whose memory is already collected is gone for good (B-210).
	if (IsTaken())
	{
		RemoveFromMap();
	}
}

bool AMemoryFragment::IsTaken() const
{
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	return (TrialRun && TrialRun->HasFound(GetFeatureId())) || MemoryUnlock::IsCollected(GetGameInstance(), Memory);
}

void AMemoryFragment::Interact(APawn * /*Pawn*/)
{
	UTrialRunSubsystem *TrialRun = GetTrialRun();
	UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!TrialRun || !GameInstance || IsTaken())
	{
		return;
	}
	if (!Memory)
	{
		UE_LOG(LogWoR, Warning, TEXT("[MemoryFragment] %s has no Memory set - nothing to pick up"), *GetName());
		return;
	}
	// B-209 (D-100 part 2): the pickup is an event; its memory needs its own conditions, and other
	// memories at this fragment can unlock too. Unmet: the fragment stays for another party.
	FMemoryEvent Event = MemoryConditions::MakeEvent(GameInstance, EMemorySource::Fragment);
	Event.Locations.Add(GetFeatureId());
	Event.Named.Add(Memory);
	MemoryConditions::CheckEvent(GameInstance, Event);
	if (!MemoryUnlock::IsCollected(GameInstance, Memory))
	{
		ShowMessage(LOCTEXT("FragmentUnmoved", "Nothing happens."));
		return;
	}
	TrialRun->RecordFound(GetFeatureId(), LOCTEXT("FragmentFound", "A memory fragment"));
	ShowMessage(LOCTEXT("FragmentMessage", "You found a memory fragment."));
	RemoveFromMap();
}

FString AMemoryFragment::GetHintText() const
{
	return IsTaken()
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
