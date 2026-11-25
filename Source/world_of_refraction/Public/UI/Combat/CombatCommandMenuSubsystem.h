// CombatCommandMenuSubsystem.h
// Drives the combat command menu. Builds capabilities from LoadoutComponent,
// exposes button arrays to Blueprint via delegates, handles selection routing.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Combat/WoRFightSubsystem.h"
#include "UI/Combat/FCombatCapabilities.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Character/ECharacterClass.h"
#include "Infusion/ElementColors.h"
#include "Combat/TargetType.h"
#include "Combat/Actions/ActionStructs.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Skills/Definitions/ESpellElement.h"
#include "CombatCommandMenuSubsystem.generated.h"

class ULoadoutComponent;
struct FInfusionCostPreview;
enum class ECombatFreeCommand : uint8;
enum class ECombatRow : uint8;
class UCharacterData;
class USpellData;
class UAbilityData;
class UBrokenDarknessManager;
class UInfusionVFXComponent;

// ==================== DELEGATES ====================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommandMenuReady,
                                            const TArray<FPieMenuButtonData> &, Buttons);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCommandMenuClosed);

/** B-399 (study R2): the open menu's depth changed (CommandMenuRules::CameraDepthFor); Actor is whose menu it is.
 *  Closing the menu sends nothing (the camera holds for the action); the next open sends its depth if it differs. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommandMenuDepthChanged, AActor *, Actor, int32, MenuDepth);

/** B-400 (study R3 / §9): the single-target picker's highlighted target changed. Actor is whose menu it is; Target is
 *  null when the picker closes (cancel, confirm or the menu closing); TargetCount is how many it can pick from. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCommandMenuTargetHighlighted, AActor *, Actor, AActor *, Target, int32, TargetCount);

// ==================== MENU DEPTH ====================

enum class ECombatMenuDepth : uint8
{
    Closed,
    Main,
    Submenu,
    TargetCategory,
    TargetSelection
};

// ==================== WHERE THE MENU OPENS (B-232b) ====================

namespace CommandMenuRules
{
	/** The command menu opens on the machine whose player commands the member (Q-232, D-150): never for an AI seat;
	 *  with no run state (standalone, specs) the one player commands everything, as before. */
	inline bool OpensHere(bool bAIControlled, bool bHasRunState, bool bCommandedHere)
	{
		return !bAIControlled && (!bHasRunState || bCommandedHere);
	}

	/** B-391 (Q-391): a category key that works on the main menu only (one key per category, two presses to act,
	 *  FeelGuide 13). The infusion keys and Back keep their own rules (B-093, B-436g). B-170e: End Turn too (its button
	 *  shows only in an action-pips fight, so its key does nothing otherwise). B-436g (D-258c): Defend too, so a press
	 *  of RB past the main menu is only ever the infusion level; one Skills key replaces Skills and Spells. */
	inline bool IsMainMenuKey(EPieMenuCategory Key)
	{
		switch (Key)
		{
		case EPieMenuCategory::Attack:
		case EPieMenuCategory::Skills:
		case EPieMenuCategory::Items:
		case EPieMenuCategory::Defend:
		case EPieMenuCategory::Flee:
		case EPieMenuCategory::SwitchWeapon:
		case EPieMenuCategory::ChangeRing:
		case EPieMenuCategory::EndTurn:
		case EPieMenuCategory::MoveRow: // B-588: Position
			return true;
		default:
			return false;
		}
	}

	/** B-588 (D-263): in the Position picker the main menu's first three keys pick its rows, top to bottom (1 / A
	 *  Front, 2 / X Middle, 3 / Y Back), as they press the main menu's first three rows; INDEX_NONE for other keys. */
	inline int32 PositionPickerRowForKey(EPieMenuCategory Key)
	{
		switch (Key)
		{
		case EPieMenuCategory::Attack:
			return 0;
		case EPieMenuCategory::Skills:
			return 1;
		case EPieMenuCategory::Items:
			return 2;
		default:
			return INDEX_NONE;
		}
	}

	/** B-391 (Q-391): whether a key presses a button. The Skills key (Abilities) also presses a weapon resonance, and
	 *  the Spells key (Refractions) a breakthrough or a ring resonance, so each class's skills and spells share a key.
	 *  OBSOLETE (B-436g, D-258a) past its first line: the main menu has one Skills button, so only Key == Button can
	 *  match there. Kept until PIE (docs/PIE/Sweep.md). */
	inline bool KeyPressesButton(EPieMenuCategory Key, EPieMenuCategory Button)
	{
		if (Key == Button)
		{
			return true;
		}
		if (Key == EPieMenuCategory::Abilities)
		{
			return Button == EPieMenuCategory::ResonateWeapon;
		}
		if (Key == EPieMenuCategory::Refractions)
		{
			return Button == EPieMenuCategory::Breakthrough || Button == EPieMenuCategory::ResonateRing;
		}
		return false;
	}
	/** B-399: the main menu (or a closed menu) and a sub-menu (Skills / Items / a spell source), as the camera sees
	 *  them (UCombatCameraDirector steps one closer for a sub-menu). */
	constexpr int32 MAIN_MENU_DEPTH = 0;
	constexpr int32 SUBMENU_DEPTH = 1;

	/** B-399 (study R2, R3, Q-399): the menu depth the camera follows. A target picker keeps the depth it was opened
	 *  from (Attack's picker the main menu's, a skill's the sub-menu's), so picking a target never moves the camera. */
	inline int32 CameraDepthFor(ECombatMenuDepth Depth, ECombatMenuDepth DepthBeforeTargets)
	{
		switch (Depth)
		{
		case ECombatMenuDepth::Submenu:
			return SUBMENU_DEPTH;
		case ECombatMenuDepth::TargetCategory:
		case ECombatMenuDepth::TargetSelection:
			return DepthBeforeTargets == ECombatMenuDepth::Submenu ? SUBMENU_DEPTH : MAIN_MENU_DEPTH;
		default:
			return MAIN_MENU_DEPTH;
		}
	}

	/** B-400: the index one step from Current in Direction's sign through Count entries, wrapping at both ends; the
	 *  first entry for an index out of range, INDEX_NONE for no entries. Direction 0 keeps Current. */
	inline int32 CycleIndex(int32 Current, int32 Direction, int32 Count)
	{
		if (Count <= 0)
		{
			return INDEX_NONE;
		}
		if (Current < 0 || Current >= Count)
		{
			return 0;
		}
		const int32 Step = FMath::Sign(Direction);
		return (Current + Step + Count) % Count;
	}

	/** B-592 (D-268): in a target step the Skills key (2 / X) is the weapon line and the Items key (3 / Y) the
	 *  infusion lines, as B-588's Position picker reuses 1-3; None for other keys. */
	inline EPieMenuCategory TargetPanelButtonForKey(EPieMenuCategory Key)
	{
		switch (Key)
		{
		case EPieMenuCategory::Skills:
			return EPieMenuCategory::TargetPanelWeapon;
		case EPieMenuCategory::Items:
			return EPieMenuCategory::TargetPanelInfusion;
		default:
			return EPieMenuCategory::None;
		}
	}
}

