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

FName UWeaponMeshComponent::ResolveRightSocket(const UWeaponData *Weapon) const
{
    if (Weapon && !Weapon->RightHandSocket.IsNone())
    {
        return Weapon->RightHandSocket;
    }
    return RightHandSocket;
}

FName UWeaponMeshComponent::ResolveLeftSocket(const UWeaponData *Weapon) const
{
    if (Weapon && !Weapon->LeftHandSocket.IsNone())
    {
        return Weapon->LeftHandSocket;
    }
    return LeftHandSocket;
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
        if (ActiveWeapon->IsDualWielded())
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

    const FName RightSocket = ResolveRightSocket(Weapon);

    // Check for Skeletal Mesh first, then Static Mesh
    if (Weapon->WeaponSkeletalMesh)
    {
        PrimarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        PrimarySkeletalMeshComp->SetSkeletalMesh(Weapon->WeaponSkeletalMesh);
        PrimarySkeletalMeshComp->RegisterComponent();
        PrimarySkeletalMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            RightSocket);

        PrimarySkeletalMeshComp->SetRelativeRotation(Weapon->MeshRotation);

        UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned skeletal mesh '%s' at socket '%s'"),
               *Weapon->Name, *RightSocket.ToString());
    }
    else if (Weapon->WeaponStaticMesh)
    {
        PrimaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        PrimaryStaticMeshComp->SetStaticMesh(Weapon->WeaponStaticMesh);
        PrimaryStaticMeshComp->RegisterComponent();
        PrimaryStaticMeshComp->AttachToComponent(
            OwnerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            RightSocket);

        // Apply rotation offset
        PrimaryStaticMeshComp->SetRelativeRotation(Weapon->MeshRotation);

        UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned static mesh '%s' at socket '%s'"),
               *Weapon->Name, *RightSocket.ToString());
    }
}

