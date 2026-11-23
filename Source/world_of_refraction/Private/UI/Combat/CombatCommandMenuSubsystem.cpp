// CombatCommandMenuSubsystem.cpp
// World of Refraction - Combat UI

#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Player/TurnStateComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Save/SaveSubsystem.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Combat/TurnManager.h"
#include "Character/CharacterData.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Loadout/FCombatLoadout.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/CombatPlayerController.h"
#include "Player/RunStateComponent.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Actions/CombatPickRequest.h"
#include "WoRAuthority.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Infusion/InfusionConstants.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "WoRGameNames.h"
#include "Combat/Actions/ActionPipsText.h"
#include "UI/WoRIconTable.h"
#include "UI/Combat/CombatHUD.h"

namespace
{
    // B-228: teams from the GameState's replicated turn state (every player sees them), else the server-only
    // TurnManager (standalone, headless).
    int32 TeamOf(const UTurnStateComponent *TurnState, UTurnManager *TM, AActor *Actor)
    {
        return TurnState ? TurnState->GetActorTeam(Actor) : (TM ? TM->GetActorTeam(Actor) : INDEX_NONE);
    }

    TArray<AActor *> MembersOf(const UTurnStateComponent *TurnState, UTurnManager *TM, int32 Team)
    {
        return TurnState ? TurnState->GetTeamMembers(Team) : (TM ? TM->GetTeamMembers(Team) : TArray<AActor *>());
    }

    /** EPieMenuCategory::MoveRow's DataIndex: which way the step goes. */
    constexpr int32 MOVE_ROW_BACK_INDEX = 0;
    constexpr int32 MOVE_ROW_FORWARD_INDEX = 1;
    /** B-588: the Position button's DataIndex (it opens the picker); the picker's rows carry their row's index. */
    constexpr int32 POSITION_OPEN_INDEX = INDEX_NONE;
    /** B-588: the picker's rows, top to bottom. */
    constexpr ECombatRow POSITION_PICKER_ROWS[] = {ECombatRow::Front, ECombatRow::Middle, ECombatRow::Back};

    bool ResolveImmuneFlag(UObject *ActionData, AActor *Actor)
    {
        // Equipment-level immunity (active weapon / active ring on the wielder)
        bool bEquipmentImmune = false;
        if (Actor)
        {
            if (ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>())
            {
                if (UWeaponData *Weapon = LC->GetActiveWeapon())
                {
                    if (Weapon->bImmuneToInfusion) bEquipmentImmune = true;
                }
                if (URingData *Ring = LC->GetActiveRing())
                {
                    if (Ring->bImmuneToInfusion) bEquipmentImmune = true;
                }
            }
        }

        // Action-level immunity — single cast to base is sufficient
        if (const USkillDataBase *Skill = Cast<USkillDataBase>(ActionData))
        {
            return bEquipmentImmune || Skill->bImmuneToInfusion;
        }

        return bEquipmentImmune;
    }
}

// ==================== LIFECYCLE ====================

void UCombatCommandMenuSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Initialized"));
}

void UCombatCommandMenuSubsystem::Deinitialize()
{
    ClearCombatOrchestrator();
    CurrentActor.Reset();
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Deinitialized"));
}

// ==================== COMBAT REGISTRATION ====================

void UCombatCommandMenuSubsystem::SetCombatOrchestrator(ACombatOrchestrator *Orchestrator)
{
    // Defensive: clear any prior binding first
    ClearCombatOrchestrator();

    if (!Orchestrator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] SetCombatOrchestrator called with null"));
        return;
    }

    CurrentOrchestrator = Orchestrator;

    Orchestrator->OnActionRequested.AddDynamic(this, &UCombatCommandMenuSubsystem::HandlePlayerActionRequested);
    Orchestrator->OnActionExecuted.AddDynamic(this, &UCombatCommandMenuSubsystem::HandleActionExecuted);

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Combat orchestrator set"));
}

void UCombatCommandMenuSubsystem::ClearCombatOrchestrator()
{
    if (ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get())
    {
        Orchestrator->OnActionRequested.RemoveDynamic(this, &UCombatCommandMenuSubsystem::HandlePlayerActionRequested);
        Orchestrator->OnActionExecuted.RemoveDynamic(this, &UCombatCommandMenuSubsystem::HandleActionExecuted);
    }

    CurrentOrchestrator.Reset();

    if (bIsOpen)
    {
        Close();
    }

    // Nothing half-picked in this fight carries into the next one (B-030).
    ActiveSubmenuType = EPieMenuCategory::None;
    PendingActionCategory = EPieMenuCategory::None;
    PendingActionID.Empty();
    PendingActionData.Reset();
    PendingActionDataIndex = -1;
    PendingInfusionLevel = 0;
    PendingTargetType = ETargetType::Enemy;
    PendingTargetCount = ETargetCount::Single;
    ResolvedTargetType = PendingTargetType;
    ResolvedTargetCount = PendingTargetCount;
    DepthBeforeTargetSelection = ECombatMenuDepth::Main;
    CurrentCapabilities = FCombatCapabilities();
    LastAttackTargets.Reset(); // B-582: remembered for one fight

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Combat orchestrator cleared"));
}

void UCombatCommandMenuSubsystem::HandlePlayerActionRequested(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    if (!Orchestrator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] HandlePlayerActionRequested - no orchestrator"));
        return;
    }

    // AI actors handle their own decisions; we only show the menu for player-controlled actors. B-232b: and only on the
    // machine whose player commands this member (a client's member opens on that client, UTurnStateComponent).
    const URunStateComponent *Run = URunStateComponent::Find(this);
    const UWorld *World = GetWorld();
    // Review fix: only a roster member follows the seat rule; off the roster (a test actor, a guest) the host opens it.
    const bool bOnRoster = Run && Run->FindRosterEntry(Actor);
    const bool bCommandedHere = bOnRoster && World && Run->IsCommandedBy(Actor, World->GetFirstPlayerController());
    if (!CommandMenuRules::OpensHere(Orchestrator->IsActorAIControlled(Actor), bOnRoster, bCommandedHere))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] %s is not commanded here - no menu on this machine"), *Actor->GetName());
        return;
    }

    OpenForActor(Actor);
}

void UCombatCommandMenuSubsystem::HandleActionExecuted(AActor *Actor, const FActionResult &Result)
{
    // Action complete - close the menu so it can be re-opened on the next player turn.
    if (bIsOpen)
    {
        Close();
    }
}

// ==================== BLUEPRINT API ====================

void UCombatCommandMenuSubsystem::OpenForActor(AActor *Actor)
{
    ON_SCOPE_EXIT { BroadcastMenuDepthIfChanged(); RefreshTargetHighlight(); }; // B-399, B-400
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] OpenForActor: null actor"));
        return;
    }

    CurrentActor = Actor;

    ULoadoutComponent *LC = GetLoadoutComponent();
    if (!LC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] OpenForActor: no LoadoutComponent on %s"),
               *Actor->GetName());
        return;
    }

    UCharacterDataComponent *CDC = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CDC || !CDC->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] OpenForActor: no CharacterData on %s"),
               *Actor->GetName());
        return;
    }

    // Build capabilities - single LC query for entire turn
    RebuildCapabilities();

    // Live refresh: a BD's castable spell pool changes when it absorbs a new
    // element — bind OnAlignmentChanged so an open menu can rebuild.
    BindBDAbsorptionDelegate();

    CurrentDepth = ECombatMenuDepth::Main;
    bIsOpen = true;

    TArray<FPieMenuButtonData> Buttons = BuildMainMenuButtons();
    OnCommandMenuReady.Broadcast(Buttons);

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Opened for %s — %d buttons"),
           *Actor->GetName(), Buttons.Num());
}

void UCombatCommandMenuSubsystem::HandleSelection(const FPieMenuButtonData &ButtonData)
{
    NoteTurnActivity(); // B-453e: the player is choosing
    // B-399: a sub-menu or a picker opened. B-400: then the picker's highlight (first target on open, none on close).
    ON_SCOPE_EXIT { BroadcastMenuDepthIfChanged(); RefreshTargetHighlight(); };
    // A greyed button (D-083 broken source, empty pool) does nothing, whichever widget sent it.
    if (!bIsOpen || !ButtonData.bEnabled)
    {
        return;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Selected: %s (ID: %s)"),
           *ButtonData.DisplayName.ToString(),
           *ButtonData.ButtonID);

    switch (ButtonData.Category)
    {
        // === IMMEDIATE ACTIONS ===
    case EPieMenuCategory::Attack:
    {
        USkillDataBase *Attack = Cast<USkillDataBase>(ButtonData.DataReference);
        if (!Attack)
        {
            if (ULoadoutComponent *LC = GetLoadoutComponent())
            {
                Attack = LC->GetCurrentAttack();
            }
        }
        const ETargetType TT = Attack ? Attack->TargetType : ETargetType::Enemy;
        const ETargetCount TC = Attack ? Attack->TargetCount : ETargetCount::Single;
        OpenTargetSelection(EPieMenuCategory::Attack, ButtonData, TT, TC);
        break;
    }

    case EPieMenuCategory::Ability:
    {
        UAbilityData *Ability = Cast<UAbilityData>(ButtonData.DataReference);
        ETargetType TT = Ability ? Ability->TargetType : ETargetType::Enemy;
        ETargetCount TC = Ability ? Ability->TargetCount : ETargetCount::Single;
        OpenTargetSelection(EPieMenuCategory::Ability, ButtonData, TT, TC);
        break;
    }

    case EPieMenuCategory::Spell:
    {
        USpellData *Spell = Cast<USpellData>(ButtonData.DataReference);
        ETargetType TT = Spell ? Spell->TargetType : ETargetType::Enemy;
        ETargetCount TC = Spell ? Spell->TargetCount : ETargetCount::Single;
        OpenTargetSelection(EPieMenuCategory::Spell, ButtonData, TT, TC);
        break;
    }

    case EPieMenuCategory::Item:
    {
        UEvolutionItemData *Item = Cast<UEvolutionItemData>(ButtonData.DataReference);
        const ETargetType TT = Item ? Item->GetItemTargetType() : ETargetType::Anyone;
        // Items target a single recipient by default (no per-item count axis yet).
        OpenTargetSelection(EPieMenuCategory::Item, ButtonData, TT, ETargetCount::Single);
        break;
    }

    case EPieMenuCategory::Target:
    {
        if (ButtonData.Targets.Num() > 0)
        {
            // Group target — labelled confirm button carries the full array
            ConfirmActionWithTargets(ButtonData.Targets);
        }
        else
        {
            // Per-actor pick — existing single-target path
            AActor *Selected = Cast<AActor>(ButtonData.DataReference);
            ConfirmActionWithTarget(Selected);
        }
        break;
    }

    case EPieMenuCategory::TargetCategory:
    {
        // Allies / Enemies side picked — narrow to that team and show the
        // per-actor picker. The narrowed type is carried in DataIndex; the
        // narrowed pick is always a single recipient.
        ResolvedTargetType = static_cast<ETargetType>(ButtonData.DataIndex);
        ResolvedTargetCount = ETargetCount::Single;
        const TArray<AActor *> Targets = ResolveTargets(ResolvedTargetType);
        CurrentDepth = ECombatMenuDepth::TargetSelection;
        OnCommandMenuReady.Broadcast(BuildTargetButtons(Targets));
        break;
    }

    case EPieMenuCategory::Skills:
        OpenSkills();
        break;

    case EPieMenuCategory::Abilities:
        OpenAbilitySubmenu();
        break;

    case EPieMenuCategory::Items:
        OpenItemsSubmenu();
        break;

    case EPieMenuCategory::ResonateWeapon:
        OpenSpellSubmenu(EPieMenuCategory::ResonateWeapon);
        break;

    case EPieMenuCategory::Refractions:
    case EPieMenuCategory::Breakthrough:
    case EPieMenuCategory::ResonateRing:
        OpenSpellSubmenu(ButtonData.Category);
        break;

    case EPieMenuCategory::School:
    {
        // Build spell grid for selected school
        const TArray<USpellData *> &Spells =
            CurrentCapabilities.GetSpellsForCategory(ActiveSubmenuSource);
        TArray<FPieMenuButtonData> SpellButtons =
            BuildSpellButtons(Spells, ButtonData.School);
        ActiveSubmenuType = EPieMenuCategory::School; // B-436g: Back steps to the schools
        OnCommandMenuReady.Broadcast(SpellButtons);
        break;
    }

    case EPieMenuCategory::Flee:
        // No target; the fight ends on the next tick (D-091).
        SubmitConfirmedAction(ButtonData);
        break;

    case EPieMenuCategory::HandToAI:
    {
        // Close first: the AI takes this same turn as soon as the switch lands (D-026).
        AActor *Member = CurrentActor.Get();
        ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
        if (SendFreeCommandToServer(ECombatFreeCommand::HandToAI))
        {
            Close(); // B-232c: the server hands the member over
            break;
        }
        Close();
        if (Orchestrator)
        {
            Orchestrator->SwitchMemberControl(Member);
        }
        break;
    }

    case EPieMenuCategory::MoveRow:
        // B-588 (D-263): Position opens the picker; a picked row moves there. Free, like a weapon switch: the turn goes
        // on (D-096).
        if (ButtonData.DataIndex == POSITION_OPEN_INDEX)
        {
            OpenPositionPicker();
        }
        else
        {
            ExecuteMoveToRow(CombatRowHelpers::GetRowFromIndex(ButtonData.DataIndex));
        }
        break;

    case EPieMenuCategory::Defend:
        // No target to pick: Defend applies to the actor itself. No Close() here: Defend
        // executes synchronously, so HandleActionExecuted has already closed this menu and
        // the next player's turn may have reopened it by the time Submit returns.
        SubmitConfirmedAction(ButtonData);
        break;

    case EPieMenuCategory::EndTurn:
        // B-170e: the same path as any pick (a co-op client's goes to the server, B-229); the orchestrator ends the
        // turn at once (EndTurnByChoice), and SubmitConfirmedAction closes the menu once the turn has moved on.
        SubmitConfirmedAction(ButtonData);
        break;

    // === SWITCH ACTIONS ===
    case EPieMenuCategory::SwitchWeapon:
        ExecuteSwitchWeapon();
        break;

    case EPieMenuCategory::ChangeRing:
        ExecuteSwitchRing();
        break;

    case EPieMenuCategory::CycleSource:
    {
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            VFX->CycleToNextSource();
            if (PendingInfusionLevel > 0)
            {
                VFX->ActivateCurrentSource();
            }
        }
        // Re-broadcast picker with updated label — preserve original shape
        // (group confirm vs per-actor) via RebuildCurrentPicker.
        if (CurrentDepth == ECombatMenuDepth::TargetSelection)
        {
            OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
        }
        break;
    }

    case EPieMenuCategory::CycleLevel:
    {
        if (GetInfusionVFXComponent())
        {
            SetPendingInfusionLevel((PendingInfusionLevel + 1) % (InfusionConstants::MAX_INFUSION_LEVEL + 1));
        }
        // Re-broadcast picker with updated label — preserve original shape
        // (group confirm vs per-actor) via RebuildCurrentPicker.
        if (CurrentDepth == ECombatMenuDepth::TargetSelection)
        {
            OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
        }
        break;
    }

    // === B-592 (D-268): THE TARGET PANEL'S LINES ===
    case EPieMenuCategory::TargetPanelWeapon:
        // X opens the weapon line; X again switches to the next weapon (the line stays open).
        if (OpenTargetPanelLine == ETargetPanelLine::Weapon)
        {
            ExecuteSwitchWeapon();
            break;
        }
        OpenTargetPanelLine = ETargetPanelLine::Weapon;
        OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
        break;

    case EPieMenuCategory::TargetPanelInfusion:
        // Y opens the infusion lines (closing the weapon line); Y again closes them, back to the small panel.
        OpenTargetPanelLine = OpenTargetPanelLine == ETargetPanelLine::Infusion ? ETargetPanelLine::None : ETargetPanelLine::Infusion;
        OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
        break;

    // === NAVIGATION ===
    case EPieMenuCategory::Back:
        HandleBack();
        break;

    default:
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] Unhandled category: %d"),
               static_cast<int32>(ButtonData.Category));
        break;
    }
}

