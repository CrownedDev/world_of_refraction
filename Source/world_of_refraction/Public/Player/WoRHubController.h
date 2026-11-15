// WoRHubController.h
// The one hub / trial controller (D-076 = A, B-158): C++ base for BP_HubPlayerController.
//   - CommonUI root layout and the hub mode enum (F2a).
//   - IA_Interact -> the nearest in-range interactable (trial door, merchant, recruit, map
//     feature) -> Interact() (HubInteract; moved here from AExplorationPlayerController).
//   - OnPossess -> UPartySessionSubsystem::EnsureLocalParty (creates the solo Trial Party on
//     first possession, rebinds the leader after every level change), then gives the pawn its
//     UFieldMovementComponent (B-214a). IA_Field_MovementSkill -> the pawn's active field skill.
//
// The Blueprint still owns its BeginPlay: the input mapping context and the party display.
// Editor step (docs/PIE/B-158.md): delete the BP's IA_Interact and EnsureLocalParty nodes,
// else E fires twice (EnsureLocalParty twice is harmless: it is a get-or-create).

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSkillTypes.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "WoRHubController.generated.h"

class ACombatCharacter;
class UCommonActivatableWidget;
class UInputAction;
class UWoRRootLayout;
struct FInputActionValue;
class UFieldMovementComponent;

/** Which non-combat context this controller is driving.
 *
 *  ⚠️ Combat is deliberately NOT a value here. Combat runs on a DIFFERENT
 *  controller (ACombatPlayerController) with its own mapping context, so
 *  "combat mode" on this class would describe a state it can never be in.
 *  Trial IS a value because the trial level uses this same controller — the
 *  trial GameMode names it as its PlayerControllerClass. */
UENUM(BlueprintType)
enum class EHubMode : uint8
{
	Hub,
	Trial
};

UCLASS()
class WORLD_OF_REFRACTION_API AWoRHubController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Hub")
	EHubMode GetHubMode() const { return CurrentMode; }

	UFUNCTION(BlueprintCallable, Category = "Hub")
	void SetHubMode(EHubMode NewMode);

	/** Push a widget onto the root layout's menu layer. Forwards to the layout;
	 *  returns null (with a warning) when no layout exists. BlueprintCallable so
	 *  the BP's toggle can drive it without knowing the layout type. */
	UFUNCTION(BlueprintCallable, Category = "Hub|UI")
	UCommonActivatableWidget *PushMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	UFUNCTION(BlueprintCallable, Category = "Hub|UI")
	void PopMenu();

	UFUNCTION(BlueprintPure, Category = "Hub|UI")
	UWoRRootLayout *GetRootLayout() const { return RootLayout; }

	/** B-439: opens the game menu (UPauseMenuWidget) on the menu layer; nothing when a screen is already up.
	 *  IA_ToggleMenu calls it; also callable from BP / console (wor.UI.Pause). */
	UFUNCTION(BlueprintCallable, Category = "Hub|UI")
	void OpenGameMenu();

	/** B-158: interact with the nearest in-range door / merchant / recruit / map feature.
	 *  Also callable from BP / console. */
	UFUNCTION(BlueprintCallable, Category = "Hub|Interaction")
	void InteractWithNearest();

	/** Debug: what E would hit right now, with distances. */
	UFUNCTION(BlueprintCallable, Category = "Hub|Interaction|Debug")
	FString GetInteractionStateString() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn *InPawn) override;
	virtual void SetupInputComponent() override;

	/** Leave empty to use DefaultInteractAction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> InteractAction;

	/** IA_Interact, set in Config/DefaultGame.ini; loaded when InteractAction is empty (B-205). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultInteractAction;

	/** B-214a: uses the possessed pawn's active field skill (double-tap sprint). Leave empty to use
	 *  DefaultFieldMovementSkillAction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> FieldMovementSkillAction;

	/** IA_Field_MovementSkill, set in Config/DefaultGame.ini once B-214g makes the asset; until then
	 *  wor.Field.Activate stands in. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultFieldMovementSkillAction;

	/** B-214g: the field attack, on release (left mouse, RB). B-503m4 (D-217): a hold of RB opens the wheel instead (the
	 *  same key is on FieldSwitchSourceAction); no held attack. Leave empty to use DefaultFieldAttackAction (IA_Field_Attack). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> FieldAttackAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultFieldAttackAction;

	/** B-214g, B-503m4 (D-217): held, opens the field attack wheel (R, RB); a tap does nothing. Leave empty to use
	 *  DefaultFieldSwitchSourceAction (IA_Field_SwitchSource; the name is kept so the asset and the ini line stand). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> FieldSwitchSourceAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultFieldSwitchSourceAction;

	/** B-214g: fly up (+) / down (-) while flying, a 1D axis. Leave empty to use DefaultFieldFlyVerticalAction
	 *  (IA_Field_FlyVertical). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> FieldFlyVerticalAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultFieldFlyVerticalAction;

	/** B-439: opens the game menu. Leave empty to use DefaultToggleMenuAction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> ToggleMenuAction;

	/** IA_ToggleMenu, set in Config/DefaultGame.ini; loaded when ToggleMenuAction is empty. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultToggleMenuAction;

	/** Leader pawn class for the lazily created Trial Party. Null = the possessed pawn's class,
	 *  then PartySessionSubsystem's DefaultSoloPawnClass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Party")
	TSoftClassPtr<ACombatCharacter> LocalPartyPawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Party")
	bool bEnsureLocalPartyOnPossess = true;

	/** WBP deriving from UWoRRootLayout. Unset is a graceful skip, not a crash —
	 *  the same tolerance UMerchantShopSubsystem applies to its window class, so
	 *  a controller with no layout authored still plays. */
	UPROPERTY(EditDefaultsOnly, Category = "Hub|UI")
	TSubclassOf<UWoRRootLayout> RootLayoutClass;

