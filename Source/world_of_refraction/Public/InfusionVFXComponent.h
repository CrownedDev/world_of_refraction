// InfusionVFXComponent.h
// Manages infusion VFX spawning based on weapon's display location setting

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EInfusionSourceOption.h"
#include "EInfusionDisplayLocation.h"
#include "SpellElement.h"
#include "InfusionVFXComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UCharacterDataComponent;
class UCharacterData;
class UWeaponMeshComponent;

/**
 * Infusion VFX Component
 * Spawns infusion VFX at location specified by weapon's InfusionDisplay asset
 * Tints based on active element from selected infusion source
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UInfusionVFXComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInfusionVFXComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

    // ==================== CONFIGURATION ====================

    /** Socket name for weapon VFX attachment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion VFX|Sockets")
    FName WeaponSocket = FName("hand_r");

    /** Socket name for body VFX attachment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion VFX|Sockets")
    FName BodySocket = FName("pelvis");

    /** Offset for aura VFX (relative to actor root) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion VFX|Sockets")
    FVector AuraOffset = FVector(0.f, 0.f, -90.f);

    /** Niagara parameter name for element color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion VFX|Parameters")
    FName ColorParameterName = FName("ElementColor");

    /** Niagara parameter name for weapon mesh (for surface sampling) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion VFX|Parameters")
    FName WeaponMeshParameterName = FName("WeaponMesh");

    /** Niagara parameter name for body mesh (for surface sampling) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion VFX|Parameters")
    FName BodyMeshParameterName = FName("BodyMesh");

    // ==================== Spell/Ability Infusion ====================
    /** Set infusion level - scales VFX size (0 = off, 1 = base, 2 = large) */
    UFUNCTION(BlueprintCallable, Category = "InfusionVFX")
    void SetInfusionLevel(int32 Level);

    /** Current infusion level */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InfusionVFX")
    int32 CurrentInfusionLevel = 0;

    // ==================== PUBLIC API ====================

    /** Activate infusion VFX with specified source */
    UFUNCTION(BlueprintCallable, Category = "Infusion VFX")
    void ActivateInfusion(EInfusionSourceOption Source);

    /** Deactivate infusion VFX */
    UFUNCTION(BlueprintCallable, Category = "Infusion VFX")
    void DeactivateInfusion();

    /** Check if infusion VFX is active */
    UFUNCTION(BlueprintPure, Category = "Infusion VFX")
    bool IsInfusionActive() const { return bIsInfusionActive; }

    /** Get current infusion source */
    UFUNCTION(BlueprintPure, Category = "Infusion VFX")
    EInfusionSourceOption GetCurrentSource() const { return CurrentSource; }

    /** Force refresh VFX (call when weapon changes) */
    UFUNCTION(BlueprintCallable, Category = "Infusion VFX")
    void RefreshVFX();

    /** Cache available sources from character data */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void CacheAvailableSources();

    /** Cycle to next available source (logs current) */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void CycleToNextSource();

    /** Activate infusion with current source index */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void ActivateCurrentSource();

    /** Get display name of current source */
    UFUNCTION(BlueprintPure, Category = "Debug")
    FString GetCurrentSourceName() const;

    // ==================== DEBUG ====================

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
    void DebugLogVFXState();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
    void DebugActivateWithElement(ESpellElement TestElement);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
    void DebugDeactivate();

    /** Toggle infusion on/off */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void ToggleInfusion();

    /** Cycle through infusion levels: 0 → 1 → 2 → 0 */
    UFUNCTION(BlueprintCallable, Category = "InfusionVFX")
    void CycleInfusionLevel();

protected:
    // ==================== CACHED REFERENCES ====================

    UPROPERTY()
    UCharacterDataComponent *CharacterDataComponent;

    UPROPERTY()
    USkeletalMeshComponent *OwnerMesh;

    UPROPERTY()
    UWeaponMeshComponent *WeaponMeshComponent;

    UPROPERTY()
    UNiagaraComponent *ActiveVFXComponent;

    // ==================== STATE ====================

    UPROPERTY()
    EInfusionSourceOption CurrentSource = EInfusionSourceOption::None;

    UPROPERTY()
    ESpellElement CurrentElement = ESpellElement::Generic;

    UPROPERTY()
    bool bIsInfusionActive = false;

    UPROPERTY()
    TArray<EInfusionSourceOption> CachedSources;

    UPROPERTY()
    int32 CurrentSourceIndex = 0;

    // ==================== INTERNAL ====================

    /** Spawn VFX at location specified by weapon's display data */
    void SpawnVFX(UNiagaraSystem *VFXSystem, EInfusionDisplayLocation Location);

    /** Destroy active VFX */
    void ClearVFX();

    /** Apply element color to active VFX */
    void ApplyColorToVFX(FLinearColor Color);

    /** Pass mesh reference to Niagara for surface sampling */
    void SetMeshParameterForLocation(EInfusionDisplayLocation Location);

    /** Get current weapon's VFX data */
    UCharacterData *GetActiveCharacterData() const;

    /** Get element from ActionExecutor based on source */
    ESpellElement GetElementForSource(EInfusionSourceOption Source) const;

    /** Get color for element */
    FLinearColor GetElementColor(ESpellElement Element) const;

    /** Scale multiplier for L1 */
    float L1ScaleMultiplier = 1.25f;

    /** Scale multiplier for L2 */
    float L2ScaleMultiplier = 1.5f;

    /** Niagara parameter name for scale */
    FName ScaleParameterName = TEXT("Scale_All");
};