void UCombatCommandMenuSubsystem::HandleBack()
{
    NoteTurnActivity(); // B-453e
    ON_SCOPE_EXIT { BroadcastMenuDepthIfChanged(); RefreshTargetHighlight(); }; // B-399: cancel steps back; B-400
    if (!bIsOpen)
    {
        return;
    }

    switch (CurrentDepth)
    {
    case ECombatMenuDepth::TargetSelection:
        if (OpenTargetPanelLine != ETargetPanelLine::None)
        {
            // B-592 (D-268): Back closes the panel's open line first, back to the small panel.
            OpenTargetPanelLine = ETargetPanelLine::None;
            OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
        }
        else if (ResolvedTargetType != PendingTargetType)
        {
            // Entered the per-actor list via the Allies/Enemies category step —
            // Back steps out to that category view; the pending action stays alive.
            ResolvedTargetType = PendingTargetType;
            CurrentDepth = ECombatMenuDepth::TargetCategory;
            OnCommandMenuReady.Broadcast(BuildTargetCategoryButtons());
        }
        else
        {
            // Flat single-team / group picker — Back exits to the action menu.
            ExitTargetSelectionToActionMenu();
        }
        break;

    case ECombatMenuDepth::TargetCategory:
        // Back from the category step — exit to the action menu.
        ExitTargetSelectionToActionMenu();
        break;

    case ECombatMenuDepth::Submenu:
    {
        // B-436g: one step at a time: a school's spells to the schools, a kind to the kinds when there are several.
        if (ActiveSubmenuType == EPieMenuCategory::School)
        {
            OpenSpellSubmenu(ActiveSubmenuSource);
            break;
        }
        const TArray<FPieMenuButtonData> Kinds = BuildSkillKindButtons();
        const EPieMenuCategory Open = ActiveSubmenuType;
        if (Kinds.Num() > 1 &&
            Kinds.ContainsByPredicate([Open](const FPieMenuButtonData &Kind) { return Kind.Category == Open; }))
        {
            OpenSkillKinds();
            break;
        }
        CurrentDepth = ECombatMenuDepth::Main;
        ActiveSubmenuSource = EPieMenuCategory::None;
        ActiveSubmenuType = EPieMenuCategory::None; // NEW
        OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
        break;
    }

    case ECombatMenuDepth::Main:
        Close();
        break;

    default:
        break;
    }
}

void UCombatCommandMenuSubsystem::Close()
{
    bIsOpen = false;
    CurrentDepth = ECombatMenuDepth::Closed;
    ActiveSubmenuSource = EPieMenuCategory::None;
    RefreshTargetHighlight(); // B-400: the highlight clears while CurrentActor still names whose menu it was
    UnbindBDAbsorptionDelegate();
    CurrentActor.Reset();
    OnCommandMenuClosed.Broadcast();

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Closed"));
}

void UCombatCommandMenuSubsystem::BroadcastMenuDepthIfChanged()
{
    if (!bIsOpen)
    {
        return; // closed for the action: the camera holds until the next open says where it is
    }
    const int32 Depth = CommandMenuRules::CameraDepthFor(CurrentDepth, DepthBeforeTargetSelection);
    if (Depth == BroadcastMenuDepth)
    {
        return; // a redraw, an infusion step or a target cycle: the camera holds (study R3)
    }
    BroadcastMenuDepth = Depth;
    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Menu depth %d for %s"), Depth, *GetNameSafe(CurrentActor.Get()));
    OnMenuDepthChanged.Broadcast(CurrentActor.Get(), Depth);
}

// ==================== TARGET HIGHLIGHT (B-400) ====================

bool UCombatCommandMenuSubsystem::IsSingleTargetPickerOpen() const
{
    // The per-actor picker (RebuildCurrentPicker's BuildTargetButtons branch); Self and All confirm a group.
    return bIsOpen && CurrentDepth == ECombatMenuDepth::TargetSelection && ResolvedTargetType != ETargetType::Self &&
           ResolvedTargetCount != ETargetCount::All;
}

int32 UCombatCommandMenuSubsystem::IndexOfHighlightTarget(const AActor *Target) const
{
    return Target ? HighlightTargets.IndexOfByPredicate([Target](const TWeakObjectPtr<AActor> &Entry) { return Entry.Get() == Target; })
                  : INDEX_NONE;
}

void UCombatCommandMenuSubsystem::RefreshTargetHighlight()
{
    HighlightTargets.Reset();
    if (IsSingleTargetPickerOpen())
    {
        for (AActor *Target : ResolveTargets(ResolvedTargetType))
        {
            HighlightTargets.Add(Target);
        }
    }
    AActor *Keep = HighlightedTarget.Get();
    if (!Keep || IndexOfHighlightTarget(Keep) == INDEX_NONE)
    {
        // B-582 (D-266): the Attack picker opens on this fighter's last attack target while it's offered (alive);
        // otherwise the first target, as before.
        AActor *Last = PendingActionCategory == EPieMenuCategory::Attack ? GetLastAttackTarget(CurrentActor.Get()) : nullptr;
        Keep = IndexOfHighlightTarget(Last) != INDEX_NONE ? Last : (HighlightTargets.IsEmpty() ? nullptr : HighlightTargets[0].Get());
    }
    SetHighlightedTarget(Keep);
}

bool UCombatCommandMenuSubsystem::ConfirmHighlightedTarget()
{
    const AActor *Highlighted = IsSingleTargetPickerOpen() ? HighlightedTarget.Get() : nullptr;
    if (!Highlighted)
    {
        return false;
    }
    const TArray<FPieMenuButtonData> Rows = RebuildCurrentPicker();
    const FPieMenuButtonData *Row = Rows.FindByPredicate([Highlighted](const FPieMenuButtonData &Candidate)
                                                         { return Candidate.Category == EPieMenuCategory::Target && Candidate.DataReference == Highlighted &&
                                                                  Candidate.Targets.IsEmpty() && Candidate.bEnabled; });
    if (!Row)
    {
        return false;
    }
    const FPieMenuButtonData Pressed = *Row; // HandleSelection rebuilds the menu
    HandleSelection(Pressed);
    return true;
}

AActor *UCombatCommandMenuSubsystem::GetLastAttackTarget(const AActor *Fighter) const
{
    const TWeakObjectPtr<AActor> *Last = Fighter ? LastAttackTargets.Find(Fighter) : nullptr;
    return Last ? Last->Get() : nullptr;
}

void UCombatCommandMenuSubsystem::SetHighlightedTarget(AActor *Target)
{
    if (HighlightedTarget.Get() == Target)
    {
        return; // a redraw (an infusion step) or a hover on the target already highlighted
    }
    HighlightedTarget = Target;
    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Highlighted target %s (%d to pick from) for %s"), *GetNameSafe(Target),
           HighlightTargets.Num(), *GetNameSafe(CurrentActor.Get()));
    OnTargetHighlighted.Broadcast(CurrentActor.Get(), Target, HighlightTargets.Num());
}

void UCombatCommandMenuSubsystem::HighlightTarget(AActor *Target)
{
    if (Target && IsSingleTargetPickerOpen() && IndexOfHighlightTarget(Target) != INDEX_NONE)
    {
        SetHighlightedTarget(Target);
    }
}

void UCombatCommandMenuSubsystem::CycleHighlightedTarget(int32 Direction)
{
    NoteTurnActivity(); // B-453e
    if (!IsSingleTargetPickerOpen())
    {
        return;
    }
    // A target that died while the picker is open stays in the list until it rebuilds: skip it.
    int32 Index = IndexOfHighlightTarget(HighlightedTarget.Get());
    for (int32 Step = 0; Step < HighlightTargets.Num(); ++Step)
    {
        Index = CommandMenuRules::CycleIndex(Index, Direction, HighlightTargets.Num());
        if (Index == INDEX_NONE)
        {
            return;
        }
        if (AActor *Next = HighlightTargets[Index].Get(); IsValid(Next))
        {
            SetHighlightedTarget(Next);
            return;
        }
    }
}

// ==================== CAPABILITIES / BD LIVE REFRESH ====================

void UCombatCommandMenuSubsystem::RebuildCapabilities()
{
    ULoadoutComponent *LC = GetLoadoutComponent();
    UCharacterDataComponent *CDC =
        CurrentActor.IsValid() ? CurrentActor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    if (!LC || !CDC || !CDC->CharacterData)
    {
        return;
    }

    // Refraction tint: a BD paints with the pure-BD colour constant (its display
    // element is now Darkness, so derive the BD identity from IsBrokenDarkness());
    // everyone else uses their element's colour.
    const FLinearColor RefractionColor = CDC->IsBrokenDarkness()
        ? ElementColors::BrokenDarkness
        : GetElementColor(static_cast<int32>(CDC->GetDisplayElement()));

    CurrentCapabilities = FCombatCapabilities::BuildFrom(
        LC,
        CDC->CharacterData->CharacterClass,
        RefractionColor,
        [this](int32 ElementIndex)
        { return GetElementColor(ElementIndex); });
}

void UCombatCommandMenuSubsystem::RebroadcastCurrentView()
{
    switch (CurrentDepth)
    {
    case ECombatMenuDepth::Main:
        OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
        break;

    case ECombatMenuDepth::Submenu:
        BroadcastCurrentSubmenu();
        break;

    default:
        // Closed / TargetSelection — no refresh (an action is already chosen).
        break;
    }
}

void UCombatCommandMenuSubsystem::BindBDAbsorptionDelegate()
{
    UnbindBDAbsorptionDelegate();

    AActor *Actor = CurrentActor.Get();
    if (!Actor)
    {
        return;
    }

    // OnAlignmentChanged fires precisely when the absorbed element changes —
    // re-absorbing the same element does not move the active pool, so it does
    // not fire. That makes it the exact hook for "BD castable pool changed".
    if (UBrokenDarknessManager *BDManager = Actor->FindComponentByClass<UBrokenDarknessManager>())
    {
        BDManager->OnAlignmentChanged.AddDynamic(this, &UCombatCommandMenuSubsystem::HandleBDAlignmentChanged);
        BoundBDManager = BDManager;
    }
}

void UCombatCommandMenuSubsystem::UnbindBDAbsorptionDelegate()
{
    if (UBrokenDarknessManager *BDManager = BoundBDManager.Get())
    {
        BDManager->OnAlignmentChanged.RemoveDynamic(this, &UCombatCommandMenuSubsystem::HandleBDAlignmentChanged);
    }
    BoundBDManager.Reset();
}

void UCombatCommandMenuSubsystem::HandleBDAlignmentChanged(AActor *Actor, ESpellElement OldElement, ESpellElement NewElement)
{
    // A BD's absorbed element changed while its menu is open — the castable BD
    // spell pool (Darkness pool + the absorbed element's pool) is now
    // different. Rebuild capabilities and re-broadcast the current view.
    if (!bIsOpen || Actor != CurrentActor.Get())
    {
        return;
    }

    RebuildCapabilities();
    RebroadcastCurrentView();
}

