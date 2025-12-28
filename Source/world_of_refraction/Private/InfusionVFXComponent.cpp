// InfusionVFXComponent.cpp
// Manages infusion VFX spawning based on weapon's display location setting

#include "InfusionVFXComponent.h"
#include "CharacterDataComponent.h"
#include "WeaponData.h"
#include "WeaponMeshComponent.h"
#include "InfusionDisplayData.h"
#include "ElementColors.h"
#include "ActionExecutor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "ActionExecutor.h"
#include "LoadoutComponent.h"
#include "InfusionChargeManager.h"

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

    LoadoutComponent = Owner->FindComponentByClass<ULoadoutComponent>();
    if (!LoadoutComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No LoadoutComponent found"));
    }

    WeaponMeshComponent = Owner->FindComponentByClass<UWeaponMeshComponent>();
    if (!WeaponMeshComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No WeaponMeshComponent found"));
    }

    if (UGameInstance *GI = Cast<UGameInstance>(UGameplayStatics::GetGameInstance(this)))
    {
        if (UInfusionChargeManager *ChargeMgr = GI->GetSubsystem<UInfusionChargeManager>())
        {
            ChargeMgr->OnChargeLevelChanged.AddDynamic(this, &UInfusionVFXComponent::OnChargeLevelChanged);
            ChargeMgr->OnChargeComplete.AddDynamic(this, &UInfusionVFXComponent::OnChargeComplete);
            ChargeMgr->OnChargeCancelled.AddDynamic(this, &UInfusionVFXComponent::OnChargeCancelled);
        }
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

void UInfusionVFXComponent::SetInfusionLevel(int32 Level)
{
    CurrentInfusionLevel = FMath::Clamp(Level, 0, 2);

    // Level 0 (Raw) = No VFX
    if (CurrentInfusionLevel == 0)
    {
        DeactivateInfusion();
        UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Level 0 (Raw) - VFX disabled"));
        return;
    }

    // Activate if not already active
    if (!bIsInfusionActive)
    {
        ActivateCurrentSource();
    }

    // Scale based on level
    float ScaleMultiplier = (CurrentInfusionLevel >= 2) ? L2ScaleMultiplier : L1ScaleMultiplier;

    if (ActiveVFXComponent)
    {
        ActiveVFXComponent->SetVariableFloat(ScaleParameterName, ScaleMultiplier);
        UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Level %d - Scale: %.2f"), CurrentInfusionLevel, ScaleMultiplier);
    }
}

void UInfusionVFXComponent::CycleInfusionLevel()
{
    int32 NextLevel = (CurrentInfusionLevel + 1) % 3;
    SetInfusionLevel(NextLevel);
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

    // Check for character data
    if (!CharacterDataComponent || !CharacterDataComponent->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No CharacterData"));
        return;
    }

    // Check for infusion display
    UInfusionDisplayData *InfusionDisplay = LoadoutComponent ? LoadoutComponent->GetInfusionDisplay() : nullptr;
    if (!InfusionDisplay || !InfusionDisplay->HasVFX())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No InfusionDisplay or VFX assigned"));
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
    SpawnVFX(InfusionDisplay->VFXSystem, InfusionDisplay->DisplayLocation);

    // Apply element color
    FLinearColor Color;
    if (InfusionDisplay->bOverrideElementColor)
    {
        Color = InfusionDisplay->ColorOverride;
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

void UInfusionVFXComponent::OnChargeLevelChanged(AActor *Actor, int32 OldLevel, int32 NewLevel, float HPCostDeducted)
{
    if (Actor != GetOwner())
        return;
    SetInfusionLevel(NewLevel);
}

void UInfusionVFXComponent::OnChargeComplete(AActor *Actor, EChargeInfusionType ChargeType, EInfusionSourceOption Source, int32 FinalLevel)
{
    if (Actor != GetOwner())
        return;
    // VFX stays at final level - will be deactivated after spell completes
}

void UInfusionVFXComponent::OnChargeCancelled(AActor *Actor, int32 LevelAtCancel, float TotalHPCostPaid)
{
    if (Actor != GetOwner())
        return;
    DeactivateInfusion();
}

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

    if (CharacterDataComponent && CharacterDataComponent->CharacterData)
    {
        UInfusionDisplayData *InfusionDisplay = LoadoutComponent ? LoadoutComponent->GetInfusionDisplay() : nullptr;
        if (InfusionDisplay)
        {
            UE_LOG(LogTemp, Display, TEXT("Display: %s"), *InfusionDisplay->DisplayName);
            UE_LOG(LogTemp, Display, TEXT("Location: %d"), (int32)InfusionDisplay->DisplayLocation);
            UE_LOG(LogTemp, Display, TEXT("Has VFX: %s"), InfusionDisplay->HasVFX() ? TEXT("YES") : TEXT("NO"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Display: None assigned"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("CharacterData: None"));
    }

    UE_LOG(LogTemp, Display, TEXT("========================================="));
}

void UInfusionVFXComponent::DebugActivateWithElement(ESpellElement TestElement)
{
    ClearVFX();

    CurrentSource = EInfusionSourceOption::Innate; // Fake source for debug
    CurrentElement = TestElement;

    UInfusionDisplayData *InfusionDisplay = LoadoutComponent ? LoadoutComponent->GetInfusionDisplay() : nullptr;
    if (InfusionDisplay)
    {
        SpawnVFX(InfusionDisplay->VFXSystem, InfusionDisplay->DisplayLocation);
    }
    ApplyColorToVFX(GetElementColor(TestElement));

    bIsInfusionActive = true;

    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX Debug] Activated with element: %d"), (int32)TestElement);
}

void UInfusionVFXComponent::DebugDeactivate()
{
    DeactivateInfusion();
    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX Debug] Deactivated"));
}

// ==================== SOURCE CYCLING ====================

void UInfusionVFXComponent::CacheAvailableSources()
{
    CachedSources.Empty();
    CurrentSourceIndex = 0;

    AActor *Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No owner"));
        return;
    }

    UGameInstance *GI = Cast<UGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (!GI)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No GameInstance"));
        return;
    }

    UActionExecutor *Executor = GI->GetSubsystem<UActionExecutor>();
    if (!Executor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No ActionExecutor"));
        return;
    }

    CachedSources = Executor->GetAvailableInfusionSources(Owner);

    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Cached %d sources"), CachedSources.Num());
}

void UInfusionVFXComponent::CycleToNextSource()
{
    if (CachedSources.Num() == 0)
    {
        CacheAvailableSources();
    }

    if (CachedSources.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No sources available"));
        return;
    }

    CurrentSourceIndex = (CurrentSourceIndex + 1) % CachedSources.Num();

    EInfusionSourceOption Source = CachedSources[CurrentSourceIndex];
    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Cycled to: [%d/%d] %s"),
           CurrentSourceIndex + 1,
           CachedSources.Num(),
           *GetCurrentSourceName());
}

