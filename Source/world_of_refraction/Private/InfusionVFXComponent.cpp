// InfusionVFXComponent.cpp
// Manages infusion VFX spawning based on weapon's display location setting

#include "InfusionVFXComponent.h"
#include "CharacterDataComponent.h"
#include "WeaponData.h"
#include "WeaponMeshComponent.h"
#include "WeaponInfusionDisplayData.h"
#include "ElementColors.h"
#include "ActionExecutor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

UInfusionVFXComponent::UInfusionVFXComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UInfusionVFXComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor *Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No owner actor"));
        return;
    }

    // Cache component references
    CharacterDataComponent = Owner->FindComponentByClass<UCharacterDataComponent>();
    if (!CharacterDataComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No CharacterDataComponent found"));
    }

    WeaponMeshComponent = Owner->FindComponentByClass<UWeaponMeshComponent>();
    if (!WeaponMeshComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No WeaponMeshComponent found"));
    }

    // Get skeletal mesh for body attachment
    if (ACharacter *Character = Cast<ACharacter>(Owner))
    {
        OwnerMesh = Character->GetMesh();
    }
    else
    {
        OwnerMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    }

    if (!OwnerMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No SkeletalMeshComponent found"));
    }
}

void UInfusionVFXComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// ==================== PUBLIC API ====================

void UInfusionVFXComponent::ActivateInfusion(EInfusionSourceOption Source)
{
    // None = no infusion VFX
    if (Source == EInfusionSourceOption::None)
    {
        DeactivateInfusion();
        return;
    }

    // Get weapon data for VFX info
    UWeaponData *Weapon = GetActiveWeaponData();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No active weapon data"));
        return;
    }

    // Check for infusion display
    if (!Weapon->InfusionDisplay || !Weapon->InfusionDisplay->HasVFX())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] Weapon has no InfusionDisplay or VFX"));
        return;
    }

    // Clear existing VFX if source changed
    if (bIsInfusionActive && CurrentSource != Source)
    {
        ClearVFX();
    }

    CurrentSource = Source;
    CurrentElement = GetElementForSource(Source);

    // Spawn VFX at location specified by display data
    SpawnVFX(Weapon->InfusionDisplay->VFXSystem, Weapon->InfusionDisplay->DisplayLocation);

    // Apply element color
    FLinearColor Color;
    if (Weapon->InfusionDisplay->bOverrideElementColor)
    {
        Color = Weapon->InfusionDisplay->ColorOverride;
    }
    else
    {
        Color = GetElementColor(CurrentElement);
    }
    ApplyColorToVFX(Color);

    bIsInfusionActive = true;

    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Activated - Source: %d, Element: %d"),
           (int32)CurrentSource, (int32)CurrentElement);
}

void UInfusionVFXComponent::DeactivateInfusion()
{
    if (!bIsInfusionActive)
    {
        return;
    }

    ClearVFX();

    CurrentSource = EInfusionSourceOption::None;
    CurrentElement = ESpellElement::Generic;
    bIsInfusionActive = false;

    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Deactivated"));
}

void UInfusionVFXComponent::RefreshVFX()
{
    if (!bIsInfusionActive)
    {
        return;
    }

    // Re-activate with current source to pick up new weapon data
    EInfusionSourceOption PreviousSource = CurrentSource;
    DeactivateInfusion();
    ActivateInfusion(PreviousSource);
}

// ==================== INTERNAL ====================

void UInfusionVFXComponent::SpawnVFX(UNiagaraSystem *VFXSystem, EInfusionDisplayLocation Location)
{
    if (!VFXSystem)
    {
        return;
    }

    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Clear any existing VFX first
    ClearVFX();

    switch (Location)
    {
    case EInfusionDisplayLocation::Weapon:
        if (OwnerMesh)
        {
            ActiveVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
                VFXSystem,
                OwnerMesh,
                WeaponSocket,
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget,
                true);
        }
        break;

    case EInfusionDisplayLocation::Body:
        if (OwnerMesh)
        {
            ActiveVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
                VFXSystem,
                OwnerMesh,
                BodySocket,
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget,
                true);
        }
        break;

    case EInfusionDisplayLocation::Aura:
        ActiveVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            VFXSystem,
            Owner->GetRootComponent(),
            NAME_None,
            AuraOffset,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            true);
        break;
    }

    // Pass mesh reference for surface sampling
    if (ActiveVFXComponent)
    {
        SetMeshParameterForLocation(Location);
    }
}

void UInfusionVFXComponent::ClearVFX()
{
    if (ActiveVFXComponent)
    {
        ActiveVFXComponent->DestroyComponent();
        ActiveVFXComponent = nullptr;
    }
}

