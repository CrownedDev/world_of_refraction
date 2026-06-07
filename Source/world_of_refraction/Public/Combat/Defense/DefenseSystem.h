// DefenseSystem.h
// Real-time defense system for Block/Parry/Dodge mechanics

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Character.h"
#include "DefenseSystem.generated.h"

class UCharacterDataComponent;

// ========================================
// STRUCTS
// ========================================

/**
 * Result of a defense attempt
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseResult
{
	GENERATED_BODY()

	/** Was defense window active when input received? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bWasInWindow = false;

	/** Type of defense attempted */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseType DefenseType = EDefenseType::None;

	/** Was the defense successful? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bSuccess = false;

	/** Damage after defense reduction */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 FinalDamage = 0;

	/** Damage reflected back to attacker (Parry only) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 ReflectedDamage = 0;

	/** Why dodge failed (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	FString FailureReason;
};

/**
 * Active defense state for an actor
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseState
{
	GENERATED_BODY()

	/** Actor being defended */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Defender;

	/** Actor attacking */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	TWeakObjectPtr<AActor> Attacker;

	/** Is defense window currently open? */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	bool bWindowOpen = false;

	/** Size of incoming attack (affects dodge viability) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	float AttackSize = 1.0f;

	/** Base damage before defense */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	int32 BaseDamage = 0;

	/** Defense chosen by player (None until input) */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseType DefenseChosen = EDefenseType::None;

	/** Dodge direction chosen */
	UPROPERTY(BlueprintReadOnly, Category = "Defense")
	EDefenseDirection DodgeDirection = EDefenseDirection::None;

	/** Time window opened */
	double WindowOpenTime = 0.0;

	/** Duration of window */
	float WindowDuration = 0.3f;

	/** Has player already submitted defense input? */
	bool bInputReceived = false;
};

/**
 * Defense window configuration (embedded in attack data)
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FDefenseWindowConfig
{
	GENERATED_BODY()

	/** When in animation does window open? (seconds from start) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
	float WindowStartTime = 0.5f;

	/** How long is window open? (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
	float WindowDuration = 0.3f;

	/** Visual/audio cue timing (seconds before window opens) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense")
	float CueLeadTime = 0.1f;
};

// ========================================
// DELEGATES
// ========================================

/** Broadcast when defense window opens */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDefenseWindowOpened, AActor *, Defender, float, AttackSize, float, WindowDuration);

/** Broadcast when defense window closes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenseWindowClosed, AActor *, Defender, const FDefenseResult &, Result);

/** Broadcast when player submits defense input */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDefenseInputReceived, AActor *, Defender, EDefenseType, DefenseType, EDefenseDirection, Direction);

/** Broadcast when visual cue should show (before window opens) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenseCueTriggered, AActor *, Defender, float, TimeUntilWindow);

/** Broadcast when parry reflects damage */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnParryReflect, AActor *, Defender, AActor *, Attacker, int32, ReflectedDamage);

// ========================================
// DEFENSE SYSTEM
// ========================================

/**
 * UDefenseSystem
 *
 * GameInstanceSubsystem that handles real-time defense mechanics.
 * Players can Block, Parry, or Dodge incoming attacks during defense windows.
 *
 * Defense Types:
 * - Block: 50% damage reduction (always works)
 * - Parry: 70% reduction + 30% reflect (tight timing required)
 * - Dodge: 100% avoidance IF attack size < threshold (left/right only)
 *
 * Usage:
 *   UDefenseSystem* Defense = GetGameInstance()->GetSubsystem<UDefenseSystem>();
 *   Defense->OpenDefenseWindow(Attacker, Defender, AttackSize, BaseDamage, WindowDuration);
 *   // Player presses button
 *   Defense->SubmitDefenseInput(Defender, EDefenseType::Block);
 *   // Window closes, damage applied
 */
