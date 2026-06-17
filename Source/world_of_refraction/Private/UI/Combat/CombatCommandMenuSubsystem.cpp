// CombatCommandMenuSubsystem.cpp
// World of Refraction - Combat UI

#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Loadout/LoadoutComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Combat/TurnManager.h"
#include "Character/CharacterData.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Loadout/FCombatLoadout.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionStructs.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Engine/GameInstance.h"

namespace
{
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

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Combat orchestrator set"));
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

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Combat orchestrator cleared"));
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

    // AI actors handle their own decisions; we only show the menu for player-controlled actors.
    if (Orchestrator->IsActorAIControlled(Actor))
    {
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

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Opened for %s — %d buttons"),
           *Actor->GetName(), Buttons.Num());
}

void UCombatCommandMenuSubsystem::HandleSelection(const FPieMenuButtonData &ButtonData)
{
    if (!bIsOpen)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Selected: %s (ID: %s)"),
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
        const ETargetType TT = Attack ? Attack->TargetType : ETargetType::SingleEnemy;
        OpenTargetSelection(EPieMenuCategory::Attack, ButtonData, TT);
        break;
    }

    case EPieMenuCategory::Ability:
    {
        UAbilityData *Ability = Cast<UAbilityData>(ButtonData.DataReference);
        ETargetType TT = Ability ? Ability->TargetType : ETargetType::SingleEnemy;
        OpenTargetSelection(EPieMenuCategory::Ability, ButtonData, TT);
        break;
    }

    case EPieMenuCategory::Spell:
    {
        USpellData *Spell = Cast<USpellData>(ButtonData.DataReference);
        ETargetType TT = Spell ? Spell->TargetType : ETargetType::SingleEnemy;
        OpenTargetSelection(EPieMenuCategory::Spell, ButtonData, TT);
        break;
    }

    case EPieMenuCategory::Item:
    {
        UEvolutionItemData *Item = Cast<UEvolutionItemData>(ButtonData.DataReference);
        const ETargetType TT = Item ? Item->GetItemTargetType() : ETargetType::SingleAnyone;
        OpenTargetSelection(EPieMenuCategory::Item, ButtonData, TT);
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
        // per-actor picker. The narrowed type is carried in DataIndex.
        ResolvedTargetType = static_cast<ETargetType>(ButtonData.DataIndex);
        const TArray<AActor *> Targets = ResolveTargets(ResolvedTargetType);
        CurrentDepth = ECombatMenuDepth::TargetSelection;
        OnCommandMenuReady.Broadcast(BuildTargetButtons(Targets));
        break;
    }

    case EPieMenuCategory::Abilities:
        OpenAbilitySubmenu();
        break;

    case EPieMenuCategory::Items:
        OpenItemsSubmenu();
        break;

    case EPieMenuCategory::ResonateWeapon:
        if (CurrentCapabilities.bHasAugmentStoneAbilities)
        {
            // Augment stone: list augment-stone abilities as Ability buttons under the
            // same Resonate Weapon branch. Leaf buttons carry EPieMenuCategory::Ability
            // so they exit via the ability action path — no spell source. ActiveSubmenuSource
            // is deliberately left unset (the ability path never reads it).
            CurrentDepth = ECombatMenuDepth::Submenu;
            ActiveSubmenuType = EPieMenuCategory::ResonateWeapon;
            OnCommandMenuReady.Broadcast(
                BuildAbilityButtonsFrom(CurrentCapabilities.AugmentStoneAbilities));
        }
        else
        {
            OpenSpellSubmenu(EPieMenuCategory::ResonateWeapon);
        }
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
        OnCommandMenuReady.Broadcast(SpellButtons);
        break;
    }

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
            if (VFX->GetCurrentInfusionLevel() > 0)
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
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            VFX->CycleInfusionLevel();
        }
        // Re-broadcast picker with updated label — preserve original shape
        // (group confirm vs per-actor) via RebuildCurrentPicker.
        if (CurrentDepth == ECombatMenuDepth::TargetSelection)
        {
            OnCommandMenuReady.Broadcast(RebuildCurrentPicker());
        }
        break;
    }

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
    if (!bIsOpen)
    {
        return;
    }

    switch (CurrentDepth)
    {
    case ECombatMenuDepth::TargetSelection:
        if (ResolvedTargetType != PendingTargetType)
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
        CurrentDepth = ECombatMenuDepth::Main;
        ActiveSubmenuSource = EPieMenuCategory::None;
        ActiveSubmenuType = EPieMenuCategory::None; // NEW
        OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
        break;

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
    UnbindBDAbsorptionDelegate();
    CurrentActor.Reset();
    OnCommandMenuClosed.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Closed"));
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

    CurrentCapabilities = FCombatCapabilities::BuildFrom(
        LC,
        CDC->CharacterData->CharacterClass,
        CDC->CharacterData->InnateElement,
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
        switch (ActiveSubmenuType)
        {
        case EPieMenuCategory::Abilities:
            OnCommandMenuReady.Broadcast(BuildAbilitySubmenu());
            break;
        case EPieMenuCategory::Items:
            OnCommandMenuReady.Broadcast(BuildItemsSubmenu());
            break;
        case EPieMenuCategory::ResonateWeapon:
            if (CurrentCapabilities.bHasAugmentStoneAbilities)
                OnCommandMenuReady.Broadcast(
                    BuildAbilityButtonsFrom(CurrentCapabilities.AugmentStoneAbilities));
            else
                OnCommandMenuReady.Broadcast(
                    BuildSchoolButtons(CurrentCapabilities.GetSpellsForCategory(EPieMenuCategory::ResonateWeapon)));
            break;

        case EPieMenuCategory::Refractions:
        case EPieMenuCategory::Breakthrough:
        case EPieMenuCategory::ResonateRing:
            OnCommandMenuReady.Broadcast(
                BuildSchoolButtons(CurrentCapabilities.GetSpellsForCategory(ActiveSubmenuSource)));
            break;
        default:
            break;
        }
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

    if (CurrentCapabilities.bCanAttack)
        Buttons.Add(CreateAttackButton());
    if (CurrentCapabilities.bCanUseAbilities)
        Buttons.Add(CreateAbilitiesButton());
    if (CurrentCapabilities.bHasWeaponCrystal || CurrentCapabilities.bHasAugmentStoneAbilities)
        Buttons.Add(CreateResonateWeaponButton());
    if (CurrentCapabilities.bHasRefractions)
        Buttons.Add(CreateRefractionsButton());
    if (CurrentCapabilities.bHasBreakthrough)
        Buttons.Add(CreateBreakthroughButton());
    if (CurrentCapabilities.ShouldShowResonateRing())
        Buttons.Add(CreateResonateRingButton());
    if (CurrentCapabilities.bHasItems)
        Buttons.Add(CreateItemsButton());
    if (CurrentCapabilities.bCanSwitchWeapon)
        Buttons.Add(CreateSwitchWeaponButton());
    if (CurrentCapabilities.bCanSwitchRing)
        Buttons.Add(CreateSwitchRingButton());

    return Buttons;
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
    if (CurrentCapabilities.bHasAugmentStoneAbilities && !CurrentCapabilities.bHasWeaponCrystal)
    {
        // Augment stone (ability mode): count is abilities, not spells. A augment stone has
        // no spell-element color, so leave the default (neutral) tint.
        Button.Description = FText::FromString(
            FString::Printf(TEXT("%d extra abilities"), CurrentCapabilities.AugmentStoneAbilities.Num()));
    }
    else
    {
        Button.Description = FText::FromString(
            FString::Printf(TEXT("%d spells"), CurrentCapabilities.WeaponCrystalSpells.Num()));
        Button.ButtonTint = CurrentCapabilities.WeaponCrystalColor;
    }
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
    Button.DisplayName = FText::FromString(TEXT("Resonate (R)"));
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

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateSwitchWeaponButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("SwitchWeapon");
    Button.DisplayName = FText::FromString(TEXT("Switch Weapon"));
    Button.Category = EPieMenuCategory::SwitchWeapon;
    Button.bEnabled = true;
    return Button;
}

FPieMenuButtonData UCombatCommandMenuSubsystem::CreateSwitchRingButton() const
{
    FPieMenuButtonData Button;
    Button.ButtonID = TEXT("SwitchRing");
    Button.DisplayName = FText::FromString(TEXT("Switch Ring"));
    Button.Category = EPieMenuCategory::ChangeRing;
    Button.bEnabled = true;
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
    return BuildAbilityButtonsFrom(CurrentCapabilities.WeaponAbilities);
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
        Buttons.Add(Button);
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildItemsSubmenu() const
{
    TArray<FPieMenuButtonData> Buttons;

    // Walk loadout slots so we can display remaining uses alongside the item
    ULoadoutComponent *LC = GetLoadoutComponent();
    if (LC)
    {
        const FCombatLoadout &Loadout = LC->GetActiveLoadout();

        for (int32 SlotIndex = 0; SlotIndex < Loadout.ItemSlots.Num(); ++SlotIndex)
        {
            const FItemLoadoutSlot &Slot = Loadout.ItemSlots[SlotIndex];
            if (Slot.IsEmpty())
                continue;

            const FString DisplayName = FString::Printf(TEXT("%dx %s"),
                                                        Slot.Quantity,
                                                        *ItemIdentity::GetDisplayName(Slot.CrystalId));

            // Carry the slot index (DataIndex), not a UObject* — consumables have
            // no asset identity now; the click handler reads the live CrystalId
            // from Loadout.ItemSlots[DataIndex].
            FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
                FString::Printf(TEXT("Item_%d"), SlotIndex),
                FText::FromString(DisplayName),
                EPieMenuCategory::Item,
                nullptr,
                SlotIndex);

            Button.bEnabled = true;
            Buttons.Add(Button);
        }
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildSchoolButtons(
    const TArray<USpellData *> &Spells) const
{
    TArray<FPieMenuButtonData> Buttons;

    // Count spells per school
    TMap<EPieMenuSpellSchool, int32> SchoolCounts;
    for (USpellData *Spell : Spells)
    {
        if (!Spell)
            continue;
        EPieMenuSpellSchool School = static_cast<EPieMenuSpellSchool>(
            static_cast<int32>(Spell->School));
        SchoolCounts.FindOrAdd(School)++;
    }

    // Add a button for each school that has spells
    for (auto &Pair : SchoolCounts)
    {
        Buttons.Add(FPieMenuButtonData::MakeSchoolButton(Pair.Key, Pair.Value));
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

        EPieMenuSpellSchool SpellSchool = static_cast<EPieMenuSpellSchool>(
            static_cast<int32>(Spell->School));

        if (SpellSchool != School)
            continue;
        if (Index >= 6)
            break; // Max 6 per school

        FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
            FString::Printf(TEXT("Spell_%d"), Index),
            FText::FromString(Spell->Name),
            EPieMenuCategory::Spell,
            Spell,
            Index);

        Button.Description = FText::FromString(Spell->Description);
        Button.ButtonTint = GetElementColor(static_cast<int32>(Spell->Element));
        Button.bEnabled = true;
        Buttons.Add(Button);
        Index++;
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

// ==================== SELECTION HANDLERS ====================

void UCombatCommandMenuSubsystem::ExecuteSwitchWeapon()
{
    if (!CurrentActor.IsValid())
        return;

    ULoadoutComponent *LC = GetLoadoutComponent();
    if (!LC)
        return;

    LC->ToggleEquipment();

    // Rebuild capabilities with the new active weapon and refresh main menu
    RebuildCapabilities();

    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

void UCombatCommandMenuSubsystem::ExecuteSwitchRing()
{
    if (!CurrentActor.IsValid())
        return;

    ULoadoutComponent *LC = GetLoadoutComponent();
    if (!LC)
        return;

    // Advance ring index, wrap around
    const FCombatLoadout &Loadout = LC->GetActiveLoadout();
    int32 RingCount = 0;
    for (const FRingLoadoutEntry &Entry : Loadout.RingLoadout)
        if (Entry.IsValid())
            RingCount++;

    if (RingCount < 2)
        return;

    int32 NextIndex = (Loadout.ActiveRingIndex + 1) % RingCount;
    LC->SetActiveRingIndex(NextIndex);

    // Rebuild capabilities with new active ring
    RebuildCapabilities();

    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

// ==================== TARGET SELECTION ====================

void UCombatCommandMenuSubsystem::OpenTargetSelection(
    EPieMenuCategory ActionCategory,
    const FPieMenuButtonData &ActionButton,
    ETargetType TargetType)
{
    // Reset immunity on every entry — guarantees a clean VFX state regardless
    // of how the previous interaction exited (back, commit, combat end, death).
    if (UInfusionVFXComponent *EntryVFX = GetInfusionVFXComponent())
    {
        EntryVFX->SetImmuneToInfusion(false);
    }

    // Stash the pending action
    PendingActionCategory = ActionCategory;
    PendingActionID = ActionButton.ButtonID;
    PendingActionData = ActionButton.DataReference;
    PendingActionDataIndex = ActionButton.DataIndex; // carry slot index (consumables)
    PendingTargetType = TargetType;
    // Synced on entry — diverges only once a side is picked in the category step.
    ResolvedTargetType = TargetType;
    DepthBeforeTargetSelection = CurrentDepth;

    // Log target type
    const UEnum *TargetEnum = StaticEnum<ETargetType>();
    FString TargetTypeName = TargetEnum
                                 ? TargetEnum->GetNameStringByValue(static_cast<int64>(TargetType))
                                 : TEXT("Unknown");
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] OpenTargetSelection: TargetType=%s"),
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
            const bool bImmune = ResolveImmuneFlag(PendingActionData.Get(), CurrentActor.Get());
            VFX->SetImmuneToInfusion(bImmune);

            VFX->CacheAvailableSources(); // refresh available sources
            VFX->SetInfusionLevel(0);     // reset to L0 (no VFX)

            // Breakthrough spells are the only spell submenu that exposes a
            // CycleSource button — narrow the cache to the meaningful
            // evolution-cast subset {Evolution, Innate}. Other spell submenus
            // stay element-locked (no cycle UI), so no filtering needed for them.
            if (ActionCategory == EPieMenuCategory::Spell &&
                ActiveSubmenuSource == EPieMenuCategory::Breakthrough)
            {
                VFX->RestrictCachedSources({
                    EInfusionSourceOption::Evolution,
                    EInfusionSourceOption::Innate});
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
            }

            UE_LOG(LogTemp, Verbose,
                   TEXT("[CombatCommandMenu] Picker opened — infusion reset to L0"));
        }
    }

    // Resolve targets
    TArray<AActor *> Targets = ResolveTargets(TargetType);

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]   Resolved %d targets:"), Targets.Num());
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
        UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]     - %s"), *TName);
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

    // SingleAnyone spans both teams — drill in via an Allies / Enemies category
    // step first, then narrow to a single-team per-actor list on selection.
    if (TargetType == ETargetType::SingleAnyone)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]   Showing Allies/Enemies category step"));
        CurrentDepth = ECombatMenuDepth::TargetCategory;
        OnCommandMenuReady.Broadcast(BuildTargetCategoryButtons());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]   Showing target picker"));

    CurrentDepth = ECombatMenuDepth::TargetSelection;

    TArray<FPieMenuButtonData> TargetButtons;
    switch (TargetType)
    {
    case ETargetType::Self:
    case ETargetType::AllEnemies:
    case ETargetType::AllAllies:
    case ETargetType::Everyone:
        TargetButtons = BuildGroupTargetButtons(TargetType, Targets);
        break;

    default:
        // SingleEnemy / SingleAlly — single-team per-actor picker
        TargetButtons = BuildTargetButtons(Targets);
        break;
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
    Allies.DataIndex = static_cast<int32>(ETargetType::SingleAlly);
    Allies.ButtonTint = AllyTint;
    Allies.bEnabled = true;
    Buttons.Add(Allies);

    FPieMenuButtonData Enemies;
    Enemies.ButtonID = TEXT("TargetCategory_Enemies");
    Enemies.DisplayName = FText::FromString(TEXT("Enemies"));
    Enemies.Category = EPieMenuCategory::TargetCategory;
    Enemies.DataIndex = static_cast<int32>(ETargetType::SingleEnemy);
    Enemies.ButtonTint = EnemyTint;
    Enemies.bEnabled = true;
    Buttons.Add(Enemies);

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

void UCombatCommandMenuSubsystem::ExitTargetSelectionToActionMenu()
{
    // Reset infusion state — picker is closing without commit
    if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
    {
        VFX->SetInfusionLevel(0);
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
        switch (ActiveSubmenuType)
        {
        case EPieMenuCategory::Abilities:
            OnCommandMenuReady.Broadcast(BuildAbilitySubmenu());
            break;
        case EPieMenuCategory::Items:
            OnCommandMenuReady.Broadcast(BuildItemsSubmenu());
            break;
        case EPieMenuCategory::Refractions:
        case EPieMenuCategory::Breakthrough:
        case EPieMenuCategory::ResonateWeapon:
        case EPieMenuCategory::ResonateRing:
        {
            const TArray<USpellData *> &Spells =
                CurrentCapabilities.GetSpellsForCategory(ActiveSubmenuSource);
            OnCommandMenuReady.Broadcast(BuildSchoolButtons(Spells));
            break;
        }
        default:
            // Unknown submenu type, fall back to main
            CurrentDepth = ECombatMenuDepth::Main;
            ActiveSubmenuSource = EPieMenuCategory::None;
            ActiveSubmenuType = EPieMenuCategory::None;
            OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
            break;
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
        TM = GI->GetSubsystem<UTurnManager>();
    }
    if (TM)
    {
        if (AActor *User = CurrentActor.Get())
        {
            UserTeam = TM->GetActorTeam(User);
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
        Button.DisplayName = FText::FromString(TargetName);
        Button.Category = EPieMenuCategory::Target;
        Button.DataReference = Target;
        Button.ButtonTint = Tint;
        Button.bEnabled = true;
        return Button;
    };

    // Flat single-team list — enemies tinted red, allies default. SingleAnyone
    // is split into teams upstream by the Allies/Enemies category step, so this
    // picker only ever receives one side's actors.
    for (AActor *Target : Targets)
    {
        if (!Target)
            continue;

        FLinearColor Tint = AllyTint;
        if (UserTeam >= 0 && TM)
        {
            const int32 TargetTeam = TM->GetActorTeam(Target);
            if (TargetTeam >= 0 && TargetTeam != UserTeam)
            {
                Tint = EnemyTint;
            }
        }
        Buttons.Add(MakeTargetButton(Target, Tint));
    }

    // ==================== INFUSION CONTROLS ====================
    // Below targets, minimal buttons. Items skip infusion entirely.
    // Actions flagged bImmuneToInfusion also skip — the controls would be inert.
    // Equipment-level immunity (active weapon / ring on the wielder) ORs in.
    //   - Cycle Level button doubles as the state readout: "<source>: L<n>"
    //     Pressing it cycles 0 -> 1 -> 2 -> 0.
    //   - Cycle Source is a separate button, only for Attack / Ability.
    //   - Spells show their intrinsic source in the label (not cyclable).
    if (PendingActionCategory != EPieMenuCategory::Item &&
        !ResolveImmuneFlag(PendingActionData.Get(), CurrentActor.Get()))
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

            FString SourceLabel;
            switch (SourceLookup)
            {
            case EPieMenuCategory::Refractions:
                SourceLabel = TEXT("Innate");
                break;
            case EPieMenuCategory::ResonateWeapon:
                SourceLabel = TEXT("Weapon Crystal");
                break;
            case EPieMenuCategory::ResonateRing:
                SourceLabel = TEXT("Ring");
                break;
            case EPieMenuCategory::Attack:
            case EPieMenuCategory::Ability:
            case EPieMenuCategory::Breakthrough:
            default:
                // Breakthrough spells can cycle source (Evolution / Innate),
                // so label reads from the live VFX state like Attack/Ability.
                SourceLabel = VFX->GetCurrentSourceName();
                break;
            }

            // Combined cycle-level + state-display button
            const FString CycleLvlText = FString::Printf(
                TEXT("%s: L%d"),
                *SourceLabel,
                VFX->GetCurrentInfusionLevel());

            FPieMenuButtonData CycleLvl;
            CycleLvl.ButtonID = TEXT("CycleLevel");
            CycleLvl.DisplayName = FText::FromString(CycleLvlText);
            CycleLvl.Category = EPieMenuCategory::CycleLevel;
            CycleLvl.bEnabled = true;
            Buttons.Add(CycleLvl);

            // Cycle Source — Attack / Ability always; Breakthrough spells too
            // (so BD can pick Innate for Darkness-conversion). Other spell
            // submenus stay element-locked. Suppressed when the filtered cache
            // has ≤1 entry (cycling would be a no-op).
            const bool bShowCycleSource =
                ((PendingActionCategory == EPieMenuCategory::Attack) ||
                 (PendingActionCategory == EPieMenuCategory::Ability) ||
                 (PendingActionCategory == EPieMenuCategory::Spell &&
                  ActiveSubmenuSource == EPieMenuCategory::Breakthrough)) &&
                VFX->GetCachedSources().Num() > 1;

            if (bShowCycleSource)
            {
                FPieMenuButtonData CycleSrc;
                CycleSrc.ButtonID = TEXT("CycleSource");
                CycleSrc.DisplayName = FText::FromString(TEXT("Cycle Source"));
                CycleSrc.Category = EPieMenuCategory::CycleSource;
                CycleSrc.bEnabled = true;
                Buttons.Add(CycleSrc);
            }
        }
    }

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
    case ETargetType::AllEnemies:
        ConfirmBtn.ButtonID = TEXT("GroupTarget_AllEnemies");
        ConfirmBtn.DisplayName = FText::FromString(TEXT("All Enemies"));
        break;
    case ETargetType::AllAllies:
        ConfirmBtn.ButtonID = TEXT("GroupTarget_AllAllies");
        ConfirmBtn.DisplayName = FText::FromString(TEXT("All Allies"));
        break;
    case ETargetType::Everyone:
        ConfirmBtn.ButtonID = TEXT("GroupTarget_Everyone");
        ConfirmBtn.DisplayName = FText::FromString(TEXT("Everyone"));
        break;
    default:
        // Should never hit this — caller routes only group types here
        break;
    }
    Buttons.Add(ConfirmBtn);

    // ==================== INFUSION CONTROLS ====================
    // Same block as BuildTargetButtons. Items skip infusion entirely.
    // Actions flagged bImmuneToInfusion also skip — the controls would be inert.
    // Equipment-level immunity (active weapon / ring on the wielder) ORs in.
    //   - Cycle Level button doubles as the state readout: "<source>: L<n>"
    //   - Cycle Source is a separate button, only for Attack / Ability.
    //   - Spells show their intrinsic source in the label (not cyclable).
    if (PendingActionCategory != EPieMenuCategory::Item &&
        !ResolveImmuneFlag(PendingActionData.Get(), CurrentActor.Get()))
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

            FString SourceLabel;
            switch (SourceLookup)
            {
            case EPieMenuCategory::Refractions:
                SourceLabel = TEXT("Innate");
                break;
            case EPieMenuCategory::ResonateWeapon:
                SourceLabel = TEXT("Weapon Crystal");
                break;
            case EPieMenuCategory::ResonateRing:
                SourceLabel = TEXT("Ring");
                break;
            case EPieMenuCategory::Attack:
            case EPieMenuCategory::Ability:
            case EPieMenuCategory::Breakthrough:
            default:
                // Breakthrough spells can cycle source (Evolution / Innate),
                // so label reads from the live VFX state like Attack/Ability.
                SourceLabel = VFX->GetCurrentSourceName();
                break;
            }

            // Combined cycle-level + state-display button
            const FString CycleLvlText = FString::Printf(
                TEXT("%s: L%d"),
                *SourceLabel,
                VFX->GetCurrentInfusionLevel());

            FPieMenuButtonData CycleLvl;
            CycleLvl.ButtonID = TEXT("CycleLevel");
            CycleLvl.DisplayName = FText::FromString(CycleLvlText);
            CycleLvl.Category = EPieMenuCategory::CycleLevel;
            CycleLvl.bEnabled = true;
            Buttons.Add(CycleLvl);

            // Cycle Source — Attack / Ability always; Breakthrough spells too
            // (so BD can pick Innate for Darkness-conversion). Other spell
            // submenus stay element-locked. Suppressed when the filtered cache
            // has ≤1 entry (cycling would be a no-op).
            const bool bShowCycleSource =
                ((PendingActionCategory == EPieMenuCategory::Attack) ||
                 (PendingActionCategory == EPieMenuCategory::Ability) ||
                 (PendingActionCategory == EPieMenuCategory::Spell &&
                  ActiveSubmenuSource == EPieMenuCategory::Breakthrough)) &&
                VFX->GetCachedSources().Num() > 1;

            if (bShowCycleSource)
            {
                FPieMenuButtonData CycleSrc;
                CycleSrc.ButtonID = TEXT("CycleSource");
                CycleSrc.DisplayName = FText::FromString(TEXT("Cycle Source"));
                CycleSrc.Category = EPieMenuCategory::CycleSource;
                CycleSrc.bEnabled = true;
                Buttons.Add(CycleSrc);
            }
        }
    }

    Buttons.Add(FPieMenuButtonData::MakeBackButton());
    return Buttons;
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::RebuildCurrentPicker() const
{
    // ResolvedTargetType — not PendingTargetType — so that after the category
    // step this rebuilds the narrowed single-team list, not the SingleAnyone
    // set. The two are equal whenever no category step was taken.
    const TArray<AActor *> Targets = ResolveTargets(ResolvedTargetType);
    switch (ResolvedTargetType)
    {
    case ETargetType::Self:
    case ETargetType::AllEnemies:
    case ETargetType::AllAllies:
    case ETargetType::Everyone:
        return BuildGroupTargetButtons(ResolvedTargetType, Targets);
    default:
        return BuildTargetButtons(Targets);
    }
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

    UTurnManager *TM = GI->GetSubsystem<UTurnManager>();
    if (!TM)
        return Result;

    int32 UserTeam = TM->GetActorTeam(User);
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

    switch (TargetType)
    {
    case ETargetType::Self:
        Result.Add(User);
        break;

    case ETargetType::SingleEnemy:
    case ETargetType::AllEnemies:
    {
        int32 EnemyTeam = (UserTeam == 0) ? 1 : 0;
        for (AActor *Member : TM->GetTeamMembers(EnemyTeam))
        {
            if (IsAlive(Member)) // <-- was: TM->IsActorAlive(Member)
            {
                Result.Add(Member);
            }
        }
        break;
    }

    case ETargetType::SingleAlly:
    case ETargetType::AllAllies:
        for (AActor *Member : TM->GetTeamMembers(UserTeam))
        {
            if (IsAlive(Member)) // <-- was: TM->IsActorAlive(Member)
            {
                Result.Add(Member);
            }
        }
        break;
    case ETargetType::SingleAnyone:
    case ETargetType::Everyone:
        for (AActor *Member : TM->GetTeamMembers(0))
        {
            if (IsAlive(Member))
                Result.Add(Member); // <-- was: TM->IsActorAlive(Member)
        }
        for (AActor *Member : TM->GetTeamMembers(1))
        {
            if (IsAlive(Member))
                Result.Add(Member); // <-- was: TM->IsActorAlive(Member)
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

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Action confirmed: %s on %s"),
           *PendingActionID,
           *TargetName);

    // Clear pending state
    PendingActionCategory = EPieMenuCategory::None;
    PendingActionID.Empty();
    PendingActionData.Reset();
    PendingActionDataIndex = -1;
    ResolvedTargetType = PendingTargetType;

    SubmitConfirmedAction(ActionButton);
    Close();
}

void UCombatCommandMenuSubsystem::ConfirmActionWithTargets(const TArray<AActor *> &SelectedTargets)
{
    FPieMenuButtonData ActionButton;
    ActionButton.ButtonID = PendingActionID;
    ActionButton.Category = PendingActionCategory;
    ActionButton.DataReference = PendingActionData.Get();
    ActionButton.DataIndex = PendingActionDataIndex; // restore slot index (consumables)
    ActionButton.Targets = SelectedTargets;

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Action confirmed: %s on %d targets"),
           *PendingActionID, SelectedTargets.Num());

    // Clear pending state — same pattern as ConfirmActionWithTarget
    PendingActionCategory = EPieMenuCategory::None;
    PendingActionID.Empty();
    PendingActionData.Reset();
    PendingActionDataIndex = -1;
    ResolvedTargetType = PendingTargetType;

    SubmitConfirmedAction(ActionButton);
    Close();
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
        // Consumable identity comes from the live loadout slot the button points
        // at (DataIndex), not a UObject* payload.
        if (ULoadoutComponent *LC = GetLoadoutComponent())
        {
            const FCombatLoadout &Loadout = LC->GetActiveLoadout();
            if (Loadout.ItemSlots.IsValidIndex(ButtonData.DataIndex))
            {
                Action.ItemData = Loadout.ItemSlots[ButtonData.DataIndex].CrystalId;
            }
            else
            {
                // Surface a dropped slot index instead of a silent empty action.
                UE_LOG(LogTemp, Warning,
                       TEXT("[CombatCommandMenu] Item action has invalid slot DataIndex=%d (ItemSlots.Num=%d) — ItemData left empty"),
                       ButtonData.DataIndex, Loadout.ItemSlots.Num());
            }
        }
        break;
    }
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

    // Infusion stamp — items skip entirely
    if (Action.ActionType != EActionType::Item)
    {
        if (UInfusionVFXComponent *VFX = GetInfusionVFXComponent())
        {
            const int32 Level = VFX->GetCurrentInfusionLevel();
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

    UE_LOG(LogTemp, Log,
           TEXT("[CombatCommandMenu] Submitting action: type=%d targets=%d level(spell/ab)=%d/%d source=%d spellSource=%d"),
           static_cast<int32>(Action.ActionType),
           Action.Targets.Num(),
           Action.SpellInfusionLevel,
           Action.AbilityInfusionLevel,
           static_cast<int32>(Action.SelectedSource),
           static_cast<int32>(Action.SpellSource));

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

    Orchestrator->SubmitActionAsync(Action);
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
    UE_LOG(LogTemp, Log, TEXT("ActiveWeapon:   %s"), *CurrentCapabilities.ActiveWeaponName);
    UE_LOG(LogTemp, Log, TEXT("ActiveRing:     %s"), *CurrentCapabilities.ActiveRingName);
    UE_LOG(LogTemp, Log, TEXT("BreakChance:    %s"), CurrentCapabilities.bRingHasBreakChance ? TEXT("YES") : TEXT("NO"));
}

void UCombatCommandMenuSubsystem::RefreshMenu()
{
    if (!bIsOpen)
        return;
    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}