// ==================== MAIN MENU ====================

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildMainMenuButtons() const
{
    TArray<FPieMenuButtonData> Buttons;

    if (IsCurrentActorStunned())
    {
        // D-001: stunned = basic attack or Defend only (ActionExecutor enforces the same rule).
        if (CurrentCapabilities.bCanAttack)
            Buttons.Add(CreateAttackButton());
        Buttons.Add(CreateDefendButton());
        return Buttons;
    }

    // B-436g (D-258, D-260): three rows that keep their places, greyed with why when there is nothing to pick, then
    // the actions the HUD draws as corner hints.
    FPieMenuButtonData Attack = CreateAttackButton();
    if (!CurrentCapabilities.bCanAttack)
    {
        Attack = FPieMenuButtonData::MakeDisabledButton(Attack.ButtonID, Attack.DisplayName,
                                                        NSLOCTEXT("CombatCommandMenu", "NoAttack", "No weapon to attack with."),
                                                        EPieMenuCategory::Attack);
    }
    Buttons.Add(Attack);

    const FText SkillsName = NSLOCTEXT("CombatCommandMenu", "Skills", "Skills");
    if (BuildSkillKindButtons().IsEmpty())
    {
        Buttons.Add(FPieMenuButtonData::MakeDisabledButton(TEXT("Skills"), SkillsName,
                                                           NSLOCTEXT("CombatCommandMenu", "NoSkills", "No skills to use."),
                                                           EPieMenuCategory::Skills));
    }
    else
    {
        Buttons.Add(FPieMenuButtonData(TEXT("Skills"), SkillsName, EPieMenuCategory::Skills));
    }

    FPieMenuButtonData Items = CreateItemsButton();
    if (!CurrentCapabilities.bHasItems)
    {
        Items = FPieMenuButtonData::MakeDisabledButton(Items.ButtonID, Items.DisplayName,
                                                       NSLOCTEXT("CombatCommandMenu", "NoItems", "No items to use."),
                                                       EPieMenuCategory::Items);
    }
    Buttons.Add(Items);

    if (CurrentCapabilities.bCanSwitchWeapon)
        Buttons.Add(CreateSwitchWeaponButton());
    if (CurrentCapabilities.bCanSwitchRing)
        Buttons.Add(CreateSwitchRingButton());
    Buttons.Add(CreatePositionButton()); // B-588 (D-263): replaces Step Forward and Step Back
    // B-170e; a stunned actor's menu stays Attack + Defend (D-001). The player's setting can hide it (Crown, 2026-11-12).
    if (ActionPipsText::ShouldShowEndTurnButton(this))
        Buttons.Add(CreateEndTurnButton());
    Buttons.Add(CreateFleeButton());
    Buttons.Add(CreateDefendButton()); // last: the hint nearest the corner (D-260)
    // OBSOLETE (B-346): no "Let AI Play" button; seats are fixed for the fight (Q-21-audit, PA-13).
    // The switch stays for debug (wor.Combat.SwitchControl).

    return Buttons;
}

// ==================== SKILLS (B-436g) ====================

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildSkillKindButtons() const
{
    TArray<FPieMenuButtonData> Kinds;
    if (CurrentCapabilities.bCanUseAbilities)
        Kinds.Add(CreateAbilitiesButton());
    if (CurrentCapabilities.bHasWeaponCrystal)
        Kinds.Add(CreateResonateWeaponButton());
    if (CurrentCapabilities.bHasRefractions)
        Kinds.Add(CreateRefractionsButton());
    if (CurrentCapabilities.bHasBreakthrough)
        Kinds.Add(CreateBreakthroughButton());
    if (CurrentCapabilities.ShouldShowResonateRing())
        Kinds.Add(CreateResonateRingButton());
    for (FPieMenuButtonData &Kind : Kinds)
    {
        Kind.Description = FText::GetEmpty(); // D-260 (Crown): names only, no counts
    }
    return Kinds;
}

void UCombatCommandMenuSubsystem::OpenSkills()
{
    const TArray<FPieMenuButtonData> Kinds = BuildSkillKindButtons();
    if (Kinds.Num() > 1)
    {
        OpenSkillKinds();
    }
    else if (Kinds.Num() == 1)
    {
        if (Kinds[0].Category == EPieMenuCategory::Abilities)
            OpenAbilitySubmenu();
        else
            OpenSpellSubmenu(Kinds[0].Category);
    }
}

void UCombatCommandMenuSubsystem::OpenSkillKinds()
{
    CurrentDepth = ECombatMenuDepth::Submenu;
    ActiveSubmenuType = EPieMenuCategory::Skills;
    ActiveSubmenuSource = EPieMenuCategory::None;
    BroadcastCurrentSubmenu();
}

bool UCombatCommandMenuSubsystem::BroadcastCurrentSubmenu()
{
    if (ActiveSubmenuType == EPieMenuCategory::School)
    {
        ActiveSubmenuType = ActiveSubmenuSource; // the picked school isn't kept: its schools
    }
    switch (ActiveSubmenuType)
    {
    case EPieMenuCategory::Skills:
    {
        TArray<FPieMenuButtonData> Kinds = BuildSkillKindButtons();
        Kinds.Add(FPieMenuButtonData::MakeBackButton());
        OnCommandMenuReady.Broadcast(Kinds);
        return true;
    }
    case EPieMenuCategory::Abilities:
        OnCommandMenuReady.Broadcast(BuildAbilitySubmenu());
        return true;
    case EPieMenuCategory::Items:
        OnCommandMenuReady.Broadcast(BuildItemsSubmenu());
        return true;
    case EPieMenuCategory::MoveRow:
        OnCommandMenuReady.Broadcast(BuildPositionButtons()); // B-588
        return true;
    case EPieMenuCategory::Refractions:
    case EPieMenuCategory::Breakthrough:
    case EPieMenuCategory::ResonateWeapon:
    case EPieMenuCategory::ResonateRing:
        OnCommandMenuReady.Broadcast(BuildSchoolButtons(CurrentCapabilities.GetSpellsForCategory(ActiveSubmenuSource)));
        return true;
    default:
        return false;
    }
}

FText UCombatCommandMenuSubsystem::GetViewTitle() const
{
    if (CurrentDepth == ECombatMenuDepth::TargetSelection || CurrentDepth == ECombatMenuDepth::TargetCategory)
    {
        // B-592 (D-268): the panel's open line names the step (refine_v4).
        const bool bTargets = CurrentDepth == ECombatMenuDepth::TargetSelection;
        if (bTargets && OpenTargetPanelLine == ETargetPanelLine::Weapon)
        {
            return FText::Format(NSLOCTEXT("CombatCommandMenu", "TitleWeapon", "{0} · weapon"), PendingActionLabel);
        }
        if (bTargets && OpenTargetPanelLine == ETargetPanelLine::Infusion)
        {
            return FText::Format(NSLOCTEXT("CombatCommandMenu", "TitleInfusion", "{0} · infusion"), PendingActionLabel);
        }
        return FText::Format(NSLOCTEXT("CombatCommandMenu", "TitleTargets", "{0} · choose a target"), PendingActionLabel);
    }
    if (CurrentDepth != ECombatMenuDepth::Submenu)
    {
        return FText::GetEmpty();
    }
    if (ActiveSubmenuType == EPieMenuCategory::Items)
    {
        return NSLOCTEXT("CombatCommandMenu", "TitleItems", "Items");
    }
    if (ActiveSubmenuType == EPieMenuCategory::MoveRow)
    {
        return NSLOCTEXT("CombatCommandMenu", "Position", "Position"); // B-588
    }
    const FText Skills = NSLOCTEXT("CombatCommandMenu", "Skills", "Skills");
    const EPieMenuCategory KindShown = ActiveSubmenuType == EPieMenuCategory::School ? ActiveSubmenuSource : ActiveSubmenuType;
    for (const FPieMenuButtonData &Kind : BuildSkillKindButtons())
    {
        if (Kind.Category == KindShown)
        {
            return FText::Format(NSLOCTEXT("CombatCommandMenu", "TitleSkillKind", "{0} > {1}"), Skills, Kind.DisplayName);
        }
    }
    return Skills;
}

// ==================== BUTTON CREATORS ====================

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateAttackButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Attack");
    Button.DisplayName = FText::FromString(TEXT("Attack"));
    Button.Description = FText::FromString(CurrentCapabilities.ActiveWeaponName);
    Button.Category = EPieMenuCategory::Attack;
    Button.bEnabled = true;
    ShowActionCost(Button); // B-170e
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateAbilitiesButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Abilities");
    Button.DisplayName = FText::FromString(TEXT("Abilities"));
    Button.Description = FText::FromString(
        FString::Printf(TEXT("%d abilities"), CurrentCapabilities.WeaponAbilities.Num()));
    Button.Category = EPieMenuCategory::Abilities;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateResonateWeaponButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("ResonateWeapon");
    Button.DisplayName = FText::FromString(TEXT("Resonate (W)"));
    Button.Description = FText::FromString(
        FString::Printf(TEXT("%d spells"), CurrentCapabilities.WeaponCrystalSpells.Num()));
    Button.ButtonTint = CurrentCapabilities.WeaponCrystalColor;
    Button.Category = EPieMenuCategory::ResonateWeapon;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateRefractionsButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Refractions");
    Button.DisplayName = FText::FromString(TEXT("Refractions"));
    Button.Description = FText::FromString(
        FString::Printf(TEXT("%d spells"), CurrentCapabilities.RefractionSpells.Num()));
    Button.Category = EPieMenuCategory::Refractions;
    Button.ButtonTint = CurrentCapabilities.RefractionColor;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateBreakthroughButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Breakthrough");
    Button.DisplayName = FText::FromString(TEXT("Breakthrough"));
    Button.Description = FText::FromString(
        FString::Printf(TEXT("%d spells"), CurrentCapabilities.BreakthroughSpells.Num()));
    Button.Category = EPieMenuCategory::Breakthrough;
    Button.ButtonTint = CurrentCapabilities.BreakthroughColor;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateResonateRingButton() const
{
    const FString BreakText = CurrentCapabilities.bRingHasBreakChance
                                  ? TEXT(" (break chance)")
                                  : TEXT("");

    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("ResonateRing");
    Button.DisplayName = NSLOCTEXT("CombatCommandMenu", "ResonateAdapter", "Resonate (A)"); // D-107: A for adapter
    Button.Description = FText::FromString(
        FString::Printf(TEXT("%s — %d spells%s"),
                        *CurrentCapabilities.ActiveRingName,
                        CurrentCapabilities.RingSpells.Num(),
                        *BreakText));
    Button.Category = EPieMenuCategory::ResonateRing;
    Button.ButtonTint = CurrentCapabilities.RingColor;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateItemsButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Items");
    Button.DisplayName = FText::FromString(TEXT("Items"));
    Button.Category = EPieMenuCategory::Items;
    Button.bEnabled = true;
    return Button;
}

void UCombatCommandMenuSubsystem::GreyIfTurnSwitchUsed(FPieMenuButtonData &Button) const
{
    const ULoadoutComponent *LC = GetLoadoutComponent();
    const FText Reason = LC ? LC->GetTurnSwitchRefusedReason() : FText::GetEmpty();
    if (Reason.IsEmpty())
    {
        return;
    }
    const FPieMenuButtonData Greyed = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Button.DisplayName, Reason);
    Button.Description = Greyed.Description;
    Button.ButtonTint = Greyed.ButtonTint;
    Button.bEnabled = false;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateSwitchWeaponButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("SwitchWeapon");
    Button.DisplayName = FText::FromString(TEXT("Switch Weapon"));
    Button.Category = EPieMenuCategory::SwitchWeapon;
    Button.bEnabled = true;
    GreyIfTurnSwitchUsed(Button);
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateDefendButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Defend");
    Button.DisplayName = FText::FromString(TEXT("Defend"));
    Button.Category = EPieMenuCategory::Defend;
    Button.bEnabled = true;
    Button.Description = ActionPipsText::GetDefendEndsTurnText(); // B-170e (Q-170c): Defend ends the turn
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateEndTurnButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("EndTurn");
    Button.DisplayName = WoRGameNames::GetEndTurnVerb();
    Button.Description = ActionPipsText::GetEndTurnDescription();
    Button.Category = EPieMenuCategory::EndTurn;
    Button.bEnabled = true;
    return Button;
}

void UCombatCommandMenuSubsystem::ShowActionCost(FPieMenuButtonData &Button) const
{
    const UCharacterDataComponent *CDC = CurrentActor.IsValid() ? CurrentActor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    if (!CDC)
    {
        return;
    }
    // The cost of the action this button submits (FAction::GetActionCost, B-170c), so the pips and the pay agree.
    const int32 Cost = BuildActionFromButton(Button).GetActionCost();
    Button.ActionCost = Cost;
    const FText Reason = ActionPipsText::GetCostRefusedReason(Cost, CDC->CurrentActions, CDC->GetMaxActions());
    if (Reason.IsEmpty() || !Button.bEnabled)
    {
        return;
    }
    const FPieMenuButtonData Greyed = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Button.DisplayName, Reason);
    Button.Description = Greyed.Description;
    Button.ButtonTint = Greyed.ButtonTint;
    Button.bEnabled = false;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateFleeButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("Flee");
    Button.DisplayName = NSLOCTEXT("CombatCommandMenu", "Flee", "Flee");
    Button.Category = EPieMenuCategory::Flee;
    Button.bEnabled = true;

    // D-091: greyed with the reason in a mini-boss or boss fight.
    const ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    const FText Reason = Orchestrator ? Orchestrator->GetFleeRefusedReason(CurrentActor.Get()) : FText::GetEmpty();
    if (!Reason.IsEmpty())
    {
        const FPieMenuButtonData Greyed = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Button.DisplayName, Reason);
        Button.Description = Greyed.Description;
        Button.ButtonTint = Greyed.ButtonTint;
        Button.bEnabled = false;
    }
    return Button;
}

