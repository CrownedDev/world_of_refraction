// CombatHUD.cpp

#include "UI/Combat/CombatHUD.h"
#include "UI/Combat/CharacterPanelWidget.h"
#include "UI/Combat/TurnOrderStripWidget.h"
#include "UI/Combat/CommandMenu/CombatCommandMenuWidget.h"
#include "UI/Combat/DefensePromptWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Layout/Anchors.h"
#include "WoRLog.h"

namespace CombatHUDLayout
{
    // TODO: PIE-tune. Slate units, DPI-scaled. Panels stack away from their corner.
    constexpr float ScreenMargin = 24.0f;
    constexpr float PanelSpacing = 150.0f;

    const FVector2D LocalPartyAnchor(0.0f, 1.0f);    // bottom-left, stacks upward
    const FVector2D OpposingPartyAnchor(1.0f, 0.0f); // top-right, stacks downward
    const FVector2D TurnStripAnchor(0.5f, 0.0f);     // top-centre
    const FVector2D CommandMenuAnchor(1.0f, 1.0f);   // bottom-right
    const FVector2D DefensePromptAnchor(0.5f, 0.5f); // centre

    constexpr int32 PanelZOrder = 10;
    constexpr int32 StripZOrder = 10;
    constexpr int32 MenuZOrder = 20;
    constexpr int32 PromptZOrder = 30;
}

void UCombatHUD::Build(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty)
{
    if (bBuilt)
    {
        Teardown();
    }

    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] Build: no player controller - HUD skipped"));
        return;
    }

    if (UClass *PanelClass = ResolvePanelClass())
    {
        BuildPartyPanels(PC, PanelClass, LocalParty, /*bIsLocalParty=*/true);
        BuildPartyPanels(PC, PanelClass, OpposingParty, /*bIsLocalParty=*/false);
    }

    if (UClass *StripClass = TurnOrderStripClass.LoadSynchronous())
    {
        TurnOrderStrip = CreateWidget<UTurnOrderStripWidget>(PC, StripClass);
        if (TurnOrderStrip)
        {
            TurnOrderStrip->AddToViewport(CombatHUDLayout::StripZOrder);
            PlaceInViewport(TurnOrderStrip, CombatHUDLayout::TurnStripAnchor,
                            FVector2D(0.0f, CombatHUDLayout::ScreenMargin));
            TurnOrderStrip->InitialiseForCombat();
        }
    }
    else
    {
        UE_LOG(LogWoRUI, Error, TEXT("[CombatHUD] TurnOrderStripClass unresolved ('%s'): set it in Config/DefaultGame.ini"),
               *TurnOrderStripClass.ToString());
    }

    if (UClass *MenuClass = CommandMenuClass.LoadSynchronous())
    {
        CommandMenu = CreateWidget<UCombatCommandMenuWidget>(PC, MenuClass);
        if (CommandMenu)
        {
            CommandMenu->AddToViewport(CombatHUDLayout::MenuZOrder);
            PlaceInViewport(CommandMenu, CombatHUDLayout::CommandMenuAnchor,
                            FVector2D(-CombatHUDLayout::ScreenMargin, -CombatHUDLayout::ScreenMargin));
            // Replays an already-open menu, so turn 1 (fired inside InitializeCombat) is not missed.
            CommandMenu->InitialiseForCombat();
        }
    }
    else
    {
        UE_LOG(LogWoRUI, Error, TEXT("[CombatHUD] CommandMenuClass unresolved ('%s'): set it in Config/DefaultGame.ini"),
               *CommandMenuClass.ToString());
    }

    UClass *PromptClass = DefensePromptClass.IsNull() ? UDefensePromptWidget::StaticClass()
                                                      : DefensePromptClass.LoadSynchronous();
    if (PromptClass)
    {
        DefensePrompt = CreateWidget<UDefensePromptWidget>(PC, PromptClass);
        if (DefensePrompt)
        {
            DefensePrompt->AddToViewport(CombatHUDLayout::PromptZOrder);
            PlaceInViewport(DefensePrompt, CombatHUDLayout::DefensePromptAnchor, FVector2D::ZeroVector);
            DefensePrompt->InitialiseForCombat();
        }
    }

    bBuilt = true;

    UE_LOG(LogTemp, Log, TEXT("[CombatHUD] Built: %d panel(s), strip=%s, menu=%s, prompt=%s"),
           Panels.Num(),
           TurnOrderStrip ? TEXT("yes") : TEXT("no"),
           CommandMenu ? TEXT("yes") : TEXT("no"),
           DefensePrompt ? TEXT("yes") : TEXT("no"));
}

