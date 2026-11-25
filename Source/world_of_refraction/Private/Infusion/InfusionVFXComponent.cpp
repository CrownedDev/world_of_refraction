// InfusionVFXComponent.cpp
// Manages infusion VFX spawning based on weapon's display location setting

#include "Infusion/InfusionVFXComponent.h"
#include "WoRLog.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "Infusion/InfusionDisplayData.h"
#include "Infusion/ElementColors.h"
#include "Combat/Actions/ActionExecutor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Loadout/LoadoutComponent.h"
#include "Infusion/InfusionChargeManager.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Combat/Presentation/EffectTint.h"
#include "Combat/WoRFightSubsystem.h"
#include "Loadout/FCombatLoadout.h"
#include "NiagaraSystem.h"

namespace
{
    /** The display's own glow, else the default glow on DA_ImpactVisualDefaults (Raw's, Physical's or the element's);
     *  null with neither. */
    UNiagaraSystem *FindGlowSystem(const UInfusionDisplayData *Display, ESpellElement Element, bool bRaw, bool bIce)
    {
        if (Display && Display->HasVFX())
        {
            return Display->VFXSystem;
        }
        const UImpactVisualDefaults *Defaults = UImpactVisualDefaults::Get();
        // shortcut: loads the default glow on first use, which may hitch once; preload with the fight's assets when the art lands.
        return Defaults ? Defaults->GetInfusionGlowVFX(Element, bRaw, bIce).LoadSynchronous() : nullptr;
    }
}

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
    if (WeaponMeshComponent)
    {
        WeaponMeshComponent->OnWeaponMeshRebuilt.AddUObject(this, &UInfusionVFXComponent::HandleWeaponMeshRebuilt);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No WeaponMeshComponent found"));
    }

    if (UGameInstance *GI = Cast<UGameInstance>(UGameplayStatics::GetGameInstance(this)))
    {
        if (UInfusionChargeManager *ChargeMgr = WoRFight::Get<UInfusionChargeManager>(GI))
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

void UInfusionVFXComponent::SetInfusionLevel(int32 Level)
{
    if (bSuppressForImmuneAction)
    {
        CurrentInfusionLevel = 0;
        DeactivateInfusion();
        UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Suppressed - action is immune to infusion"));
        return;
    }

    CurrentInfusionLevel = FMath::Clamp(Level, 0, 2);

    // Level 0 = no infusion = no VFX. Raw is a *source* choice paying HP cost
    // at non-zero levels; it is unrelated to Level 0.
    if (CurrentInfusionLevel == 0)
    {
        DeactivateInfusion();
        UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Level 0 - VFX disabled"));
        return;
    }

    // Activate if not already active
    if (!bIsInfusionActive)
    {
        ActivateCurrentSource();
    }

    ApplyLevelToVFX();
}

void UInfusionVFXComponent::ApplyLevelToVFX()
{
    if (!ActiveVFXComponent)
    {
        return;
    }
    const FInfusionLevelVisual Visual = UImpactVisualDefaults::ForInfusionLevel(CurrentInfusionLevel);
    ForEachLook([this, &Visual](UNiagaraComponent &Look) // B-626: every stacked look plays the level
                {
                    Look.SetVariableInt(InfusionLevelParameterName, CurrentInfusionLevel);
                    Look.SetVariableFloat(ScaleParameterName, Visual.Size);
                });
    ApplyColorToVFX(Visual.Brighten(CurrentGlowColor));
    UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Level %d - intensity x%.2f, size x%.2f"), CurrentInfusionLevel, Visual.Intensity, Visual.Size);
}

void UInfusionVFXComponent::CycleInfusionLevel()
{
    int32 NextLevel = (CurrentInfusionLevel + 1) % 3;
    SetInfusionLevel(NextLevel);
}

void UInfusionVFXComponent::SetImmuneToInfusion(bool bImmune)
{
    if (bSuppressForImmuneAction == bImmune)
    {
        return;
    }

    bSuppressForImmuneAction = bImmune;

    if (bImmune && bIsInfusionActive)
    {
        DeactivateInfusion();
    }

    UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Immunity flag set: %s"),
           bImmune ? TEXT("true") : TEXT("false"));
}

