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
class UMeshComponent;

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

    /** Crown 2026-11-24: the weapon in hand's own mesh, for what plays on it (the weapon infusion glow, the trail): the
     *  main-hand piece (static or skeletal), else the off hand's; null with none shown. It sits in whichever hand holds
     *  the piece, so a two-hander or a left-hand weapon needs nothing more. */
    UMeshComponent *GetWeaponVFXMesh() const;

    /** Fires after the weapon meshes are rebuilt (a swap, a set change), so what rides on them can re-attach. */
    FSimpleMulticastDelegate OnWeaponMeshRebuilt;

    // ==================== SOCKETS (D-242) ====================
    // One hold per hand on every body, and each weapon made to fit it; a shield has its own point in either hand. The
    // older per-type sockets (hand_r_sword, hand_l_axe, hand_l_sas...) stay as the fallback and behind the switch
    // wor.Weapons.UseGrip (default on) until the grip path is proven in PIE.
    static const FName RIGHT_GRIP_SOCKET;
    static const FName LEFT_GRIP_SOCKET;
    static const FName LEFT_SHIELD_SOCKET;
    static const FName RIGHT_SHIELD_SOCKET;

    /** The D-242 socket for a piece in that hand: the hand's grip, or its shield point for a shield. */
    static FName GetGripSocket(bool bRightHand, bool bShield);

    /** Piece is a shield in that hand: a shield piece, or (left hand) the shield baked into an OffHandShield weapon. */
    static bool IsShieldInHand(const UWeaponData &Piece, bool bRightHand);

    /** The socket Piece attaches to in that hand: the D-242 one when bUseGrip and HasSocket finds it on the body,
     *  else the older per-type one (a shield's: hand_l_sas). bOutFellBack: the D-242 socket was wanted and missing. */
    static FName ChooseSocket(const UWeaponData &Piece, bool bRightHand, bool bUseGrip, TFunctionRef<bool(FName)> HasSocket, bool &bOutFellBack);

    /** wor.Weapons.UseGrip. */
    static bool UsesGripSockets();

    /** Rebuilds the weapon meshes (the switch changed, or a socket was added to the body). */
    void RefreshWeaponMesh();

    // ==================== DEBUG ====================

    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void DebugLogMeshState();

    /** One line: the owner, the weapon shown, the body mesh, and for each hand the socket used and whether it is the
     *  D-242 one or the older fallback. Console: wor.Weapons.PrintSockets. */
    FString GetSocketString() const;

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
    static FName GetRightSocketForWeapon(const UWeaponData *Weapon);

    /** Resolve the left-hand attachment socket. OffHandShield short-circuits to
     *  hand_l_sas; otherwise derived from WeaponType. Returns NAME_None (and
     *  logs an Error) for an unmapped type. */
    static FName GetLeftSocketForWeapon(const UWeaponData *Weapon);

    /** D-242: the socket Piece goes in on this body (ChooseSocket), remembered for the debug print; warns once per
     *  missing D-242 socket. */
    FName ResolveSocket(const UWeaponData *Piece, bool bRightHand);
    FName RightSocketUsed;
    FName LeftSocketUsed;
    TSet<FName> WarnedMissingSockets;

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