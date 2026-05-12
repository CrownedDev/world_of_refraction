// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "EPhysicalDamageType.h"
#include "ESpellElement.h"
#include "ItemTier.h"
#include "WeaponManager.generated.h"

class UWeaponData;
class UWeaponAttackData;
class UAbilityData;
class UCharacterDataComponent;
class UCharacterData;
class ULoadoutComponent;
// ========================================
// WEAPON STATE
// ========================================

/**
 * EWeaponSlot
 * Which weapon slot is active
 */
UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Unarmed UMETA(DisplayName = "Unarmed"),
	Primary UMETA(DisplayName = "Primary Weapon"),
	Secondary UMETA(DisplayName = "Secondary Weapon"),
	Conjured UMETA(DisplayName = "Conjured Weapon")
};

/**
 * FWeaponState
 * Tracks current weapon state for an actor
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWeaponState
{
	GENERATED_BODY()

	/** Current active weapon slot */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon State")
	EWeaponSlot ActiveSlot = EWeaponSlot::Unarmed;
};

/**
 * FWeaponAttackResult
 * Result of executing a weapon attack
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWeaponAttackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 TotalDamageDealt = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 EnergySpent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bWasCritical = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bCausedDeath = false;

	/** Physical damage type used */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	EPhysicalDamageType PhysicalType = EPhysicalDamageType::Impact;

	/** Status buildup applied this attack */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float StatusBuildupApplied = 0.0f;

	/** Did attack trigger physical status? (Bleed/Stun/ArmorBreak) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bTriggeredPhysicalStatus = false;

	/** Was attack infused with element? */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bWasInfused = false;

	/** Element used if infused */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	ESpellElement InfusedElement = ESpellElement::Generic;

	/** Per-target damage breakdown */
	TMap<AActor *, int32> DamagePerTarget;
};

// ========================================
// DELEGATES
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponSwitched, AActor *, Actor, EWeaponSlot, OldSlot, EWeaponSlot, NewSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponAttackExecuted, AActor *, Attacker, const FWeaponAttackResult &, Result, UWeaponData *, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPhysicalStatusTriggered, AActor *, Target, EPhysicalDamageType, DamageType, AActor *, Attacker);

