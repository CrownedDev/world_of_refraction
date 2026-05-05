// InfusionChargeManager.h
// Tracks hold-to-charge timing for the infusion system
// Converts button hold duration → InfusionLevel (0, 1, 2)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EInfusionType.h"
#include "EChargeInfusionType.h"
#include "EInfusionSourceOption.h"
#include "InfusionConstants.h"
#include "ActionStructs.h"
#include "InfusionChargeManager.generated.h"

class AActor;

/**
 * Current charging state
 */
UENUM(BlueprintType)
enum class EChargeState : uint8
{
	/** Not charging */
	Idle UMETA(DisplayName = "Idle"),

	/** Actively charging (button held) */
	Charging UMETA(DisplayName = "Charging"),

	/** Charge complete, awaiting release */
	Ready UMETA(DisplayName = "Ready"),

	/** Cancelled */
	Cancelled UMETA(DisplayName = "Cancelled")
};

/**
 * Snapshot of current charge status (for UI/AI queries)
 */
USTRUCT(BlueprintType)
struct FChargeStatus
{
	GENERATED_BODY()

	/** Current state */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	EChargeState State = EChargeState::Idle;

	/** Charge type (Spell or Ability) */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	EChargeInfusionType ChargeType = EChargeInfusionType::None;

	/** Selected source for ability infusion */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	EInfusionSourceOption SelectedSource = EInfusionSourceOption::None;

	/** Current charge level (0, 1, 2) */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	int32 ChargeLevel = 0;

	/** Time spent charging (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	float ChargeTime = 0.0f;

	/** Progress to next level (0.0 - 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	float ProgressToNextLevel = 0.0f;

	/** Time until next level (seconds), -1 if at max */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	float TimeToNextLevel = -1.0f;

	/** Actor currently charging */
	UPROPERTY(BlueprintReadOnly, Category = "Charge")
	AActor *ChargingActor = nullptr;
};

/**
 * Delegate signatures
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnChargeStarted, AActor *, Actor, EChargeInfusionType, ChargeType, EInfusionSourceOption, Source, int32, InitialLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnChargeLevelChanged, AActor *, Actor, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnChargeComplete, AActor *, Actor, EChargeInfusionType, ChargeType, EInfusionSourceOption, Source, int32, FinalLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChargeCancelled, AActor *, Actor, int32, LevelAtCancel);
/**
 * UInfusionChargeManager
 *
 * GameInstanceSubsystem that tracks hold-to-charge timing for the infusion system.
 * Converts button hold duration into discrete charge levels (0, 1, 2).
 *
 * Usage:
 * 1. Call BeginCharge() when player starts holding action button
 * 2. System tracks time, broadcasts OnChargeLevelChanged at thresholds
 * 3. Call CompleteCharge() when button released to get final level
 * 4. Use returned level in FAction.InfusionLevel
 *
 * Timing (from InfusionConstants):
 * - < 0.5s  → Level 0 (no infusion, immediate action)
 * - 0.5-1.5s → Level 1 (first charge)
 * - > 1.5s  → Level 2 (full charge)
 *
 * AI Integration (future):
 * - AI can call SetChargeLevel() directly without timing
 * - Or use SimulateCharge() for testing
 *
 * UI Integration (future):
 * - Bind to OnChargeLevelChanged for progress updates
 * - Query GetChargeStatus() each frame for smooth progress bar
 */
