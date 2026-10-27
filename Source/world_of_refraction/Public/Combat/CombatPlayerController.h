// CombatPlayerController.h
// Player controller for turn-based combat. Owns the REAL-TIME defense input
// (block / parry / dodge) via Enhanced Input. Turn-based action selection is
// handled independently by UCombatCommandMenuSubsystem + the orchestrator — this
// controller is not in that loop; it only routes defense presses into DefenseSystem.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "UI/Combat/PieMenuButtonData.h"
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

	/** The Guard input (D-084: Block and Parry are one defense). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> BlockAction;

	/** Legacy: also a Guard press until its key is unbound in the mapping context (Q-168). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> ParryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeLeftAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeRightAction;

	// ========================================
	// MENU KEYS (B-093, Q-093) — built in code, fixed keys (D-063)
	// ========================================
	// Defend, the infusion source and the infusion level press the command menu's own
	// buttons (UCombatCommandMenuSubsystem::PressMenuKey), so they follow the menu's rules.

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuKeysContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefendAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InfusionAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InfusionLevelAction;

	/** Creates the menu-key actions and their mapping context once. */
	void BuildMenuKeys();

	// ========================================
	// DEFENSE INPUT HANDLERS
	// ========================================

	void OnBlock(const FInputActionValue &Value);
	void OnParry(const FInputActionValue &Value);
	void OnDodgeLeft(const FInputActionValue &Value);
	void OnDodgeRight(const FInputActionValue &Value);

	void OnDefend(const FInputActionValue &Value);
	void OnInfusion(const FInputActionValue &Value);
	void OnInfusionLevel(const FInputActionValue &Value);

	/** Routes a menu key to the command menu; only the local player's keys count. */
	void PressMenuKey(EPieMenuCategory Category);

public:
	/** Debug: each menu key's action, keyboard key and gamepad button. */
	static FString GetMenuKeysString();

protected:

	// ========================================
	// HELPERS
	// ========================================

	/** DefenseSystem subsystem (GameInstance). Mirrors the GI-subsystem getter pattern. */
	UDefenseSystem *GetDefenseSystem() const;
};
