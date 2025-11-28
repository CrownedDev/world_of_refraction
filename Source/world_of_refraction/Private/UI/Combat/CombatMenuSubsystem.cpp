// CombatMenuSubsystem.cpp
// Combat menu logic implementation
// World of Refraction - Combat UI

#include "UI/Combat/CombatMenuSubsystem.h"
#include "CharacterData.h"
#include "WeaponData.h"
#include "RingData.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "ItemData.h"

// ==================== SUBSYSTEM LIFECYCLE ====================

void UCombatMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Initialized"));
}

void UCombatMenuSubsystem::Deinitialize()
{
	CurrentCharacter.Reset();
	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Deinitialized"));
}

// ==================== MAIN MENU ====================

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetMainMenuButtons(UCharacterData* CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatMenuSubsystem] GetMainMenuButtons: null CharacterData"));
		return Buttons;
	}

	CurrentCharacter = CharacterData;
	SetMenuState(EPieMenuState::Main);

	switch (CharacterData->CharacterClass)
	{
	case ECharacterClass::Generic:
		BuildGenericMainMenu(CharacterData, Buttons);
		break;

	case ECharacterClass::Caster:
		BuildCasterMainMenu(CharacterData, Buttons);
		break;

	case ECharacterClass::Resonator:
		BuildResonatorMainMenu(CharacterData, Buttons);
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d main menu buttons for %s (%s)"),
		Buttons.Num(), *CharacterData->CharacterName,
		*UEnum::GetValueAsString(CharacterData->CharacterClass));

	return Buttons;
}

// ==================== MAIN MENU BUILDERS ====================

void UCombatMenuSubsystem::BuildGenericMainMenu(UCharacterData* CharacterData, TArray<FPieMenuButtonData>& OutButtons)
{
	// Generic: Attack, Abilities, Items, Switch Weapon
	OutButtons.Add(CreateAttackButton(CharacterData));
	OutButtons.Add(CreateAbilitiesButton(CharacterData));
	OutButtons.Add(CreateItemsButton(CharacterData));
	OutButtons.Add(CreateSwitchWeaponButton(CharacterData));
}

void UCombatMenuSubsystem::BuildCasterMainMenu(UCharacterData* CharacterData, TArray<FPieMenuButtonData>& OutButtons)
{
	// Caster: Refractions, Items, Switch Weapon
	OutButtons.Add(CreateRefractionsButton(CharacterData));
	OutButtons.Add(CreateItemsButton(CharacterData));
	OutButtons.Add(CreateSwitchWeaponButton(CharacterData));
}

void UCombatMenuSubsystem::BuildResonatorMainMenu(UCharacterData* CharacterData, TArray<FPieMenuButtonData>& OutButtons)
{
	// Resonator: ChangeRing, Resonate, Items, Switch Weapon
	OutButtons.Add(CreateChangeRingButton(CharacterData));
	OutButtons.Add(CreateResonateButton(CharacterData));
	OutButtons.Add(CreateItemsButton(CharacterData));
	OutButtons.Add(CreateSwitchWeaponButton(CharacterData));
}

// ==================== BUTTON CREATORS ====================