UCLASS()
class WORLD_OF_REFRACTION_API UDefenseSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// DEFENSE WINDOW MANAGEMENT
	// ========================================

	/**
	 * Open a defense window for a defender
	 * @param Attacker The actor attacking
	 * @param Defender The actor defending
	 * @param AttackSize Size of attack (affects dodge viability)
	 * @param BaseDamage Damage before defense reduction
	 * @param WindowDuration How long window stays open
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void OpenDefenseWindow(
		AActor *Attacker,
		AActor *Defender,
		float AttackSize,
		int32 BaseDamage,
		float WindowDuration = 0.3f);

	/**
	 * Close defense window and calculate result
	 * Called automatically when window expires, or manually
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	FDefenseResult CloseDefenseWindow(AActor *Defender);

	/**
	 * Submit player's defense input
	 * @param Defender The defending actor
	 * @param DefenseType Block/Parry/Dodge
	 * @param Direction Dodge direction (only for Dodge, left/right only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void SubmitDefenseInput(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction = EDefenseDirection::None);
	/**
	 * Play defense animation on defender
	 * @param Defender The defending actor
	 * @param DefenseType Block/Parry/Dodge
	 * @param Direction Dodge direction (only for Dodge)
	 */
	UFUNCTION(BlueprintCallable, Category = "Defense System")
	void PlayDefenseAnimation(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction = EDefenseDirection::None);

	/**
	 * Check if defense window is currently open for actor
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	bool IsDefenseWindowOpen(AActor *Defender) const;

	/**
	 * Get current defense state for actor
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	FDefenseState GetDefenseState(AActor *Defender) const;

	/**
	 * Get remaining time in defense window
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System")
	float GetRemainingWindowTime(AActor *Defender) const;

	// ========================================
	// DODGE CALCULATIONS
	// ========================================

	/**
	 * Check if an attack can be dodged based on size
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System|Dodge")
	bool CanDodgeAttack(AActor *Defender, float AttackSize) const;

	/**
	 * Get dodge threshold for a defender
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System|Dodge")
	float GetDodgeThreshold(AActor *Defender) const;

	// ========================================
	// DAMAGE CALCULATION
	// ========================================

	/**
	 * Calculate final damage after defense
	 * @param BaseDamage Original damage
	 * @param DefenseType Defense used
	 * @param bDefenseSuccessful Was timing correct?
	 * @param AttackSize Size of attack (for dodge)
	 * @param DodgeThreshold Defender's dodge threshold
	 */
	UFUNCTION(BlueprintPure, Category = "Defense System|Damage")
	FDefenseResult CalculateDefenseResult(
		int32 BaseDamage,
		EDefenseType DefenseType,
		bool bDefenseSuccessful,
		float AttackSize,
		float DodgeThreshold);

	// ========================================
	// EVENTS
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseWindowOpened OnDefenseWindowOpened;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseWindowClosed OnDefenseWindowClosed;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseInputReceived OnDefenseInputReceived;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnDefenseCueTriggered OnDefenseCueTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Defense System|Events")
	FOnParryReflect OnParryReflect;

	// ========================================
	// CONFIGURATION
	// ========================================

	/** Block damage reduction (0.5 = 50% reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float BlockReduction = 0.5f;

	/** Parry damage reduction (0.7 = 70% reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float ParryReduction = 0.7f;

	/** Parry reflect percentage (0.3 = 30% reflected) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float ParryReflect = 0.3f;

	/** Base dodge threshold (hitbox + dodge distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float BaseDodgeThreshold = 2.5f;

	/** Default defense window duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float DefaultWindowDuration = 0.3f;

	/** Defense window duration for AOE attacks (longer than default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense System|Config")
	float AoeWindowDuration = 0.5f;

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	/** Active defense states per defender */
	TMap<TWeakObjectPtr<AActor>, FDefenseState> ActiveDefenseStates;

	/** Timer handles for auto-closing windows */
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> WindowTimerHandles;

	// ========================================
	// INTERNAL METHODS
	// ========================================

	/** Called when window timer expires */
	void OnWindowTimerExpired(AActor *Defender);

	/** Apply reflected damage from parry */
	void ApplyReflectedDamage(AActor *Attacker, int32 Damage);

	/** Get CharacterDataComponent from actor */
	UCharacterDataComponent *GetCharacterDataComponent(AActor *Actor) const;
};