void UCombatCommandMenuSubsystem::NoteTurnActivity() const
{
    if (ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get())
    {
        Orchestrator->NoteTurnActivity(); // B-453e: the idle clock starts again
    }
}

bool UCombatCommandMenuSubsystem::CanHandToAI() const
{
    const ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    return Orchestrator && Orchestrator->GetControlSwitchRefusedReason(CurrentActor.Get()).IsEmpty();
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateHandToAIButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("HandToAI");
    Button.DisplayName = NSLOCTEXT("CombatCommandMenu", "HandToAI", "Auto");
    Button.Category = EPieMenuCategory::HandToAI;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateMoveRowButton(bool bForward) const
{
    FPieMenuButtonData Button;
    Button.ButtonID = bForward ? TEXT("StepForward") : TEXT("StepBack");
    Button.DisplayName = bForward ? NSLOCTEXT("CombatCommandMenu", "StepForward", "Step Forward")
                                  : NSLOCTEXT("CombatCommandMenu", "StepBack", "Step Back");
    Button.Category = EPieMenuCategory::MoveRow;
    Button.DataIndex = bForward ? MOVE_ROW_FORWARD_INDEX : MOVE_ROW_BACK_INDEX;
    Button.bEnabled = true;

    const ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    FText Reason = Orchestrator ? Orchestrator->GetRowMoveRefusedReason(CurrentActor.Get(), bForward) : FText::GetEmpty();
    // B-230b: a client has no orchestrator; its member's replicated row greys the step it can't take (the server
    // still checks the rest: the step already used, a stun).
    FCombatGridPosition Position;
    const UCombatGridSubsystem *Grid = Orchestrator ? nullptr : WoRFight::Get<UCombatGridSubsystem>(this);
    if (Grid && Grid->GetActorPosition(CurrentActor.Get(), Position))
    {
        Reason = ACombatOrchestrator::GetRowEdgeRefusal(Position.Row, bForward); // review: one text for both
    }
    if (!Reason.IsEmpty())
    {
        const FPieMenuButtonData Greyed = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Button.DisplayName, Reason);
        Button.Description = Greyed.Description;
        Button.ButtonTint = Greyed.ButtonTint;
        Button.bEnabled = false;
    }
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreatePositionButton() const
{
    FPieMenuButtonData Button(TEXT("Position"), NSLOCTEXT("CombatCommandMenu", "Position", "Position"), EPieMenuCategory::MoveRow);
    Button.DataIndex = POSITION_OPEN_INDEX;
    // Greyed when no row can be taken (the move used, a stun), with the first refused row's reason.
    FText Reason;
    for (const FPieMenuButtonData &Row : BuildPositionButtons())
    {
        if (Row.Category != EPieMenuCategory::MoveRow)
        {
            continue;
        }
        if (Row.bEnabled)
        {
            return Button;
        }
        if (Reason.IsEmpty() && Row.ButtonID != TEXT("PositionHere"))
        {
            Reason = Row.Description;
        }
    }
    FPieMenuButtonData Greyed = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Button.DisplayName, Reason, EPieMenuCategory::MoveRow);
    Greyed.DataIndex = POSITION_OPEN_INDEX;
    return Greyed;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildPositionButtons() const
{
    const ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    const UCombatGridSubsystem *Grid = WoRFight::Get<UCombatGridSubsystem>(this);
    FCombatGridPosition Position;
    const bool bOnGrid = Grid && Grid->GetActorPosition(CurrentActor.Get(), Position); // a client's is replicated (B-230b)

    TArray<FPieMenuButtonData> Buttons;
    for (const ECombatRow Row : POSITION_PICKER_ROWS)
    {
        const FText Name = WoRGameNames::GetCombatRowName(Row);
        FPieMenuButtonData Button(TEXT("PositionRow"), Name, EPieMenuCategory::MoveRow);
        FText Reason;
        if (bOnGrid && Position.Row == Row)
        {
            Button.ButtonID = TEXT("PositionHere"); // the current row, marked (D-263)
            Reason = NSLOCTEXT("CombatCommandMenu", "PositionHere", "You are here.");
        }
        else if (Orchestrator)
        {
            Reason = Orchestrator->GetMoveToRowRefusedReason(CurrentActor.Get(), Row);
        }
        if (!Reason.IsEmpty())
        {
            Button = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Name, Reason, EPieMenuCategory::MoveRow);
        }
        Button.DataIndex = CombatRowHelpers::GetRowIndex(Row);
        Buttons.Add(Button);
    }
    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateSwitchRingButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("SwitchRing");
    Button.DisplayName = NSLOCTEXT("CombatCommandMenu", "SwitchAdapter", "Switch Adapter"); // D-107
    Button.Category = EPieMenuCategory::ChangeRing;
    Button.bEnabled = true;
    GreyIfTurnSwitchUsed(Button);
    return Button;
}

// ==================== SUBMENU BUILDERS ====================

void UCombatCommandMenuSubsystem::OpenAbilitySubmenu()
{
    CurrentDepth = ECombatMenuDepth::Submenu;
    ActiveSubmenuType = EPieMenuCategory::Abilities; // NEW
    TArray<FPieMenuButtonData> AbilityButtons = BuildAbilitySubmenu();
    OnCommandMenuReady.Broadcast(AbilityButtons);
}

void UCombatCommandMenuSubsystem::OpenItemsSubmenu()
{
    CurrentDepth = ECombatMenuDepth::Submenu;
    ActiveSubmenuType = EPieMenuCategory::Items; // NEW
    TArray<FPieMenuButtonData> ItemButtons = BuildItemsSubmenu();
    OnCommandMenuReady.Broadcast(ItemButtons);
}

