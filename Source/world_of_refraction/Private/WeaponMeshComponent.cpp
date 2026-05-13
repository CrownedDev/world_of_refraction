// WeaponMeshComponent.cpp

#include "WeaponMeshComponent.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "WeaponData.h"
#include "EWeaponType.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

UWeaponMeshComponent::UWeaponMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

void UWeaponMeshComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheReferences();
    UpdateWeaponMesh();
}

void UWeaponMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CharacterDataComponent)
    {
        return;
    }

    UWeaponData *CurrentWeapon = CharacterDataComponent->GetActiveWeapon();

    if (CurrentWeapon != CachedWeapon)
    {
        UpdateWeaponMesh();
    }
}

void UWeaponMeshComponent::CacheReferences()
{
    AActor *Owner = GetOwner();
    if (Owner)
    {
        CharacterDataComponent = Owner->FindComponentByClass<UCharacterDataComponent>();
    }
}

USkeletalMeshComponent *UWeaponMeshComponent::GetOwnerMesh() const
{
    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }

    if (ACharacter *Character = Cast<ACharacter>(Owner))
    {
        return Character->GetMesh();
    }

    return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

void UWeaponMeshComponent::UpdateWeaponMesh()
{
    if (!CharacterDataComponent)
    {
        ClearWeaponMesh();
        return;
    }

    UWeaponData *ActiveWeapon = CharacterDataComponent->GetActiveWeapon();

    if (ActiveWeapon == CachedWeapon)
    {
        return;
    }

    ClearWeaponMesh();

    if (ActiveWeapon && (ActiveWeapon->WeaponStaticMesh || ActiveWeapon->WeaponSkeletalMesh))
    {
        if (ActiveWeapon->WeaponType == EWeaponType::DualBlades)
        {
            SpawnDualWeaponMesh(ActiveWeapon);
        }
        else
        {
            SpawnWeaponMesh(ActiveWeapon);
        }
    }

    CachedWeapon = ActiveWeapon;

    UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Updated weapon mesh: %s"),
           ActiveWeapon ? *ActiveWeapon->Name : TEXT("None"));
}

void UWeaponMeshComponent::SpawnWeaponMesh(UWeaponData *Weapon)
{
    USkeletalMeshComponent *OwnerMesh = GetOwnerMesh();
    if (!OwnerMesh || !Weapon)
    {
        return;
    }

    // Check for Skeletal Mesh first, then Static Mesh
    if (Weapon->WeaponSkeletalMesh)
    {
        PrimarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        PrimarySkeletalMeshComp->SetSkeletalMesh(Weapon->WeaponSkeletalMesh);
        PrimarySkeletalMeshComp->RegisterComponent();
        PrimarySkeletalMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            RightHandSocket);

        PrimarySkeletalMeshComp->SetRelativeRotation(Weapon->MeshRotation);

        UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned skeletal mesh '%s' at socket '%s'"),
               *Weapon->Name, *RightHandSocket.ToString());
    }
    else if (Weapon->WeaponStaticMesh)
    {
        PrimaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        PrimaryStaticMeshComp->SetStaticMesh(Weapon->WeaponStaticMesh);
        PrimaryStaticMeshComp->RegisterComponent();
        PrimaryStaticMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            RightHandSocket);

        // Apply rotation offset
        PrimaryStaticMeshComp->SetRelativeRotation(Weapon->MeshRotation);

        UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned static mesh '%s' at socket '%s'"),
               *Weapon->Name, *RightHandSocket.ToString());
    }
}

void UWeaponMeshComponent::SpawnDualWeaponMesh(UWeaponData *Weapon)
{
    USkeletalMeshComponent *OwnerMesh = GetOwnerMesh();
    if (!OwnerMesh || !Weapon)
    {
        return;
    }

    // Check for Skeletal Mesh first, then Static Mesh
    if (Weapon->WeaponSkeletalMesh)
    {
        // Right hand
        PrimarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        PrimarySkeletalMeshComp->SetSkeletalMesh(Weapon->WeaponSkeletalMesh);
        PrimarySkeletalMeshComp->RegisterComponent();
        PrimarySkeletalMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            RightHandSocket);

        PrimarySkeletalMeshComp->SetRelativeRotation(Weapon->MeshRotation);

        // Left hand
        SecondarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        SecondarySkeletalMeshComp->SetSkeletalMesh(Weapon->WeaponSkeletalMesh);
        SecondarySkeletalMeshComp->RegisterComponent();
        SecondarySkeletalMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            LeftHandSocket);
        SecondarySkeletalMeshComp->SetWorldScale3D(FVector(-1.0f, 1.0f, 1.0f));

        UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned dual skeletal weapon s '%s'"),
               *Weapon->Name);
    }
    else if (Weapon->WeaponStaticMesh)
    {
        // Right hand
        PrimaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        PrimaryStaticMeshComp->SetStaticMesh(Weapon->WeaponStaticMesh);
        PrimaryStaticMeshComp->RegisterComponent();
        PrimaryStaticMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            RightHandSocket);

        // Left hand
        SecondaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        SecondaryStaticMeshComp->SetStaticMesh(Weapon->WeaponStaticMesh);
        SecondaryStaticMeshComp->RegisterComponent();
        SecondaryStaticMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            LeftHandSocket);
        SecondaryStaticMeshComp->SetWorldScale3D(FVector(-1.0f, 1.0f, 1.0f));

        UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned dual static weapon  '%s'"),
               *Weapon->Name);
    }
}

void UWeaponMeshComponent::ClearWeaponMesh()
{
    if (PrimaryStaticMeshComp)
    {
        PrimaryStaticMeshComp->DestroyComponent();
        PrimaryStaticMeshComp = nullptr;
    }

    if (SecondaryStaticMeshComp)
    {
        SecondaryStaticMeshComp->DestroyComponent();
        SecondaryStaticMeshComp = nullptr;
    }

    if (PrimarySkeletalMeshComp)
    {
        PrimarySkeletalMeshComp->DestroyComponent();
        PrimarySkeletalMeshComp = nullptr;
    }

    if (SecondarySkeletalMeshComp)
    {
        SecondarySkeletalMeshComp->DestroyComponent();
        SecondarySkeletalMeshComp = nullptr;
    }

    CachedWeapon = nullptr;
}

void UWeaponMeshComponent::DebugLogMeshState()
{
    UE_LOG(LogTemp, Display, TEXT("=== WEAPON MESH STATE ==="));
    UE_LOG(LogTemp, Display, TEXT("Cached Weapon: %s"),
           CachedWeapon ? *CachedWeapon->Name : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Primary Static: %s"),
           PrimaryStaticMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Secondary Static: %s"),
           SecondaryStaticMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Primary Skeletal: %s"),
           PrimarySkeletalMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Secondary Skeletal: %s"),
           SecondarySkeletalMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Right Socket: %s"), *RightHandSocket.ToString());
    UE_LOG(LogTemp, Display, TEXT("Left Socket: %s"), *LeftHandSocket.ToString());
    UE_LOG(LogTemp, Display, TEXT("========================="));
}