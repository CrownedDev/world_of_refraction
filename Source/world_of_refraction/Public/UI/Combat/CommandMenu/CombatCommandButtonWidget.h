// CombatCommandButtonWidget.h
// Single button widget for the combat command menu.
// Owns visuals + click forwarding only. NO subsystem binding logic.
// This separation (no shared base with the container) is the key
// architectural fix from the 2026-04-29 menu C++ migration postmortem.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "CombatCommandButtonWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCombatCommandButtonWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Update display from button data. Called by the container on each refresh. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Button")
    void SetData(const FPieMenuButtonData &InData);

    /** Show / hide for pool slots that aren't currently in use. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Button")
    void Show();

    UFUNCTION(BlueprintCallable, Category = "Combat Command Button")
    void Hide();

    /** Debug: log this button's current data. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Button|Debug",
              meta = (DevelopmentOnly))
    void DebugLogData() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton *CommandButton = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock *LabelText = nullptr;

    /** BP hook for visual flourish when data changes (icon swap, tint, etc.) */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Command Button")
    void OnDataChanged(const FPieMenuButtonData &Data);

private:
    /** Current button data. Stored so HandleClicked knows what to forward. */
    FPieMenuButtonData CurrentData;

    /** True after data has been set at least once. Prevents stale clicks on unused pool slots. */
    bool bHasData = false;

    UFUNCTION()
    void HandleClicked();
};