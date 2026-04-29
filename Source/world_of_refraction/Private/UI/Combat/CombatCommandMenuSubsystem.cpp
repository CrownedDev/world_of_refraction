// CombatCommandMenuSubsystem.cpp
// World of Refraction - Combat UI

#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "LoadoutComponent.h"
#include "CharacterDataComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "WeaponAttackData.h"
#include "ItemData.h"
#include "TurnManager.h"
#include "CharacterData.h"
#include "Engine/GameInstance.h"

// ==================== LIFECYCLE ====================

void UCombatCommandMenuSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Initialized"));
}

void UCombatCommandMenuSubsystem::Deinitialize()
{
    CurrentActor.Reset();
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Deinitialized"));
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
    CurrentCapabilities = FCombatCapabilities::BuildFrom(
        LC,
        CDC->CharacterData->CharacterClass,
        CDC->CharacterData->InnateElement,
        [this](int32 ElementIndex)
        { return GetElementColor(ElementIndex); });

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
        OpenTargetSelection(EPieMenuCategory::Attack, ButtonData, ETargetType::SingleEnemy);
        break;

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
        UItemData *Item = Cast<UItemData>(ButtonData.DataReference);
        // Items default to SingleAlly (consumables on self/party); override per item if needed later
        ETargetType TT = ETargetType::SingleAlly;
        OpenTargetSelection(EPieMenuCategory::Item, ButtonData, TT);
        break;
    }

    case EPieMenuCategory::Target:
    {
        AActor *Target = Cast<AActor>(ButtonData.DataReference);
        ConfirmActionWithTarget(Target);
        break;
    }

    case EPieMenuCategory::Abilities:
        OpenAbilitySubmenu();
        break;

    case EPieMenuCategory::Refractions:
    case EPieMenuCategory::Breakthrough:
    case EPieMenuCategory::ResonateWeapon:
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
    {
        // Cancel pending action and return to whatever depth we came from
        PendingActionCategory = EPieMenuCategory::None;
        PendingActionID.Empty();
        PendingActionData.Reset();

        if (DepthBeforeTargetSelection == ECombatMenuDepth::Submenu)
        {
            // Go back to spell/ability submenu
            CurrentDepth = ECombatMenuDepth::Submenu;
            if (ActiveSubmenuSource != EPieMenuCategory::None)
            {
                const TArray<USpellData *> &Spells = CurrentCapabilities.GetSpellsForCategory(ActiveSubmenuSource);
                OnCommandMenuReady.Broadcast(BuildSchoolButtons(Spells));
            }
            else
            {
                OnCommandMenuReady.Broadcast(BuildAbilitySubmenu());
            }
        }
        else
        {
            // Came from main menu (e.g. Attack)
            CurrentDepth = ECombatMenuDepth::Main;
            OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
        }
        break;
    }
    case ECombatMenuDepth::Submenu:
        // If we were in a spell grid, go back to school list
        // If we were in school list or ability grid, go back to main
        CurrentDepth = ECombatMenuDepth::Main;
        ActiveSubmenuSource = EPieMenuCategory::None;
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
    CurrentActor.Reset();
    OnCommandMenuClosed.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] Closed"));
}