void UInfusionVFXComponent::ActivateCurrentSource()
{
    if (CachedSources.Num() == 0)
    {
        CacheAvailableSources();
    }

    if (!CachedSources.IsValidIndex(CurrentSourceIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] Invalid source index"));
        return;
    }

    EInfusionSourceOption Source = CachedSources[CurrentSourceIndex];
    UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Activating: %s"), *GetCurrentSourceName());

    ActivateInfusion(Source);
}

FString UInfusionVFXComponent::GetCurrentSourceName() const
{
    if (!CachedSources.IsValidIndex(CurrentSourceIndex))
    {
        return TEXT("None");
    }

    EInfusionSourceOption Source = CachedSources[CurrentSourceIndex];

    switch (Source)
    {
    case EInfusionSourceOption::None:
        return TEXT("Physical");
    case EInfusionSourceOption::Innate:
        return TEXT("Innate");
    case EInfusionSourceOption::ActiveRing:
        return TEXT("Active Ring");
    case EInfusionSourceOption::WeaponCrystal:
        return TEXT("Weapon Crystal");
    case EInfusionSourceOption::Evolution:
        return TEXT("Evolution");
    default:
        return TEXT("Unknown");
    }
}

void UInfusionVFXComponent::ToggleInfusion()
{
    if (bIsInfusionActive)
    {
        DeactivateInfusion();
        UE_LOG(LogTemp, Display, TEXT("[InfusionVFX] Toggled OFF"));
    }
    else
    {
        ActivateCurrentSource();
    }
}