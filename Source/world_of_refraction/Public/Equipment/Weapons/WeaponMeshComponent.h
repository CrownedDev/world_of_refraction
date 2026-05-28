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

    // ==================== FUNCTIONS ====================
    // Attachment sockets are derived from the weapon's WeaponType (and
    // WieldMode for the left hand) — there are no configurable socket fields.

    /** Force update weapon mesh (call after weapon change) */
    UFUNCTION(BlueprintCallable, Category = "Weapon Mesh")
    void UpdateWeaponMesh();

    /** Clear all weapon meshes */
    UFUNCTION(BlueprintCallable, Category = "Weapon Mesh")
    void ClearWeaponMesh();

    /** Get currently displayed weapon */
    UFUNCTION(BlueprintPure, Category = "Weapon Mesh")
    UWeaponData *GetDisplayedWeapon() const { return CachedWeapon; }

    /** Get the primary static mesh component (for VFX sampling) */
    UFUNCTION(BlueprintPure, Category = "Weapon Mesh")
    UStaticMeshComponent *GetPrimaryStaticMeshComp() const { return PrimaryStaticMeshComp; }

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

    /** Resolve the right-hand attachment socket from the weapon's WeaponType.
     *  Returns NAME_None (and logs an Error) for an unmapped type. */
    FName GetRightSocketForWeapon(const UWeaponData *Weapon) const;

    /** Resolve the left-hand attachment socket. OffHandShield short-circuits to
     *  hand_l_sas; otherwise derived from WeaponType. Returns NAME_None (and
     *  logs an Error) for an unmapped type. */
    FName GetLeftSocketForWeapon(const UWeaponData *Weapon) const;

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