// ==================== MAIN MENU ====================

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildMainMenuButtons() const
{
    TArray<FPieMenuButtonData> Buttons;

    if (CurrentCapabilities.bCanAttack)
        Buttons.Add(CreateAttackButton());
    if (CurrentCapabilities.bCanUseAbilities)
        Buttons.Add(CreateAbilitiesButton());
    if (CurrentCapabilities.bHasWeaponCrystal)
        Buttons.Add(CreateResonateWeaponButton());
    if (CurrentCapabilities.bHasRefractions)
        Buttons.Add(CreateRefractionsButton());
    if (CurrentCapabilities.bHasBreakthrough)
        Buttons.Add(CreateBreakthroughButton());
    if (CurrentCapabilities.ShouldShowResonateRing())
        Buttons.Add(CreateResonateRingButton());
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
    Button.Description = FText::FromString(
        FString::Printf(TEXT("%d spells"), CurrentCapabilities.WeaponCrystalSpells.Num()));
    Button.Category = EPieMenuCategory::ResonateWeapon;
    Button.ButtonTint = CurrentCapabilities.WeaponCrystalColor;
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

void UCombatCommandMenuSubsystem::OpenSpellSubmenu(EPieMenuCategory Source)
{
    ActiveSubmenuSource = Source;
    CurrentDepth = ECombatMenuDepth::Submenu;

    const TArray<USpellData *> &Spells = CurrentCapabilities.GetSpellsForCategory(Source);
    TArray<FPieMenuButtonData> SchoolButtons = BuildSchoolButtons(Spells);
    OnCommandMenuReady.Broadcast(SchoolButtons);
}

void UCombatCommandMenuSubsystem::OpenAbilitySubmenu()
{
    CurrentDepth = ECombatMenuDepth::Submenu;
    TArray<FPieMenuButtonData> AbilityButtons = BuildAbilitySubmenu();
    OnCommandMenuReady.Broadcast(AbilityButtons);
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildSpellSubmenu(
    EPieMenuCategory Source) const
{
    const TArray<USpellData *> &Spells = CurrentCapabilities.GetSpellsForCategory(Source);
    return BuildSchoolButtons(Spells);
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildAbilitySubmenu() const
{
    TArray<FPieMenuButtonData> Buttons;

    for (int32 i = 0; i < CurrentCapabilities.WeaponAbilities.Num(); i++)
    {
        UAbilityData *Ability = CurrentCapabilities.WeaponAbilities[i];
        if (!Ability)
            continue;

        FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
            FString::Printf(TEXT("Ability_%d"), i),
            FText::FromString(Ability->AbilityName),
            EPieMenuCategory::Ability,
            Ability,
            i);

        Button.bEnabled = true;
        Buttons.Add(Button);
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
            FText::FromString(Spell->SpellName),
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

void UCombatCommandMenuSubsystem::ExecuteAttack()
{
    FPieMenuButtonData AttackButton;
    AttackButton.ButtonID = TEXT("Attack");
    AttackButton.Category = EPieMenuCategory::Attack;
    OnActionSelected.Broadcast(AttackButton);
    Close();
}

void UCombatCommandMenuSubsystem::ExecuteSwitchWeapon()
{
    if (!CurrentActor.IsValid())
        return;

    ULoadoutComponent *LC = GetLoadoutComponent();
    if (!LC)
        return;

    LC->ToggleEquipment();

    // Rebuild capabilities with the new active weapon and refresh main menu
    UCharacterDataComponent *CDC =
        CurrentActor->FindComponentByClass<UCharacterDataComponent>();
    if (CDC && CDC->CharacterData)
    {
        CurrentCapabilities = FCombatCapabilities::BuildFrom(
            LC,
            CDC->CharacterData->CharacterClass,
            CDC->CharacterData->InnateElement,
            [this](int32 ElementIndex)
            { return GetElementColor(ElementIndex); });
    }

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
    UCharacterDataComponent *CDC =
        CurrentActor->FindComponentByClass<UCharacterDataComponent>();
    if (CDC && CDC->CharacterData)
    {
        CurrentCapabilities = FCombatCapabilities::BuildFrom(
            LC,
            CDC->CharacterData->CharacterClass,
            CDC->CharacterData->InnateElement,
            [this](int32 ElementIndex)
            { return GetElementColor(ElementIndex); });
    }

    OnCommandMenuReady.Broadcast(BuildMainMenuButtons());
}

// ==================== TARGET SELECTION ====================

void UCombatCommandMenuSubsystem::OpenTargetSelection(
    EPieMenuCategory ActionCategory,
    const FPieMenuButtonData &ActionButton,
    ETargetType TargetType)
{
    // Stash the pending action
    PendingActionCategory = ActionCategory;
    PendingActionID = ActionButton.ButtonID;
    PendingActionData = ActionButton.DataReference;
    DepthBeforeTargetSelection = CurrentDepth;

    // Log target type
    const UEnum *TargetEnum = StaticEnum<ETargetType>();
    FString TargetTypeName = TargetEnum
                                 ? TargetEnum->GetNameStringByValue(static_cast<int64>(TargetType))
                                 : TEXT("Unknown");
    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu] OpenTargetSelection: TargetType=%s"),
           *TargetTypeName);

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
            if (CDC->CharacterData && !CDC->CharacterData->CharacterName.IsEmpty())
            {
                TName = CDC->CharacterData->CharacterName;
            }
        }
        UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]     - %s"), *TName);
    }

    // Auto-resolving target types skip selection
    if (TargetType == ETargetType::Self ||
        TargetType == ETargetType::AllEnemies ||
        TargetType == ETargetType::AllAllies ||
        TargetType == ETargetType::Everyone ||
        TargetType == ETargetType::SingleAlly)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]   Auto-resolving (no picker shown)"));
        AActor *AutoTarget = Targets.Num() > 0 ? Targets[0] : nullptr;
        ConfirmActionWithTarget(AutoTarget);
        return;
    }

    // SingleEnemy - need to pick
    if (Targets.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCommandMenu] OpenTargetSelection: no valid targets"));
        // Fallback: cancel action, return to previous menu
        PendingActionCategory = EPieMenuCategory::None;
        PendingActionID.Empty();
        PendingActionData.Reset();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatCommandMenu]   Showing target picker"));

    // Show target selection
    CurrentDepth = ECombatMenuDepth::TargetSelection;
    TArray<FPieMenuButtonData> TargetButtons = BuildTargetButtons(Targets);
    OnCommandMenuReady.Broadcast(TargetButtons);
}

