// WBPCommandMenu.cpp
// Master command menu controller implementation
// World of Refraction - Combat UI

#include "UI/Combat/WBPCommandMenu.h"
#include "UI/Combat/WBPCommandList.h"
#include "UI/Combat/WBPCommandGrid.h"
#include "CharacterData.h"
#include "CharacterDataComponent.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"
#include "RingData.h"
#include "WeaponData.h"
#include "RingManager.h"
#include "Components/HorizontalBox.h"

UWBPCommandMenu::UWBPCommandMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWBPCommandMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind child widget events
	if (MainMenuList)
	{
		MainMenuList->OnButtonClicked.AddDynamic(this, &UWBPCommandMenu::HandleMainMenuClicked);
	}

	if (SchoolsList)
	{
		SchoolsList->OnButtonClicked.AddDynamic(this, &UWBPCommandMenu::HandleSchoolsClicked);
	}

	if (ActionGrid)
	{
		ActionGrid->OnButtonClicked.AddDynamic(this, &UWBPCommandMenu::HandleGridClicked);
	}

	// Start closed
	SetMenuState(ECommandMenuState::Closed);
}

// ==================== MENU CONTROL ====================

void UWBPCommandMenu::OpenMenu(AActor* InOwningActor)
{
	if (!InOwningActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WBPCommandMenu] Cannot open menu - no OwningActor provided"));
		return;
	}

	OwningActor = InOwningActor;

	// Derive CharacterData from component
	UCharacterDataComponent* Comp = OwningActor->FindComponentByClass<UCharacterDataComponent>();
	CharacterData = Comp ? Comp->CharacterData : nullptr;

	if (!CharacterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WBPCommandMenu] Cannot open menu - actor has no CharacterDataComponent or CharacterData"));
		return;
	}

	// Populate and show main menu
	PopulateMainMenu();
	SetMenuState(ECommandMenuState::Main);

	// Broadcast open event
	OnMenuOpened.Broadcast();
	OnMenuOpenAnimated();

	UE_LOG(LogTemp, Log, TEXT("[WBPCommandMenu] Menu opened for %s (%s)"), 
		*OwningActor->GetName(), *CharacterData->CharacterName);
}

void UWBPCommandMenu::CloseMenu()
{
	OwningActor = nullptr;
	CharacterData = nullptr;
	bHasSelectedSchool = false;

	SetMenuState(ECommandMenuState::Closed);

	// Clear child widgets
	if (MainMenuList) MainMenuList->ClearButtons();
	if (SchoolsList) SchoolsList->ClearButtons();
	if (ActionGrid) ActionGrid->ClearButtons();

	// Broadcast close event
	OnMenuClosed.Broadcast();
	OnMenuCloseAnimated();

	UE_LOG(LogTemp, Log, TEXT("[WBPCommandMenu] Menu closed"));
}

// ==================== NAVIGATION ====================

void UWBPCommandMenu::NavigateUp()
{
	switch (CurrentState)
	{
	case ECommandMenuState::Main:
		if (MainMenuList) MainMenuList->NavigateUp();
		break;

	case ECommandMenuState::Schools:
		if (SchoolsList) SchoolsList->NavigateUp();
		break;

	case ECommandMenuState::SpellGrid:
	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		if (ActionGrid) ActionGrid->NavigateUp();
		break;

	default:
		break;
	}
}

void UWBPCommandMenu::NavigateDown()
{
	switch (CurrentState)
	{
	case ECommandMenuState::Main:
		if (MainMenuList) MainMenuList->NavigateDown();
		break;

	case ECommandMenuState::Schools:
		if (SchoolsList) SchoolsList->NavigateDown();
		break;

	case ECommandMenuState::SpellGrid:
	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		if (ActionGrid) ActionGrid->NavigateDown();
		break;

	default:
		break;
	}
}

void UWBPCommandMenu::NavigateLeft()
{
	switch (CurrentState)
	{
	case ECommandMenuState::SpellGrid:
	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		if (ActionGrid) ActionGrid->NavigateLeft();
		break;

	default:
		break;
	}
}