FPieMenuButtonData UCombatMenuSubsystem::CreateAttackButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Attack");
	Button.DisplayName = FText::FromString(TEXT("Attack"));
	Button.Category = EPieMenuCategory::Attack;

	// Get current weapon for description
	UWeaponData* CurrentWeapon = CharacterData->bUsePrimary ? 
		CharacterData->PrimaryWeapon : CharacterData->SecondaryWeapon;

	if (CurrentWeapon)
	{
		Button.Description = FText::FromString(FString::Printf(TEXT("Attack with %s"), *CurrentWeapon->WeaponName));
		Button.Icon = nullptr; // TODO: Add Icon property to WeaponData
		Button.bEnabled = true;
	}
	else
	{
		Button.Description = FText::FromString(TEXT("No weapon equipped"));
		Button.bEnabled = false;
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateAbilitiesButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Abilities");
	Button.DisplayName = FText::FromString(TEXT("Abilities"));
	Button.Category = EPieMenuCategory::Abilities;

	TArray<UAbilityData*> Abilities = GetCurrentWeaponAbilities(CharacterData);
	int32 Count = Abilities.Num();

	Button.Description = FText::FromString(FString::Printf(TEXT("%d abilities"), Count));
	Button.bEnabled = Count > 0;

	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateRefractionsButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Refractions");
	Button.DisplayName = FText::FromString(TEXT("Refractions"));
	Button.Category = EPieMenuCategory::Refractions;

	TArray<USpellData*> Spells = GetAllSpells(CharacterData);
	int32 Count = Spells.Num();

	Button.Description = FText::FromString(FString::Printf(TEXT("%d spells"), Count));
	Button.bEnabled = Count > 0;

	// Tint with innate element color
	if (CharacterData->IsCaster())
	{
		Button.ButtonTint = GetElementColor(static_cast<int32>(CharacterData->InnateElement));
	}

	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateChangeRingButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("ChangeRing");
	Button.DisplayName = FText::FromString(TEXT("Change Ring"));
	Button.Category = EPieMenuCategory::ChangeRing;

	int32 Count = FMath::Min(CharacterData->EquippedRings.Num(), MAX_RINGS);
	Button.Description = FText::FromString(FString::Printf(TEXT("%d rings"), Count));
	Button.bEnabled = Count > 1; // Need at least 2 rings to switch

	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateResonateButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Resonate");
	Button.DisplayName = FText::FromString(TEXT("Resonate"));
	Button.Category = EPieMenuCategory::Resonate;

	TArray<USpellData*> Spells = GetAllSpells(CharacterData);
	int32 Count = Spells.Num();

	Button.Description = FText::FromString(FString::Printf(TEXT("%d spells from ring"), Count));
	Button.bEnabled = Count > 0 && CharacterData->EquippedRings.Num() > 0;

	// Tint with current ring's element
	// TODO: Get active ring index from runtime state
	if (CharacterData->EquippedRings.Num() > 0 && CharacterData->EquippedRings[0])
	{
		Button.ButtonTint = GetElementColor(static_cast<int32>(CharacterData->EquippedRings[0]->Element));
	}

	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateItemsButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Items");
	Button.DisplayName = FText::FromString(TEXT("Items"));
	Button.Category = EPieMenuCategory::Items;
	Button.Description = FText::FromString(TEXT("Use items"));
	Button.bEnabled = true; // Always enabled, sub-menu shows actual availability

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateSwitchWeaponButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("SwitchWeapon");
	Button.DisplayName = FText::FromString(TEXT("Switch"));
	Button.Category = EPieMenuCategory::SwitchWeapon;

	bool bCanSwitch = CanSwitchWeapon(CharacterData);

	if (bCanSwitch)
	{
		// Show what we'll switch TO
		UWeaponData* OtherWeapon = CharacterData->bUsePrimary ? 
			CharacterData->SecondaryWeapon : CharacterData->PrimaryWeapon;
		
		if (OtherWeapon)
		{
			Button.Description = FText::FromString(FString::Printf(TEXT("Switch to %s"), *OtherWeapon->WeaponName));
		}
		else
		{
			Button.Description = FText::FromString(TEXT("Switch to unarmed"));
		}
		Button.bEnabled = true;
	}
	else
	{
		Button.Description = FText::FromString(TEXT("No alternate weapon"));
		Button.bEnabled = false;
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

// ==================== SUB-MENUS ====================

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetSchoolsButtons(UCharacterData* CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::Schools);

	// Check each school for spells
	for (int32 i = 0; i < 4; ++i)
	{
		EPieMenuSpellSchool School = static_cast<EPieMenuSpellSchool>(i);
		int32 Count = CountSpellsInSchool(CharacterData, School);

		// Only add schools that have spells
		if (Count > 0)
		{
			Buttons.Add(FPieMenuButtonData::MakeSchoolButton(School, Count));
		}
	}

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d school buttons"), Buttons.Num() - 1);
	return Buttons;
}

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetSpellsForSchool(UCharacterData* CharacterData, EPieMenuSpellSchool School)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SelectedSchool = School;
	SetMenuState(EPieMenuState::SpellGrid);

	TArray<USpellData*> Spells = GetSpellsBySchool(CharacterData, School);

	// Cap at MAX_SPELLS_PER_SCHOOL
	int32 Count = FMath::Min(Spells.Num(), MAX_SPELLS_PER_SCHOOL);

	for (int32 i = 0; i < Count; ++i)
	{
		USpellData* Spell = Spells[i];
		if (!Spell) continue;

		FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
			FString::Printf(TEXT("Spell_%d"), i),
			FText::FromString(Spell->SpellName),
			EPieMenuCategory::Spell,
			Spell,
			i,
			nullptr); // TODO: Add Icon property to SpellData

		Button.Description = FText::FromString(Spell->Description);
		Button.ButtonTint = GetElementColor(static_cast<int32>(Spell->Element));
		Button.bEnabled = true; // TODO: Check energy cost

		Buttons.Add(Button);
	}

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d spell buttons for school %d"), 
		Buttons.Num() - 1, static_cast<int32>(School));
	return Buttons;
}

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetAbilitiesButtons(UCharacterData* CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::AbilityGrid);

	TArray<UAbilityData*> Abilities = GetCurrentWeaponAbilities(CharacterData);

	for (int32 i = 0; i < Abilities.Num(); ++i)
	{
		UAbilityData* Ability = Abilities[i];
		if (!Ability) continue;

		FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
			FString::Printf(TEXT("Ability_%d"), i),
			FText::FromString(Ability->AbilityName),
			EPieMenuCategory::Ability,
			Ability,
			i,
			nullptr); // TODO: Add Icon property to AbilityData

		Button.Description = FText::FromString(Ability->Description);
		Button.bEnabled = true; // TODO: Check cooldown, energy cost

		Buttons.Add(Button);
	}

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d ability buttons"), Buttons.Num() - 1);
	return Buttons;
}

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetRingsButtons(UCharacterData* CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData || !CharacterData->IsResonator())
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::RingGrid);

	// Cap at MAX_RINGS
	int32 Count = FMath::Min(CharacterData->EquippedRings.Num(), MAX_RINGS);

	for (int32 i = 0; i < Count; ++i)
	{
		URingData* Ring = CharacterData->EquippedRings[i];
		if (!Ring) continue;

		FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
			FString::Printf(TEXT("Ring_%d"), i),
			FText::FromString(Ring->RingName),
			EPieMenuCategory::Ring,
			Ring,
			i,
			nullptr); // TODO: Add Icon property to RingData

		Button.Description = FText::FromString(Ring->Description);
		Button.ButtonTint = GetElementColor(static_cast<int32>(Ring->Element));
		Button.bEnabled = true;

		// TODO: Mark currently active ring
		// if (i == ActiveRingIndex) { ... }

		Buttons.Add(Button);
	}

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d ring buttons"), Buttons.Num() - 1);
	return Buttons;
}

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetItemsButtons(UCharacterData* CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::ItemGrid);

	// TODO: Get items from inventory system
	// For now, placeholder items

	FPieMenuButtonData Potion;
	Potion.ButtonID = TEXT("Item_Potion");
	Potion.DisplayName = FText::FromString(TEXT("Health Potion"));
	Potion.Description = FText::FromString(TEXT("Restores 50 HP"));
	Potion.Category = EPieMenuCategory::Item;
	Potion.DataIndex = 0;
	Potion.bEnabled = true;
	Buttons.Add(Potion);

	FPieMenuButtonData Ether;
	Ether.ButtonID = TEXT("Item_Ether");
	Ether.DisplayName = FText::FromString(TEXT("Ether"));
	Ether.Description = FText::FromString(TEXT("Restores 30 EP"));
	Ether.Category = EPieMenuCategory::Item;
	Ether.DataIndex = 1;
	Ether.bEnabled = true;
	Buttons.Add(Ether);

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d item buttons (placeholder)"), Buttons.Num() - 1);
	return Buttons;
}