void UCombatCommandMenuSubsystem::OpenSpellSubmenu(EPieMenuCategory Source)
{
    ActiveSubmenuSource = Source;
    ActiveSubmenuType = Source;
    CurrentDepth = ECombatMenuDepth::Submenu;

    const TArray<USpellData *> &Spells = CurrentCapabilities.GetSpellsForCategory(Source);
    TArray<FPieMenuButtonData> SchoolButtons = BuildSchoolButtons(Spells);
    OnCommandMenuReady.Broadcast(SchoolButtons);
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildAbilitySubmenu() const
{
    TArray<FPieMenuButtonData> Buttons = BuildAbilityButtonsFrom(CurrentCapabilities.WeaponAbilities);
    // B-474 (D-207): while "Switch to the weapon of the skill you use" is on, the other weapon's skills follow, named
    // with their weapon; using one switches to it (UActionExecutor -> ULoadoutComponent::SwitchToWeaponOfSkill).
    const USaveSubsystem *Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if (!Save || !Save->IsSwitchToSkillWeaponOn() || CurrentCapabilities.OtherWeaponAbilities.Num() == 0)
    {
        return Buttons;
    }
    TArray<FPieMenuButtonData> Other = BuildAbilityButtonsFrom(CurrentCapabilities.OtherWeaponAbilities);
    Other.Pop(); // its own Back button: the list keeps one, last
    const FText WeaponName = FText::FromString(CurrentCapabilities.OtherWeaponName);
    for (FPieMenuButtonData &Button : Other)
    {
        Button.ButtonID = TEXT("Other") + Button.ButtonID;
        Button.DisplayName = FText::Format(NSLOCTEXT("CombatCommandMenu", "OtherWeaponSkill", "{0} ({1})"), Button.DisplayName, WeaponName);
    }
    Buttons.Insert(Other, FMath::Max(0, Buttons.Num() - 1));
    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildAbilityButtonsFrom(
    const TArray<UAbilityData *> &Abilities) const
{
    TArray<FPieMenuButtonData> Buttons;

    for (int32 i = 0; i < Abilities.Num(); i++)
    {
        UAbilityData *Ability = Abilities[i];
        if (!Ability)
            continue;

        FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
            FString::Printf(TEXT("Ability_%d"), i),
            FText::FromString(Ability->Name),
            EPieMenuCategory::Ability,
            Ability,
            i);

        Button.bEnabled = true;
        ShowActionCost(Button); // B-170e
        Buttons.Add(Button);
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildItemsSubmenu() const
{
    TArray<FPieMenuButtonData> Buttons;

    // B-313 (D-111, D-117): the member's usable items (its pouch; an actor with no pouch, its loadout's
    // item slots), with how many this fight can still use.
    ULoadoutComponent *LC = GetLoadoutComponent();
    if (LC)
    {
        const TArray<FItemLoadoutSlot> Items = LC->GetUsableItems();

        for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
        {
            const FItemLoadoutSlot &Item = Items[ItemIndex];
            const FText DisplayName = FText::FromString(ItemIdentity::GetDisplayName(Item.CrystalId));

            // Carry the index into GetUsableItems (DataIndex), not a UObject* — consumables have
            // no asset identity; the click handler reads the live CrystalId from that list.
            FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
                FString::Printf(TEXT("Item_%d"), ItemIndex),
                DisplayName,
                EPieMenuCategory::Item,
                nullptr,
                ItemIndex);

            Button.Quantity = Item.Quantity; // D-260: drawn beside the name
            const bool bPlainStone = CrystalTypeHelpers::IsAugmentStoneType(Item.CrystalId.Type) ||
                                     CrystalTypeHelpers::IsPotionType(Item.CrystalId.Type);
            FWoRIcon Gem; // the backpack's icon for the same stack (B-584)
            Gem.Kind = bPlainStone ? EWoRIconKind::Stone : EWoRIconKind::LooseGem;
            Gem.Element = ItemIdentity::GetElement(Item.CrystalId);
            Button.Icon = WoRIcons::Find(Gem);
            Button.bEnabled = true;
            ShowActionCost(Button); // B-170e
            Buttons.Add(Button);
        }
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

namespace
{
    /** A spell's menu school from its shown school. The two enums' orders differ (the old int cast put
     *  Enhancement spells under Conjuration, B-356); B-374 (D-163): one menu school per spell school. */
    EPieMenuSpellSchool ToPieMenuSchool(const USpellData &Spell)
    {
        switch (Spell.GetSchool())
        {
        case ESpellSchool::Destruction:
            return EPieMenuSpellSchool::Destruction;
        case ESpellSchool::Enhancement:
            return EPieMenuSpellSchool::Enhancement;
        case ESpellSchool::Restoration:
            return EPieMenuSpellSchool::Restoration;
        case ESpellSchool::Manipulation:
            return EPieMenuSpellSchool::Manipulation;
        case ESpellSchool::Impairment:
            return EPieMenuSpellSchool::Impairment;
        case ESpellSchool::Conjuration:
        default:
            return EPieMenuSpellSchool::Conjuration;
        }
    }

    ESpellSchool ToSpellSchool(EPieMenuSpellSchool School)
    {
        switch (School)
        {
        case EPieMenuSpellSchool::Destruction:
            return ESpellSchool::Destruction;
        case EPieMenuSpellSchool::Enhancement:
            return ESpellSchool::Enhancement;
        case EPieMenuSpellSchool::Restoration:
            return ESpellSchool::Restoration;
        case EPieMenuSpellSchool::Manipulation:
            return ESpellSchool::Manipulation;
        case EPieMenuSpellSchool::Impairment:
            return ESpellSchool::Impairment;
        case EPieMenuSpellSchool::Conjuration:
        default:
            return ESpellSchool::Conjuration;
        }
    }
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildSchoolButtons(const TArray<USpellData *> &Spells)
{
    TArray<FPieMenuButtonData> Buttons;

    TMap<EPieMenuSpellSchool, int32> SchoolCounts;
    for (USpellData *Spell : Spells)
    {
        if (!Spell)
            continue;
        SchoolCounts.FindOrAdd(ToPieMenuSchool(*Spell))++;
    }

    // B-374: a button for each school that has spells, in the enum's order so the radial layout is stable.
    const UEnum *SchoolEnum = StaticEnum<EPieMenuSpellSchool>();
    for (int32 Index = 0; Index < SchoolEnum->NumEnums() - 1; ++Index) // minus the generated _MAX
    {
        const EPieMenuSpellSchool School = static_cast<EPieMenuSpellSchool>(SchoolEnum->GetValueByIndex(Index));
        const int32 *Count = SchoolCounts.Find(School);
        if (!Count)
            continue;
        FPieMenuButtonData Button = FPieMenuButtonData::MakeSchoolButton(School, *Count);
        // B-356: the school's name from the LOCTEXT name table (D-061 / D-062).
        Button.DisplayName = WoRGameNames::GetSpellSchoolName(ToSpellSchool(School));
        Button.Description = FText::Format(NSLOCTEXT("CombatCommandMenu", "SchoolSpellCount", "{0} {0}|plural(one=spell,other=spells)"), *Count);
        Buttons.Add(Button);
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildSpellButtons(
    const TArray<USpellData *> &Spells, EPieMenuSpellSchool School) const
{
    TArray<FPieMenuButtonData> Buttons;
    int32 Index = 0;

    for (USpellData *Spell : Spells)
    {
        if (!Spell)
            continue;

        const EPieMenuSpellSchool SpellSchool = ToPieMenuSchool(*Spell);

        if (SpellSchool != School)
            continue;
        if (Index >= 6)
            break; // Max 6 per school

        // Generic spells display (and tint) by their RESOLVED source element ("Fire Ball").
        // Resolve once via the executor (Generic -> source/pool element); fixed-element spells
        // pass through unchanged (ResolveSpellCastElement returns their authored element).
        ESpellElement ResolvedElement = Spell->Element;
        bool bIsBD = false;
        if (CurrentActor.IsValid())
        {
            if (UActionExecutor *Exec = WoRFight::Get<UActionExecutor>(this))
            {
                ResolvedElement = Exec->ResolveSpellCastElement(CurrentActor.Get(), Spell);
            }
            if (UCharacterDataComponent *CDC = CurrentActor->FindComponentByClass<UCharacterDataComponent>())
            {
                bIsBD = CDC->IsBrokenDarkness();
            }
        }

        FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
            FString::Printf(TEXT("Spell_%d"), Index),
            FText::FromString(Spell->GetDisplayName(ResolvedElement, bIsBD)),
            EPieMenuCategory::Spell,
            Spell,
            Index);

        Button.Description = FText::FromString(Spell->Description);
        Button.ButtonTint = GetElementColor(static_cast<int32>(ResolvedElement));
        Button.bEnabled = true;

        // D-083: a spell whose source broke stays in the menu, greyed out with the reason.
        const FText BlockedReason = CurrentCapabilities.GetBlockedReason(Spell);
        if (!BlockedReason.IsEmpty())
        {
            const FPieMenuButtonData Greyed = FPieMenuButtonData::MakeDisabledButton(Button.ButtonID, Button.DisplayName, BlockedReason);
            Button.Description = Greyed.Description;
            Button.ButtonTint = Greyed.ButtonTint;
            Button.bEnabled = false;
        }
        ShowActionCost(Button); // B-170e: after D-083, so a broken source keeps its own reason
        Buttons.Add(Button);
        Index++;
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

// ==================== SELECTION HANDLERS ====================

void UCombatCommandMenuSubsystem::ExecuteSwitchWeapon()
{
    if (!CurrentActor.IsValid() || SendFreeCommandToServer(ECombatFreeCommand::SwitchWeapon))
        return; // B-232c: a client's switch runs on the server; HandleFreeCommandApplied redraws

    ULoadoutComponent *LC = GetLoadoutComponent();
    if (!LC)
        return;

    FText RefusedReason;
    if (!LC->TrySwitchActiveSlot(EGearKind::Weapon, RefusedReason))
    {
        return;
    }

    // Rebuild capabilities with the new active weapon and refresh the menu (B-592: the Attack step stays open)
    RebuildCapabilities();
    BroadcastAfterWeaponSwitch();
}

void UCombatCommandMenuSubsystem::ExecuteMoveRow(bool bForward)
{
    if (SendFreeCommandToServer(bForward ? ECombatFreeCommand::MoveRowForward : ECombatFreeCommand::MoveRowBack))
    {
        return; // B-232c
    }
    ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    FText RefusedReason;
    if (!Orchestrator || !Orchestrator->TryMoveRow(CurrentActor.Get(), bForward, RefusedReason))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Row step refused: %s"),
               Orchestrator ? *RefusedReason.ToString() : TEXT("no orchestrator"));
        return;
    }
    // The new row greys the step that is no longer possible.
    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

void UCombatCommandMenuSubsystem::OpenPositionPicker()
{
    CurrentDepth = ECombatMenuDepth::Submenu;
    ActiveSubmenuType = EPieMenuCategory::MoveRow;
    ActiveSubmenuSource = EPieMenuCategory::None;
    BroadcastCurrentSubmenu();
}

void UCombatCommandMenuSubsystem::ExecuteMoveToRow(ECombatRow Row)
{
    // B-232c: a client's move runs on the server (HandleFreeCommandApplied redraws); the host and solo run it here.
    // TODO: B-232d: a client's refused move gives no feedback (as the old step), and its Position stays lit.
    if (!SendFreeCommandToServer(CombatFreeCommands::MoveTo(Row)))
    {
        ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
        FText RefusedReason;
        if (!Orchestrator || !Orchestrator->TryMoveToRow(CurrentActor.Get(), Row, RefusedReason))
        {
            UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Move to the %s row refused: %s"), *CombatRowHelpers::GetRowName(Row),
                   Orchestrator ? *RefusedReason.ToString() : TEXT("no orchestrator"));
            BroadcastCurrentSubmenu(); // the picker again, with the reason on the row
            return;
        }
    }
    // The move is spent (or on its way): the main menu, Position greyed.
    CurrentDepth = ECombatMenuDepth::Main;
    ActiveSubmenuType = EPieMenuCategory::None;
    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

void UCombatCommandMenuSubsystem::ExecuteSwitchRing()
{
    if (!CurrentActor.IsValid() || SendFreeCommandToServer(ECombatFreeCommand::SwitchAdapter))
        return; // B-232c

    ULoadoutComponent *LC = GetLoadoutComponent();
    if (!LC)
        return;

    // The next adapter, wrapping; one adapter (or a used switch) changes nothing.
    FText RefusedReason;
    if (!LC->TrySwitchActiveSlot(EGearKind::Adapter, RefusedReason))
    {
        return;
    }

    // Rebuild capabilities with new active ring
    RebuildCapabilities();

    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

bool UCombatCommandMenuSubsystem::SendFreeCommandToServer(ECombatFreeCommand Command)
{
    const UWorld *World = GetWorld();
    ACombatPlayerController *PC = World && World->GetNetMode() == NM_Client ? ACombatPlayerController::FindLocal(this) : nullptr;
    if (!PC || !CurrentActor.IsValid())
    {
        return false; // the host and solo run the step here, as before
    }
    PC->RequestFreeCommand(CurrentActor.Get(), Command);
    return true;
}

void UCombatCommandMenuSubsystem::HandleFreeCommandApplied(AActor *Actor, ECombatFreeCommand Command)
{
    if (!bIsOpen || Actor != CurrentActor.Get())
    {
        return; // the menu moved on (or closed for the hand-off)
    }
    if (Command == ECombatFreeCommand::SwitchWeapon || Command == ECombatFreeCommand::SwitchAdapter)
    {
        RebuildCapabilities();
    }
    if (Command == ECombatFreeCommand::SwitchWeapon)
    {
        BroadcastAfterWeaponSwitch(); // B-592: a client's X in the Attack step stays there too
        return;
    }
    // TODO: B-232d: the row lives on the server's orchestrator; the client's greyed row step can lag until then.
    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

// ==================== TARGET SELECTION ====================

void UCombatCommandMenuSubsystem::OpenTargetSelection(
    EPieMenuCategory ActionCategory,
    const FPieMenuButtonData &ActionButton,
    ETargetType TargetType,
    ETargetCount TargetCount)
{
    // Reset immunity on every entry — guarantees a clean VFX state regardless
    // of how the previous interaction exited (back, commit, combat end, death).
    if (UInfusionVFXComponent *EntryVFX = GetInfusionVFXComponent())
    {
        EntryVFX->SetImmuneToInfusion(false);
    }

    // Stash the pending action
    PendingActionCategory = ActionCategory;
    PendingActionLabel = ActionButton.DisplayName;
    PendingActionID = ActionButton.ButtonID;
    PendingActionData = ActionButton.DataReference;
    PendingActionDataIndex = ActionButton.DataIndex; // carry slot index (consumables)
    PendingTargetType = TargetType;
    PendingTargetCount = TargetCount;
    // Synced on entry — diverges only once a side is picked in the category step.
    ResolvedTargetType = TargetType;
    ResolvedTargetCount = TargetCount;
    DepthBeforeTargetSelection = CurrentDepth;
    OpenTargetPanelLine = ETargetPanelLine::None; // B-592: every target step opens small

    // Log target type
    const UEnum *TargetEnum = StaticEnum<ETargetType>();
    FString TargetTypeName = TargetEnum
                                 ? TargetEnum->GetNameStringByValue(static_cast<int64>(TargetType))
                                 : TEXT("Unknown");
    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] OpenTargetSelection: TargetType=%s"),
           *TargetTypeName);

    // Reset infusion state for this picker. Per locked design (p2),
    // every action's picker opens at Level 0 with a fresh source cache.
    // For spell actions, also lock the VFX source to the spell's intrinsic
    // source so cycling level activates the correct element.
    // Items skip infusion entirely.
    if (ActionCategory != EPieMenuCategory::Item)
    {
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            VFX->SetImmuneToInfusion(IsInfusionLockedForPendingAction());

            VFX->CacheAvailableSources(); // refresh available sources
            SetPendingInfusionLevel(0);   // reset to L0 (no VFX)

            // Breakthrough spells are the only spell submenu that exposes a
            // CycleSource button. 6-5-f: narrow the cache to EXACTLY the sources the
            // spell's origin allows (GetAllowedInfusionSourcesForSpell) — one source
            // of truth shared with ValidateAction, so the UI can never offer a source
            // the validator would reject. Plain Caster evolution spell -> {Evolution}
            // (size 1 -> cycle button auto-hides); BD/Reality -> {Evolution, Innate}
            // (cycle stays, the documented exception). Other spell submenus stay
            // element-locked (no cycle UI), so no filtering needed for them.
            if (ActionCategory == EPieMenuCategory::Spell &&
                ActiveSubmenuSource == EPieMenuCategory::Breakthrough)
            {
                USpellData *BreakthroughSpell = Cast<USpellData>(PendingActionData.Get());
                UActionExecutor *Exec = WoRFight::Get<UActionExecutor>(this);
                if (Exec && BreakthroughSpell && CurrentActor.IsValid())
                {
                    VFX->RestrictCachedSources(
                        Exec->GetAllowedInfusionSourcesForSpell(CurrentActor.Get(), BreakthroughSpell));
                }
                else
                {
                    // Fallback (executor/spell/actor unreachable): the prior hardcoded subset.
                    VFX->RestrictCachedSources({
                        EInfusionSourceOption::Evolution,
                        EInfusionSourceOption::Innate});
                }
            }

            // For Spell action category, source is intrinsic — read from
            // ActiveSubmenuSource (set by OpenSpellSubmenu) to know which
            // spell submenu the player navigated through.
            const EPieMenuCategory SourceLookup =
                (ActionCategory == EPieMenuCategory::Spell)
                    ? ActiveSubmenuSource
                    : ActionCategory;
            EInfusionSourceOption SpellSource = EInfusionSourceOption::None;
            bool bIsSpell = true;

            switch (SourceLookup)
            {
            case EPieMenuCategory::Refractions:
                SpellSource = EInfusionSourceOption::Innate;
                break;
            case EPieMenuCategory::ResonateWeapon:
                SpellSource = EInfusionSourceOption::WeaponCrystal;
                break;
            case EPieMenuCategory::ResonateRing:
                SpellSource = (CurrentCapabilities.CharacterClass == ECharacterClass::Resonator)
                                  ? EInfusionSourceOption::ActiveRing
                                  : EInfusionSourceOption::PrimaryRing;
                break;
            case EPieMenuCategory::Breakthrough:
                SpellSource = EInfusionSourceOption::Evolution;
                break;
            default:
                bIsSpell = false;
                break;
            }

            if (bIsSpell)
            {
                // Find the matching cached source and set the index there
                const TArray<EInfusionSourceOption> &Cached = VFX->GetCachedSources();
                int32 Idx = Cached.IndexOfByKey(SpellSource);
                if (Idx != INDEX_NONE)
                {
                    VFX->SetSourceIndex(Idx);
                }
                else
                {
                    VFX->SetImmuneToInfusion(true); // the spell's source is not on offer (e.g. a broken ring or weapon crystal, Q-163)
                }
            }

            UE_LOG(LogTemp, Verbose,
                   TEXT("[CombatCommandMenu] Picker opened — infusion reset to L0"));
        }
    }

    // Resolve targets
    TArray<AActor *> Targets = ResolveTargets(TargetType);

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu]   Resolved %d targets:"), Targets.Num());
    for (AActor *T : Targets)
    {
        if (!T)
            continue;
        FString TName = T->GetName();
        if (UCharacterDataComponent *CDC = T->FindComponentByClass<UCharacterDataComponent>())
        {
            if (CDC->CharacterData && !CDC->CharacterData->Name.IsEmpty())
            {
                TName = CDC->CharacterData->Name;
            }
        }
        UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu]     - %s"), *TName);
    }

    // Empty-target safety — bail and clear pending state
    if (Targets.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] OpenTargetSelection: no valid targets"));
        PendingActionCategory = EPieMenuCategory::None;
        PendingActionID.Empty();
        PendingActionData.Reset();
        PendingActionDataIndex = -1;
        return;
    }

    // Anyone (single/double) spans both teams — drill in via an Allies / Enemies
    // category step first, then narrow to a single-team per-actor list on
    // selection. Anyone + All is a group ("Everyone") and skips the category step.
    if (TargetType == ETargetType::Anyone && TargetCount != ETargetCount::All)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu]   Showing Allies/Enemies category step"));
        CurrentDepth = ECombatMenuDepth::TargetCategory;
        OnCommandMenuReady.Broadcast(BuildTargetCategoryButtons());
        return;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu]   Showing target picker"));

    CurrentDepth = ECombatMenuDepth::TargetSelection;

    TArray<FPieMenuButtonData> TargetButtons;
    if (TargetType == ETargetType::Self || TargetCount == ETargetCount::All)
    {
        // Self and All auto-confirm via a single labelled group button.
        TargetButtons = BuildGroupTargetButtons(TargetType, Targets);
    }
    else
    {
        // Single (and Double) — per-actor picker. TODO: Double should let the
        // player pick exactly 2 recipients (sequential single-picks); for now it
        // falls through to a single pick. No Double-authored skills exist yet, so
        // this is inert. Passive Double effects auto-resolve in GetEffectTargets.
        TargetButtons = BuildTargetButtons(Targets);
    }

    OnCommandMenuReady.Broadcast(TargetButtons);
}

// ==================== TARGET CATEGORY STEP ====================

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildTargetCategoryButtons() const
{
    // Same tints used by the per-actor picker in BuildTargetButtons.
    const FLinearColor EnemyTint(1.0f, 0.3f, 0.3f, 1.0f); // Red
    const FLinearColor AllyTint = FLinearColor::White;

    TArray<FPieMenuButtonData> Buttons;

    // The narrowed ETargetType rides in DataIndex — the same field School
    // buttons use to carry their distinguishing enum.
    FPieMenuButtonData Allies;
    Allies.ButtonID = TEXT("TargetCategory_Allies");
    Allies.DisplayName = FText::FromString(TEXT("Allies"));
    Allies.Category = EPieMenuCategory::TargetCategory;
    Allies.DataIndex = static_cast<int32>(ETargetType::Ally);
    Allies.ButtonTint = AllyTint;
    Allies.bEnabled = true;
    Buttons.Add(Allies);

    FPieMenuButtonData Enemies;
    Enemies.ButtonID = TEXT("TargetCategory_Enemies");
    Enemies.DisplayName = FText::FromString(TEXT("Enemies"));
    Enemies.Category = EPieMenuCategory::TargetCategory;
    Enemies.DataIndex = static_cast<int32>(ETargetType::Enemy);
    Enemies.ButtonTint = EnemyTint;
    Enemies.bEnabled = true;
    Buttons.Add(Enemies);

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

void UCombatCommandMenuSubsystem::ExitTargetSelectionToActionMenu()
{
    // Reset infusion state — picker is closing without commit
    SetPendingInfusionLevel(0);
    if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
    {
        VFX->SetImmuneToInfusion(false);
    }

    // Cancel pending action
    PendingActionCategory = EPieMenuCategory::None;
    PendingActionID.Empty();
    PendingActionData.Reset();
    PendingActionDataIndex = -1;
    ResolvedTargetType = PendingTargetType;

    if (DepthBeforeTargetSelection == ECombatMenuDepth::Submenu)
    {
        // Restore whichever submenu we came from
        CurrentDepth = ECombatMenuDepth::Submenu;
        if (!BroadcastCurrentSubmenu())
        {
            // Unknown submenu type, fall back to main
            CurrentDepth = ECombatMenuDepth::Main;
            ActiveSubmenuSource = EPieMenuCategory::None;
            ActiveSubmenuType = EPieMenuCategory::None;
            OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
        }
    }
    else
    {
        // Came directly from main (e.g. Attack)
        CurrentDepth = ECombatMenuDepth::Main;
        OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
    }
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildTargetButtons(
    const TArray<AActor *> &Targets) const
{
    TArray<FPieMenuButtonData> Buttons;

    // Resolve caster's team for ally/enemy tinting + section grouping.
    // TM is cached once and reused for the per-target team lookups below.
    int32 UserTeam = -1;
    UTurnManager *TM = nullptr;
    if (UGameInstance *GI = GetGameInstance())
    {
        TM = WoRFight::Get<UTurnManager>(GI);
    }
    const UTurnStateComponent *TurnState = UTurnStateComponent::Find(GetGameInstance());
    if (TM || TurnState)
    {
        if (AActor *User = CurrentActor.Get())
        {
            UserTeam = TeamOf(TurnState, TM, User);
        }
    }

    // Tints
    const FLinearColor EnemyTint(1.0f, 0.3f, 0.3f, 1.0f); // Red
    const FLinearColor AllyTint = FLinearColor::White;

    // Build a single per-actor target button (display name from CharacterData
    // when available, falling back to the actor name).
    auto MakeTargetButton = [](AActor *Target, const FLinearColor &Tint) -> FPieMenuButtonData
    {
        FString TargetName = Target->GetName();
        if (UCharacterDataComponent *CDC = Target->FindComponentByClass<UCharacterDataComponent>())
        {
            if (CDC->CharacterData && !CDC->CharacterData->Name.IsEmpty())
            {
                TargetName = CDC->CharacterData->Name;
            }
        }

        FPieMenuButtonData Button;
        Button.ButtonID = Target->GetName();
        // D-253: a dead ally is listed only for a revive (ResolveTargets); the button says it is down.
        const UCharacterDataComponent *Life = Target->FindComponentByClass<UCharacterDataComponent>();
        Button.DisplayName = (Life && !Life->bIsAlive)
                                 ? FText::Format(NSLOCTEXT("CombatCommandMenu", "TargetDown", "{0} (down)"), FText::FromString(TargetName))
                                 : FText::FromString(TargetName);
        Button.Category = EPieMenuCategory::Target;
        Button.DataReference = Target;
        Button.ButtonTint = Tint;
        Button.bEnabled = true;
        return Button;
    };

    // Flat single-team list — enemies tinted red, allies default. Anyone targeting
    // is split into teams upstream by the Allies/Enemies category step, so this
    // picker only ever receives one side's actors.
    for (AActor *Target : Targets)
    {
        if (!Target)
            continue;

        FLinearColor Tint = AllyTint;
        if (UserTeam >= 0 && (TM || TurnState))
        {
            const int32 TargetTeam = TeamOf(TurnState, TM, Target);
            if (TargetTeam >= 0 && TargetTeam != UserTeam)
            {
                Tint = EnemyTint;
            }
        }
        Buttons.Add(MakeTargetButton(Target, Tint));
    }

    AppendInfusionControls(Buttons); // B-254: one copy for single and group targets

    Buttons.Add(FPieMenuButtonData::MakeBackButton());

    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildGroupTargetButtons(
    ETargetType TargetType,
    const TArray<AActor *> &Targets) const
{
    TArray<FPieMenuButtonData> Buttons;

    // ==================== GROUP CONFIRM BUTTON ====================
    FPieMenuButtonData ConfirmBtn;
    ConfirmBtn.Category = EPieMenuCategory::Target;
    ConfirmBtn.bEnabled = true;
    ConfirmBtn.Targets = Targets;

    switch (TargetType)
    {
    case ETargetType::Self:
    {
        // Label with caster's character name
        FString CasterName = TEXT("Self");
        if (AActor *User = CurrentActor.Get())
        {
            if (UCharacterDataComponent *CDC = User->FindComponentByClass<UCharacterDataComponent>())
            {
                if (CDC->CharacterData && !CDC->CharacterData->Name.IsEmpty())
                {
                    CasterName = CDC->CharacterData->Name;
                }
                else
                {
                    CasterName = User->GetName();
                }
            }
        }
        ConfirmBtn.ButtonID = TEXT("GroupTarget_Self");
        ConfirmBtn.DisplayName = FText::FromString(CasterName);
        break;
    }
    case ETargetType::Enemy:
        ConfirmBtn.ButtonID = TEXT("GroupTarget_AllEnemies");
        ConfirmBtn.DisplayName = FText::FromString(TEXT("All Enemies"));
        break;
    case ETargetType::Ally:
        ConfirmBtn.ButtonID = TEXT("GroupTarget_AllAllies");
        ConfirmBtn.DisplayName = FText::FromString(TEXT("All Allies"));
        break;
    case ETargetType::Anyone:
        ConfirmBtn.ButtonID = TEXT("GroupTarget_Everyone");
        ConfirmBtn.DisplayName = FText::FromString(TEXT("Everyone"));
        break;
    default:
        // Should never hit this — caller routes only group types here
        break;
    }
    Buttons.Add(ConfirmBtn);

    AppendInfusionControls(Buttons);

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

#define LOCTEXT_NAMESPACE "CombatCommandMenu"

void UCombatCommandMenuSubsystem::AppendInfusionControls(TArray<FPieMenuButtonData> &Buttons) const
{
    // B-254 (DRY audit A11): the infusion controls under a target list, single or group. Labels are FText (D-061).
    // Below targets, minimal buttons. Items skip infusion entirely.
    // Actions flagged bImmuneToInfusion also skip — the controls would be inert.
    // Equipment-level immunity (active weapon / ring on the wielder) ORs in.
    //   - Cycle Level button doubles as the state readout: "<source>: L<n>"
    //     Pressing it cycles 0 -> 1 -> 2 -> 0.
    //   - Cycle Source is a separate button, only for Attack / Ability.
    //   - Spells show their intrinsic source in the label (not cyclable).
    if (CanInfusePendingAction())
    {
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            // Resolve source label depending on action type.
            // Attack / Ability: read VFX cycled source.
            // Spell submenus: source is intrinsic to the action category.
            // For Spell, intrinsic source comes from the originating submenu.
            // For Attack/Ability, source is what the player has cycled to.
            const EPieMenuCategory SourceLookup =
                (PendingActionCategory == EPieMenuCategory::Spell)
                    ? ActiveSubmenuSource
                    : PendingActionCategory;

            FText SourceLabel;
            switch (SourceLookup)
            {
            case EPieMenuCategory::Refractions:
                SourceLabel = LOCTEXT("InfusionSourceInnate", "Innate");
                break;
            case EPieMenuCategory::ResonateWeapon:
                SourceLabel = LOCTEXT("InfusionSourceWeaponCrystal", "Weapon crystal");
                break;
            case EPieMenuCategory::ResonateRing:
                SourceLabel = WoRGameNames::GetAdapterWord(); // D-107
                break;
            case EPieMenuCategory::Attack:
            case EPieMenuCategory::Ability:
            case EPieMenuCategory::Breakthrough:
            default:
                // Breakthrough spells can cycle source (Evolution / Innate),
                // so label reads from the live VFX state like Attack/Ability.
                SourceLabel = FText::FromString(VFX->GetCurrentSourceName()); // TODO: FText at the source (D-061)
                break;
            }

            // Combined cycle-level + state-display button
            FPieMenuButtonData CycleLvl;
            CycleLvl.ButtonID = TEXT("CycleLevel");
            CycleLvl.DisplayName = FText::Format(LOCTEXT("InfusionCycleLevel", "{0} Lv {1}"), SourceLabel, FText::AsNumber(PendingInfusionLevel));
            CycleLvl.Category = EPieMenuCategory::CycleLevel;
            CycleLvl.bEnabled = true;
            Buttons.Add(CycleLvl);

            // Cycle Source — Attack / Ability always; Breakthrough spells too
            // (so BD can pick Innate for Darkness-conversion). Other spell
            // submenus stay element-locked. Suppressed when the filtered cache
            // has ≤1 entry (cycling would be a no-op).
            if (CanChooseInfusionElement())
            {
                FPieMenuButtonData CycleSrc;
                CycleSrc.ButtonID = TEXT("CycleSource");
                CycleSrc.DisplayName = LOCTEXT("InfusionCycleSource", "Source");
                CycleSrc.Category = EPieMenuCategory::CycleSource;
                CycleSrc.bEnabled = true;
                Buttons.Add(CycleSrc);
            }
        }
    }

    // B-592 (D-268): the new battle HUD's panel lines, opened by X / 2 and Y / 3 (corner hints there). The old HUD
    // keeps its rows as they were.
    if (!UCombatHUD::WantsNewHUD() || CurrentDepth != ECombatMenuDepth::TargetSelection)
    {
        return;
    }
    if (PendingActionCategory == EPieMenuCategory::Attack && CurrentCapabilities.bCanSwitchWeapon)
    {
        FPieMenuButtonData Weapon;
        Weapon.ButtonID = TEXT("TargetPanelWeapon");
        Weapon.DisplayName = OpenTargetPanelLine == ETargetPanelLine::Weapon ? LOCTEXT("PanelNextWeapon", "Next weapon")
                                                                             : LOCTEXT("PanelWeapon", "Weapon");
        Weapon.Category = EPieMenuCategory::TargetPanelWeapon;
        Buttons.Add(Weapon);
    }
    if (CanInfusePendingAction())
    {
        FPieMenuButtonData Infusion;
        Infusion.ButtonID = TEXT("TargetPanelInfusion");
        Infusion.DisplayName = OpenTargetPanelLine == ETargetPanelLine::Infusion ? LOCTEXT("PanelClose", "Close")
                               : CanChooseInfusionElement()                     ? LOCTEXT("PanelInfusion", "Infusion")
                                                                                : LOCTEXT("PanelInfusionLevel", "Infusion level");
        Infusion.Category = EPieMenuCategory::TargetPanelInfusion;
        Buttons.Add(Infusion);
    }
}

bool UCombatCommandMenuSubsystem::CanInfusePendingAction() const
{
    // Items skip infusion; so does an action immune to it (bImmuneToInfusion, or the wielder's equipment).
    return PendingActionCategory != EPieMenuCategory::Item && !IsInfusionLockedForPendingAction() && GetInfusionVFXComponent();
}

bool UCombatCommandMenuSubsystem::CanChooseInfusionElement() const
{
    // Attack / Ability always; Breakthrough spells too (BD can pick Innate for Darkness-conversion). Other spells are
    // origin-bound (one fixed element). One cached source leaves nothing to choose (a plain Caster evolution spell).
    const UInfusionVFXComponent *VFX = GetInfusionVFXComponent();
    const bool bChoosableKind = PendingActionCategory == EPieMenuCategory::Attack || PendingActionCategory == EPieMenuCategory::Ability ||
                                (PendingActionCategory == EPieMenuCategory::Spell && ActiveSubmenuSource == EPieMenuCategory::Breakthrough);
    return CanInfusePendingAction() && bChoosableKind && VFX && VFX->GetCachedSources().Num() > 1;
}

FTargetPanelView UCombatCommandMenuSubsystem::GetTargetPanel() const
{
    FTargetPanelView View;
    if (!bIsOpen || CurrentDepth != ECombatMenuDepth::TargetSelection)
    {
        return View;
    }
    View.bShown = true;
    View.Open = OpenTargetPanelLine;

    TArray<FText> Parts;
    const ULoadoutComponent *LC = GetLoadoutComponent();
    const bool bWeaponLine = PendingActionCategory == EPieMenuCategory::Attack;
    if (bWeaponLine && !CurrentCapabilities.ActiveWeaponName.IsEmpty())
    {
        Parts.Add(FText::FromString(CurrentCapabilities.ActiveWeaponName)); // a data name (WeaponData::Name)
    }
    if (bWeaponLine && LC && CurrentCapabilities.bCanSwitchWeapon && OpenTargetPanelLine == ETargetPanelLine::Weapon)
    {
        const FCombatLoadout &Loadout = LC->GetActiveLoadout();
        for (const FWeaponLoadoutEntry *Entry : {&Loadout.PrimaryWeapon, &Loadout.SecondaryWeapon})
        {
            View.Weapons.Add(Entry->WeaponEntry.Weapon ? FText::FromString(Entry->WeaponEntry.Weapon->Name) : FText::GetEmpty());
        }
        View.ActiveWeapon = Loadout.bShowPrimary ? 0 : 1;
    }

    const UInfusionVFXComponent *VFX = GetInfusionVFXComponent();
    if (CanInfusePendingAction() && VFX)
    {
        static const FText LevelNames[] = {LOCTEXT("PanelLevel0", "—"), LOCTEXT("PanelLevel1", "I"), LOCTEXT("PanelLevel2", "II")};
        const auto LevelName = [](int32 Level) { return Level >= 0 && Level < UE_ARRAY_COUNT(LevelNames) ? LevelNames[Level] : FText::AsNumber(Level); };
        const bool bElement = CanChooseInfusionElement();
        const TArray<EInfusionSourceOption> &Sources = VFX->GetCachedSources();
        const UActionExecutor *Exec = WoRFight::Get<UActionExecutor>(this);
        const auto SourceName = [this, Exec](EInfusionSourceOption Source)
        {
            if (Source == EInfusionSourceOption::None)
            {
                return LOCTEXT("PanelSourceNone", "None");
            }
            if (Source == EInfusionSourceOption::Raw)
            {
                return LOCTEXT("PanelSourceRaw", "Raw");
            }
            return Exec ? WoRGameNames::GetElementName(Exec->GetElementForSourceOption(CurrentActor.Get(), Source))
                        : FText::FromString(InfusionSourceOptionHelpers::GetDisplayName(Source));
        };
        if (PendingInfusionLevel <= 0)
        {
            Parts.Add(LOCTEXT("PanelNoInfusion", "No infusion"));
        }
        else
        {
            if (bElement && Sources.IsValidIndex(VFX->GetCurrentSourceIndex()))
            {
                Parts.Add(SourceName(Sources[VFX->GetCurrentSourceIndex()]));
            }
            Parts.Add(FText::Format(LOCTEXT("PanelLevel", "Lv {0}"), LevelName(PendingInfusionLevel)));
        }
        if (OpenTargetPanelLine == ETargetPanelLine::Infusion)
        {
            for (int32 Index = 0; bElement && Index < Sources.Num(); ++Index)
            {
                View.Elements.Add(SourceName(Sources[Index]));
            }
            View.ActiveElement = bElement ? VFX->GetCurrentSourceIndex() : INDEX_NONE;
            for (int32 Level = 0; Level <= InfusionConstants::MAX_INFUSION_LEVEL; ++Level)
            {
                View.Levels.Add(LevelName(Level));
            }
            View.ActiveLevel = PendingInfusionLevel;
        }
    }
    View.Summary = FText::Join(LOCTEXT("PanelSummarySeparator", " · "), Parts);
    return View;
}

void UCombatCommandMenuSubsystem::PickTargetPanelChoice(ETargetPanelChoice Line, int32 Index)
{
    if (!bIsOpen || CurrentDepth != ECombatMenuDepth::TargetSelection)
    {
        return;
    }
    const FTargetPanelView View = GetTargetPanel();
    switch (Line)
    {
    case ETargetPanelChoice::Weapon:
        if (View.Weapons.IsValidIndex(Index) && Index != View.ActiveWeapon)
        {
            ExecuteSwitchWeapon(); // two weapons: the other one
        }
        return;
    case ETargetPanelChoice::Element:
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent(); VFX && View.Elements.IsValidIndex(Index))
        {
            VFX->SetSourceIndex(Index);
            if (PendingInfusionLevel > 0)
            {
                VFX->ActivateCurrentSource();
            }
        }
        break;
    case ETargetPanelChoice::Level:
        if (View.Levels.IsValidIndex(Index))
        {
            SetPendingInfusionLevel(Index);
        }
        break;
    }
    OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
}

void UCombatCommandMenuSubsystem::BroadcastAfterWeaponSwitch()
{
    if (CurrentDepth != ECombatMenuDepth::TargetSelection)
    {
        OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
        return;
    }
    // B-592: X in the Attack step switches the weapon and stays; the new weapon's crystal is a new source.
    if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
    {
        VFX->CacheAvailableSources();
        if (PendingInfusionLevel > 0)
        {
            VFX->ActivateCurrentSource();
        }
    }
    OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
}

#undef LOCTEXT_NAMESPACE

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::RebuildCurrentPicker() const
{
    // ResolvedTargetType — not PendingTargetType — so that after the category
    // step this rebuilds the narrowed single-team list, not the full Anyone
    // set. The two are equal whenever no category step was taken.
    const TArray<AActor *> Targets = ResolveTargets(ResolvedTargetType);
    if (ResolvedTargetType == ETargetType::Self || ResolvedTargetCount == ETargetCount::All)
    {
        return BuildGroupTargetButtons(ResolvedTargetType, Targets);
    }
    return BuildTargetButtons(Targets);
}

bool UCombatCommandMenuSubsystem::IsPendingItemRevive() const
{
    if (PendingActionCategory != EPieMenuCategory::Item)
    {
        return false;
    }
    const ULoadoutComponent *LC = GetLoadoutComponent();
    const TArray<FItemLoadoutSlot> Items = LC ? LC->GetUsableItems() : TArray<FItemLoadoutSlot>();
    return Items.IsValidIndex(PendingActionDataIndex) &&
           ItemIdentity::GetItemEffectType(Items[PendingActionDataIndex].CrystalId) == EItemEffectType::DefyDeath;
}

TArray<AActor *> UCombatCommandMenuSubsystem::ResolveTargets(ETargetType TargetType) const
{
    TArray<AActor *> Result;

    AActor *User = CurrentActor.Get();
    if (!User)
        return Result;

    UGameInstance *GI = GetGameInstance();
    if (!GI)
        return Result;

    UTurnManager *TM = WoRFight::Get<UTurnManager>(GI);
    const UTurnStateComponent *TurnState = UTurnStateComponent::Find(GI);
    if (!TM && !TurnState)
        return Result;

    int32 UserTeam = TeamOf(TurnState, TM, User);
    if (UserTeam < 0)
        return Result;

    // ADD THIS LAMBDA HERE — right before the switch
    auto IsAlive = [](AActor *Actor) -> bool
    {
        if (!Actor)
            return false;
        UCharacterDataComponent *CDC = Actor->FindComponentByClass<UCharacterDataComponent>();
        return CDC && CDC->bIsAlive;
    };
    // D-253: the dead can be picked for one thing, a Sapphire on a dead ally (the revive). The user's own side
    // then lists its dead too; enemies are always the living only.
    const bool bDeadAlliesToo = IsPendingItemRevive();
    auto IsAllyTarget = [&IsAlive, bDeadAlliesToo](AActor *Actor) -> bool
    {
        return Actor && (bDeadAlliesToo || IsAlive(Actor));
    };

    // Returns the candidate pool for the role only; the count axis
    // (Single/Double/All) is applied at selection time, not here.
    switch (TargetType)
    {
    case ETargetType::Self:
        Result.Add(User);
        break;

    case ETargetType::Enemy:
    {
        int32 EnemyTeam = (UserTeam == 0) ? 1 : 0;
        for (AActor *Member : MembersOf(TurnState, TM, EnemyTeam))
        {
            if (IsAlive(Member))
            {
                Result.Add(Member);
            }
        }
        break;
    }

    case ETargetType::Ally:
        for (AActor *Member : MembersOf(TurnState, TM, UserTeam))
        {
            if (IsAllyTarget(Member))
            {
                Result.Add(Member);
            }
        }
        break;

    case ETargetType::Anyone:
        for (const int32 Team : {0, 1})
        {
            for (AActor *Member : MembersOf(TurnState, TM, Team))
            {
                if (Team == UserTeam ? IsAllyTarget(Member) : IsAlive(Member))
                {
                    Result.Add(Member);
                }
            }
        }
        break;
    }

    return Result;
}

void UCombatCommandMenuSubsystem::ConfirmActionWithTarget(AActor *SelectedTarget)
{
    // Rebuild the action button with the resolved target attached
    FPieMenuButtonData ActionButton;
    ActionButton.ButtonID = PendingActionID;
    ActionButton.Category = PendingActionCategory;
    ActionButton.DataReference = PendingActionData.Get();
    ActionButton.DataIndex = PendingActionDataIndex; // restore slot index (consumables)
    ActionButton.TargetActor = SelectedTarget;

    FString TargetName = TEXT("(no target)");
    if (SelectedTarget)
    {
        if (UCharacterDataComponent *CDC = SelectedTarget->FindComponentByClass<UCharacterDataComponent>())
        {
            if (CDC->CharacterData && !CDC->CharacterData->Name.IsEmpty())
            {
                TargetName = CDC->CharacterData->Name;
            }
            else
            {
                TargetName = SelectedTarget->GetName();
            }
        }
        else
        {
            TargetName = SelectedTarget->GetName();
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Action confirmed: %s on %s"),
           *PendingActionID,
           *TargetName);
    if (PendingActionCategory == EPieMenuCategory::Attack && SelectedTarget && CurrentActor.IsValid())
    {
        LastAttackTargets.Add(CurrentActor.Get(), SelectedTarget); // B-582 (D-266): the next Attack picker opens on it
    }

    // Clear pending state
    PendingActionCategory = EPieMenuCategory::None;
    PendingActionID.Empty();
    PendingActionData.Reset();
    PendingActionDataIndex = -1;
    ResolvedTargetType = PendingTargetType;

    SubmitConfirmedAction(ActionButton);
}

void UCombatCommandMenuSubsystem::ConfirmActionWithTargets(const TArray<AActor *> &SelectedTargets)
{
    FPieMenuButtonData ActionButton;
    ActionButton.ButtonID = PendingActionID;
    ActionButton.Category = PendingActionCategory;
    ActionButton.DataReference = PendingActionData.Get();
    ActionButton.DataIndex = PendingActionDataIndex; // restore slot index (consumables)
    ActionButton.Targets = SelectedTargets;

    UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Action confirmed: %s on %d targets"),
           *PendingActionID, SelectedTargets.Num());

    // Clear pending state — same pattern as ConfirmActionWithTarget
    PendingActionCategory = EPieMenuCategory::None;
    PendingActionID.Empty();
    PendingActionData.Reset();
    PendingActionDataIndex = -1;
    ResolvedTargetType = PendingTargetType;

    SubmitConfirmedAction(ActionButton);
}

// ==================== ACTION SUBMISSION ====================

ESpellSource UCombatCommandMenuSubsystem::MapCategoryToSpellSource(
    EPieMenuCategory SubmenuCategory) const
{
    switch (SubmenuCategory)
    {
    case EPieMenuCategory::Refractions:
        return ESpellSource::Innate;
    case EPieMenuCategory::ResonateRing:
        return ESpellSource::RingCrystal;
    case EPieMenuCategory::ResonateWeapon:
        return ESpellSource::WeaponCrystal;
    case EPieMenuCategory::Breakthrough:
        return ESpellSource::Evolution;
    default:
        return ESpellSource::Innate;
    }
}

FAction UCombatCommandMenuSubsystem::BuildActionFromButton(
    const FPieMenuButtonData &ButtonData) const
{
    FAction Action;

    // Determine action type + data ref. For per-actor target buttons (Category=Target,
    // single-target), or group confirm buttons (Category=Target, Targets populated),
    // the actual action type lives in PendingActionCategory.
    const EPieMenuCategory ResolvedCategory =
        (ButtonData.Category == EPieMenuCategory::Target)
            ? PendingActionCategory
            : ButtonData.Category;

    UObject *DataRef = (ButtonData.Category == EPieMenuCategory::Target)
                           ? PendingActionData.Get()
                           : ButtonData.DataReference;

    switch (ResolvedCategory)
    {
    case EPieMenuCategory::Attack:
        Action.ActionType = EActionType::Ability; // attack/ability merge: attacks dispatch as Ability (SkillData + IsAttack)
        Action.SkillData = Cast<USkillDataBase>(DataRef);
        if (!Action.SkillData)
        {
            // Attack main-menu button doesn't carry data — pull the active weapon's attack.
            if (ULoadoutComponent *LC = GetLoadoutComponent())
            {
                Action.SkillData = LC->GetCurrentAttack();
            }
        }
        break;
    case EPieMenuCategory::Ability:
        Action.ActionType = EActionType::Ability;
        Action.SkillData = Cast<UAbilityData>(DataRef);
        break;
    case EPieMenuCategory::Spell:
        Action.ActionType = EActionType::Spell;
        Action.SpellData = Cast<USpellData>(DataRef);
        Action.SpellSource = MapCategoryToSpellSource(ActiveSubmenuSource);
        break;
    case EPieMenuCategory::Item:
    {
        Action.ActionType = EActionType::Item;
        // Consumable identity comes from the live usable-items entry the button points
        // at (DataIndex, B-313), not a UObject* payload.
        if (ULoadoutComponent *LC = GetLoadoutComponent())
        {
            const TArray<FItemLoadoutSlot> Items = LC->GetUsableItems();
            if (Items.IsValidIndex(ButtonData.DataIndex))
            {
                Action.ItemData = Items[ButtonData.DataIndex].CrystalId;
            }
            else
            {
                // Surface a dropped index instead of a silent empty action.
                UE_LOG(LogTemp, Warning,
                       TEXT("[CombatCommandMenu] Item action has invalid DataIndex=%d (usable items=%d) — ItemData left empty"),
                       ButtonData.DataIndex, Items.Num());
            }
        }
        break;
    }
    case EPieMenuCategory::Defend:
        Action.ActionType = EActionType::Defend;
        break;
    case EPieMenuCategory::EndTurn:
        Action.ActionType = EActionType::EndTurn; // B-170e
        break;
    case EPieMenuCategory::Flee:
        Action.ActionType = EActionType::Flee;
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] BuildActionFromButton: unhandled category %d"),
               static_cast<int32>(ResolvedCategory));
        break;
    }

    // Targets — group buttons carry array; per-actor buttons carry single TargetActor;
    // legacy fallback to DataReference (for older Target buttons that stuffed the actor there).
    if (ButtonData.Targets.Num() > 0)
    {
        Action.Targets = ButtonData.Targets;
    }
    else if (ButtonData.TargetActor)
    {
        Action.Targets.Add(ButtonData.TargetActor);
    }
    else if (AActor *Single = Cast<AActor>(ButtonData.DataReference))
    {
        Action.Targets.Add(Single);
    }

    // Infusion stamp: spells and abilities/attacks only, and never while stunned (D-023 A)
    if ((Action.ActionType == EActionType::Spell || Action.ActionType == EActionType::Ability) &&
        !IsCurrentActorStunned())
    {
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            const int32 Level = PendingInfusionLevel;
            if (Level > 0)
            {
                const TArray<EInfusionSourceOption> &Sources = VFX->GetCachedSources();
                const int32 Idx = VFX->GetCurrentSourceIndex();
                Action.SelectedSource = Sources.IsValidIndex(Idx)
                                            ? Sources[Idx]
                                            : EInfusionSourceOption::None;

                // Route level by action type
                if (Action.ActionType == EActionType::Spell)
                {
                    Action.SpellInfusionLevel = Level;
                }
                else if (Action.ActionType == EActionType::Ability)
                {
                    Action.AbilityInfusionLevel = Level;
                }
            }
        }
    }

    return Action;
}