void UWBPCommandMenu::NavigateRight()
{
	switch (CurrentState)
	{
	case ECommandMenuState::SpellGrid:
	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		if (ActionGrid) ActionGrid->NavigateRight();
		break;

	default:
		break;
	}
}

void UWBPCommandMenu::Confirm()
{
	switch (CurrentState)
	{
	case ECommandMenuState::Main:
		if (MainMenuList) MainMenuList->ConfirmSelection();
		break;

	case ECommandMenuState::Schools:
		if (SchoolsList) SchoolsList->ConfirmSelection();
		break;

	case ECommandMenuState::SpellGrid:
	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		if (ActionGrid) ActionGrid->ConfirmSelection();
		break;

	default:
		break;
	}
}

void UWBPCommandMenu::Back()
{
	switch (CurrentState)
	{
	case ECommandMenuState::Main:
		// Back from main closes menu
		CloseMenu();
		break;

	case ECommandMenuState::Schools:
		// Back to main
		SetMenuState(ECommandMenuState::Main);
		break;

	case ECommandMenuState::SpellGrid:
		// Back to schools
		SetMenuState(ECommandMenuState::Schools);
		break;

	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		// Back to main
		SetMenuState(ECommandMenuState::Main);
		break;

	default:
		break;
	}
}

// ==================== STATE MACHINE ====================

void UWBPCommandMenu::SetMenuState(ECommandMenuState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	ECommandMenuState OldState = CurrentState;
	CurrentState = NewState;

	// Update visibility
	UpdateWidgetVisibility();

	// Broadcast state change
	OnStateChanged.Broadcast(OldState, NewState);
	OnStateChangeAnimated(OldState, NewState);

	UE_LOG(LogTemp, Verbose, TEXT("[WBPCommandMenu] State: %d -> %d"), (int32)OldState, (int32)NewState);
}

void UWBPCommandMenu::UpdateWidgetVisibility()
{
	// Default: hide everything
	ESlateVisibility MainVis = ESlateVisibility::Collapsed;
	ESlateVisibility SchoolsVis = ESlateVisibility::Collapsed;
	ESlateVisibility GridVis = ESlateVisibility::Collapsed;
	ESlateVisibility ContainerVis = ESlateVisibility::Collapsed;

	switch (CurrentState)
	{
	case ECommandMenuState::Closed:
		// All hidden
		break;

	case ECommandMenuState::Main:
		ContainerVis = ESlateVisibility::Visible;
		MainVis = ESlateVisibility::Visible;
		break;

	case ECommandMenuState::Schools:
		ContainerVis = ESlateVisibility::Visible;
		MainVis = ESlateVisibility::Visible;
		SchoolsVis = ESlateVisibility::Visible;
		break;

	case ECommandMenuState::SpellGrid:
		ContainerVis = ESlateVisibility::Visible;
		MainVis = ESlateVisibility::Visible;
		SchoolsVis = ESlateVisibility::Visible;
		GridVis = ESlateVisibility::Visible;
		break;

	case ECommandMenuState::AbilityGrid:
	case ECommandMenuState::ItemGrid:
	case ECommandMenuState::RingGrid:
		ContainerVis = ESlateVisibility::Visible;
		MainVis = ESlateVisibility::Visible;
		GridVis = ESlateVisibility::Visible;
		break;
	}

	// Apply visibility
	if (MenuContainer) MenuContainer->SetVisibility(ContainerVis);
	if (MainMenuList) MainMenuList->SetVisibility(MainVis);
	if (SchoolsList) SchoolsList->SetVisibility(SchoolsVis);
	if (ActionGrid) ActionGrid->SetVisibility(GridVis);
}

// ==================== POPULATION ====================