// ==================== ACTIONS ====================

bool UCombatMenuSubsystem::ExecuteSwitchWeapon(UCharacterData* CharacterData)
{
	if (!CharacterData || !CanSwitchWeapon(CharacterData))
	{
		return false;
	}

	// Toggle bUsePrimary
	CharacterData->bUsePrimary = !CharacterData->bUsePrimary;

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Switched weapon. Now using %s"),
		CharacterData->bUsePrimary ? TEXT("Primary") : TEXT("Secondary"));

	// Request menu refresh
	OnMenuRefreshRequested.Broadcast();

	return true;
}

bool UCombatMenuSubsystem::ExecuteRingSwitch(UCharacterData* CharacterData, int32 RingIndex)
{
	if (!CharacterData || !CharacterData->IsResonator())
	{
		return false;
	}

	if (!CharacterData->EquippedRings.IsValidIndex(RingIndex))
	{
		return false;
	}

	// TODO: Set active ring index in runtime state
	// For now, just log
	URingData* Ring = CharacterData->EquippedRings[RingIndex];
	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Switched to ring: %s"),
		Ring ? *Ring->RingName : TEXT("None"));

	// Request menu refresh (Resonate spells change)
	OnMenuRefreshRequested.Broadcast();

	return true;
}

// ==================== SELECTION HANDLING ====================

