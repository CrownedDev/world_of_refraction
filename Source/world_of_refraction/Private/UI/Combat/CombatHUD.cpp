// CombatHUD.cpp

#include "UI/Combat/CombatHUD.h"
#include "UI/Combat/CharacterPanelWidget.h"
#include "UI/Combat/TurnOrderStripWidget.h"
#include "UI/Combat/CommandMenu/CombatCommandMenuWidget.h"
#include "UI/Combat/DefensePromptWidget.h"
#include "UI/Combat/Battle/BattleCommandMenuWidget.h"
#include "UI/Combat/Battle/BattleHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
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

namespace
{
    TAutoConsoleVariable<int32> CVarNewHUD(
        TEXT("wor.UI.NewHUD"), 1,
        TEXT("B-391: 1 = the new plain C++ battle HUD (cards, turn order, statuses, command menu); 0 = the old WBP panels, "
             "turn strip and menu (fallback until Crown's PIE). Read when a fight's HUD is built."));
}

bool UCombatHUD::WantsNewHUD()
{
    return CVarNewHUD.GetValueOnGameThread() != 0;
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

    bUsingNewHUD = WantsNewHUD();
    if (bUsingNewHUD)
    {
        BuildNewHUD(PC, LocalParty, OpposingParty);
    }
    else
    {
        BuildOldHUD(PC, LocalParty, OpposingParty);
    }

    // The new HUD needs no WBP: the native prompt builds its own text (B-391).
    UClass *PromptClass = bUsingNewHUD || DefensePromptClass.IsNull() ? UDefensePromptWidget::StaticClass()
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

    UE_LOG(LogTemp, Log, TEXT("[CombatHUD] Built (%s HUD): %d panel(s), strip=%s, menu=%s, battleHUD=%s, battleMenu=%s, prompt=%s"),
           bUsingNewHUD ? TEXT("new") : TEXT("old"),
           Panels.Num(),
           TurnOrderStrip ? TEXT("yes") : TEXT("no"),
           CommandMenu ? TEXT("yes") : TEXT("no"),
           BattleHUD ? TEXT("yes") : TEXT("no"),
           BattleMenu ? TEXT("yes") : TEXT("no"),
           DefensePrompt ? TEXT("yes") : TEXT("no"));
}

void UCombatHUD::BuildNewHUD(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty)
{
    // Full screen; Initialise before AddToViewport so the widget builds one card per combatant once.
    BattleHUD = CreateWidget<UBattleHUDWidget>(PC, UBattleHUDWidget::StaticClass());
    if (BattleHUD)
    {
        BattleHUD->Initialise(LocalParty, OpposingParty);
        BattleHUD->AddToViewport(CombatHUDLayout::PanelZOrder);
    }

    BattleMenu = CreateWidget<UBattleCommandMenuWidget>(PC, UBattleCommandMenuWidget::StaticClass());
    if (BattleMenu)
    {
        BattleMenu->AddToViewport(CombatHUDLayout::MenuZOrder);
        PlaceInViewport(BattleMenu, CombatHUDLayout::CommandMenuAnchor,
                        FVector2D(-CombatHUDLayout::ScreenMargin, -CombatHUDLayout::ScreenMargin));
        // Replays an already-open menu, so turn 1 (fired inside InitializeCombat) is not missed.
        BattleMenu->Initialise();
    }
}

void UCombatHUD::BuildOldHUD(APlayerController *PC, const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty)
{
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
}

TArray<UUserWidget *> UCombatHUD::GetAllWidgets() const
{
    TArray<UUserWidget *> Widgets;
    for (UCharacterPanelWidget *Panel : Panels)
    {
        Widgets.Add(Panel);
    }
    Widgets.Add(TurnOrderStrip);
    Widgets.Add(CommandMenu);
    Widgets.Add(DefensePrompt);
    Widgets.Add(BattleHUD); // B-391's new battle HUD (trial merge with B-406)
    Widgets.Add(BattleMenu);
    Widgets.RemoveAll([](const UUserWidget *Widget) { return Widget == nullptr; });
    return Widgets;
}

