// MemorySpot.cpp

#include "Memory/MemorySpot.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryUnlock.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "MemorySpot"

AMemorySpot::AMemorySpot()
{
	// A spot, not an object: nothing to see or bump into, only the trigger.
	FeatureMesh->SetHiddenInGame(true);
	FeatureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMemorySpot::BeginPlay()
{
	Super::BeginPlay();
	InteractionTrigger->OnComponentBeginOverlap.AddDynamic(this, &AMemorySpot::OnSpotBeginOverlap);
	// The trial level reloads after each fight; a spot reached earlier this run stays spent, and
	// one whose memory is already collected is spent for good (B-210).
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	if ((TrialRun && TrialRun->HasFound(GetFeatureId())) || MemoryUnlock::IsCollected(GetGameInstance(), Memory))
	{
		Spend();
	}
}

void AMemorySpot::Interact(APawn * /*Pawn*/)
{
	Reach();
}

bool AMemorySpot::Reach()
{
	UTrialRunSubsystem *TrialRun = GetTrialRun();
	UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!TrialRun || !GameInstance || TrialRun->HasFound(GetFeatureId()))
	{
		return false;
	}
	if (!Memory)
	{
		UE_LOG(LogWoR, Warning, TEXT("[MemorySpot] %s has no Memory set - spent"), *GetName());
		Spend();
		return false;
	}
	if (MemoryUnlock::IsCollected(GameInstance, Memory))
	{
		Spend();
		return false;
	}
	// B-209 (D-100 part 2): reaching the spot is an event, like a fragment pickup. Unmet: the spot
	// stays, silent, for another party.
	FMemoryEvent Event = MemoryConditions::MakeEvent(GameInstance, EMemorySource::MapSpot);
	Event.Locations.Add(GetFeatureId());
	Event.Named.Add(Memory);
	MemoryConditions::CheckEvent(GameInstance, Event);
	if (!MemoryUnlock::IsCollected(GameInstance, Memory))
	{
		return false;
	}
	TrialRun->RecordFound(GetFeatureId(), LOCTEXT("SpotFound", "A memory spot"));
	ShowMessage(LOCTEXT("SpotMessage", "A memory surfaces."));
	Spend();
	return true;
}

void AMemorySpot::OnSpotBeginOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
									 UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
									 const FHitResult & /*SweepResult*/)
{
	const APawn *Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		Reach();
	}
}

void AMemorySpot::Spend()
{
	InteractionTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefreshHint();
}

#undef LOCTEXT_NAMESPACE