void UCombatHUD::Teardown()
{
    for (UCharacterPanelWidget *Panel : Panels)
    {
        if (Panel)
        {
            Panel->TeardownPanel();
            Panel->RemoveFromParent();
        }
    }
    Panels.Reset();

    if (TurnOrderStrip)
    {
        TurnOrderStrip->TeardownStrip();
        TurnOrderStrip->RemoveFromParent();
        TurnOrderStrip = nullptr;
    }

    if (CommandMenu)
    {
        CommandMenu->ShutdownForCombat();
        CommandMenu->RemoveFromParent();
        CommandMenu = nullptr;
    }

    if (DefensePrompt)
    {
        DefensePrompt->TeardownPrompt();
        DefensePrompt->RemoveFromParent();
        DefensePrompt = nullptr;
    }

    if (bBuilt)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatHUD] Torn down"));
    }
    bBuilt = false;
}

FString UCombatHUD::GetHUDStateString() const
{
    FString Out = FString::Printf(TEXT("[CombatHUD] built=%s panels=%d\n"), bBuilt ? TEXT("yes") : TEXT("no"), Panels.Num());
    for (int32 i = 0; i < Panels.Num(); ++i)
    {
        const UCharacterPanelWidget *Panel = Panels[i];
        const AActor *Bound = Panel ? Panel->GetBoundActor() : nullptr;
        Out += FString::Printf(TEXT("  panel[%d] -> %s\n"), i, Bound ? *Bound->GetName() : TEXT("<unbound>"));
    }
    Out += FString::Printf(TEXT("  strip=%s menu=%s prompt=%s"),
                           TurnOrderStrip ? TEXT("ok") : TEXT("none"),
                           CommandMenu ? TEXT("ok") : TEXT("none"),
                           DefensePrompt ? TEXT("ok") : TEXT("none"));
    return Out;
}

UClass *UCombatHUD::ResolvePanelClass() const
{
    if (UClass *Primary = CharacterPanelClass.LoadSynchronous())
    {
        return Primary;
    }
    if (UClass *Fallback = FallbackCharacterPanelClass.LoadSynchronous())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] CharacterPanelClass unresolved (%s) - using fallback %s"),
               *CharacterPanelClass.ToString(), *FallbackCharacterPanelClass.ToString());
        return Fallback;
    }
    UE_LOG(LogWoRUI, Error, TEXT("[CombatHUD] No character panel class resolved - panels skipped (set CharacterPanelClass "
                                 "in Config/DefaultGame.ini)"));
    return nullptr;
}

void UCombatHUD::BuildPartyPanels(APlayerController *PC, UClass *PanelClass, const TArray<AActor *> &Party, bool bIsLocalParty)
{
    const FVector2D Anchor = bIsLocalParty ? CombatHUDLayout::LocalPartyAnchor : CombatHUDLayout::OpposingPartyAnchor;
    const float StackDirection = bIsLocalParty ? -1.0f : 1.0f;
    const float EdgeX = bIsLocalParty ? CombatHUDLayout::ScreenMargin : -CombatHUDLayout::ScreenMargin;

    for (int32 i = 0; i < Party.Num(); ++i)
    {
        AActor *Actor = Party[i];
        if (!Actor)
        {
            continue;
        }

        UCharacterPanelWidget *Panel = CreateWidget<UCharacterPanelWidget>(PC, PanelClass);
        if (!Panel)
        {
            continue;
        }

        Panel->AddToViewport(CombatHUDLayout::PanelZOrder);
        const float OffsetY = StackDirection * (CombatHUDLayout::ScreenMargin + i * CombatHUDLayout::PanelSpacing);
        PlaceInViewport(Panel, Anchor, FVector2D(EdgeX, OffsetY));
        Panel->InitialiseForActor(Actor);
        Panels.Add(Panel);
    }
}

void UCombatHUD::PlaceInViewport(UUserWidget *Widget, const FVector2D &Anchor, const FVector2D &Offset)
{
    if (!Widget)
    {
        return;
    }
    Widget->SetAnchorsInViewport(FAnchors(Anchor.X, Anchor.Y));
    Widget->SetAlignmentInViewport(Anchor);
    Widget->SetPositionInViewport(Offset, /*bRemoveDPIScale=*/false);
}