void UInfusionVFXComponent::SetWeaponInfusion(bool bWeapon)
{
    if (bWeaponInfusion == bWeapon)
    {
        return;
    }
    bWeaponInfusion = bWeapon;
    RespawnKeepingLevel();
}

void UInfusionVFXComponent::SetDealsIce(bool bIce)
{
    if (bDealsIce == bIce)
    {
        return;
    }
    bDealsIce = bIce;
    RespawnKeepingLevel();
}

void UInfusionVFXComponent::HandleWeaponMeshRebuilt()
{
    // B-626: a weapon crystal's glow, or a stacked weapon look, rides the weapon too.
    if (bWeaponInfusion || CurrentSource == EInfusionSourceOption::WeaponCrystal || !StackLookComponents.IsEmpty())
    {
        RespawnKeepingLevel();
    }
}

void UInfusionVFXComponent::RespawnKeepingLevel()
{
    if (!bIsInfusionActive)
    {
        return;
    }
    RefreshVFX();
    SetInfusionLevel(CurrentInfusionLevel);
}

// ==================== PUBLIC API ====================
void UInfusionVFXComponent::ActivateInfusion(EInfusionSourceOption Source)
{
    if (bSuppressForImmuneAction)
    {
        DeactivateInfusion();
        return;
    }

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

    // The display's own glow, else the element's default (Crown 2026-11-24)
    UInfusionDisplayData *InfusionDisplay = LoadoutComponent ? LoadoutComponent->GetInfusionDisplay() : nullptr;
    const ESpellElement Element = GetElementForSource(Source);
    const bool bRaw = Source == EInfusionSourceOption::Raw;
    const bool bIcePicked = IsCurrentSourceIce() && CachedSources[CurrentSourceIndex] == Source; // B-593: the picker's Ice entry
    const bool bIce = !bRaw && (bDealsIce || bIcePicked) && Element == ESpellElement::Water;
    // B-626 (D-277): the source picks its look kind. A ring flame or an aura with no art yet plays the glow, as before.
    const EInfusionLookKind Kind = InfusionLook::KindForSource(Source);
    UNiagaraComponent *Look = (Kind == EInfusionLookKind::Ring || Kind == EInfusionLookKind::Aura) ? SpawnLook(Kind, Element, bRaw, bIce) : nullptr;
    UNiagaraSystem *Glow = Look ? nullptr : FindGlowSystem(InfusionDisplay, Element, bRaw, bIce);
    if (!Look && !Glow)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No glow: the display has no VFX and %s has no default glow"),
               bRaw ? TEXT("Raw") : (bIce ? TEXT("Ice") : *UEnum::GetValueAsString(Element)));
        return;
    }

    CurrentSource = Source;
    CurrentElement = Element;

    if (Look)
    {
        ClearVFX();
        ActiveVFXComponent = Look;
    }
    else
    {
        SpawnVFX(Glow, InfusionDisplay ? InfusionDisplay->DisplayLocation : EInfusionDisplayLocation::Weapon); // clears first
    }
    // B-626: each other stacked source's kind shows too (a kind the glow above already covers isn't doubled).
    const bool bGlowOnWeapon = bWeaponInfusion || Source == EInfusionSourceOption::WeaponCrystal ||
                               !InfusionDisplay || InfusionDisplay->DisplayLocation == EInfusionDisplayLocation::Weapon;
    const EInfusionLookKind ShownKind = (Look || !bGlowOnWeapon) ? Kind : EInfusionLookKind::Weapon;
    for (const EInfusionLookKind StackKind : GetLookKindsFor(Source, CurrentInfusionLevel))
    {
        UNiagaraComponent *Extra = StackKind != ShownKind && StackKind != Kind ? SpawnLook(StackKind, Element, bRaw, bIce) : nullptr;
        if (Extra)
        {
            StackLookComponents.Add(Extra);
        }
    }

    // Element color, brightened and sized by the level
    CurrentGlowColor = (InfusionDisplay && InfusionDisplay->bOverrideElementColor) ? InfusionDisplay->ColorOverride
                       : (bIce ? ElementColors::Ice : GetElementColor(CurrentElement));
    ApplyLevelToVFX();

    bIsInfusionActive = true;

    UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Activated - Source: %d, Element: %d"),
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
    CurrentElement = ESpellElement::None;
    bIsInfusionActive = false;

    UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Deactivated"));
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

