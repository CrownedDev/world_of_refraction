// CombatCommandMenuWidget.h
// Container widget for the combat command menu.
// Owns subsystem binding and a pre-spawned pool of button widgets.
// Buttons are NEVER created or destroyed during gameplay — only their data
// and visibility update on each menu refresh. This bypasses the
// RF_Transactional accumulation that broke the previous menu.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatCommandMenuWidget.generated.h"

class UPanelWidget;
class UCombatCommandButtonWidget;
class UCombatCommandMenuSubsystem;
struct FPieMenuButtonData;

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UCombatCommandMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Called by the owning character actor after AddToViewport. Binds to subsystem. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void InitialiseForCombat();

    /** Manual unbind — call before removing from viewport if not relying on BeginDestroy. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void ShutdownForCombat();

    /** Debug: log pool state and subsystem binding status. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu|Debug",
              meta = (DevelopmentOnly))
    void DebugLogPoolState() const;

protected:
    virtual void NativeConstruct() override;
    virtual void BeginDestroy() override;

    /** BP-side panel that holds the button pool. VerticalBox in the BP child. */
    UPROPERTY(meta = (BindWidget))
    UPanelWidget *ButtonContainer = nullptr;

    /** BP-side class used to spawn the pre-spawned buttons. Set in the BP child Defaults. */
    UPROPERTY(EditDefaultsOnly, Category = "Combat Command Menu")
    TSubclassOf<UCombatCommandButtonWidget> ButtonWidgetClass;

    /** Maximum buttons across all menu states (largest grid in any class). */
    static constexpr int32 PoolSize = 6;

    /** BP hook for any visual flourish on refresh (animations, sounds, etc.) */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Command Menu")
    void OnButtonsRefreshed(int32 ActiveCount);

private:
    /** Pre-spawned pool of button widgets. Created once in NativeConstruct. */
    UPROPERTY(Transient)
    TArray<UCombatCommandButtonWidget *> ButtonPool;

    /** Cached subsystem reference for unbinding in BeginDestroy. NOT a UPROPERTY. */
    TWeakObjectPtr<UCombatCommandMenuSubsystem> CachedSubsystem;

    /** True after InitialiseForCombat completes successfully. */
    bool bIsInitialised = false;

    UFUNCTION()
    void HandleCommandMenuReady(const TArray<FPieMenuButtonData> &Buttons);

    UFUNCTION()
    void HandleCommandMenuClosed();

    void SpawnButtonPool();
    void UnbindFromSubsystem();
};