void UInfusionVFXComponent::ApplyColorToVFX(FLinearColor Color)
{
    if (!ActiveVFXComponent)
    {
        return;
    }

    ActiveVFXComponent->SetVariableLinearColor(ColorParameterName, Color);
}

void UInfusionVFXComponent::SetMeshParameterForLocation(EInfusionDisplayLocation Location)
{
    if (!ActiveVFXComponent)
    {
        return;
    }

    switch (Location)
    {
    case EInfusionDisplayLocation::Weapon:
        // Pass weapon static mesh component for surface sampling
        if (WeaponMeshComponent)
        {
            UStaticMeshComponent *WeaponStaticMesh = WeaponMeshComponent->GetPrimaryStaticMeshComp();
            if (WeaponStaticMesh)
            {
                UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMeshComponent(
                    ActiveVFXComponent,
                    WeaponMeshParameterName.ToString(),
                    WeaponStaticMesh);
            }
        }
        break;

    case EInfusionDisplayLocation::Body:
        // Pass skeletal mesh component for body surface sampling
        if (OwnerMesh)
        {
            UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(
                ActiveVFXComponent,
                BodyMeshParameterName.ToString(),
                OwnerMesh);
        }
        break;

    case EInfusionDisplayLocation::Aura:
        // Aura VFX follows character via attachment
        // Aura doesn't need mesh reference - particles float freely
        break;
    }
}

UWeaponData *UInfusionVFXComponent::GetActiveWeaponData() const
{
    if (!CharacterDataComponent)
    {
        return nullptr;
    }

    return CharacterDataComponent->GetActiveWeapon();
}

ESpellElement UInfusionVFXComponent::GetElementForSource(EInfusionSourceOption Source) const
{
    if (Source == EInfusionSourceOption::None)
    {
        return ESpellElement::Generic;
    }

    // Use ActionExecutor to resolve element
    UGameInstance *GI = Cast<UGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (GI)
    {
        UActionExecutor *Executor = GI->GetSubsystem<UActionExecutor>();
        if (Executor)
        {
            return Executor->GetElementForSourceOption(GetOwner(), Source);
        }
    }

    return ESpellElement::Generic;
}

FLinearColor UInfusionVFXComponent::GetElementColor(ESpellElement Element) const
{
    return ElementColors::GetColorForElement(Element);
}

// ==================== DEBUG ====================

void UInfusionVFXComponent::DebugLogVFXState()
{
    UE_LOG(LogTemp, Display, TEXT("========== INFUSION VFX STATE =========="));
    UE_LOG(LogTemp, Display, TEXT("Active: %s"), bIsInfusionActive ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Display, TEXT("Current Source: %d"), (int32)CurrentSource);
    UE_LOG(LogTemp, Display, TEXT("Current Element: %d"), (int32)CurrentElement);
    UE_LOG(LogTemp, Display, TEXT("VFX Component: %s"), ActiveVFXComponent ? TEXT("Spawned") : TEXT("None"));

    UWeaponData *Weapon = GetActiveWeaponData();
    if (Weapon)
    {
        UE_LOG(LogTemp, Display, TEXT("Weapon: %s"), *Weapon->WeaponName);
        if (Weapon->InfusionDisplay)
        {
            UE_LOG(LogTemp, Display, TEXT("Display: %s"), *Weapon->InfusionDisplay->DisplayName);
            UE_LOG(LogTemp, Display, TEXT("Location: %d"), (int32)Weapon->InfusionDisplay->DisplayLocation);
            UE_LOG(LogTemp, Display, TEXT("Has VFX: %s"), Weapon->InfusionDisplay->HasVFX() ? TEXT("YES") : TEXT("NO"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Display: None assigned"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Weapon: None"));
    }

    UE_LOG(LogTemp, Display, TEXT("========================================="));
}

void UInfusionVFXComponent::DebugActivateWithElement(ESpellElement TestElement)
{
    // Force activate with test element (bypasses source resolution)
    UWeaponData *Weapon = GetActiveWeaponData();
    if (!Weapon || !Weapon->InfusionDisplay || !Weapon->InfusionDisplay->HasVFX())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX Debug] No weapon or VFX to test"));
        return;
    }

    ClearVFX();

    CurrentSource = EInfusionSourceOption::Innate; // Fake source for debug
    CurrentElement = TestElement;

    SpawnVFX(Weapon->InfusionDisplay->VFXSystem, Weapon->InfusionDisplay->DisplayLocation);
    ApplyColorToVFX(GetElementColor(TestElement));

    bIsInfusionActive = true;

    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX Debug] Activated with element: %d"), (int32)TestElement);
}

void UInfusionVFXComponent::DebugDeactivate()
{
    DeactivateInfusion();
    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX Debug] Deactivated"));
}