void UWeaponMeshComponent::SpawnDualWeaponMesh(UWeaponData *Weapon)
{
    USkeletalMeshComponent *OwnerMesh = GetOwnerMesh();
    if (!OwnerMesh || !Weapon)
    {
        return;
    }

    const FName RightSocket = ResolveRightSocket(Weapon);
    const FName LeftSocket = ResolveLeftSocket(Weapon);
    const FAttachmentTransformRules AttachRules =
        FAttachmentTransformRules::SnapToTargetNotIncludingScale;

    // ---- Right hand: skeletal preferred, then static ----
    if (Weapon->WeaponSkeletalMesh)
    {
        PrimarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        PrimarySkeletalMeshComp->SetSkeletalMesh(Weapon->WeaponSkeletalMesh);
        PrimarySkeletalMeshComp->RegisterComponent();
        PrimarySkeletalMeshComp->AttachToComponent(OwnerMesh, AttachRules, RightSocket);
        PrimarySkeletalMeshComp->SetRelativeRotation(Weapon->MeshRotation);
    }
    else if (Weapon->WeaponStaticMesh)
    {
        PrimaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        PrimaryStaticMeshComp->SetStaticMesh(Weapon->WeaponStaticMesh);
        PrimaryStaticMeshComp->RegisterComponent();
        PrimaryStaticMeshComp->AttachToComponent(OwnerMesh, AttachRules, RightSocket);
        PrimaryStaticMeshComp->SetRelativeRotation(Weapon->MeshRotation);
    }

    // ---- Left hand: distinct skeletal, distinct static, else reuse right ----
    // Sockets handle handedness — no mirror scale is applied.
    if (Weapon->LeftHandSkeletalMesh)
    {
        SecondarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        SecondarySkeletalMeshComp->SetSkeletalMesh(Weapon->LeftHandSkeletalMesh);
        SecondarySkeletalMeshComp->RegisterComponent();
        SecondarySkeletalMeshComp->AttachToComponent(OwnerMesh, AttachRules, LeftSocket);
        SecondarySkeletalMeshComp->SetRelativeRotation(Weapon->MeshRotation);
    }
    else if (Weapon->LeftHandStaticMesh)
    {
        SecondaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        SecondaryStaticMeshComp->SetStaticMesh(Weapon->LeftHandStaticMesh);
        SecondaryStaticMeshComp->RegisterComponent();
        SecondaryStaticMeshComp->AttachToComponent(OwnerMesh, AttachRules, LeftSocket);
        SecondaryStaticMeshComp->SetRelativeRotation(Weapon->MeshRotation);
    }
    else if (Weapon->WeaponSkeletalMesh)
    {
        // Fallback: reuse the right-hand skeletal asset on the left.
        SecondarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        SecondarySkeletalMeshComp->SetSkeletalMesh(Weapon->WeaponSkeletalMesh);
        SecondarySkeletalMeshComp->RegisterComponent();
        SecondarySkeletalMeshComp->AttachToComponent(OwnerMesh, AttachRules, LeftSocket);
        SecondarySkeletalMeshComp->SetRelativeRotation(Weapon->MeshRotation);
    }
    else if (Weapon->WeaponStaticMesh)
    {
        // Fallback: reuse the right-hand static asset on the left.
        SecondaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        SecondaryStaticMeshComp->SetStaticMesh(Weapon->WeaponStaticMesh);
        SecondaryStaticMeshComp->RegisterComponent();
        SecondaryStaticMeshComp->AttachToComponent(OwnerMesh, AttachRules, LeftSocket);
        SecondaryStaticMeshComp->SetRelativeRotation(Weapon->MeshRotation);
    }

    UE_LOG(LogTemp, Log, TEXT("[WeaponMeshComponent] Spawned dual weapon '%s' (R socket '%s', L socket '%s')"),
           *Weapon->Name, *RightSocket.ToString(), *LeftSocket.ToString());
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
    UE_LOG(LogTemp, Display, TEXT("Wield Mode: %s"),
           CachedWeapon ? *UEnum::GetValueAsString(CachedWeapon->WieldMode) : TEXT("N/A"));
    UE_LOG(LogTemp, Display, TEXT("Is Dual Wielded: %s"),
           CachedWeapon ? (CachedWeapon->IsDualWielded() ? TEXT("Yes") : TEXT("No")) : TEXT("N/A"));
    UE_LOG(LogTemp, Display, TEXT("Primary Static: %s"),
           PrimaryStaticMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Secondary Static: %s"),
           SecondaryStaticMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Primary Skeletal: %s"),
           PrimarySkeletalMeshComp ? TEXT("Spawned") : TEXT("None"));
    UE_LOG(LogTemp, Display, TEXT("Secondary Skeletal: %s"),
           SecondarySkeletalMeshComp ? TEXT("Spawned") : TEXT("None"));

    // Socket overrides authored on the weapon asset (NAME_None = use component default).
    UE_LOG(LogTemp, Display, TEXT("Weapon Right Socket Override: %s"),
           (CachedWeapon && !CachedWeapon->RightHandSocket.IsNone())
               ? *CachedWeapon->RightHandSocket.ToString() : TEXT("<unset>"));
    UE_LOG(LogTemp, Display, TEXT("Weapon Left Socket Override: %s"),
           (CachedWeapon && !CachedWeapon->LeftHandSocket.IsNone())
               ? *CachedWeapon->LeftHandSocket.ToString() : TEXT("<unset>"));

    // Sockets actually used for attachment, with their resolution source.
    UE_LOG(LogTemp, Display, TEXT("Resolved Right Socket: %s %s"),
           *ResolveRightSocket(CachedWeapon).ToString(),
           (CachedWeapon && !CachedWeapon->RightHandSocket.IsNone())
               ? TEXT("(weapon)") : TEXT("(component default)"));
    UE_LOG(LogTemp, Display, TEXT("Resolved Left Socket: %s %s"),
           *ResolveLeftSocket(CachedWeapon).ToString(),
           (CachedWeapon && !CachedWeapon->LeftHandSocket.IsNone())
               ? TEXT("(weapon)") : TEXT("(component default)"));

    // Which fallback tier supplies the left-hand mesh.
    FString LeftMeshSource = TEXT("<none>");
    if (CachedWeapon && CachedWeapon->IsDualWielded())
    {
        if (CachedWeapon->LeftHandSkeletalMesh)
        {
            LeftMeshSource = TEXT("LeftHandSkeletalMesh");
        }
        else if (CachedWeapon->LeftHandStaticMesh)
        {
            LeftMeshSource = TEXT("LeftHandStaticMesh");
        }
        else
        {
            LeftMeshSource = TEXT("<reused right-hand>");
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Left Mesh Source: %s"), *LeftMeshSource);
    UE_LOG(LogTemp, Display, TEXT("========================="));
}