TArray<FPieMenuButtonData> UCombatCommandMenuSubsystem::BuildTargetButtons(
    const TArray<AActor *> &Targets) const
{
    TArray<FPieMenuButtonData> Buttons;

    for (AActor *Target : Targets)
    {
        if (!Target)
            continue;

        // Pull display name from CharacterData if available
        FString TargetName = Target->GetName();
        if (UCharacterDataComponent *CDC = Target->FindComponentByClass<UCharacterDataComponent>())
        {
            if (CDC->CharacterData && !CDC->CharacterData->CharacterName.IsEmpty())
            {
                TargetName = CDC->CharacterData->CharacterName;
            }
        }

        FPieMenuButtonData Button;
        Button.ButtonID = Target->GetName(); // keep internal name for ID uniqueness
        Button.DisplayName = FText::FromString(TargetName);
        Button.Category = EPieMenuCategory::Target;
        Button.DataReference = Target;
        Button.bEnabled = true;
        Buttons.Add(Button);
    }

    // Add back button so player can cancel target selection
    Buttons.Add(FPieMenuButtonData::MakeBackButton());

    return Buttons;
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
    ActionButton.TargetActor = SelectedTarget;

    FString TargetName = TEXT("(no target)");
    if (SelectedTarget)
    {
        if (UCharacterDataComponent *CDC = SelectedTarget->FindComponentByClass<UCharacterDataComponent>())
        {
            if (CDC->CharacterData && !CDC->CharacterData->CharacterName.IsEmpty())
            {
                TargetName = CDC->CharacterData->CharacterName;
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

    OnActionSelected.Broadcast(ActionButton);
    Close();
}

// ==================== HELPERS ====================

ULoadoutComponent *UCombatCommandMenuSubsystem::GetLoadoutComponent() const
{
    if (!CurrentActor.IsValid())
        return nullptr;
    return CurrentActor->FindComponentByClass<ULoadoutComponent>();
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