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
// Widget classes are config-backed soft refs with constructor defaults pointing
// at the existing WBP assets — no editor step needed; override under
// [/Script/world_of_refraction.CombatHUD] in DefaultGame.ini if an asset moves.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatHUD.generated.h"

class AActor;
class APlayerController;
class UUserWidget;
class UCharacterPanelWidget;
class UTurnOrderStripWidget;
class UCombatCommandMenuWidget;
class UDefensePromptWidget;

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UCombatHUD : public UObject
{
    GENERATED_BODY()

public:
    UCombatHUD();

    /** Create + initialise every combat widget. Idempotent: tears down first if already built. */
    void Build(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty);

    /** Unbind and remove every widget this HUD created. Safe to call twice. */
    void Teardown();

    bool IsBuilt() const { return bBuilt; }

    /** Debug: one line per widget with bound actor / validity. */
    FString GetHUDStateString() const;

    // ========================================
    // Widget classes (config-backed, defaults = current WBP assets)
    // ========================================

    UPROPERTY(Config, EditAnywhere, Category = "Combat HUD")
    TSoftClassPtr<UCharacterPanelWidget> CharacterPanelClass;

    /** Tried when CharacterPanelClass fails to load / isn't a UCharacterPanelWidget
     *  (the project has two WBP_CharacterPanel assets — see review §9.5). */
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

    bool bBuilt = false;

    UClass *ResolvePanelClass() const;

    void BuildPartyPanels(APlayerController *PC, UClass *PanelClass, const TArray<AActor *> &Party, bool bIsLocalParty);

    /** Anchor + align + offset a standalone viewport widget (Slate units, DPI-scaled). */
    static void PlaceInViewport(UUserWidget *Widget, const FVector2D &Anchor, const FVector2D &Offset);
};
