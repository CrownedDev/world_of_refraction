// ElementColorDebugComponent.h
// Debug component for visualizing character element via mesh color
// Temporary testing tool - remove when proper character visuals are implemented

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skills/Definitions/ESpellElement.h"
#include "ElementColorDebugComponent.generated.h"

class UCharacterDataComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Debug component that colors a character's mesh based on their element
 *
 * Usage:
 * 1. Add to any actor with a mesh and CharacterDataComponent
 * 2. Optionally assign MeshComponent directly, or it auto-finds
 * 3. Call ApplyElementColor() or let BeginPlay handle it
 *
 * For BD characters, call UpdateAbsorptionColor() when they absorb
 */
UCLASS(ClassGroup = (Debug), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UElementColorDebugComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UElementColorDebugComponent();

    // ==================== CONFIG ====================

    /** Material parameter name for base color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Config")
    FName ColorParameterName = FName("BaseColor");

    /** Material index to modify */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Config")
    int32 MaterialIndex = 0;

    /** Auto-apply color on BeginPlay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Config")
    bool bApplyOnBeginPlay = true;

    /** Optional: Directly assign mesh (auto-finds if null) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Config")
    UMeshComponent *TargetMesh;

    // ==================== FUNCTIONS ====================

    /** Apply element color from CharacterDataComponent */
    UFUNCTION(BlueprintCallable, Category = "Debug|Element Color")
    void ApplyElementColor();

    /** Update color for BD absorption (call when absorption changes) */
    UFUNCTION(BlueprintCallable, Category = "Debug|Element Color")
    void UpdateAbsorptionColor(ESpellElement AbsorbedElement);

    /** Force set color directly (bypasses element lookup) */
    UFUNCTION(BlueprintCallable, Category = "Debug|Element Color")
    void SetColorDirect(FLinearColor Color);

    /** Get current applied color */
    UFUNCTION(BlueprintPure, Category = "Debug|Element Color")
    FLinearColor GetCurrentColor() const { return CurrentColor; }

protected:
    virtual void BeginPlay() override;

private:
    /** Cached dynamic material instance */
    UPROPERTY()
    UMaterialInstanceDynamic *DynamicMaterial;

    /** Current applied color */
    FLinearColor CurrentColor;

    /** Find mesh component on owner */
    UMeshComponent *FindMeshComponent() const;

    /** Create or get dynamic material instance */
    UMaterialInstanceDynamic *GetOrCreateDynamicMaterial();

    /** Apply color to material */
    void ApplyColorToMaterial(FLinearColor Color);
};
