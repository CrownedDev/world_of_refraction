// WoRHubController.h
// C++ base for BP_HubPlayerController (F2a). Introduced so the hub/trial
// controller has a typed home for the CommonUI root layout and the mode enum —
// both of which several C++ systems will need to query, and neither of which is
// readable from a Blueprint-only controller.
//
// ADDITIVE BY DESIGN. The Blueprint keeps its own BeginPlay, its IA_Interact
// arbitration, and its variables; this base only creates the root layout. The
// BP's BeginPlay runs AFTER this one (Super-first ordering), so nothing here
// duplicates or pre-empts the mapping context, EnsureLocalParty, or the party
// strip creation that already live in the graph.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "WoRHubController.generated.h"

class UCommonActivatableWidget;
class UWoRRootLayout;

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

protected:
	virtual void BeginPlay() override;

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
};