/** B-592 (D-268): the extra line the target step's panel shows; None is the small panel (heading and summary). */
enum class ETargetPanelLine : uint8
{
	None,
	Weapon,
	Infusion,
};

/** B-592 (D-268): a choice line of the target panel, for a click on one of its chips. */
enum class ETargetPanelChoice : uint8
{
	Weapon,
	Element,
	Level,
};

/** B-592 (D-268): what the target step's panel shows. A line's choices are empty when the line is not shown. */
struct FTargetPanelView
{
	/** A target step is open. */
	bool bShown = false;
	ETargetPanelLine Open = ETargetPanelLine::None;
	/** "Hammer · Lightning · Lv I": the weapon (Attack only), the element (when it can be chosen), the level. */
	FText Summary;
	TArray<FText> Weapons;
	int32 ActiveWeapon = INDEX_NONE;
	/** Empty for an origin-bound skill or spell (one fixed element): only the level line shows. */
	TArray<FText> Elements;
	int32 ActiveElement = INDEX_NONE;
	TArray<FText> Levels;
	int32 ActiveLevel = INDEX_NONE;
};

// ==================== SUBSYSTEM ====================

UCLASS()
class WORLD_OF_REFRACTION_API UCombatCommandMenuSubsystem : public UWoRFightSubsystem
{
    GENERATED_BODY()

public:
    // ==================== LIFECYCLE ====================