void UInfusionVFXComponent::OnChargeLevelChanged(AActor *Actor, int32 OldLevel, int32 NewLevel)
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

void UInfusionVFXComponent::OnChargeCancelled(AActor *Actor, int32 LevelAtCancel)
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

    // B-626 (D-277): a weapon crystal's glow is on the weapon, whatever the action.
    const bool bOnWeapon = bWeaponInfusion || CurrentSource == EInfusionSourceOption::WeaponCrystal;
    UMeshComponent *WeaponMesh = (bOnWeapon && WeaponMeshComponent) ? WeaponMeshComponent->GetWeaponVFXMesh() : nullptr;
    if (WeaponMesh)
    {
        ActiveVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            VFXSystem, WeaponMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
        if (ActiveVFXComponent)
        {
            SetMeshParameterForLocation(EInfusionDisplayLocation::Weapon);
        }
        return;
    }
    if (bOnWeapon)
    {
        UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Weapon infusion with no weapon mesh shown - using the display's place"));
    }

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
    ForEachLook([](UNiagaraComponent &Look) { Look.DestroyComponent(); });
    ActiveVFXComponent = nullptr;
    StackLookComponents.Reset();
}

void UInfusionVFXComponent::ApplyColorToVFX(FLinearColor Color)
{
    // B-626: the ring flames and auras may be SoftTofu effects, which read User.Color_ (EffectTint, an element only).
    const bool bSoftTofu = EffectTint::ShouldSetSoftTofuColour(CurrentElement);
    ForEachLook([this, &Color, bSoftTofu](UNiagaraComponent &Look)
                {
                    Look.SetVariableLinearColor(ColorParameterName, Color);
                    if (bSoftTofu)
                    {
                        EffectTint::SetSoftTofuColour(Look, Color);
                    }
                });
}

void UInfusionVFXComponent::ForEachLook(TFunctionRef<void(UNiagaraComponent &)> Visit) const
{
    if (ActiveVFXComponent)
    {
        Visit(*ActiveVFXComponent);
    }
    for (UNiagaraComponent *Look : StackLookComponents)
    {
        if (Look)
        {
            Visit(*Look);
        }
    }
}

TArray<EInfusionSourceOption> UInfusionVFXComponent::ResolveShownStack(EInfusionSourceOption Source, int32 Level) const
{
    const UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(this);
    if (!Executor || !InfusionSourceOptionHelpers::HasElement(Source))
    {
        return {Source};
    }
    // shortcut: the stack as an infused ability's (every same-element source); a spell's origin rule (B-435b) isn't
    // applied to the look. Resolve it from the pending action if a spell's look must match its stack exactly.
    FAction Probe;
    Probe.ActionType = EActionType::Ability;
    Probe.SelectedSource = Source;
    Probe.AbilityInfusionLevel = FMath::Max(1, Level);
    TArray<EInfusionSourceOption> Stack = Executor->GetSameElementInfusionSources(GetOwner(), Probe);
    if (Stack.IsEmpty())
    {
        Stack.Add(Source);
    }
    return Stack;
}

TArray<EInfusionLookKind> UInfusionVFXComponent::GetLookKindsFor(EInfusionSourceOption Source, int32 Level) const
{
    return InfusionLook::KindsForStack(ResolveShownStack(Source, Level));
}