void UCombatHUD::SetHiddenForCinematic(bool bHidden)
{
    if (bHidden == bHiddenForCinematic)
    {
        return;
    }
    bHiddenForCinematic = bHidden;
    if (bHidden)
    {
        VisibilityBeforeCinematic.Reset();
        for (UUserWidget *Widget : GetAllWidgets())
        {
            VisibilityBeforeCinematic.Add(Widget, Widget->GetVisibility());
            Widget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        // TODO: a widget that changes its own visibility while hidden gets its old one back; nothing does today
        // (no action is requested and no defence opens under a boss cinematic).
        for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility> &Entry : VisibilityBeforeCinematic)
        {
            if (UUserWidget *Widget = Entry.Key.Get())
            {
                Widget->SetVisibility(Entry.Value);
            }
        }
        VisibilityBeforeCinematic.Reset();
    }
    UE_LOG(LogWoRUI, Log, TEXT("[CombatHUD] %s for a boss cinematic"), bHidden ? TEXT("Hidden") : TEXT("Shown"));
}

void UCombatHUD::Teardown()
{
    bHiddenForCinematic = false; // B-406
    VisibilityBeforeCinematic.Reset();
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

    if (BattleHUD)
    {
        BattleHUD->Teardown();
        BattleHUD->RemoveFromParent();
        BattleHUD = nullptr;
    }

    if (BattleMenu)
    {
        BattleMenu->Teardown();
        BattleMenu->RemoveFromParent();
        BattleMenu = nullptr;
    }

    if (bBuilt)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatHUD] Torn down"));
    }
    bBuilt = false;
}

namespace
{
    // B-396: everything that decides whether a built widget is on screen, so "built but invisible" shows its cause.
    FString DescribeOnScreen(const UUserWidget *Widget)
    {
        if (!Widget)
        {
            return TEXT("none");
        }
        const FGeometry &Geometry = Widget->GetCachedGeometry();
        const FVector2D Position(Geometry.GetAbsolutePosition());
        const FVector2D Size(Geometry.GetAbsoluteSize());
        return FString::Printf(TEXT("%s inViewport=%s visibility=%s opacity=%.2f drawn at (%.0f, %.0f) size %.0f x %.0f"),
                               *Widget->GetClass()->GetName(), Widget->IsInViewport() ? TEXT("yes") : TEXT("NO"),
                               *StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility())),
                               Widget->GetRenderOpacity(), Position.X, Position.Y, Size.X, Size.Y);
    }
}

FString UCombatHUD::GetHUDStateString() const
{
    FString Out = FString::Printf(TEXT("[CombatHUD] built=%s hud=%s (wor.UI.NewHUD %d) panels=%d\n"), bBuilt ? TEXT("yes") : TEXT("no"),
                                  bUsingNewHUD ? TEXT("new") : TEXT("old"), WantsNewHUD() ? 1 : 0, Panels.Num());
    const UWorld *World = GetWorld();
    if (const UGameViewportClient *Viewport = World ? World->GetGameViewport() : nullptr)
    {
        FVector2D ViewportSize;
        Viewport->GetViewportSize(ViewportSize);
        Out += FString::Printf(TEXT("  game viewport %.0f x %.0f\n"), ViewportSize.X, ViewportSize.Y);
    }
    else
    {
        Out += TEXT("  game viewport: NONE for this world\n");
    }
    for (int32 i = 0; i < Panels.Num(); ++i)
    {
        const UCharacterPanelWidget *Panel = Panels[i];
        const AActor *Bound = Panel ? Panel->GetBoundActor() : nullptr;
        Out += FString::Printf(TEXT("  panel[%d] -> %s: %s\n"), i, Bound ? *Bound->GetName() : TEXT("<unbound>"), *DescribeOnScreen(Panel));
    }
    Out += FString::Printf(TEXT("  strip: %s\n  menu: %s\n  prompt: %s"), *DescribeOnScreen(TurnOrderStrip),
                           *DescribeOnScreen(CommandMenu), *DescribeOnScreen(DefensePrompt));
    // B-391: the new battle HUD, where it was drawn and what it shows.
    Out += FString::Printf(TEXT("\n  battleHUD: %s\n  battleMenu: %s"), *DescribeOnScreen(BattleHUD), *DescribeOnScreen(BattleMenu));
    if (BattleHUD)
    {
        Out += TEXT("\n") + BattleHUD->GetBattleHUDString();
    }
    if (BattleMenu)
    {
        Out += TEXT("\n") + BattleMenu->GetCommandMenuString();
    }
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
        UE_LOG(LogWoRUI, Warning, TEXT("[CombatHUD] CharacterPanelClass unresolved (%s) - using fallback %s"),
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