    virtual void Initialize(FSubsystemCollectionBase &Collection) override;

    // ==================== COMBAT REGISTRATION ====================

    /** Set the active combat orchestrator. Binds turn lifecycle events. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void SetCombatOrchestrator(class ACombatOrchestrator *Orchestrator);

    /** Clear combat orchestrator. Unbinds events and closes menu. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void ClearCombatOrchestrator();

    virtual void Deinitialize() override;

    // ==================== BLUEPRINT API ====================

    /** Call when a player turn starts. Builds capabilities and broadcasts OnCommandMenuReady. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void OpenForActor(AActor *Actor);

    /** Call when the player clicks a button. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void HandleSelection(const FPieMenuButtonData &ButtonData);

    /** Call when the player clicks Back. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void HandleBack();

    /** B-093: a key press for a menu button (Defend, CycleSource, CycleLevel). Presses the
     *  button the menu would show for it now, through HandleSelection, so the key obeys every
     *  rule the button does: Defend from the main menu (D-258c), the infusion keys in the target picker when it
     *  offers them, Back anywhere past the main menu (B-436g). False (nothing done) when there's no such button.
     *  B-391: the category keys (CommandMenuRules::IsMainMenuKey) press their main-menu button, from the main menu
     *  only (CommandMenuRules::KeyPressesButton: the first enabled match in menu order). */
    bool PressMenuKey(EPieMenuCategory Category);

    /** B-582 / B-582b (D-266, D-267): in an open single-target picker, press the highlighted target's row, as a click
     *  on it. False (nothing done) when no such picker is open or the row is greyed. PressMenuKey(Target) is the
     *  confirm keys (Enter / Space); PressMenuKey(Attack) in the Attack picker is the second 1 / A. */
    bool ConfirmHighlightedTarget();

    /** Call when the turn ends or combat closes the menu. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void Close();

    // ==================== DELEGATES ====================

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuReady OnCommandMenuReady;

    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuClosed OnCommandMenuClosed;

    /** B-399: once per menu-depth change (CommandMenuRules::CameraDepthFor), never for a redraw or a scroll. The
     *  combat camera listens (whichever HUD draws the menu). */
    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuDepthChanged OnMenuDepthChanged;

    /** B-399: the depth last broadcast on OnMenuDepthChanged. */
    int32 GetCameraMenuDepth() const { return BroadcastMenuDepth; }

    // ==================== TARGET HIGHLIGHT (B-400) ====================

    /** B-400: once per change of the single-target picker's highlighted target (the first valid target when it opens;
     *  null when it closes). The combat camera listens; a world marker can too. */
    UPROPERTY(BlueprintAssignable, Category = "Combat Command Menu|Events")
    FOnCommandMenuTargetHighlighted OnTargetHighlighted;

    /** B-400: highlight Target in the open single-target picker (a target button's hover or focus). Nothing for a
     *  target the picker doesn't offer. Confirm is unchanged: the pressed button's target is the one picked. */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void HighlightTarget(AActor *Target);

    /** B-400: highlight the next (Direction > 0) or previous (< 0) target, wrapping (wor.Camera.TargetCycle). */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void CycleHighlightedTarget(int32 Direction);

    /** B-400: the highlighted target, or null when no single-target picker is open. */
    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    AActor *GetHighlightedTarget() const { return HighlightedTarget.Get(); }

