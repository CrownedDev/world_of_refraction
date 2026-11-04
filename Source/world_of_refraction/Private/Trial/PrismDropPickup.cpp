// PrismDropPickup.cpp

#include "Trial/PrismDropPickup.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Currency/CurrencyTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "PrismDropPickup"

namespace PrismDropPickupConstants
{
	/** Placeholder look: a small cube (the base feature's mesh scaled down). */
	const FVector MESH_SCALE(0.3);
	/** Overlaps this soon after placing come from a pawn spawned on top of it (the level's start). */
	constexpr double ARRIVAL_GRACE_SECONDS = 1.0;
}

APrismDropPickup::APrismDropPickup()
{
	// Something to walk through, not a wall.
	FeatureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FeatureMesh->SetRelativeScale3D(PrismDropPickupConstants::MESH_SCALE);
	// TODO: a Prism look (soft mesh / Niagara on a data asset, D-057) once the art exists.
}

void APrismDropPickup::BeginPlay()
{
	Super::BeginPlay();
	PlacedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	InteractionTrigger->OnComponentBeginOverlap.AddDynamic(this, &APrismDropPickup::OnPickupBeginOverlap);
	InteractionTrigger->OnComponentEndOverlap.AddDynamic(this, &APrismDropPickup::OnPickupEndOverlap);
}

void APrismDropPickup::Interact(APawn * /*Pawn*/)
{
	PickUp();
}

bool APrismDropPickup::PickUp()
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return false; // the server's pickup gives it back; a second overlap of the same pawn finds it gone
	}
	const int32 Taken = WoRDefeat::PickUpDrop(GetWorld());
	if (Taken <= 0)
	{
		UE_LOG(LogWoR, Warning, TEXT("[PrismDrop] %s: nothing taken (no run drop or no local wallet)"), *GetName());
		return false;
	}
	ShowMessage(FText::Format(LOCTEXT("PickedUp", "You pick up your {0} {1}."), FText::AsNumber(Taken),
							  WoRGameNames::GetCurrencyName(ECurrencyType::Gold)));
	Destroy();
	return true;
}

APrismDropPickup *APrismDropPickup::PlaceForRun(UWorld *World, UTrialRunSubsystem &Run)
{
	if (!World)
	{
		return nullptr;
	}
	// Only the newest drop exists: an earlier pickup goes first.
	for (TActorIterator<APrismDropPickup> It(World); It; ++It)
	{
		It->Destroy();
	}
	if (Run.GetDroppedPrisms() <= 0)
	{
		return nullptr;
	}
	FVector Spot;
	if (!Run.GetDropSpot(Spot))
	{
		TActorIterator<APlayerStart> Start(World);
		Spot = Start ? Start->GetActorLocation() : FVector::ZeroVector;
		UE_LOG(LogWoR, Log, TEXT("[PrismDrop] The fight had no map spot: the drop lies at the trial's start (%s)"),
			   Start ? *Start->GetName() : TEXT("no PlayerStart, the world origin"));
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APrismDropPickup *Pickup = World->SpawnActor<APrismDropPickup>(Spot, FRotator::ZeroRotator, Params);
	UE_LOG(LogWoR, Log, TEXT("[PrismDrop] %d Prism(s) lie at %s"), Run.GetDroppedPrisms(), *Spot.ToCompactString());
	return Pickup;
}

int32 APrismDropPickup::CountIn(const UWorld *World)
{
	int32 Count = 0;
	if (!World)
	{
		return Count;
	}
	for (TActorIterator<APrismDropPickup> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed())
		{
			++Count;
		}
	}
	return Count;
}

FString APrismDropPickup::GetHintText() const
{
	const UTrialRunSubsystem *Run = GetTrialRun();
	const int32 Amount = Run ? Run->GetDroppedPrisms() : 0;
	return FText::Format(LOCTEXT("Hint", "Press E to pick up your {0} {1}"), FText::AsNumber(Amount),
						 WoRGameNames::GetCurrencyName(ECurrencyType::Gold)).ToString();
}

void APrismDropPickup::OnPickupBeginOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
											UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/,
											bool /*bFromSweep*/, const FHitResult & /*SweepResult*/)
{
	APawn *Pawn = Cast<APawn>(OtherActor);
	if (!HasAuthority() || IsActorBeingDestroyed() || !Pawn || !Pawn->IsPlayerControlled() || ArrivedInside.Contains(Pawn))
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : PlacedTime;
	if (Now - PlacedTime < PrismDropPickupConstants::ARRIVAL_GRACE_SECONDS)
	{
		ArrivedInside.Add(Pawn);
		return;
	}
	PickUp();
}

void APrismDropPickup::OnPickupEndOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
										  UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	ArrivedInside.Remove(Cast<APawn>(OtherActor));
}

#undef LOCTEXT_NAMESPACE
