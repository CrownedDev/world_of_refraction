// WeaponMeshComponent.h
// Manages weapon mesh spawning and attachment to character skeleton

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponMeshComponent.generated.h"

class UCharacterDataComponent;
class UWeaponData;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UWeaponMeshComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponMeshComponent();

    // ==================== SOCKET CONFIGURATION ====================

    /** Socket name for right hand weapon attachment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sockets")
    FName RightHandSocket = FName("hand_r_socket");

    /** Socket name for left hand weapon attachment (DualBlades) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sockets")
    FName LeftHandSocket = FName("hand_l_socket");

    // ==================== FUNCTIONS ====================

    /** Force update weapon mesh (call after weapon change) */
    UFUNCTION(BlueprintCallable, Category = "Weapon Mesh")
    void UpdateWeaponMesh();

    /** Clear all weapon meshes */
    UFUNCTION(BlueprintCallable, Category = "Weapon Mesh")
    void ClearWeaponMesh();

    /** Get currently displayed weapon */
    UFUNCTION(BlueprintPure, Category = "Weapon Mesh")
    UWeaponData *GetDisplayedWeapon() const { return CachedWeapon; }

    // ==================== DEBUG ====================

    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void DebugLogMeshState();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

private:
    // ==================== INTERNAL ====================

    void CacheReferences();
    void SpawnWeaponMesh(UWeaponData *Weapon);
    void SpawnDualWeaponMesh(UWeaponData *Weapon);
    USkeletalMeshComponent *GetOwnerMesh() const;

    // ==================== CACHED REFERENCES ====================

    UPROPERTY()
    UCharacterDataComponent *CharacterDataComponent;

    UPROPERTY()
    UWeaponData *CachedWeapon;

    // ==================== SPAWNED MESHES ====================

    UPROPERTY()
    UStaticMeshComponent *PrimaryStaticMeshComp;

    UPROPERTY()
    UStaticMeshComponent *SecondaryStaticMeshComp;

    UPROPERTY()
    USkeletalMeshComponent *PrimarySkeletalMeshComp;

    UPROPERTY()
    USkeletalMeshComponent *SecondarySkeletalMeshComp;
};