void UWBPCommandMenu::PopulateMainMenu()
{
	if (!MainMenuList || !CharacterData)
	{
		return;
	}

	TArray<FCommandButtonData> Buttons;

	if (IsGeneric())
	{
		// Generic: Attack, Abilities, Items
		if (HasWeaponEquipped())
		{
			Buttons.Add(FCommandButtonData(FText::FromString(TEXT("Attack")), ECommandType::Attack));
		}

		TArray<UAbilityData*> Abilities = GetEquippedAbilities();
		if (Abilities.Num() > 0)
		{
			Buttons.Add(FCommandButtonData(FText::FromString(TEXT("Abilities")), ECommandType::Abilities));
		}
	}
	else if (IsCaster())
	{
		// Caster: Refractions, Items (no Attack)
		TArray<ESpellSchool> Schools = GetAvailableSchools();
		if (Schools.Num() > 0)
		{
			Buttons.Add(FCommandButtonData(FText::FromString(TEXT("Refractions")), ECommandType::Refractions));
		}
	}
	else if (IsResonator())
	{
		// Resonator: ChangeRing, Resonate, Items (no Attack)
		TArray<URingData*> Rings = GetAvailableRings();
		if (Rings.Num() > 1) // Only show if more than 1 ring to switch to
		{
			Buttons.Add(FCommandButtonData(FText::FromString(TEXT("Change Ring")), ECommandType::ChangeRing));
		}

		TArray<ESpellSchool> Schools = GetAvailableSchools();
		if (Schools.Num() > 0)
		{
			Buttons.Add(FCommandButtonData(FText::FromString(TEXT("Resonate")), ECommandType::Resonate));
		}
	}

	// Items - available for all classes
	TArray<UItemData*> Items = GetUsableItems();
	if (Items.Num() > 0)
	{
		Buttons.Add(FCommandButtonData(FText::FromString(TEXT("Items")), ECommandType::Items));
	}

	MainMenuList->SetButtons(Buttons);
}

void UWBPCommandMenu::PopulateSchoolsList()
{
	if (!SchoolsList)
	{
		return;
	}

	TArray<FCommandButtonData> Buttons;
	TArray<ESpellSchool> Schools = GetAvailableSchools();

	for (ESpellSchool School : Schools)
	{
		FString SchoolName = CommandMenuHelpers::GetSchoolDisplayName(School);
		Buttons.Add(FCommandButtonData(FText::FromString(SchoolName), School));
	}

	SchoolsList->SetButtons(Buttons);
}

void UWBPCommandMenu::PopulateSpellGrid(ESpellSchool School)
{
	if (!ActionGrid)
	{
		return;
	}

	SelectedSchool = School;
	bHasSelectedSchool = true;
	TArray<FCommandButtonData> Buttons;
	TArray<USpellData*> Spells = GetSpellsForSchool(School);

	// Max 6 spells
	int32 Count = FMath::Min(Spells.Num(), 6);
	for (int32 i = 0; i < Count; i++)
	{
		USpellData* Spell = Spells[i];
		if (Spell)
		{
			FCommandButtonData Data;
			Data.DisplayName = FText::FromString(Spell->SpellName);
			Data.DataReference = Spell;
			// TODO: Add spell icon when SpellData has icon field
			Buttons.Add(Data);
		}
	}

	ActionGrid->SetGridType(EGridType::Spells);
	ActionGrid->SetButtons(Buttons);
}

void UWBPCommandMenu::PopulateAbilityGrid()
{
	if (!ActionGrid)
	{
		return;
	}

	TArray<FCommandButtonData> Buttons;
	TArray<UAbilityData*> Abilities = GetEquippedAbilities();

	// Max 6 abilities
	int32 Count = FMath::Min(Abilities.Num(), 6);
	for (int32 i = 0; i < Count; i++)
	{
		UAbilityData* Ability = Abilities[i];
		if (Ability)
		{
			FCommandButtonData Data;
			Data.DisplayName = FText::FromString(Ability->AbilityName);
			Data.DataReference = Ability;
			Buttons.Add(Data);
		}
	}

	ActionGrid->SetGridType(EGridType::Abilities);
	ActionGrid->SetButtons(Buttons);
}

void UWBPCommandMenu::PopulateItemGrid()
{
	if (!ActionGrid)
	{
		return;
	}

	TArray<FCommandButtonData> Buttons;
	TArray<UItemData*> Items = GetUsableItems();

	// Max 6 items
	int32 Count = FMath::Min(Items.Num(), 6);
	for (int32 i = 0; i < Count; i++)
	{
		UItemData* Item = Items[i];
		if (Item)
		{
			FCommandButtonData Data;
			Data.DisplayName = FText::FromString(Item->ItemName);
			Data.DataReference = Item;
			Buttons.Add(Data);
		}
	}

	ActionGrid->SetGridType(EGridType::Items);
	ActionGrid->SetButtons(Buttons);
}