void UCombatCommandMenuSubsystem::SubmitConfirmedAction(
    const FPieMenuButtonData &ButtonData)
{
    FAction Action = BuildActionFromButton(ButtonData);

    UE_LOG(LogTemp, Verbose,
           TEXT("[CombatCommandMenu] Submitting action: type=%d targets=%d level(spell/ab)=%d/%d source=%d spellSource=%d"),
           static_cast<int32>(Action.ActionType),
           Action.Targets.Num(),
           Action.SpellInfusionLevel,
           Action.AbilityInfusionLevel,
           static_cast<int32>(Action.SelectedSource),
           static_cast<int32>(Action.SpellSource));

    // B-229: the pick goes through this machine's combat player controller. On a client it is sent to the
    // server (the client's orchestrator runs no fight), and the menu closes: the server's turn state says what
    // happens next (the client HUD is B-228b).
    ACombatPlayerController *LocalController = ACombatPlayerController::FindLocal(GetGameInstance());
    if (LocalController && !WoRAuthority::HasAuthority(LocalController))
    {
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            VFX->SetImmuneToInfusion(false);
        }
        LocalController->RequestMenuPick(FCombatPickRequest::FromAction(CurrentActor.Get(), Action));
        Close();
        return;
    }

    ACombatOrchestrator *Orchestrator = CurrentOrchestrator.Get();
    if (!Orchestrator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] SubmitConfirmedAction: no orchestrator bound"));
        return;
    }

    if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
    {
        VFX->SetImmuneToInfusion(false);
    }

    if (LocalController)
    {
        // Authority (host, standalone): the same server-side check a client's pick gets, then the orchestrator.
        LocalController->RequestMenuPick(FCombatPickRequest::FromAction(CurrentActor.Get(), Action));
    }
    else
    {
        // No combat player controller (headless specs, another controller class): straight to the orchestrator.
        Orchestrator->SubmitActionAsync(Action);
    }

    // B-110: a finished (sync) action was closed by HandleActionExecuted, and a refused one
    // re-requested this actor's turn, which reopened the menu; closing after either would shut a
    // menu the player needs. Close only while the action is still running, or when the menu's
    // actor no longer has the turn (combat not in progress, submission dropped).
    if (bIsOpen && (Orchestrator->IsWaitingForAsyncAction() || !Orchestrator->IsActorsTurn(CurrentActor.Get())))
    {
        Close();
    }
}

