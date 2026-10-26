// WoRRootLayout.h
// CommonUI root layout (F2a): the single full-screen widget that owns the
// named layer stacks. Created once by AWoRHubController::BeginPlay and added
// to the viewport; everything CommonUI-driven is pushed to one of its layers
// rather than added to the viewport directly.
//
// NOT Lyra's UPrimaryGameLayout — that lives in the CommonGame plugin, which
// this project does not use. This is the hand-rolled equivalent: two stacks,
// bound by name, with an explicit push/pop API.
//
// ⚠️ BindWidget (not BindWidgetOptional): the WBP deriving from this MUST
// contain two UCommonActivatableWidgetStack widgets named exactly GameLayer
// and MenuLayer, or it fails to compile in the widget editor. That is
// deliberate — a silently-missing layer would fail at push time instead.

#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "WoRRootLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Push a widget onto the menu layer and activate it. Returns the instance,
	 *  or null if the class is unset or the layer failed to bind.
	 *
	 *  ⚠️ Do NOT cache the returned pointer across pushes. The container pools
	 *  its widgets (see CommonActivatableWidgetContainer.h) — a later push of the
	 *  same class may hand back a recycled instance with prior state intact, so
	 *  callers must establish everything they care about on each push. */
	UFUNCTION(BlueprintCallable, Category = "Root Layout")
	UCommonActivatableWidget *PushToMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Remove the top widget from the menu layer. No-op when the layer is empty.
	 *  The stack re-activates the entry beneath automatically. */
	UFUNCTION(BlueprintCallable, Category = "Root Layout")
	void PopMenu();

protected:
	/** Persistent gameplay HUD layer. Present but EMPTY for now — the existing
	 *  viewport-added HUD (party strip, combat panels) moves here in a later
	 *  cluster, not this one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonActivatableWidgetStack *GameLayer;

	/** Menu layer — the party menu and anything else non-modal pushes here. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonActivatableWidgetStack *MenuLayer;
};