void UWBPCommandMenu::PopulateRingGrid()
{
	if (!ActionGrid)
	{
		return;
	}

	TArray<FCommandButtonData> Buttons;
	TArray<URingData*> Rings = GetAvailableRings();
	URingData* ActiveRing = GetActiveRing();

	// Max 6 rings
	int32 Count = FMath::Min(Rings.Num(), 6);
	for (int32 i = 0; i < Count; i++)
	{
		URingData* Ring = Rings[i];
		if (Ring)
		{
			FCommandButtonData Data;
			Data.DisplayName = FText::FromString(Ring->RingName);
			Data.DataReference = Ring;
			Data.bIsCurrentlyEquipped = (Ring == ActiveRing);
			Buttons.Add(Data);
		}
	}

	ActionGrid->SetGridType(EGridType::Rings);
	ActionGrid->SetButtons(Buttons);
}

// ==================== DATA QUERIES ====================

TArray<ESpellSchool> UWBPCommandMenu::GetAvailableSchools() const
{
	TArray<ESpellSchool> AvailableSchools;

	if (!CharacterData)
	{
		return AvailableSchools;
	}

	// Check each school for spells
	for (ESpellSchool School : CommandMenuHelpers::GetAllSpellSchools())
	{
		TArray<USpellData*> Spells = GetSpellsForSchool(School);
		if (Spells.Num() > 0)
		{
			AvailableSchools.Add(School);
		}
	}

	return AvailableSchools;
}

TArray<USpellData*> UWBPCommandMenu::GetSpellsForSchool(ESpellSchool School) const
{
	TArray<USpellData*> Result;

	if (!CharacterData || !OwningActor)
	{
		return Result;
	}

	TArray<USpellData*> AllSpells;

	// Get spells based on character class
	if (IsResonator())
	{
		// Resonator: Get spells from active ring via RingManager
		URingManager* RingMgr = GetRingManager();
		if (RingMgr)
		{
			AllSpells = RingMgr->GetAvailableSpells(OwningActor);
		}
	}
	else if (IsCaster())
	{
		// Caster: Get innate spells
		AllSpells = CharacterData->InnateSpells;
	}
	// Generic has no spells

	// Filter by school
	for (USpellData* Spell : AllSpells)
	{
		if (Spell && Spell->School == School)
		{
			Result.Add(Spell);
		}
	}

	return Result;
}

TArray<UAbilityData*> UWBPCommandMenu::GetEquippedAbilities() const
{
	TArray<UAbilityData*> Result;

	if (!CharacterData)
	{
		return Result;
	}

	// Get abilities from equipped weapon
	// TODO: Use WeaponManager to get active weapon's abilities
	if (CharacterData->PrimaryWeapon)
	{
		for (UAbilityData* Ability : CharacterData->PrimaryWeapon->PresetAbilities)
		{
			if (Ability)
			{
				Result.Add(Ability);
			}
		}
	}

	return Result;
}

TArray<UItemData*> UWBPCommandMenu::GetUsableItems() const
{
	TArray<UItemData*> Result;

	// TODO: Implement inventory system query
	// For now, return empty - items will be populated later

	return Result;
}

TArray<URingData*> UWBPCommandMenu::GetAvailableRings() const
{
	TArray<URingData*> Result;

	if (!OwningActor || !IsResonator())
	{
		return Result;
	}

	// Get rings from RingManager (runtime state)
	URingManager* RingMgr = GetRingManager();
	if (RingMgr)
	{
		Result = RingMgr->GetEquippedRings(OwningActor);
	}

	return Result;
}

URingData* UWBPCommandMenu::GetActiveRing() const
{
	if (!OwningActor || !IsResonator())
	{
		return nullptr;
	}

	// Get active ring from RingManager
	URingManager* RingMgr = GetRingManager();
	if (RingMgr)
	{
		return RingMgr->GetActiveRing(OwningActor);
	}

	return nullptr;
}

