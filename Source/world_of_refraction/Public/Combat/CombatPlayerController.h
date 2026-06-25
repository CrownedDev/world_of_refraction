// CombatPlayerController.h
// Player controller for turn-based combat. Owns the REAL-TIME defense input
// (block / parry / dodge) via Enhanced Input. Turn-based action selection is
// handled independently by UCombatCommandMenuSubsystem + the orchestrator — this
// controller is not in that loop; it only routes defense presses into DefenseSystem.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "CombatPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UDefenseSystem;

UCLASS()
class WORLD_OF_REFRACTION_API ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================
	// INPUT ASSETS (assign in Blueprint / editor)
	// ========================================

	/** Combat input mapping context — added at priority 0 in BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> ParryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeLeftAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeRightAction;

	// ========================================
	// INFUSION INPUT — PLACEHOLDERS (not implemented)
	// ========================================
	// TODO: Infusion input — charge/select the infusion source. Will hook the
	//   infusion system. The live command menu drives infusion via
	//   UInfusionVFXComponent::CycleInfusionLevel; the controller-side mechanic
	//   (hold-to-charge via UInfusionChargeManager vs. cycle) is TBD by Crown.
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Infusion")
	// TObjectPtr<UInputAction> InfusionAction;
	//
	// TODO: Infusion level input — cycle/select the infusion level.
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Infusion")
	// TObjectPtr<UInputAction> InfusionLevelAction;

	// ========================================
	// DEFENSE INPUT HANDLERS
	// ========================================

	void OnBlock(const FInputActionValue &Value);
	void OnParry(const FInputActionValue &Value);
	void OnDodgeLeft(const FInputActionValue &Value);
	void OnDodgeRight(const FInputActionValue &Value);

	// TODO: Infusion handlers (commented until the infusion mechanic is decided):
	// void OnInfusion(const FInputActionValue &Value);
	// void OnInfusionLevel(const FInputActionValue &Value);

	// ========================================
	// HELPERS
	// ========================================

	/** DefenseSystem subsystem (GameInstance). Mirrors the GI-subsystem getter pattern. */
	UDefenseSystem *GetDefenseSystem() const;
};