void UCombatMenuSubsystem::HandleButtonSelection(const FPieMenuButtonData& ButtonData)
{
	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Button selected: %s (Category: %d)"),
		*ButtonData.ButtonID, static_cast<int32>(ButtonData.Category));

	switch (ButtonData.Category)
	{
	// === IMMEDIATE ACTIONS (execute and close) ===
	case EPieMenuCategory::Attack:
		OnExecuteAction.Broadcast(ButtonData);
		OnMenuCloseRequested.Broadcast();
		break;

	case EPieMenuCategory::Spell:
	case EPieMenuCategory::Ability:
	case EPieMenuCategory::Item:
		OnExecuteAction.Broadcast(ButtonData);
		OnMenuCloseRequested.Broadcast();
		break;

	// === SWITCH WEAPON (toggle and refresh) ===
	case EPieMenuCategory::SwitchWeapon:
		if (CurrentCharacter.IsValid())
		{
			ExecuteSwitchWeapon(CurrentCharacter.Get());
		}
		break;

	// === RING SELECTION (switch and stay open) ===
	case EPieMenuCategory::Ring:
		if (CurrentCharacter.IsValid())
		{
			ExecuteRingSwitch(CurrentCharacter.Get(), ButtonData.DataIndex);
		}
		// Menu stays open, returns to main
		SetMenuState(EPieMenuState::Main);
		break;

	// === OPEN SUB-MENUS ===
	case EPieMenuCategory::Abilities:
	case EPieMenuCategory::Refractions:
	case EPieMenuCategory::Resonate:
	case EPieMenuCategory::ChangeRing:
	case EPieMenuCategory::Items:
	case EPieMenuCategory::School:
		OnOpenSubMenu.Broadcast(ButtonData);
		break;

	// === NAVIGATION ===
	case EPieMenuCategory::Back:
		NavigateBack();
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[CombatMenuSubsystem] Unhandled category: %d"),
			static_cast<int32>(ButtonData.Category));
		break;
	}
}

// ==================== STATE MANAGEMENT ====================

void UCombatMenuSubsystem::SetMenuState(EPieMenuState NewState)
{
	if (CurrentState != NewState)
	{
		EPieMenuState OldState = CurrentState;
		CurrentState = NewState;
		OnStateChanged.Broadcast(OldState, NewState);

		UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] State: %s -> %s"),
			*UEnum::GetValueAsString(OldState),
			*UEnum::GetValueAsString(NewState));
	}
}

void UCombatMenuSubsystem::NavigateBack()
{
	switch (CurrentState)
	{
	case EPieMenuState::Schools:
	case EPieMenuState::AbilityGrid:
	case EPieMenuState::RingGrid:
	case EPieMenuState::ItemGrid:
		SetMenuState(EPieMenuState::Main);
		OnMenuRefreshRequested.Broadcast();
		break;

	case EPieMenuState::SpellGrid:
		SetMenuState(EPieMenuState::Schools);
		OnMenuRefreshRequested.Broadcast();
		break;

	case EPieMenuState::Main:
		SetMenuState(EPieMenuState::Closed);
		OnMenuCloseRequested.Broadcast();
		break;

	default:
		break;
	}
}

// ==================== HELPERS ====================

TArray<USpellData*> UCombatMenuSubsystem::GetSpellsBySchool(UCharacterData* CharacterData, EPieMenuSpellSchool School) const
{
	TArray<USpellData*> Result;
	TArray<USpellData*> AllSpells = GetAllSpells(CharacterData);

	for (USpellData* Spell : AllSpells)
	{
		if (Spell && static_cast<int32>(Spell->School) == static_cast<int32>(School))
		{
			Result.Add(Spell);
			if (Result.Num() >= MAX_SPELLS_PER_SCHOOL)
			{
				break;
			}
		}
	}

	return Result;
}

TArray<USpellData*> UCombatMenuSubsystem::GetAllSpells(UCharacterData* CharacterData) const
{
	TArray<USpellData*> Result;

	if (!CharacterData)
	{
		return Result;
	}

	if (CharacterData->IsCaster())
	{
		// Caster: use innate spells
		Result = CharacterData->InnateSpells;
	}
	else if (CharacterData->IsResonator())
	{
		// Resonator: get spells from current active ring
		// TODO: Get active ring index from runtime state
		int32 ActiveRingIndex = 0;
		
		if (CharacterData->EquippedRings.IsValidIndex(ActiveRingIndex))
		{
			URingData* Ring = CharacterData->EquippedRings[ActiveRingIndex];
			if (Ring)
			{
				// TODO: Get spells from ring
				// Result = Ring->RingSpells;
			}
		}
	}

	return Result;
}