/**
 * UWeaponManager
 *
 * Handles all weapon-related combat logic:
 * - Weapon state tracking (armed/unarmed, primary/secondary)
 * - Weapon switching
 * - Attack execution with physical damage types
 * - Physical status buildup (Slash→Bleed, Pierce→ArmorBreak, Impact→Stun)
 *
 * Usage:
 *   UWeaponManager* WeaponMgr = GetGameInstance()->GetSubsystem<UWeaponManager>();
 *   WeaponMgr->SwitchWeapon(Actor, EWeaponSlot::Primary);
 *   // Attack execution flows through UActionExecutor / ApplyHit, not this subsystem.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UWeaponManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// STATE MANAGEMENT
	// ========================================

	/**
	 * Initialize weapon state for an actor at combat start
	 * Sets up based on CharacterData loadout
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	void InitializeWeaponState(AActor *Actor);

	/**
	 * Clear weapon state for an actor (combat end)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	void ClearWeaponState(AActor *Actor);

	/**
	 * Get current weapon state for actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	FWeaponState GetWeaponState(AActor *Actor) const;

	/**
	 * Get currently active weapon (or nullptr if unarmed)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	UWeaponData *GetActiveWeapon(AActor *Actor) const;

	/**
	 * Get current attack data (weapon or base attack)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	UWeaponAttackData *GetActiveAttack(AActor *Actor) const;

	/**
	 * Get abilities available in current weapon state
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	TArray<UAbilityData *> GetActiveAbilities(AActor *Actor) const;

	// ========================================
	// WEAPON SWITCHING
	// ========================================

	/**
	 * Switch to specified weapon slot
	 * @return true if switch successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	bool SwitchWeapon(AActor *Actor, EWeaponSlot TargetSlot);

	/**
	 * Check if actor can switch to target slot
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	bool CanSwitchWeapon(AActor *Actor, EWeaponSlot TargetSlot) const;

	/**
	 * Get available weapon slots for actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	TArray<EWeaponSlot> GetAvailableSlots(AActor *Actor) const;

	// ========================================
	// DURABILITY (Phase 4d)
	// ========================================

	/** Apply wear to the slotted crystal of the active weapon after an action.
	 *  Returns the wear amount applied. If wear breaks the crystal, fires
	 *  OnCrystalBroken (handled internally — weapon stays equipped, downgrades
	 *  to physical-only via WeaponData::GetWeaponElement returning Generic).
	 *
	 *  Generic signature taking EItemTier directly so it works for spells,
	 *  abilities, and attacks (vs RingManager which is spell-only and reads
	 *  USpellData::Tier directly). Caller extracts ActionTier from the weapon
	 *  itself (Phase 4d Path A: action tier inherits from weapon tier).
	 *
	 *  TODO: Reads Weapon->SlottedCrystal (asset-side), inheriting the same
	 *  architectural shortcut as URingManager. Should migrate both to runtime
	 *  inventory entries when inventory persistence is wired. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	int32 ProcessPostCastWear(AActor *Actor, EItemTier ActionTier, int32 InfusionLevel, bool bIsSpell);

	// ==================== DELEGATES (durability) ====================

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWeaponDurabilityChanged, AActor *, Actor, UWeaponData *, Weapon, int32, NewDurability, int32, MaxDurability);

	/** Fires every time a weapon crystal's durability changes (per-cast wear).
	 *  Use this for real-time UI updates of the weapon durability bar.
	 *  Differs from OnWeaponCrystalBroken which fires once at zero. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponDurabilityChanged OnWeaponDurabilityChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponCrystalBroken, AActor *, Actor, UWeaponData *, Weapon, UItemData *, Crystal);

	/** Fires when a weapon's slotted crystal hits 0 durability and breaks.
	 *  No auto-switch — weapon stays equipped, just loses crystal-derived effects. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponCrystalBroken OnWeaponCrystalBroken;

	// ========================================
	// ATTACK EXECUTION
	// ========================================

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponSwitched OnWeaponSwitched;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponAttackExecuted OnWeaponAttackExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnPhysicalStatusTriggered OnPhysicalStatusTriggered;

	// ========================================
	// DEBUG
	// ========================================

	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Debug", CallInEditor)
	void DebugPrintWeaponStates() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Debug", CallInEditor)
	void DebugPrintActorWeaponState(AActor *Actor) const;

private:
	// ========================================
	// HELPERS
	// ========================================

	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;
	/** Get LoadoutComponent from actor */
	ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
	UCharacterData *GetCharacterData(AActor *Actor) const;

	/** Get weapon from specific slot */
	UWeaponData *GetWeaponInSlot(AActor *Actor, EWeaponSlot Slot) const;

	// ========================================
	// STATE
	// ========================================

	/** Weapon state per actor */
	TMap<TWeakObjectPtr<AActor>, FWeaponState> WeaponStates;

	// ==================== CRYSTAL SUBSCRIPTIONS (Phase 4d) ====================

	/** Subscribe to OnCrystalBroken for every slotted crystal of an actor's weapons.
	 *  Iterates Primary/Secondary/Conjured weapons in the actor's loadout. */
	void SubscribeToActorWeaponCrystals(AActor *Actor);

	/** Unsubscribe from OnCrystalBroken for every slotted crystal of an actor's weapons. */
	void UnsubscribeFromActorWeaponCrystals(AActor *Actor);

	/** Handler bound to UItemData::OnCrystalBroken delegate for weapon crystals. */
	UFUNCTION()
	void HandleWeaponCrystalBroken(UItemData *BrokenCrystal);

	/** Find which actor and weapon own a given crystal (for the broadcast). */
	bool FindWeaponOwnerOfCrystal(UItemData *Crystal, AActor *&OutActor, UWeaponData *&OutWeapon) const;
};
