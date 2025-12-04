// CombatPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EInfusionSourceOption.h"
#include "EChargeInfusionType.h"
#include "CombatPlayerController.generated.h"

// Forward declarations
class UInputMappingContext;
class UInputAction;
class USpellData;
class UAbilityData;
class UInfusionChargeManager;
class UActionExecutor;

UCLASS()
class WORLD_OF_REFRACTION_API ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACombatPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// ========================================
	// INPUT ACTIONS (Assign in Blueprint)
	// ========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ChargeInfusionAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CycleSourceAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ConfirmActionAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CancelActionAction;

	// ========================================
	// TEST DATA (Assign in Blueprint)
	// ========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test|Spell")
	TObjectPtr<USpellData> TestSpell;

	/** Target actor for spell testing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Combat")
	AActor *TestTarget = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test|Ability")
	TObjectPtr<UAbilityData> TestAbility;

	/** Current action type being charged (Spell or Ability) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test")
	EChargeInfusionType TestChargeType = EChargeInfusionType::Spell;

	// ========================================
	// INPUT HANDLERS
	// ========================================

	/** Called when charge button is pressed */
	void OnChargeStarted(const FInputActionValue &Value);

	/** Called while charge button is held */
	void OnChargeTriggered(const FInputActionValue &Value);

	/** Called when charge button is released */
	void OnChargeCompleted(const FInputActionValue &Value);

	/** Called when cycle source button is pressed */
	void OnCycleSource(const FInputActionValue &Value);

	/** Called when confirm button is pressed */
	void OnConfirmAction(const FInputActionValue &Value);

	/** Called when cancel button is pressed */
	void OnCancelAction(const FInputActionValue &Value);

	// ========================================
	// STATE
	// ========================================

	/** Currently selected infusion source */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EInfusionSourceOption CurrentSource = EInfusionSourceOption::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 StoredChargeLevel = 0;

	/** Is currently charging */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsCharging = false;

	// ========================================
	// HELPERS
	// ========================================

	/** Get the controlled character actor */
	AActor *GetControlledActor() const;

	/** Get InfusionChargeManager subsystem */
	UInfusionChargeManager *GetChargeManager() const;

	/** Get ActionExecutor subsystem */
	UActionExecutor *GetActionExecutor() const;

	/** Cycle to next available source */
	EInfusionSourceOption GetNextSource(EInfusionSourceOption Current) const;

	/** Print current state to screen */
	void DebugPrintState() const;
};