// ==================== EVENT HANDLERS ====================

void UWBPCommandMenu::HandleMainMenuClicked(const FCommandButtonData& ButtonData)
{
	switch (ButtonData.CommandType)
	{
	case ECommandType::Attack:
		// Execute attack immediately
		OnAttackSelected.Broadcast(ECommandType::Attack);
		CloseMenu();
		break;

	case ECommandType::Refractions:
	case ECommandType::Resonate:
		// Go to schools list
		PopulateSchoolsList();
		SetMenuState(ECommandMenuState::Schools);
		break;

	case ECommandType::Abilities:
		// Go to ability grid
		PopulateAbilityGrid();
		SetMenuState(ECommandMenuState::AbilityGrid);
		break;

	case ECommandType::ChangeRing:
		// Go to ring grid
		PopulateRingGrid();
		SetMenuState(ECommandMenuState::RingGrid);
		break;

	case ECommandType::Items:
		// Go to item grid
		PopulateItemGrid();
		SetMenuState(ECommandMenuState::ItemGrid);
		break;

	default:
		break;
	}
}

void UWBPCommandMenu::HandleSchoolsClicked(const FCommandButtonData& ButtonData)
{
	// Populate spell grid for selected school
	PopulateSpellGrid(ButtonData.SpellSchool);
	SetMenuState(ECommandMenuState::SpellGrid);
}

void UWBPCommandMenu::HandleGridClicked(const FCommandButtonData& ButtonData)
{
	EGridType GridType = ActionGrid ? ActionGrid->GetGridType() : EGridType::None;

	switch (GridType)
	{
	case EGridType::Spells:
		if (USpellData* Spell = Cast<USpellData>(ButtonData.DataReference))
		{
			OnSpellSelected.Broadcast(Spell);
			CloseMenu();
		}
		break;

	case EGridType::Abilities:
		if (UAbilityData* Ability = Cast<UAbilityData>(ButtonData.DataReference))
		{
			OnAbilitySelected.Broadcast(Ability);
			CloseMenu();
		}
		break;

	case EGridType::Items:
		if (UItemData* Item = Cast<UItemData>(ButtonData.DataReference))
		{
			OnItemSelected.Broadcast(Item);
			CloseMenu();
		}
		break;

	case EGridType::Rings:
		if (URingData* Ring = Cast<URingData>(ButtonData.DataReference))
		{
			// Ring switch is FREE ACTION - don't close menu
			// Find ring index and switch via RingManager
			URingManager* RingMgr = GetRingManager();
			if (RingMgr)
			{
				TArray<URingData*> Rings = RingMgr->GetEquippedRings(OwningActor);
				int32 RingIndex = Rings.IndexOfByKey(Ring);
				if (RingIndex != INDEX_NONE)
				{
					RingMgr->SwitchToRing(OwningActor, RingIndex);
				}
			}

			// Broadcast ring change event
			OnRingSelected.Broadcast(Ring);

			// Refresh main menu (Resonate schools may have changed)
			PopulateMainMenu();

			// Return to main menu state
			SetMenuState(ECommandMenuState::Main);
		}
		break;

	default:
		break;
	}
}

// ==================== HELPERS ====================

bool UWBPCommandMenu::IsGeneric() const
{
	return CharacterData && CharacterData->IsGeneric();
}

bool UWBPCommandMenu::IsCaster() const
{
	return CharacterData && CharacterData->IsCaster();
}

bool UWBPCommandMenu::IsResonator() const
{
	return CharacterData && CharacterData->IsResonator();
}

bool UWBPCommandMenu::HasWeaponEquipped() const
{
	if (!CharacterData)
	{
		return false;
	}

	// Check if character has a weapon and is armed
	// TODO: Use WeaponManager for accurate state
	return CharacterData->PrimaryWeapon != nullptr && CharacterData->bUsePrimary;
}

URingManager* UWBPCommandMenu::GetRingManager() const
{
	if (!OwningActor)
	{
		return nullptr;
	}

	UGameInstance* GI = Cast<UGameInstance>(OwningActor->GetGameInstance());
	if (GI)
	{
		return GI->GetSubsystem<URingManager>();
	}

	return nullptr;
}