UNiagaraComponent *UInfusionVFXComponent::SpawnLook(EInfusionLookKind Kind, ESpellElement Element, bool bRaw, bool bIce) const
{
    const UImpactVisualDefaults *Defaults = UImpactVisualDefaults::Get();
    // A weapon look is the display's own glow (else the element's default), the same as the primary weapon glow.
    // shortcut: loads on first use, which may hitch once; preload with the fight's assets when the art lands.
    UNiagaraSystem *System = Kind == EInfusionLookKind::Weapon
                                 ? FindGlowSystem(LoadoutComponent ? LoadoutComponent->GetInfusionDisplay() : nullptr, Element, bRaw, bIce)
                                 : (Defaults ? Defaults->GetInfusionLookVFX(Kind, Element, bRaw, bIce).LoadSynchronous() : nullptr);
    if (!System)
    {
        return nullptr; // no art for this kind yet (the editor worker fills DA_ImpactVisualDefaults)
    }
    switch (Kind)
    {
    case EInfusionLookKind::Weapon:
    {
        UMeshComponent *WeaponMesh = WeaponMeshComponent ? WeaponMeshComponent->GetWeaponVFXMesh() : nullptr;
        UNiagaraComponent *Glow = WeaponMesh ? UNiagaraFunctionLibrary::SpawnSystemAttached(System, WeaponMesh, NAME_None, FVector::ZeroVector,
                                                                                            FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true)
                                             : nullptr;
        if (UStaticMeshComponent *StaticWeapon = Glow ? Cast<UStaticMeshComponent>(WeaponMesh) : nullptr)
        {
            UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMeshComponent(Glow, WeaponMeshParameterName.ToString(), StaticWeapon);
        }
        else if (USkeletalMeshComponent *SkeletalWeapon = Glow ? Cast<USkeletalMeshComponent>(WeaponMesh) : nullptr)
        {
            UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(Glow, WeaponMeshParameterName.ToString(), SkeletalWeapon);
        }
        return Glow;
    }
    case EInfusionLookKind::Ring:
    {
        const FLoadoutSlot *Adapter = LoadoutComponent ? LoadoutComponent->GetActiveSlot(EGearKind::Adapter) : nullptr;
        const int32 Finger = Adapter ? InfusionLook::RingFingerIndex(LoadoutComponent->GetActiveLoadout().Slots, Adapter) : INDEX_NONE;
        const FName Socket = InfusionLook::FingerSocket(Defaults->RingFingerSockets, Finger);
        if (!OwnerMesh || Socket.IsNone())
        {
            return nullptr;
        }
        UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Ring flame on finger %d (%s)"), Finger, *Socket.ToString());
        return UNiagaraFunctionLibrary::SpawnSystemAttached(System, OwnerMesh, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
                                                            EAttachLocation::SnapToTarget, true);
    }
    case EInfusionLookKind::Aura:
        return GetOwner() && GetOwner()->GetRootComponent()
                   ? UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetOwner()->GetRootComponent(), NAME_None, AuraOffset,
                                                                  FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true)
                   : nullptr;
    default:
        return nullptr;
    }
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
        // Pass the weapon in hand's mesh (static or skeletal) for surface sampling
        if (UMeshComponent *WeaponMesh = WeaponMeshComponent ? WeaponMeshComponent->GetWeaponVFXMesh() : nullptr)
        {
            if (UStaticMeshComponent *WeaponStaticMesh = Cast<UStaticMeshComponent>(WeaponMesh))
            {
                UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMeshComponent(
                    ActiveVFXComponent, WeaponMeshParameterName.ToString(), WeaponStaticMesh);
            }
            else if (USkeletalMeshComponent *WeaponSkeletalMesh = Cast<USkeletalMeshComponent>(WeaponMesh))
            {
                UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(
                    ActiveVFXComponent, WeaponMeshParameterName.ToString(), WeaponSkeletalMesh);
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
        return ESpellElement::None;
    }

    // Use ActionExecutor to resolve element
    UGameInstance *GI = Cast<UGameInstance>(UGameplayStatics::GetGameInstance(this));
    if (GI)
    {
        UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(GI);
        if (Executor)
        {
            return Executor->GetElementForSourceOption(GetOwner(), Source);
        }
    }

    return ESpellElement::None;
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
    if (bIsInfusionActive) // B-626: the kinds the source and its stack show, and how many looks spawned (art present)
    {
        UE_LOG(LogTemp, Display, TEXT("Look kinds: %s (%d stacked look(s) spawned)"),
               *FString::JoinBy(GetLookKindsFor(CurrentSource, CurrentInfusionLevel), TEXT(", "),
                                [](EInfusionLookKind Kind) { return UEnum::GetDisplayValueAsText(Kind).ToString(); }),
               StackLookComponents.Num());
    }

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
    SpawnVFX(FindGlowSystem(InfusionDisplay, TestElement, false, false), InfusionDisplay ? InfusionDisplay->DisplayLocation : EInfusionDisplayLocation::Weapon);
    CurrentGlowColor = GetElementColor(TestElement);
    ApplyLevelToVFX();

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
    CachedSourceIsIce.Empty();
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

    UActionExecutor *Executor = WoRFight::Get<UActionExecutor>(GI);
    if (!Executor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InfusionVFX] No ActionExecutor"));
        return;
    }

    // D-271 (B-593): each Water source is followed by its Ice entry (the same source, picked as Ice).
    for (const EInfusionSourceOption Source : Executor->GetAvailableInfusionSources(Owner))
    {
        CachedSources.Add(Source);
        CachedSourceIsIce.Add(false);
        if (InfusionSourceOptionHelpers::HasElement(Source) && Executor->GetElementForSourceOption(Owner, Source) == ESpellElement::Water)
        {
            CachedSources.Add(Source);
            CachedSourceIsIce.Add(true);
        }
    }

    UE_LOG(LogWoRCombat, Verbose, TEXT("[InfusionVFX] Cached %d sources"), CachedSources.Num());
}