// ==================== HELPERS ====================

ULoadoutComponent *UCombatCommandMenuSubsystem::GetLoadoutComponent() const
{
    if (!CurrentActor.IsValid())
        return nullptr;
    return CurrentActor->FindComponentByClass<ULoadoutComponent>();
}

UInfusionVFXComponent *UCombatCommandMenuSubsystem::GetInfusionVFXComponent() const
{
    if (!CurrentActor.IsValid())
        return nullptr;
    return CurrentActor->FindComponentByClass<UInfusionVFXComponent>();
}

bool UCombatCommandMenuSubsystem::IsCurrentActorStunned() const
{
    const USkillEffectManager *EffectManager =
        WoRFight::Get<USkillEffectManager>(this);
    return EffectManager && CurrentActor.IsValid() && EffectManager->IsStunned(CurrentActor.Get());
}

bool UCombatCommandMenuSubsystem::IsInfusionLockedForPendingAction() const
{
    return IsCurrentActorStunned() || ResolveImmuneFlag(PendingActionData.Get(), CurrentActor.Get());
}

void UCombatCommandMenuSubsystem::SetPendingInfusionLevel(int32 Level)
{
    PendingInfusionLevel = IsInfusionLockedForPendingAction()
                               ? 0
                               : FMath::Clamp(Level, 0, InfusionConstants::MAX_INFUSION_LEVEL);
    if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
    {
        VFX->SetInfusionLevel(PendingInfusionLevel);
    }
}

