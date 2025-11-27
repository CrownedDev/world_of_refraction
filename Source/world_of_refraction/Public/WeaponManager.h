// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ActionStructs.h"
#include "EPhysicalDamageType.h"
#include "SpellElement.h"
#include "WeaponManager.generated.h"

class UWeaponData;
class UWeaponAttackData;
class UBaseAttackData;
class UAbilityData;
class UCharacterDataComponent;
class UCharacterData;
class UStatusEffectManager;
URingData *GetPrimaryRing(AActor *Actor) const;
URingData *GetSecondaryRing(AActor *Actor) const;
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

	/** Is infusion toggled on? (Generic only) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon State")
	bool bInfusionActive = false;

	/** Conjured weapon reference (if any) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon State")
	UWeaponData *ConjuredWeapon = nullptr;

	/** Are spells sealed? (true when conjured weapon active) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon State")
	bool bSpellsSealed = false;

	/** Previous slot before conjuration (for dispel) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon State")
	EWeaponSlot PreConjuredSlot = EWeaponSlot::Unarmed;

	// Physical status buildup tracking per target
	TMap<TWeakObjectPtr<AActor>, float> StatusBuildupPerTarget;

	void ResetBuildup(AActor *Target)
	{
		StatusBuildupPerTarget.Remove(Target);
	}

	void AddBuildup(AActor *Target, float Amount)
	{
		float &Current = StatusBuildupPerTarget.FindOrAdd(Target);
		Current += Amount;
	}

	float GetBuildup(AActor *Target) const
	{
		const float *Current = StatusBuildupPerTarget.Find(Target);
		return Current ? *Current : 0.0f;
	}
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
	EPhysicalDamageType PhysicalType = EPhysicalDamageType::Blunt;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInfusionToggled, AActor *, Actor, bool, bIsActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponAttackExecuted, AActor *, Attacker, const FWeaponAttackResult &, Result, UWeaponData *, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPhysicalStatusTriggered, AActor *, Target, EPhysicalDamageType, DamageType, AActor *, Attacker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConjurationStarted, AActor *, Actor, UWeaponData *, ConjuredWeapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConjurationEnded, AActor *, Actor);

/**
 * UWeaponManager
 *
 * Handles all weapon-related combat logic:
 * - Weapon state tracking (armed/unarmed, primary/secondary)
 * - Weapon switching
 * - Attack execution with physical damage types
 * - Physical status buildup (Slash→Bleed, Pierce→ArmorBreak, Blunt→Stun)
 * - Infusion toggle for Generic characters
 * - Conjured weapon management for Elemental characters
 *
 * Character Type Differences:
 * - Generic: Primary/Secondary weapons, infusion toggle, no conjuration
 * - Elemental: Single weapon OR unarmed, can conjure weapons (seals spells)
 *
 * Usage:
 *   UWeaponManager* WeaponMgr = GetGameInstance()->GetSubsystem<UWeaponManager>();
 *   WeaponMgr->SwitchWeapon(Actor, EWeaponSlot::Primary);
 *   FWeaponAttackResult Result = WeaponMgr->ExecuteAttack(Actor, Targets);
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
	UBaseAttackData *GetActiveAttack(AActor *Actor) const;

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
	// INFUSION (GENERIC ONLY)
	// ========================================

	/**
	 * Toggle infusion on/off
	 * Only works for Generic characters
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Infusion")
	bool ToggleInfusion(AActor *Actor);

	/**
	 * Set infusion state directly
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Infusion")
	bool SetInfusion(AActor *Actor, bool bEnabled);

	/**
	 * Check if infusion is currently active
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Infusion")
	bool IsInfusionActive(AActor *Actor) const;

	/**
	 * Check if actor can use infusion
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Infusion")
	bool CanUseInfusion(AActor *Actor) const;

	// ========================================
	// CONJURATION (ELEMENTAL ONLY)
	// ========================================

	/**
	 * Conjure a weapon (from conjuration spell)
	 * Seals all spells until dispelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Conjuration")
	bool ConjureWeapon(AActor *Actor, UWeaponData *WeaponToConjure);

	/**
	 * Dispel conjured weapon (free action at turn start)
	 * Returns to previous state, unseals spells
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Conjuration")
	bool DispelConjuredWeapon(AActor *Actor);

	/**
	 * Check if actor has active conjured weapon
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Conjuration")
	bool HasConjuredWeapon(AActor *Actor) const;

	/**
	 * Check if spells are sealed (during conjuration)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Conjuration")
	bool AreSpellsSealed(AActor *Actor) const;

	// ========================================
	// ATTACK EXECUTION
	// ========================================

	/**
	 * Execute attack with current weapon/state
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	FWeaponAttackResult ExecuteAttack(AActor *Attacker, const TArray<AActor *> &Targets);

	/**
	 * Execute attack with explicit infusion setting
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager")
	FWeaponAttackResult ExecuteAttackWithInfusion(AActor *Attacker, const TArray<AActor *> &Targets, bool bUseInfusion);

	// ========================================
	// PHYSICAL STATUS
	// ========================================

	/**
	 * Get current status buildup for a target from attacker
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Status")
	float GetStatusBuildup(AActor *Attacker, AActor *Target) const;

	/**
	 * Get threshold to trigger physical status
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Status")
	float GetStatusThreshold(EPhysicalDamageType DamageType) const;

	/**
	 * Check if target would trigger status on next buildup
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Status")
	bool WouldTriggerStatus(AActor *Attacker, AActor *Target, float AdditionalBuildup) const;

	/**
	 * Reset status buildup for target (after trigger or combat end)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Manager|Status")
	void ResetStatusBuildup(AActor *Attacker, AActor *Target);

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponSwitched OnWeaponSwitched;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnInfusionToggled OnInfusionToggled;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnWeaponAttackExecuted OnWeaponAttackExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnPhysicalStatusTriggered OnPhysicalStatusTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnConjurationStarted OnConjurationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Manager|Events")
	FOnConjurationEnded OnConjurationEnded;

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
	UCharacterData *GetCharacterData(AActor *Actor) const;
	UStatusEffectManager *GetStatusEffectManager() const;

	/** Check if actor is Generic class (dual-wield, secondary slot) */
	bool IsGenericCharacter(AActor *Actor) const;

	/** Check if actor is Caster class (innate element, primary slot type) */
	bool IsCasterCharacter(AActor *Actor) const;

	/** Check if actor is Resonator class (ring loadout) */
	bool IsResonatorCharacter(AActor *Actor) const;

	/** Get weapon from specific slot */
	UWeaponData *GetWeaponInSlot(AActor *Actor, EWeaponSlot Slot) const;

	/** Calculate status buildup for an attack */
	float CalculateStatusBuildup(UWeaponData *Weapon, UWeaponAttackData *Attack, int32 HitCount) const;

	/** Apply physical status effect when threshold reached */
	void TriggerPhysicalStatus(AActor *Attacker, AActor *Target, EPhysicalDamageType DamageType);

	/** Apply damage using ActionExecutor's damage system */
	int32 ApplyWeaponDamage(AActor *Attacker, AActor *Target, int32 BaseDamage,
							bool bIsElemental, ESpellElement Element, bool bCanCrit, FWeaponAttackResult &OutResult);

	// ========================================
	// STATE
	// ========================================

	/** Weapon state per actor */
	TMap<TWeakObjectPtr<AActor>, FWeaponState> WeaponStates;

	/** Cached StatusEffectManager */
	UPROPERTY()
	UStatusEffectManager *StatusEffectManagerRef = nullptr;

	// ========================================
	// CONSTANTS
	// ========================================

	static constexpr float STATUS_THRESHOLD_BLEED = 100.0f;		  // Slash → Bleed
	static constexpr float STATUS_THRESHOLD_ARMOR_BREAK = 100.0f; // Pierce → ArmorBreak
	static constexpr float STATUS_THRESHOLD_STUN = 100.0f;		  // Blunt → Stun

	static constexpr int32 INFUSED_ATTACK_ENERGY_COST = 5;
	static constexpr float INFUSION_DAMAGE_PENALTY = 0.3f; // 30% damage reduction when infused
};
