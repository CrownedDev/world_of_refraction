// MerchantInteractable.cpp

#include "Merchant/MerchantInteractable.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Merchant/MerchantData.h"
#include "UObject/ConstructorHelpers.h"

namespace MerchantInteractableConstants
{
    constexpr float DEFAULT_INTERACTION_RADIUS = 200.0f;
    constexpr float HINT_DURATION = 1e9f;      // effectively "until removed"
    constexpr float STOCK_PRINT_DURATION = 8.0f;
}

AMerchantInteractable::AMerchantInteractable()
{
    PrimaryActorTick.bCanEverTick = false;

    CubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMesh"));
    RootComponent = CubeMesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        CubeMesh->SetStaticMesh(CubeFinder.Object);
    }

    InteractionTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionTrigger"));
    InteractionTrigger->SetupAttachment(RootComponent);
    InteractionTrigger->SetSphereRadius(MerchantInteractableConstants::DEFAULT_INTERACTION_RADIUS);
    InteractionTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AMerchantInteractable::BeginPlay()
{
    Super::BeginPlay();

    InteractionTrigger->OnComponentBeginOverlap.AddDynamic(this, &AMerchantInteractable::OnTriggerBeginOverlap);
    InteractionTrigger->OnComponentEndOverlap.AddDynamic(this, &AMerchantInteractable::OnTriggerEndOverlap);

    if (!Merchant)
    {
        UE_LOG(LogTemp, Warning, TEXT("MerchantInteractable '%s' has no Merchant assigned."), *GetName());
    }
}

void AMerchantInteractable::Interact()
{
    if (!Merchant)
    {
        const FString Message = FString::Printf(TEXT("MerchantInteractable '%s': no Merchant assigned — nothing to sell."), *GetName());
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, MerchantInteractableConstants::STOCK_PRINT_DURATION,
                                             FColor::Orange, Message);
        }
        return;
    }

    const FString Stock = Merchant->GetMerchantString();
    UE_LOG(LogTemp, Log, TEXT("%s"), *Stock);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, MerchantInteractableConstants::STOCK_PRINT_DURATION,
                                         FColor::Cyan, Stock);
    }
}

AMerchantInteractable *AMerchantInteractable::FindNearestInRange(APawn *Pawn)
{
    if (!Pawn)
    {
        return nullptr;
    }

    TArray<AActor *> Overlapping;
    Pawn->GetOverlappingActors(Overlapping, AMerchantInteractable::StaticClass());

    AMerchantInteractable *Nearest = nullptr;
    float NearestDistSquared = TNumericLimits<float>::Max();
    const FVector PawnLocation = Pawn->GetActorLocation();
    for (AActor *Actor : Overlapping)
    {
        const float DistSquared = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
        if (DistSquared < NearestDistSquared)
        {
            NearestDistSquared = DistSquared;
            Nearest = Cast<AMerchantInteractable>(Actor);
        }
    }
    return Nearest;
}

void AMerchantInteractable::OnTriggerBeginOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
                                                  UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/,
                                                  bool /*bFromSweep*/, const FHitResult & /*SweepResult*/)
{
    APawn *Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->IsPlayerControlled())
    {
        return;
    }

    PawnInRange = Pawn;
    if (GEngine)
    {
        // Keyed on this actor's id so the hint updates in place and is removable.
        // int32 cast: GetUniqueID() is uint32, ambiguous between the int32/uint64 overloads.
        GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()),
                                         MerchantInteractableConstants::HINT_DURATION,
                                         FColor::Yellow, GetHintText());
    }
}

void AMerchantInteractable::OnTriggerEndOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
                                                UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
    if (OtherActor != PawnInRange.Get())
    {
        return;
    }

    PawnInRange = nullptr;
    if (GEngine)
    {
        // Same int32 cast as the Add — both sign-extend to the identical uint64 key.
        GEngine->RemoveOnScreenDebugMessage(static_cast<int32>(GetUniqueID()));
    }
}

FString AMerchantInteractable::GetHintText() const
{
    const FString MerchantLabel = Merchant
                                      ? UEnum::GetDisplayValueAsText(Merchant->MerchantType).ToString()
                                      : FString::Printf(TEXT("%s (no Merchant assigned)"), *GetName());
    return FString::Printf(TEXT("Press E — %s"), *MerchantLabel);
}