FLinearColor UCombatCommandMenuSubsystem::GetElementColor(int32 ElementIndex) const
{
    return ElementColors::GetColorForElement(static_cast<ESpellElement>(ElementIndex));
}

// ==================== DEBUG ====================

void UCombatCommandMenuSubsystem::DebugLogCapabilities() const
{
    UE_LOG(LogTemp, Log, TEXT("=== CombatCommandMenu Capabilities ==="));
    UE_LOG(LogTemp, Log, TEXT("Class:          %s"), *UEnum::GetValueAsString(CurrentCapabilities.CharacterClass));
    UE_LOG(LogTemp, Log, TEXT("Attack:         %s"), CurrentCapabilities.bCanAttack ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Log, TEXT("Abilities:      %s (%d)"), CurrentCapabilities.bCanUseAbilities ? TEXT("YES") : TEXT("NO"), CurrentCapabilities.WeaponAbilities.Num());
    UE_LOG(LogTemp, Log, TEXT("WeaponCrystal:  %s (%d spells)"), CurrentCapabilities.bHasWeaponCrystal ? TEXT("YES") : TEXT("NO"), CurrentCapabilities.WeaponCrystalSpells.Num());
    UE_LOG(LogTemp, Log, TEXT("Refractions:    %s (%d spells)"), CurrentCapabilities.bHasRefractions ? TEXT("YES") : TEXT("NO"), CurrentCapabilities.RefractionSpells.Num());
    UE_LOG(LogTemp, Log, TEXT("Breakthrough:   %s (%d spells)"), CurrentCapabilities.bHasBreakthrough ? TEXT("YES") : TEXT("NO"), CurrentCapabilities.BreakthroughSpells.Num());
    UE_LOG(LogTemp, Log, TEXT("PrimaryRing:    %s"), CurrentCapabilities.bHasPrimaryRing ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Log, TEXT("RingLoadout:    %s (%d spells)"), CurrentCapabilities.bHasRingLoadout ? TEXT("YES") : TEXT("NO"), CurrentCapabilities.RingSpells.Num());
    UE_LOG(LogTemp, Log, TEXT("SwitchWeapon:   %s"), CurrentCapabilities.bCanSwitchWeapon ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Log, TEXT("SwitchRing:     %s"), CurrentCapabilities.bCanSwitchRing ? TEXT("YES") : TEXT("NO"));
    if (const ULoadoutComponent *SwitchLC = GetLoadoutComponent())
    {
        UE_LOG(LogTemp, Log, TEXT("Turn switch:    %s"), SwitchLC->GetTurnSwitchRefusedReason().IsEmpty() ? TEXT("free") : TEXT("used"));
    }
    for (const TPair<TObjectPtr<USpellData>, FText> &Blocked : CurrentCapabilities.BlockedSpellReasons)
    {
        UE_LOG(LogTemp, Log, TEXT("Greyed spell:   %s - %s"), *GetNameSafe(Blocked.Key), *Blocked.Value.ToString());
    }
    UE_LOG(LogTemp, Log, TEXT("ActiveWeapon:   %s"), *CurrentCapabilities.ActiveWeaponName);
    UE_LOG(LogTemp, Log, TEXT("ActiveRing:     %s"), *CurrentCapabilities.ActiveRingName);
    UE_LOG(LogTemp, Log, TEXT("BreakChance:    %s"), CurrentCapabilities.bRingHasBreakChance ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Log, TEXT("LastAttackOn:   %s"), *GetNameSafe(GetLastAttackTarget(CurrentActor.Get()))); // B-582
    const FTargetPanelView Panel = GetTargetPanel(); // B-592
    UE_LOG(LogTemp, Log, TEXT("TargetPanel:    %s line=%d \"%s\" weapons=%d elements=%d levels=%d"), Panel.bShown ? TEXT("shown") : TEXT("none"),
           static_cast<int32>(Panel.Open), *Panel.Summary.ToString(), Panel.Weapons.Num(), Panel.Elements.Num(), Panel.Levels.Num());
}

bool UCombatCommandMenuSubsystem::PressMenuKey(EPieMenuCategory Category)
{
    if (!bIsOpen)
    {
        return false;
    }
    if (Category == EPieMenuCategory::Back)
    {
        // B-436g (D-258): Back everywhere past the main menu. On it there is nowhere to go, and closing would strand the turn.
        if (CurrentDepth == ECombatMenuDepth::Main)
        {
            return false;
        }
        HandleBack();
        return true;
    }
    if (Category == EPieMenuCategory::Attack && PendingActionCategory == EPieMenuCategory::Attack && IsSingleTargetPickerOpen())
    {
        // B-582 (D-266): 1 / A again in the Attack picker attacks the highlighted target (1, 1 is a quick attack). The
        // Position picker's 1 is its own branch below; this needs the Attack picker.
        return ConfirmHighlightedTarget();
    }
    if (Category == EPieMenuCategory::Target)
    {
        return ConfirmHighlightedTarget(); // B-582b (D-267): Enter / Space, in any single-target picker; nothing elsewhere
    }
    TArray<FPieMenuButtonData> View;
    const bool bInfusionKey = Category == EPieMenuCategory::CycleSource || Category == EPieMenuCategory::CycleLevel;
    // B-592 (D-268): in a target step 2 / X is the weapon line and 3 / Y the infusion lines, never Skills / Items.
    const EPieMenuCategory PanelButton = CommandMenuRules::TargetPanelButtonForKey(Category);
    if (PanelButton != EPieMenuCategory::None && CurrentDepth == ECombatMenuDepth::TargetSelection)
    {
        Category = PanelButton;
        View = RebuildCurrentPicker();
    }
    else if (bInfusionKey && CurrentDepth == ECombatMenuDepth::TargetSelection)
    {
        View = RebuildCurrentPicker();
    }
    else if (CommandMenuRules::IsMainMenuKey(Category) && CurrentDepth == ECombatMenuDepth::Main) // B-391
    {
        View = BuildMainMenuButtons();
    }
    else if (const int32 Slot = CommandMenuRules::PositionPickerRowForKey(Category); Slot != INDEX_NONE && IsPositionPickerOpen())
    {
        // B-588: 1 / A, 2 / X, 3 / Y pick the picker's rows, top to bottom (as BattleHUDView::AssignMenuKeys shows them).
        TArray<FPieMenuButtonData> Rows = BuildPositionButtons();
        Rows.RemoveAll([](const FPieMenuButtonData &Row) { return Row.Category != EPieMenuCategory::MoveRow; });
        if (Rows.IsValidIndex(Slot) && Rows[Slot].bEnabled)
        {
            HandleSelection(Rows[Slot]);
            return true;
        }
        return false;
    }
    const FPieMenuButtonData *Button = View.FindByPredicate([Category](const FPieMenuButtonData &Candidate)
                                                            { return CommandMenuRules::KeyPressesButton(Category, Candidate.Category) && Candidate.bEnabled; });
    if (!Button)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[CombatCommandMenu] Key for %s: no such button here"), *UEnum::GetValueAsString(Category));
        return false;
    }
    const FPieMenuButtonData Pressed = *Button; // HandleSelection can rebuild the menu
    HandleSelection(Pressed);
    return true;
}

void UCombatCommandMenuSubsystem::RefreshMenu()
{
    if (!bIsOpen)
        return;
    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}