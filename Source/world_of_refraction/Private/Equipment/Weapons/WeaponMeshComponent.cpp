// WeaponMeshComponent.cpp

#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/EWeaponType.h"
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

FName UWeaponMeshComponent::GetRightSocketForWeapon(const UWeaponData *Weapon) const
{
    if (!Weapon)
    {
        return NAME_None;
    }

    switch (Weapon->WeaponType)
    {
    case EWeaponType::Sword:
    case EWeaponType::Dagger:
        return FName("hand_r_sword");
    case EWeaponType::Greatsword:
        return FName("hand_r_greatsword");
    case EWeaponType::Axe:
        return FName("hand_r_axe");

    default:
        UE_LOG(LogTemp, Error,
               TEXT("[WeaponMeshComponent] No right-hand socket mapped for weapon '%s' (WeaponType %s)."),
               *Weapon->Name, *UEnum::GetValueAsString(Weapon->WeaponType));
        return NAME_None;
    }
}

FName UWeaponMeshComponent::GetLeftSocketForWeapon(const UWeaponData *Weapon) const
{
    if (!Weapon)
    {
        return NAME_None;
    }

    // A shield always uses the same socket regardless of the right-hand weapon type.
    if (Weapon->WieldMode == EWeaponWieldMode::OffHandShield)
    {
        return FName("hand_l_sas");
    }

    switch (Weapon->WeaponType)
    {

    case EWeaponType::Sword:
    case EWeaponType::Dagger:
        return FName("hand_l_sword");
    case EWeaponType::Greatsword:
        return FName("hand_l_greatsword");
    case EWeaponType::Axe:
        return FName("hand_l_axe");
    default:
        UE_LOG(LogTemp, Error,
               TEXT("[WeaponMeshComponent] No left-hand socket mapped for weapon '%s' (WeaponType %s)."),
               *Weapon->Name, *UEnum::GetValueAsString(Weapon->WeaponType));
        return NAME_None;
    }
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

    const FName RightSocket = GetRightSocketForWeapon(Weapon);
    if (RightSocket.IsNone())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WeaponMeshComponent] No right-hand socket for weapon '%s' — skipping right-hand mesh spawn."),
               *Weapon->Name);
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

    const FName RightSocket = GetRightSocketForWeapon(Weapon);
    const FName LeftSocket = GetLeftSocketForWeapon(Weapon);
    const FAttachmentTransformRules AttachRules =
        FAttachmentTransformRules::SnapToTargetNotIncludingScale;

    // ---- Right hand: skeletal preferred, then static ----
    if (RightSocket.IsNone())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WeaponMeshComponent] No right-hand socket for weapon '%s' — skipping right-hand mesh spawn."),
               *Weapon->Name);
    }
    else if (Weapon->WeaponSkeletalMesh)
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

    // ---- Left hand: distinct skeletal preferred, then distinct static ----
    // Explicit assignment required — no reuse of the right-hand mesh.
    // Sockets handle handedness — no mirror scale is applied.
    if (LeftSocket.IsNone())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WeaponMeshComponent] No left-hand socket for weapon '%s' — skipping left-hand mesh spawn."),
               *Weapon->Name);
    }
    else if (Weapon->LeftHandSkeletalMesh)
    {
        SecondarySkeletalMeshComp = NewObject<USkeletalMeshComponent>(GetOwner());
        SecondarySkeletalMeshComp->SetSkeletalMesh(Weapon->LeftHandSkeletalMesh);
        SecondarySkeletalMeshComp->RegisterComponent();
        SecondarySkeletalMeshComp->AttachToComponent(OwnerMesh, AttachRules, LeftSocket);
        SecondarySkeletalMeshComp->SetRelativeRotation(Weapon->LeftMeshRotation);
    }
    else if (Weapon->LeftHandStaticMesh)
    {
        SecondaryStaticMeshComp = NewObject<UStaticMeshComponent>(GetOwner());
        SecondaryStaticMeshComp->SetStaticMesh(Weapon->LeftHandStaticMesh);
        SecondaryStaticMeshComp->RegisterComponent();
        SecondaryStaticMeshComp->AttachToComponent(OwnerMesh, AttachRules, LeftSocket);
        SecondaryStaticMeshComp->SetRelativeRotation(Weapon->LeftMeshRotation);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WeaponMeshComponent] Dual-mode weapon '%s' has no left-hand mesh assigned "
                    "(LeftHandSkeletalMesh and LeftHandStaticMesh are both null) — no left mesh spawned."),
               *Weapon->Name);
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

    // Sockets derived from the weapon's EWeaponType (and WieldMode for the left hand).
    UE_LOG(LogTemp, Display, TEXT("Derived Right Socket: %s"),
           *GetRightSocketForWeapon(CachedWeapon).ToString());
    UE_LOG(LogTemp, Display, TEXT("Derived Left Socket: %s"),
           *GetLeftSocketForWeapon(CachedWeapon).ToString());

    UE_LOG(LogTemp, Display, TEXT("Mesh Rotation (right): %s"),
           CachedWeapon ? *CachedWeapon->MeshRotation.ToString() : TEXT("N/A"));
    UE_LOG(LogTemp, Display, TEXT("Left Mesh Rotation: %s"),
           CachedWeapon ? *CachedWeapon->LeftMeshRotation.ToString() : TEXT("N/A"));

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
            LeftMeshSource = TEXT("<none — left mesh unassigned>");
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Left Mesh Source: %s"), *LeftMeshSource);
    UE_LOG(LogTemp, Display, TEXT("========================="));
}