UCLASS()
class WORLD_OF_REFRACTION_API UInfusionChargeManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UInfusionChargeManager();

	virtual void Initialize(FSubsystemCollectionBase &Collection) override;
	virtual void Deinitialize() override;

	// ========================================
	// CHARGING API
	// ========================================

	/**
	 * Begin charging an infusion
	 * HP cost is deducted immediately on charge start
	 * @param Actor Actor performing the charge
	 * @param ChargeType Type of charge (Spell or Ability)
	 * @param SelectedSource Source for ability infusion (ignored for Spell)
	 * @return true if charging started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge")
	bool BeginCharge(AActor *Actor, EChargeInfusionType ChargeType, EInfusionSourceOption SelectedSource = EInfusionSourceOption::None);

	/**
	 * Complete the charge and get final level
	 * Call when player releases button
	 * @return Final charge level (0, 1, or 2)
	 */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge")
	int32 CompleteCharge();

	/**
	 * Cancel charging without executing
	 * Call if player cancels action or switches targets
	 */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge")
	void CancelCharge();

	/**
	 * Update charge state (call from Tick if using time-based charging)
	 * Automatically called if bAutoUpdate is true
	 * @param DeltaTime Time since last update
	 */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge")
	void UpdateCharge(float DeltaTime);

	// ========================================
	// QUERY
	// ========================================

	/**
	 * Get current charge status (for UI/debugging)
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion Charge")
	FChargeStatus GetChargeStatus() const;

	/**
	 * Is currently charging?
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion Charge")
	bool IsCharging() const;

	/**
	 * Get current charge level without completing
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion Charge")
	int32 GetCurrentChargeLevel() const;

	/**
	 * Get charge type being charged (Spell or Ability)
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion Charge")
	EChargeInfusionType GetChargingType() const;

	/**
	 * Get selected source (for ability charging)
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion Charge")
	EInfusionSourceOption GetSelectedSource() const;

	/**
	 * Get actor currently charging
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion Charge")
	AActor *GetChargingActor() const;

	// ========================================
	// AI / DIRECT SET (bypasses timing)
	// ========================================

	/**
	 * Directly set charge level (for AI or testing)
	 * Bypasses hold-time requirement, still applies HP cost
	 * @param Actor Actor to set for
	 * @param ChargeType Type of charge (Spell or Ability)
	 * @param Source Source for ability infusion
	 * @param Level Desired level (0, 1, 2)
	 */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge|AI")
	void SetChargeLevel(AActor *Actor, EChargeInfusionType ChargeType, EInfusionSourceOption Source, int32 Level);

	/**
	 * Build a charged FAction directly (for AI)
	 * Sets SpellInfusionLevel or AbilityInfusionLevel based on ChargeType
	 */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge|AI")
	void ApplyChargeToAction(UPARAM(ref) FAction &Action, EChargeInfusionType ChargeType, EInfusionSourceOption Source, int32 Level);

	// ========================================
	// EVENTS
	// ========================================

	/** Broadcast when charging begins */
	UPROPERTY(BlueprintAssignable, Category = "Infusion Charge|Events")
	FOnChargeStarted OnChargeStarted;

	/** Broadcast when charge level increases (L0→L1 or L1→L2) */
	UPROPERTY(BlueprintAssignable, Category = "Infusion Charge|Events")
	FOnChargeLevelChanged OnChargeLevelChanged;

	/** Broadcast when charge completes (button released) */
	UPROPERTY(BlueprintAssignable, Category = "Infusion Charge|Events")
	FOnChargeComplete OnChargeComplete;

	/** Broadcast when charge is cancelled */
	UPROPERTY(BlueprintAssignable, Category = "Infusion Charge|Events")
	FOnChargeCancelled OnChargeCancelled;

	// ========================================
	// CONFIGURATION
	// ========================================

	/** Time to reach Level 1 (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion Charge|Config")
	float Level1Time = InfusionConstants::LEVEL_1_CHARGE_TIME;

	/** Time to reach Level 2 (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion Charge|Config")
	float Level2Time = InfusionConstants::LEVEL_2_CHARGE_TIME;

	/** Auto-update charge time using world timer (vs manual UpdateCharge calls) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Infusion Charge|Config")
	bool bAutoUpdate = true;

	// ========================================
	// DEBUG
	// ========================================

	/** Print debug info to log */
	UFUNCTION(BlueprintCallable, Category = "Infusion Charge|Debug", meta = (DevelopmentOnly))
	void DebugPrintStatus() const;

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	/** Current charge state */
	EChargeState CurrentState = EChargeState::Idle;

	/** Actor currently charging */
	UPROPERTY()
	TWeakObjectPtr<AActor> ChargingActor;

	/** Type of charge (Spell or Ability) */
	EChargeInfusionType ChargingType = EChargeInfusionType::None;

	/** Selected source for ability charging */
	EInfusionSourceOption ChargingSource = EInfusionSourceOption::None;

	/** Current charge level */
	int32 CurrentLevel = 0;

	/** Time spent charging */
	float ChargeTime = 0.0f;

	/** Timer handle for auto-update */
	FTimerHandle UpdateTimerHandle;

	// ========================================
	// INTERNAL METHODS
	// ========================================

	/** Calculate level from charge time */
	int32 CalculateLevelFromTime(float Time) const;

	/** Calculate progress to next level (0.0 - 1.0) */
	float CalculateProgressToNextLevel() const;

	/** Reset all state */
	void ResetState();

	/** Timer callback for auto-update */
	void OnUpdateTimer();
};
