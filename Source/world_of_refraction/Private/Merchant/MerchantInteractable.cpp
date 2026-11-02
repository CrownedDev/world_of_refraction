// MerchantInteractable.cpp

#include "Merchant/MerchantInteractable.h"
#include "Player/WoRInteract.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Merchant/MerchantData.h"
#include "Shop/MerchantShopSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRGameNames.h"

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

void AMerchantInteractable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(static_cast<int32>(GetUniqueID()));
    }
    PawnInRange.Reset();
    Super::EndPlay(EndPlayReason);
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

void AMerchantInteractable::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    // Before registration finishes the static root still takes a new mesh.
    // TODO(B-143 / B-144): async load once V's real meshes land.
    if (UStaticMesh *Outfit = Merchant ? Merchant->VOutfitMesh.LoadSynchronous() : nullptr)
    {
        CubeMesh->SetStaticMesh(Outfit);
    }
}

TArray<EMerchantType> AMerchantInteractable::GetMissingServices(const UWorld *World)
{
    TSet<EMerchantType> Present;
    if (!World)
    {
        return {};
    }
    for (TActorIterator<AMerchantInteractable> It(World); It; ++It)
    {
        if (It->Merchant)
        {
            Present.Add(It->Merchant->MerchantType);
        }
    }
    TArray<EMerchantType> Missing;
    for (EMerchantType Type : TEnumRange<EMerchantType>())
    {
        if (!Present.Contains(Type))
        {
            Missing.Add(Type);
        }
    }
    return Missing;
}

FString AMerchantInteractable::GetServicesString(const UWorld *World)
{
    FString Out = TEXT("Hub services:");
    if (!World)
    {
        return Out + TEXT(" (no world)");
    }
    for (TActorIterator<AMerchantInteractable> It(World); It; ++It)
    {
        Out += FString::Printf(TEXT("\n  %s: %s%s"), *It->GetName(),
                               It->Merchant ? *UEnum::GetDisplayValueAsText(It->Merchant->MerchantType).ToString() : TEXT("<no Merchant>"),
                               It->Merchant && !It->Merchant->VOutfitMesh.IsNull() ? TEXT(" (V outfit set)") : TEXT(""));
    }
    const TArray<EMerchantType> Missing = GetMissingServices(World);
    Out += FString::Printf(TEXT("\n  Missing: %s"), Missing.IsEmpty() ? TEXT("none")
        : *FString::JoinBy(Missing, TEXT(", "), [](EMerchantType Type) { return UEnum::GetDisplayValueAsText(Type).ToString(); }));
    return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GHubServicesCommand(
    TEXT("wor.Hub.Services"),
    TEXT("List every merchant NPC in the level with its service, and the hub services with no NPC (B-195)."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        UE_LOG(LogTemp, Display, TEXT("%s"), *AMerchantInteractable::GetServicesString(World));
    }));
#endif

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

    // Cluster 3a: the shop subsystem owns the window. PawnInRange IS the instigator —
    // Interact() is only reachable while a player pawn stands in the trigger.
    APawn *Pawn = PawnInRange.Get();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("MerchantInteractable '%s': Interact with no pawn in range — ignored."), *GetName());
        return;
    }

    UGameInstance *GI = GetGameInstance();
    UMerchantShopSubsystem *Shop = GI ? GI->GetSubsystem<UMerchantShopSubsystem>() : nullptr;
    if (!Shop)
    {
        UE_LOG(LogTemp, Warning, TEXT("MerchantInteractable '%s': MerchantShopSubsystem unavailable — ignored."), *GetName());
        return;
    }
    Shop->OpenForMerchant(Merchant, Pawn);
}

AMerchantInteractable *AMerchantInteractable::FindNearestInRange(APawn *Pawn)
{
    return WoRInteract::FindNearestOverlapping<AMerchantInteractable>(Pawn); // B-256
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
                                      ? WoRGameNames::GetMerchantTypeName(Merchant->MerchantType).ToString()
                                      : FString::Printf(TEXT("%s (no Merchant assigned)"), *GetName());
    return FString::Printf(TEXT("Press E — %s"), *MerchantLabel);
}
