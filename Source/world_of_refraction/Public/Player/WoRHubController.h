// WoRHubController.h
// The one hub / trial controller (D-076 = A, B-158): C++ base for BP_HubPlayerController.
//   - CommonUI root layout and the hub mode enum (F2a).
//   - IA_Interact -> the nearest in-range interactable (trial door, merchant, recruit, map
//     feature) -> Interact() (HubInteract; moved here from AExplorationPlayerController).
//   - OnPossess -> UPartySessionSubsystem::EnsureLocalParty (creates the solo Trial Party on
//     first possession, rebinds the leader after every level change).
//
// The Blueprint still owns its BeginPlay: the input mapping context and the party display.
// Editor step (docs/PIE/B-158.md): delete the BP's IA_Interact and EnsureLocalParty nodes,
// else E fires twice (EnsureLocalParty twice is harmless: it is a get-or-create).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "WoRHubController.generated.h"

class ACombatCharacter;
class UCommonActivatableWidget;
class UInputAction;
class UWoRRootLayout;
struct FInputActionValue;

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

	/** B-158: interact with the nearest in-range door / merchant / recruit / map feature.
	 *  Also callable from BP / console. */
	UFUNCTION(BlueprintCallable, Category = "Hub|Interaction")
	void InteractWithNearest();

	/** Debug: what E would hit right now, with distances. */
	UFUNCTION(BlueprintCallable, Category = "Hub|Interaction|Debug")
	FString GetInteractionStateString() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn *InPawn) override;
	virtual void SetupInputComponent() override;

	/** Leave empty to use DefaultInteractAction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hub|Input")
	TObjectPtr<UInputAction> InteractAction;

	/** IA_Interact, set in Config/DefaultGame.ini; loaded when InteractAction is empty (B-205). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Hub|Input")
	TSoftObjectPtr<UInputAction> DefaultInteractAction;

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

	EHubMode CurrentMode = EHubMode::Hub;

	void HandleInteractPressed(const FInputActionValue &Value);
};