private:
	/** Created once in BeginPlay, owned for the controller's lifetime. Transient:
	 *  a widget is runtime state and must never be saved with the controller. */
	UPROPERTY(Transient)
	TObjectPtr<UWoRRootLayout> RootLayout;

	/** B-443: the interact prompt and the money chip (a viewport widget; local player only). */
	UPROPERTY(Transient)
	TObjectPtr<class UHubHUDWidget> HubHUD;

	EHubMode CurrentMode = EHubMode::Hub;

	/** B-503e: the field keys made in code (attack, source cycle, fly up / down) when no asset is set; mapped
	 *  at WoRInputPriority::FIELD_KEYS. */
	UPROPERTY(Transient)
	TObjectPtr<class UInputMappingContext> FieldKeysContext;

	/** B-503e: makes any field action still empty (no asset, no ini line) in code with its InputMap.md keys. */
	void BuildCodeFieldActions();

	/** B-214g: the field attack's press (real time, for its hold); a release with no press (focus loss) does nothing. */
	double FieldAttackPressTime = 0.0;
	bool bFieldAttackPressed = false;
	/** B-503m5 (D-218): R / RB's press: held FIELD_SOURCE_HOLD_SECONDS it opens the wheel (FieldWheelOpenTimer), which
	 *  stays up after the release (a toggle); a tap does nothing here (RB's tap is the attack). */
	bool bFieldSourcePressed = false;
	FTimerHandle FieldWheelOpenTimer;

	/** B-503m4 (D-217): the wheel's keys, made in code and mapped only while it is up (IMC_Field_Wheel, priority
	 *  FIELD_WHEEL, eaten). B-514 (D-222, the quick strip): left / right (D-pad, left stick, A / D, the mouse wheel)
	 *  move along the pieces (IA_Field_StripMove); up / down (D-pad, left stick, W / S) step the piece's source
	 *  (IA_Field_StripSource); the mouse cursor picks the tile under it. A or a click confirms and closes; B,
	 *  Backspace, RB or R closes. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FieldWheelPickAction;
	/** B-503m5: mapped only to eat the mouse's travel while the wheel is up; the cursor picks (FieldWheelHoverTimer). */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FieldWheelMouseAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FieldWheelElementAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FieldWheelScrollAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FieldWheelConfirmAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FieldWheelCloseAction;
	UPROPERTY(Transient)
	TObjectPtr<class UInputMappingContext> FieldWheelContext;
	/** The entries read when the wheel opened, and its pick; INDEX_NONE: the wheel is down. */
	UPROPERTY(Transient)
	TArray<FFieldAttackEntry> FieldWheelEntries;
	int32 FieldWheelPick = INDEX_NONE;
	/** The entry kept when the wheel opened (its slice comes back on it). */
	int32 FieldWheelKept = INDEX_NONE;
	/** B-503m5: the wheel opened on keyboard and mouse: the cursor shows, and is read on this timer while it is up. */
	bool bFieldWheelCursor = false;
	FTimerHandle FieldWheelHoverTimer;
	/** B-514: where the cursor was at the last hover read; it picks only when it moves. */
	FVector2f FieldWheelCursorAt = FVector2f::ZeroVector;

	bool IsFieldWheelOpen() const { return FieldWheelPick != INDEX_NONE; }
	void BuildFieldWheelKeys();
	void OpenFieldWheel();
	/** Hides the wheel and unmaps its keys, picking nothing. */
	void CloseFieldWheel();
	/** B-503m5: picks the wheel's current entry, shows it on the strip and closes the wheel. */
	void ConfirmFieldWheel();
	void HandleFieldWheelPick(const FInputActionValue &Value);
	void HandleFieldWheelElement(const FInputActionValue &Value);
	void HandleFieldWheelScroll(const FInputActionValue &Value);
	void HandleFieldWheelConfirm(const FInputActionValue &Value);
	void HandleFieldWheelClose(const FInputActionValue &Value);
	/** B-503m5: the slice under the mouse cursor becomes the pick (the middle and off the wheel keep it). */
	void UpdateFieldWheelHover();
	/** Moves the pick to a slice (its first option, or the kept entry on its own slice) and redraws. */
	void PickFieldWheelSlice(int32 Slice);
	void StepFieldWheelElement(int32 Delta);
	/** B-514: Delta pieces along the strip, wrapping at the ends. */
	void StepFieldWheelPiece(int32 Delta);

	void HandleInteractPressed(const FInputActionValue &Value);
	void HandleFieldMovementSkill(const FInputActionValue &Value);
	void HandleFieldAttackStarted(const FInputActionValue &Value);
	void HandleFieldAttackCompleted(const FInputActionValue &Value);
	void HandleFieldAttackCanceled(const FInputActionValue &Value);
	void HandleFieldSwitchSourceStarted(const FInputActionValue &Value);
	void HandleFieldSwitchSource(const FInputActionValue &Value);
	void HandleFieldSwitchSourceCanceled(const FInputActionValue &Value);
	void HandleFieldFlyVertical(const FInputActionValue &Value);

	/** The possessed pawn's field movement, or null. */
	UFieldMovementComponent *GetFieldMovement() const;
	void HandleToggleMenu(const FInputActionValue &Value);

	/** B-214a: gives the field pawn its UFieldMovementComponent (once) and its party record's field skill. */
	void EnsureFieldMovement(APawn *InPawn) const;
};