    /** B-400: how many targets the open single-target picker offers (0 when none is open). */
    int32 GetHighlightTargetCount() const { return HighlightTargets.Num(); }
    /** B-582c QA: the fighters the open single-target picker offers (the HUD's pointer pick). */
    const TArray<TWeakObjectPtr<AActor>> &GetHighlightTargets() const { return HighlightTargets; }

    /** B-582 (D-266): the target Fighter last attacked in this fight (null: none yet, or it is gone). The Attack picker
     *  opens on it while it is still offered. */
    AActor *GetLastAttackTarget(const AActor *Fighter) const;

    /** B-613 (Q-613): the infusion level Fighter's next picker opens on (0: off), for this fight only. It stays on
     *  until the player confirms an action at no infusion. */
    int32 GetStickyInfusionLevel(const AActor *Fighter) const;
    /** B-613: the open picker turned the sticky infusion off for this action, which can't pay its EP. */
    bool IsStickyInfusionDropped() const { return bStickyInfusionDropped; }

    // ==================== STATE QUERIES ====================

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    bool IsOpen() const { return bIsOpen; }

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    bool IsAtMainMenu() const { return CurrentDepth == ECombatMenuDepth::Main; }

    /** B-588 (D-263): the Position row picker (Front / Middle / Back) is open. */
    bool IsPositionPickerOpen() const { return CurrentDepth == ECombatMenuDepth::Submenu && ActiveSubmenuType == EPieMenuCategory::MoveRow; }
    /** B-400: the single-target picker is open (per-actor buttons: not Self, not a group). Public for B-582c (D-270):
     *  the battle HUD picks on the field then, with no target rows. */
    bool IsSingleTargetPickerOpen() const;

    /** B-588 (D-263): the picker's rows, Front, Middle, Back, then Back: the current row greyed "You are here.", a row
     *  the move is refused for greyed with why (a co-op client greys its current row only; the server checks the rest). */
    TArray<FPieMenuButtonData> BuildPositionButtons() const;

    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    const FCombatCapabilities &GetCurrentCapabilities() const { return CurrentCapabilities; }

    /** Get the actor whose menu is currently open (or null if closed). */
    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    AActor *GetCurrentActor() const { return CurrentActor.Get(); }

    /** B-034: the infusion level the open target picker will stamp on the action (0..2).
     *  The menu owns it; the actor's InfusionVFXComponent only shows it. */
    UFUNCTION(BlueprintPure, Category = "Combat Command Menu")
    int32 GetPendingInfusionLevel() const { return PendingInfusionLevel; }

    /** B-436g (D-260): where the open menu is, for its frame's header: "Skills > Abilities", "Attack > choose a
     *  target". Empty on the main menu and when closed. */
    FText GetViewTitle() const;

    /** B-599: "Force Strike (Fire)" for an infused action, the bare name with no infusion (Source empty). */
    static FText MakeInfusedActionName(const FText &Action, const FText &Source);
    /** B-599: the infusion line's button, "Fire infusion" ("Fire infusion level" when only the level can change);
     *  "Infusion" / "Infusion level" with no source named. */
    static FText MakeInfusionButtonName(const FText &Source, bool bChooseElement);

    /** B-592 (D-268): the target step's panel: small (heading and summary) until X / 2 opens the weapon line or Y / 3
     *  the infusion lines. bShown false outside a target step. */
    FTargetPanelView GetTargetPanel() const;

    /** B-592 (D-268): a click on a chip of the open line: that weapon, element or level (the keys step them). */
    void PickTargetPanelChoice(ETargetPanelChoice Line, int32 Index);

    /** B-592b: what the pending action would cost the acting character with the current level and source
     *  (UActionExecutor::PreviewActionCost); zero outside a target step. Read on each OnCommandMenuReady. */
    FInfusionCostPreview GetTargetStepCostPreview() const;

