// CombatHUD.h
// Native owner of the combat HUD. Replaces the BP_CombatOrchestrator
// OnCombatStartedUI graph: creates one character panel per combatant, the turn
// order strip, the command menu and the defense prompt, adds them to the
// viewport as standalone widgets (CLAUDE.md TransBuffer rule — never spawned
// inside a designer-placed widget), and initialises them.
//
// Built once per combat (Build) and torn down once (Teardown). Per-turn data
// updates stay inside each widget's own delegate bindings.
//
// B-391: wor.UI.NewHUD (default 1) builds the new plain C++ battle HUD instead (UBattleHUDWidget for cards, turn order
// and statuses; UBattleCommandMenuWidget for the menu; the native UDefensePromptWidget), with no WBP. 0 builds the old
// WBP panels, strip and menu below, kept as the fallback until Crown's PIE pass. Read at each Build.
//
// Widget classes are config-backed soft refs, set under
// [/Script/world_of_refraction.CombatHUD] in Config/DefaultGame.ini (B-205);
// change them there if an asset moves.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/SlateWrapperTypes.h"
#include "CombatHUD.generated.h"

class AActor;
class APlayerController;
class UUserWidget;
class UCharacterPanelWidget;
class UTurnOrderStripWidget;
class UCombatCommandMenuWidget;
class UDefensePromptWidget;
class UBattleHUDWidget;
class UBattleCommandMenuWidget;

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UCombatHUD : public UObject
{
    GENERATED_BODY()

public:
    /** Create + initialise every combat widget. Idempotent: tears down first if already built. */
    void Build(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty);

    /** Unbind and remove every widget this HUD created. Safe to call twice. */
    void Teardown();

    bool IsBuilt() const { return bBuilt; }

    /** B-406: hides every combat widget for a boss cinematic (true) and restores each one's own visibility (false). */
    void SetHiddenForCinematic(bool bHidden);

    bool IsHiddenForCinematic() const { return bHiddenForCinematic; }
    /** B-391: whether the last Build made the new battle HUD (wor.UI.NewHUD). */
    bool IsUsingNewHUD() const { return bUsingNewHUD; }

    /** B-391: the value of wor.UI.NewHUD (1 = the new battle HUD). */
    static bool WantsNewHUD();

    /** Debug: one line per widget with bound actor / validity. */
    FString GetHUDStateString() const;

    // ========================================
    // Widget classes (config-backed: Config/DefaultGame.ini)
    // ========================================

    UPROPERTY(Config, EditAnywhere, Category = "Combat HUD")
    TSoftClassPtr<UCharacterPanelWidget> CharacterPanelClass;

    /** Optional: tried when CharacterPanelClass fails to load / isn't a UCharacterPanelWidget.
     *  Empty since B-205: the second WBP_CharacterPanel (review §9.5) is no longer in the project.
     *  TODO: remove after Crown's PIE pass if nothing needs it. */
    UPROPERTY(Config, EditAnywhere, Category = "Combat HUD")
    TSoftClassPtr<UCharacterPanelWidget> FallbackCharacterPanelClass;

    UPROPERTY(Config, EditAnywhere, Category = "Combat HUD")
    TSoftClassPtr<UTurnOrderStripWidget> TurnOrderStripClass;

    UPROPERTY(Config, EditAnywhere, Category = "Combat HUD")
    TSoftClassPtr<UCombatCommandMenuWidget> CommandMenuClass;

    /** Optional WBP skin; null = the native UDefensePromptWidget builds its own text. */
    UPROPERTY(Config, EditAnywhere, Category = "Combat HUD")
    TSoftClassPtr<UDefensePromptWidget> DefensePromptClass;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UCharacterPanelWidget>> Panels;

    UPROPERTY(Transient)
    TObjectPtr<UTurnOrderStripWidget> TurnOrderStrip;

    UPROPERTY(Transient)
    TObjectPtr<UCombatCommandMenuWidget> CommandMenu;

    UPROPERTY(Transient)
    TObjectPtr<UDefensePromptWidget> DefensePrompt;

    /** B-391: the new battle HUD (wor.UI.NewHUD 1); null when the old widgets are built. */
    UPROPERTY(Transient)
    TObjectPtr<UBattleHUDWidget> BattleHUD;

    UPROPERTY(Transient)
    TObjectPtr<UBattleCommandMenuWidget> BattleMenu;

    bool bBuilt = false;
    bool bUsingNewHUD = false;

    /** B-391: the new battle HUD's two widgets, added straight to the viewport. */
    void BuildNewHUD(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty);

    /** The old WBP widgets (wor.UI.NewHUD 0): character panels, turn order strip and command menu. */
    void BuildOldHUD(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty);

    /** B-406: hidden for a boss cinematic, and each widget's visibility from before, to put back. */
    bool bHiddenForCinematic = false;
    TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> VisibilityBeforeCinematic;

    /** B-406: every combat widget this HUD built. */
    TArray<UUserWidget *> GetAllWidgets() const;

    UClass *ResolvePanelClass() const;

    void BuildPartyPanels(APlayerController *PC, UClass *PanelClass, const TArray<AActor *> &Party, bool bIsLocalParty);

    /** Anchor + align + offset a standalone viewport widget (Slate units, DPI-scaled). */
    static void PlaceInViewport(UUserWidget *Widget, const FVector2D &Anchor, const FVector2D &Offset);
};