void UInfusionVFXComponent::RestrictCachedSources(const TArray<EInfusionSourceOption> &Allowed)
{
    TArray<EInfusionSourceOption> Filtered;
    TArray<bool> FilteredIce;
    Filtered.Reserve(CachedSources.Num());
    for (int32 Index = 0; Index < CachedSources.Num(); ++Index)
    {
        if (Allowed.Contains(CachedSources[Index]))
        {
            Filtered.Add(CachedSources[Index]);
            FilteredIce.Add(IsSourceIce(Index)); // a kept Water source keeps its Ice entry
        }
    }
    CachedSources = MoveTemp(Filtered);
    CachedSourceIsIce = MoveTemp(FilteredIce);
    CurrentSourceIndex = 0;

    UE_LOG(LogTemp, Verbose, TEXT("[InfusionVFX] Restricted to %d sources"), CachedSources.Num());
}

void UInfusionVFXComponent::DropIceEntries()
{
    for (int32 Index = CachedSourceIsIce.Num() - 1; Index >= 0; --Index)
    {
        if (CachedSourceIsIce[Index])
        {
            CachedSources.RemoveAt(Index);
            CachedSourceIsIce.RemoveAt(Index);
        }
    }
    CurrentSourceIndex = 0;
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
    UE_LOG(LogTemp, Verbose, TEXT("[InfusionVFX] Cycled to: [%d/%d] %s"),
           CurrentSourceIndex + 1,
           CachedSources.Num(),
           *GetCurrentSourceName());
}

void UInfusionVFXComponent::ActivateCurrentSource()
{
    if (bSuppressForImmuneAction)
    {
        DeactivateInfusion();
        return;
    }

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
    UE_LOG(LogTemp, Verbose, TEXT("[InfusionVFX] Activating: %s"), *GetCurrentSourceName());

    ActivateInfusion(Source);
}

FString UInfusionVFXComponent::GetCurrentSourceName() const
{
    if (!CachedSources.IsValidIndex(CurrentSourceIndex))
    {
        return TEXT("None");
    }

    const FString Name = InfusionSourceOptionHelpers::GetDisplayName(CachedSources[CurrentSourceIndex]);
    return IsCurrentSourceIce() ? Name + TEXT(" (Ice)") : Name;
}

void UInfusionVFXComponent::ToggleInfusion()
{
    if (bIsInfusionActive)
    {
        DeactivateInfusion();
        UE_LOG(LogTemp, Verbose, TEXT("[InfusionVFX] Toggled OFF"));
    }
    else
    {
        ActivateCurrentSource();
    }
}