    // ==================== DEBUG ====================

    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu|Debug",
              meta = (DevelopmentOnly))
    void DebugLogCapabilities() const;

    /** Call this to re-broadcast the current main menu buttons — use when a widget binds late */
    UFUNCTION(BlueprintCallable, Category = "Combat Command Menu")
    void RefreshMenu();

    /** The Schools sub-menu: one button per school the spells hold, in school order, then Back (D-163: up to six
     *  plus Back, a gamepad radial's eight slots). Public and static for the headless spec (B-374). */
    static TArray<FPieMenuButtonData> BuildSchoolButtons(const TArray<USpellData *> &Spells);

    /** B-232c (client): the server ran a free step for Actor (ACombatPlayerController::Client_FreeCommandApplied);
     *  the open menu redraws from the stepped loadout. */
    void HandleFreeCommandApplied(AActor *Actor, ECombatFreeCommand Command);

private:
    /** B-232c: on a client, a free step goes to the server (ACombatPlayerController::RequestFreeCommand) instead of
     *  running here; true when it was sent. */
    bool SendFreeCommandToServer(ECombatFreeCommand Command);

    // ==================== ORCHESTRATOR BINDING ====================

    UPROPERTY()
    TWeakObjectPtr<class ACombatOrchestrator> CurrentOrchestrator;

    UFUNCTION()
    void HandlePlayerActionRequested(AActor *Actor);

    UFUNCTION()
    void HandleActionExecuted(AActor *Actor, const struct FActionResult &Result);

    // ==================== STATE ====================

    FCombatCapabilities CurrentCapabilities;
    ECombatMenuDepth CurrentDepth = ECombatMenuDepth::Closed;
    bool bIsOpen = false;
    EPieMenuCategory ActiveSubmenuSource = EPieMenuCategory::None;

    EPieMenuCategory ActiveSubmenuType = EPieMenuCategory::None;
    // Pending action awaiting target selection
    EPieMenuCategory PendingActionCategory = EPieMenuCategory::None;
    /** B-436g: the pending action's name, for GetViewTitle. */
    FText PendingActionLabel;
    FString PendingActionID;
    TWeakObjectPtr<UObject> PendingActionData;
    // Slot index for identity-by-index buttons (consumables). DataReference is
    // null for these, so the index must survive the target-selection round-trip.
    int32 PendingActionDataIndex = -1;
    ETargetType PendingTargetType = ETargetType::Enemy;
    /** Effective target type after the Allies/Enemies category step. Synced to
     *  PendingTargetType on entry; set to Ally/Enemy when a side is
     *  picked. ResolvedTargetType != PendingTargetType means the category step
     *  is active (used by Back routing and RebuildCurrentPicker). */
    ETargetType ResolvedTargetType = ETargetType::Enemy;
    /** Count axis carried alongside the type through the picker. Drives the
     *  group-vs-single decision (All -> group confirm; Single/Double -> per-actor
     *  picker). Narrowed to Single after the Allies/Enemies category step. */
    ETargetCount PendingTargetCount = ETargetCount::Single;
    ETargetCount ResolvedTargetCount = ETargetCount::Single;
    /** B-034: the pending action's infusion level. Reset to 0 on every picker entry and exit. */
    int32 PendingInfusionLevel = 0;
    /** B-592 (D-268): the target panel's open line; None on every picker entry. */
    ETargetPanelLine OpenTargetPanelLine = ETargetPanelLine::None;
    /** B-592: the infusion element can be chosen here (AppendInfusionControls' Source row: more than one source). */
    bool CanChooseInfusionElement() const;
    /** B-592: infusion applies to the pending action (AppendInfusionControls shows its rows). */
    bool CanInfusePendingAction() const;
    /** B-592: the target step's rows again, or the main menu when no target step is open (after a weapon switch). */
    void BroadcastAfterWeaponSwitch();
    ECombatMenuDepth DepthBeforeTargetSelection = ECombatMenuDepth::Main;
    TWeakObjectPtr<AActor> CurrentActor;

    /** B-399: while the menu is open, broadcasts OnMenuDepthChanged when the camera depth differs from the last one
     *  sent. Runs at the end of every call that can change the depth (OpenForActor, HandleSelection, HandleBack). */
    void BroadcastMenuDepthIfChanged();
    int32 BroadcastMenuDepth = CommandMenuRules::MAIN_MENU_DEPTH;

    /** B-400: re-reads the picker's targets and keeps the highlight on one of them (the first when it isn't), or
     *  clears it when no single-target picker is open. Runs next to BroadcastMenuDepthIfChanged and in Close. */
    void RefreshTargetHighlight();
    /** B-400: sets the highlight and broadcasts OnTargetHighlighted when it changed. */
    void SetHighlightedTarget(AActor *Target);
    int32 IndexOfHighlightTarget(const AActor *Target) const;
    TArray<TWeakObjectPtr<AActor>> HighlightTargets;
    TWeakObjectPtr<AActor> HighlightedTarget;
    /** B-582 (D-266): each fighter's last confirmed Attack target, for this fight only (cleared with the orchestrator). */
    TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<AActor>> LastAttackTargets;
    /** B-613 (Q-613): each fighter's last confirmed infusion, for this fight only (cleared with the orchestrator). */
    struct FStickyInfusion
    {
        /** Set once the player picked an element (an attack, an ability, a Breakthrough spell). */
        bool bHasSource = false;
        EInfusionSourceOption Source = EInfusionSourceOption::None;
        bool bIce = false;
        int32 Level = 0;
    };
    TMap<TWeakObjectPtr<const AActor>, FStickyInfusion> StickyInfusions;
    bool bStickyInfusionDropped = false;
    /** B-613: the open picker starts on the fighter's sticky infusion; off for this action when it can't pay the EP. */
    void ApplyStickyInfusion(UInfusionVFXComponent &VFX);
    /** B-613: on confirm, keeps the picked infusion for the next action; a confirm at no infusion turns it off. */
    void RememberStickyInfusion();

    /** BrokenDarknessManager of the actor whose menu is open — bound to
     *  OnAlignmentChanged for live BD spell-pool refresh. */
    TWeakObjectPtr<UBrokenDarknessManager> BoundBDManager;

    // ==================== MAIN MENU ====================

    TArray<FPieMenuButtonData> BuildMainMenuButtons() const;
    /** B-436g (D-258a, D-260): the actor's kinds of skill, names only (no counts), without Back. */
    TArray<FPieMenuButtonData> BuildSkillKindButtons() const;
    /** B-436g: Skills pressed: straight into the one kind's list, else the kinds. */
    void OpenSkills();
    void OpenSkillKinds();
    /** B-436g: broadcasts the open submenu's list (a school's spell list as its schools); false for none. */
    bool BroadcastCurrentSubmenu();

    // ==================== BUTTON CREATORS ====================

    FPieMenuButtonData CreateAttackButton() const;
    FPieMenuButtonData CreateAbilitiesButton() const;
    FPieMenuButtonData CreateResonateWeaponButton() const;
    FPieMenuButtonData CreateRefractionsButton() const;
    FPieMenuButtonData CreateBreakthroughButton() const;
    FPieMenuButtonData CreateResonateRingButton() const;
    FPieMenuButtonData CreateItemsButton() const;
    FPieMenuButtonData CreateSwitchWeaponButton() const;
    /** D-091: Flee, greyed with the reason when the fight can't be fled. */
    FPieMenuButtonData CreateFleeButton() const;
    /** OBSOLETE (B-346): the main menu no longer adds this button; seats are fixed for the fight
     *  (Q-21-audit, PA-13). Kept until PIE; the switch stays for debug (wor.Combat.SwitchControl).
     *  D-026: "Let AI Play" for a non-leader party member on its turn. */
    FPieMenuButtonData CreateHandToAIButton() const;
    bool CanHandToAI() const;
    /** B-453e (D-190): tells the orchestrator the player is still choosing (its idle clock starts again). */
    void NoteTurnActivity() const;
    /** D-096 placeholder: "Step Forward" / "Step Back", greyed with the reason. OBSOLETE (B-588): the main menu shows
     *  Position (CreatePositionButton) instead; kept until PIE. */
    FPieMenuButtonData CreateMoveRowButton(bool bForward) const;
    /** B-588 (D-263): "Position", a corner hint that opens the row picker; greyed with why when no row can be taken. */
    FPieMenuButtonData CreatePositionButton() const;
    /** ET-19: greys a switch button, with the reason, once the turn's free switch is used. */
    void GreyIfTurnSwitchUsed(FPieMenuButtonData &Button) const;
    FPieMenuButtonData CreateSwitchRingButton() const;
    FPieMenuButtonData CreateDefendButton() const;
    /** B-170e (ActionPips.md §3.5): "End Turn", on the main menu of an action-pips fight only. */
    FPieMenuButtonData CreateEndTurnButton() const;
    /** B-170e: in an action-pips fight, puts the action cost on an Attack, Ability, Spell or Item button (ActionCost,
     *  B-436g; the widget draws it) and greys it with the reason when the current actor can't pay it now or ever
     *  (ActionPipsText::GetCostRefusedReason). A button already greyed keeps its own reason. Nothing otherwise. */
    void ShowActionCost(FPieMenuButtonData &Button) const;
    /** B-625: "Not enough EP (6 needed)." when the current actor's spendable EP is below Action's EP cost (the check
     *  ValidateAction refuses with); empty otherwise. */
    FText GetNotEnoughEPReason(const FAction &Action) const;
    /** B-625: greys the target step's target buttons with GetNotEnoughEPReason when the action the confirm would
     *  submit (its picked infusion level included) costs more EP than the actor can spend; a lower level ungreys them. */
    void GreyTargetsShortOfEP(TArray<FPieMenuButtonData> &Buttons) const;

    // ==================== SUBMENU BUILDERS ====================

    TArray<FPieMenuButtonData> BuildSpellSubmenu(EPieMenuCategory Source) const;
    TArray<FPieMenuButtonData> BuildAbilitySubmenu() const;
    TArray<FPieMenuButtonData> BuildAbilityButtonsFrom(const TArray<UAbilityData *> &Abilities) const;
    TArray<FPieMenuButtonData> BuildSpellButtons(const TArray<USpellData *> &Spells,
                                                 EPieMenuSpellSchool School) const;
    TArray<FPieMenuButtonData> BuildItemsSubmenu() const;

    // ==================== SELECTION HANDLERS ====================

    void OpenAbilitySubmenu();
    void OpenSpellSubmenu(EPieMenuCategory Source);
    void ExecuteSwitchWeapon();
    void ExecuteMoveRow(bool bForward); // OBSOLETE (B-588): ExecuteMoveToRow; kept until PIE
    /** B-588: opens the Position picker (a sub-menu: only Back stays a corner hint). */
    void OpenPositionPicker();
    /** B-588: the move; back to the main menu once it is sent or made, the picker again on a refusal. */
    void ExecuteMoveToRow(ECombatRow Row);
    void ExecuteSwitchRing();
    void OpenItemsSubmenu();

    // Target selection
    void OpenTargetSelection(EPieMenuCategory ActionCategory, const FPieMenuButtonData &ActionButton, ETargetType TargetType, ETargetCount TargetCount);
    /** Two-button Allies / Enemies view shown before the per-actor picker when
     *  the action's TargetType is Anyone (single/double). Each button carries the
     *  narrowed ETargetType (Ally / Enemy) in DataIndex. */
    TArray<FPieMenuButtonData> BuildTargetCategoryButtons() const;
    /** Exit the picker back to the originating action menu (Submenu or Main):
     *  resets infusion, cancels the pending action, clears ResolvedTargetType. */
    void ExitTargetSelectionToActionMenu();
    TArray<FPieMenuButtonData> BuildTargetButtons(const TArray<AActor *> &Targets) const;
    TArray<FPieMenuButtonData> BuildGroupTargetButtons(ETargetType TargetType, const TArray<AActor *> &Targets) const;

    /** B-254 (DRY audit A11): the infusion controls (cycle level with the source readout, cycle source) a target
     *  picker ends with; none for items or an infusion-locked action. */
    void AppendInfusionControls(TArray<FPieMenuButtonData> &Buttons) const;
    /** Rebuild the active target picker using ResolvedTargetType + ResolvedTargetCount
     *  to pick between the per-actor picker (Single/Double) and the group confirm
     *  picker (auto-resolve: Self, or any TargetType with count All).
     *  ResolvedTargetType (not PendingTargetType) so a post-category-step picker
     *  rebuilds the narrowed single-team list, not the full Anyone set.
     *  Called when an infusion control mutates state and the picker needs to
     *  re-broadcast with the same shape it was opened with. */
    TArray<FPieMenuButtonData> RebuildCurrentPicker() const;
    TArray<AActor *> ResolveTargets(ETargetType TargetType) const;

    /** D-253: the item being aimed is a Sapphire (a DefyDeath item), so the target list also offers dead allies. */
    bool IsPendingItemRevive() const;
    void ConfirmActionWithTarget(AActor *SelectedTarget);
    void ConfirmActionWithTargets(const TArray<AActor *> &SelectedTargets);

    // ==================== ACTION SUBMISSION ====================

    FAction BuildActionFromButton(const FPieMenuButtonData &ButtonData) const;
    /** B-622: a weapon buff or debuff's action with the fighter's sticky infusion source on it, for its look only. */
    FAction WithStickyInfusionLook(const FAction &Action) const;
    friend class FCommandMenuKeysSpec; // B-622: checks what a submitted weapon buff carries; B-625: the EP greying
    ESpellSource MapCategoryToSpellSource(EPieMenuCategory SubmenuCategory) const;
    void SubmitConfirmedAction(const FPieMenuButtonData &ButtonData);

    // ==================== HELPERS ====================

    ULoadoutComponent *GetLoadoutComponent() const;
    FLinearColor GetElementColor(int32 ElementIndex) const;
    class UInfusionVFXComponent *GetInfusionVFXComponent() const;
    /** B-599: a cached infusion source's name ("Fire", "Ice", "Raw", "None"); empty for an index that isn't one. */
    FText GetInfusionSourceName(int32 SourceIndex) const;
    /** B-599: the chosen source's name for the title and the button; empty for None or no infusion component. */
    FText GetCurrentInfusionSourceName() const;
    /** D-001: a stunned actor's menu offers only Attack + Defend, with no infusion (D-023 A). */
    bool IsCurrentActorStunned() const;
    /** Infusion controls are inert for this pick: immune action/equipment, or a stunned actor. */
    bool IsInfusionLockedForPendingAction() const;
    /** B-034: store the pending infusion level (0 when infusion is locked for this pick, else
     *  clamped to 0..MAX_INFUSION_LEVEL) and show it on the actor's InfusionVFXComponent. */
    void SetPendingInfusionLevel(int32 Level);

    // ==================== CAPABILITIES / BD LIVE REFRESH ====================

    /** Rebuild CurrentCapabilities from CurrentActor's LoadoutComponent.
     *  Shared by OpenForActor, weapon/ring switches, and BD absorption refresh. */
    void RebuildCapabilities();

    /** Re-broadcast the menu view for the current depth — used after a live
     *  capability change so the open menu reflects it without a close/reopen. */
    void RebroadcastCurrentView();

    /** Bind / unbind CurrentActor's BrokenDarknessManager::OnAlignmentChanged so
     *  the menu refreshes when the absorbed element — and thus the castable BD
     *  spell pool — changes while the menu is open. */
    void BindBDAbsorptionDelegate();
    void UnbindBDAbsorptionDelegate();

    /** OnAlignmentChanged handler — rebuilds capabilities and re-broadcasts. */
    UFUNCTION()
    void HandleBDAlignmentChanged(AActor *Actor, ESpellElement OldElement, ESpellElement NewElement);
};