TArray<UAbilityData*> UCombatMenuSubsystem::GetCurrentWeaponAbilities(UCharacterData* CharacterData) const
{
	TArray<UAbilityData*> Result;

	if (!CharacterData)
	{
		return Result;
	}

	UWeaponData* CurrentWeapon = CharacterData->bUsePrimary ? 
		CharacterData->PrimaryWeapon : CharacterData->SecondaryWeapon;

	if (CurrentWeapon)
	{
		// TODO: Get abilities from weapon
		// Result = CurrentWeapon->WeaponAbilities;
	}

	return Result;
}

int32 UCombatMenuSubsystem::CountSpellsInSchool(UCharacterData* CharacterData, EPieMenuSpellSchool School) const
{
	TArray<USpellData*> AllSpells = GetAllSpells(CharacterData);
	int32 Count = 0;

	for (USpellData* Spell : AllSpells)
	{
		if (Spell && static_cast<int32>(Spell->School) == static_cast<int32>(School))
		{
			Count++;
		}
	}

	return FMath::Min(Count, MAX_SPELLS_PER_SCHOOL);
}

bool UCombatMenuSubsystem::CanSwitchWeapon(UCharacterData* CharacterData) const
{
	if (!CharacterData)
	{
		return false;
	}

	// Generic: needs secondary weapon slot to have something
	if (CharacterData->IsGeneric())
	{
		return CharacterData->SecondaryWeapon != nullptr || 
			   CharacterData->SecondaryRing != nullptr;
	}

	// Caster: can toggle armed/unarmed if they have a weapon
	if (CharacterData->IsCaster())
	{
		return CharacterData->PrimaryWeapon != nullptr || 
			   CharacterData->PrimaryRing != nullptr;
	}

	// Resonator: can toggle armed/unarmed
	if (CharacterData->IsResonator())
	{
		return CharacterData->PrimaryWeapon != nullptr;
	}

	return false;
}

FLinearColor UCombatMenuSubsystem::GetElementColor(int32 ElementIndex) const
{
	// Nine-element system colors
	static const TArray<FLinearColor> ElementColors = {
		FLinearColor(1.0f, 0.3f, 0.1f, 1.0f),   // 0: Fire
		FLinearColor(0.2f, 0.5f, 1.0f, 1.0f),   // 1: Water
		FLinearColor(0.6f, 0.4f, 0.2f, 1.0f),   // 2: Earth
		FLinearColor(0.7f, 1.0f, 0.7f, 1.0f),   // 3: Wind
		FLinearColor(1.0f, 1.0f, 0.8f, 1.0f),   // 4: Light
		FLinearColor(0.3f, 0.1f, 0.4f, 1.0f),   // 5: Darkness
		FLinearColor(1.0f, 1.0f, 0.3f, 1.0f),   // 6: Lightning
		FLinearColor(0.1f, 0.1f, 0.2f, 1.0f),   // 7: Void
		FLinearColor(1.0f, 0.5f, 1.0f, 1.0f),   // 8: Reality
		FLinearColor(0.7f, 0.7f, 0.7f, 1.0f),   // 9+: Generic
	};

	if (ElementIndex >= 0 && ElementIndex < ElementColors.Num())
	{
		return ElementColors[ElementIndex];
	}
	return ElementColors.Last();
}

// ==================== DEBUG ====================

void UCombatMenuSubsystem::DebugLogMenuState() const
{
	FString StateStr = UEnum::GetValueAsString(CurrentState);
	StateStr.RemoveFromStart(TEXT("EPieMenuState::"));

	FString CharName = CurrentCharacter.IsValid() ? CurrentCharacter->CharacterName : TEXT("None");
	FString ClassStr = CurrentCharacter.IsValid() ? 
		UEnum::GetValueAsString(CurrentCharacter->CharacterClass) : TEXT("N/A");
	ClassStr.RemoveFromStart(TEXT("ECharacterClass::"));

	UE_LOG(LogTemp, Log, TEXT("========== Combat Menu State =========="));
	UE_LOG(LogTemp, Log, TEXT("  State: %s"), *StateStr);
	UE_LOG(LogTemp, Log, TEXT("  Character: %s"), *CharName);
	UE_LOG(LogTemp, Log, TEXT("  Class: %s"), *ClassStr);
	UE_LOG(LogTemp, Log, TEXT("  Selected School: %d"), static_cast<int32>(SelectedSchool));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}
