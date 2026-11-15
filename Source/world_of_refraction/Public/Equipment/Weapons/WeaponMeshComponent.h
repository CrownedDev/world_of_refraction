// WeaponMeshComponent.h
// Manages weapon mesh spawning and attachment to character skeleton

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponMeshComponent.generated.h"

class UCharacterDataComponent;
class ULoadoutComponent;
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

    /** B-503s (D-218): the field wheel's pick of an element source (the innate, an evolution, a caster's adapter)
     *  puts the weapon away; a weapon pick shows it again. Kept across mesh rebuilds. A look only, per machine. */
    void SetWeaponHidden(bool bHidden);
    bool IsWeaponHidden() const { return bWeaponHidden; }

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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // ==================== INTERNAL ====================

    /** B-138: the mesh follows ULoadoutComponent::OnActiveWeaponChanged, no polling. */
    UFUNCTION()
    void HandleActiveWeaponChanged(UWeaponData *NewWeapon);

    void CacheReferences();
    bool bWeaponHidden = false;
    void ApplyWeaponHidden();
    void SpawnWeaponMesh(UWeaponData *Weapon);
    void SpawnDualWeaponMesh(UWeaponData *Weapon);

    /** B-515: the off-hand piece of the active weapon set (a shield, or a D-209 pair's second weapon); null when the
     *  set isn't a valid pair. Its own mesh goes in the left hand, not the main hand's left-hand mesh. */
    UWeaponData *GetOffHandPiece(const UWeaponData *ActiveWeapon) const;
    void SpawnOffHandMesh(UWeaponData *OffHand);
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

    UPROPERTY(Transient)
    TObjectPtr<ULoadoutComponent> LoadoutComponent;

    UPROPERTY()
    UWeaponData *CachedWeapon;

    UPROPERTY(Transient)
    TObjectPtr<UWeaponData> CachedOffHand;

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