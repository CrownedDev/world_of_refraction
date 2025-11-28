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
#include "EWeaponSlotType.h"

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
	/**
	 * Generic has 12 states based on equipment:
	 * 
	 * SINGLE WEAPON (States 1-4):
	 * 1. Weapon only                    -> Attack, Abilities, Items
	 * 2. Evolved Weapon only            -> Attack, Abilities, Resonate, Items
	 * 3. Evolved Char + Weapon          -> Attack, Abilities, Breakthrough, Items
	 * 4. Evolved Char + Evolved Weapon  -> Attack, Abilities, Breakthrough, Resonate, Items
	 * 
	 * UNIFIED MENU - Weapon + Ring (States 5-8):
	 * 5. Weapon + Ring                  -> Attack, Abilities, Resonate (R), Items
	 * 6. Weapon + Evolved Ring          -> Attack, Abilities, Resonate (R), Items
	 * 7. Evolved Weapon + Ring          -> Attack, Abilities, Resonate (W), Resonate (R), Items
	 * 8. Evolved Weapon + Evolved Ring  -> Attack, Abilities, Resonate (W), Resonate (R), Items
	 * 
	 * SPLIT MENU - Weapon + Weapon (States 9-12):
	 * 9-10. Primary active              -> Attack, Abilities, [Resonate if evolved], Items, Switch
	 * 11-12. Secondary active           -> Attack, Abilities, [Resonate if evolved], Items, Switch
	 * 
	 * NOTE: Evolved Generic loses secondary slot (States 3-4 have DISABLED secondary)
	 */
	
	bool bCharacterEvolved = CharacterData->IsEvolved();
	
	// Evolved Generic loses secondary slot access
	bool bHasSecondaryWeapon = !bCharacterEvolved && 
							   CharacterData->SecondarySlotType == ESecondarySlotType::Weapon && 
							   CharacterData->SecondaryWeapon != nullptr;
	bool bHasSecondaryRing = !bCharacterEvolved && 
							 CharacterData->SecondarySlotType == ESecondarySlotType::Ring && 
							 CharacterData->SecondaryRing != nullptr;
	
	// Get active weapon based on current selection
	UWeaponData* ActiveWeapon = nullptr;
	if (bHasSecondaryWeapon)
	{
		// Dual weapons - use bUsePrimary to determine active
		ActiveWeapon = CharacterData->bUsePrimary ? 
			CharacterData->PrimaryWeapon : CharacterData->SecondaryWeapon;
	}
	else
	{
		// Single weapon or unified menu - always primary
		ActiveWeapon = CharacterData->PrimaryWeapon;
	}
	
	bool bActiveWeaponEvolved = ActiveWeapon && ActiveWeapon->IsEvolved();
	bool bUnifiedMenu = bHasSecondaryRing; // Weapon + Ring = unified menu
	
	// Attack + Abilities (if has weapon)
	if (ActiveWeapon)
	{
		OutButtons.Add(CreateAttackButton(CharacterData));
		OutButtons.Add(CreateAbilitiesButton(CharacterData));
	}
	
	// Breakthrough (if character is evolved)
	if (bCharacterEvolved)
	{
		OutButtons.Add(CreateBreakthroughButton(CharacterData));
	}
	
	// Resonate from evolved weapon
	// Use (W) modifier only in unified menu where ring is also present
	if (bActiveWeaponEvolved)
	{
		if (bUnifiedMenu)
		{
			OutButtons.Add(CreateResonateWeaponButton(CharacterData)); // Shows "Resonate (W)"
		}
		else
		{
			OutButtons.Add(CreateResonateButton(CharacterData)); // Shows "Resonate" (no modifier)
		}
	}
	
	// Resonate (R) from secondary ring (unified menu only)
	if (bHasSecondaryRing)
	{
		OutButtons.Add(CreateResonateRingButton(CharacterData));
	}
	
	// Items always
	OutButtons.Add(CreateItemsButton(CharacterData));
	
	// Switch only if dual wielding weapons (States 9-12) and NOT evolved
	if (bHasSecondaryWeapon)
	{
		OutButtons.Add(CreateSwitchWeaponButton(CharacterData));
	}
}

void UCombatMenuSubsystem::BuildCasterMainMenu(UCharacterData* CharacterData, TArray<FPieMenuButtonData>& OutButtons)
{
	/**
	 * Caster has 5 states:
	 * 
	 * State 1: Weapon        -> Attack, Abilities, Refractions, Items
	 * State 2: Ring          -> Refractions, Resonate, Items
	 * State 3: Evolved Weapon -> Attack, Abilities, Refractions, Resonate, Items
	 * State 4: Evolved Ring   -> Refractions, Resonate, Items
	 * State 5: Evolved Character -> Refractions, Breakthrough, Items
	 * 
	 * IMPORTANT: 
	 * - Caster has NO Switch button (single equipment slot)
	 * - Caster uses "Resonate" without modifier (single source)
	 * - Evolved Caster loses ALL equipment access
	 */
	
	bool bCharacterEvolved = CharacterData->IsEvolved();
	
	// Caster has ONE slot: Weapon OR Ring (PrimarySlotType determines which)
	bool bHasWeapon = CharacterData->PrimarySlotType == EPrimarySlotType::Weapon && 
					  CharacterData->PrimaryWeapon != nullptr;
	bool bHasRing = CharacterData->PrimarySlotType == EPrimarySlotType::Ring && 
					CharacterData->PrimaryRing != nullptr;
	bool bWeaponEvolved = bHasWeapon && CharacterData->PrimaryWeapon->IsEvolved();
	bool bRingEvolved = bHasRing && CharacterData->PrimaryRing->IsEvolved();
	
	// When character evolves, all equipment is DISABLED (State 5)
	if (bCharacterEvolved)
	{
		OutButtons.Add(CreateRefractionsButton(CharacterData));
		OutButtons.Add(CreateBreakthroughButton(CharacterData));
		OutButtons.Add(CreateItemsButton(CharacterData));
		return;
	}
	
	// Attack + Abilities only if has weapon (States 1, 3)
	if (bHasWeapon)
	{
		OutButtons.Add(CreateAttackButton(CharacterData));
		OutButtons.Add(CreateAbilitiesButton(CharacterData));
	}
	
	// Refractions always (innate spells)
	OutButtons.Add(CreateRefractionsButton(CharacterData));
	
	// Resonate from evolved weapon (State 3) or ring (States 2, 4)
	// Caster uses "Resonate" without (W)/(R) modifier since they have single source
	if (bWeaponEvolved || bHasRing)
	{
		OutButtons.Add(CreateResonateButton(CharacterData));
	}
	
	// Items always
	OutButtons.Add(CreateItemsButton(CharacterData));
	
	// NO SWITCH BUTTON for Caster
}

void UCombatMenuSubsystem::BuildResonatorMainMenu(UCharacterData* CharacterData, TArray<FPieMenuButtonData>& OutButtons)
{
	/**
	 * Resonator has 6 states:
	 * 
	 * WITH WEAPON (States 1-4):
	 * 1. Weapon + Normal Ring      -> Attack, Abilities, Resonate (R), Switch Ring, Items
	 * 2. Weapon + Evolved Ring     -> Attack, Abilities, Resonate (R), Switch Ring, Items
	 * 3. Evolved Weapon + Normal Ring   -> Attack, Abilities, Resonate (W), Resonate (R), Switch Ring, Items
	 * 4. Evolved Weapon + Evolved Ring  -> Attack, Abilities, Resonate (W), Resonate (R), Switch Ring, Items
	 * 
	 * EVOLVED CHARACTER - Weapon DISABLED (States 5-6):
	 * 5. DISABLED + Normal Ring    -> Breakthrough, Resonate (R), Switch Ring, Items
	 * 6. DISABLED + Evolved Ring   -> Breakthrough, Resonate (R), Switch Ring, Items
	 * 
	 * IMPORTANT:
	 * - Resonator has Switch Ring, NOT Switch Weapon
	 * - Resonator uses "(W)" and "(R)" modifiers since both sources may be present
	 * - Evolved Resonator loses weapon but keeps ring loadout
	 */
	
	bool bCharacterEvolved = CharacterData->IsEvolved();
	
	// Evolved Resonator loses weapon access but keeps rings
	bool bHasWeapon = !bCharacterEvolved && CharacterData->PrimaryWeapon != nullptr;
	bool bWeaponEvolved = bHasWeapon && CharacterData->PrimaryWeapon->IsEvolved();
	
	// Get active ring (TODO: track active ring index at runtime)
	URingData* ActiveRing = nullptr;
	if (CharacterData->EquippedRings.Num() > 0)
	{
		ActiveRing = CharacterData->EquippedRings[0]; // Default to first ring
	}
	
	// Attack + Abilities only if has weapon (States 1-4)
	if (bHasWeapon)
	{
		OutButtons.Add(CreateAttackButton(CharacterData));
		OutButtons.Add(CreateAbilitiesButton(CharacterData));
	}
	
	// Breakthrough (if character is evolved - States 5-6)
	if (bCharacterEvolved)
	{
		OutButtons.Add(CreateBreakthroughButton(CharacterData));
	}
	
	// Resonate (W) from evolved weapon (States 3-4 only)
	if (bWeaponEvolved)
	{
		OutButtons.Add(CreateResonateWeaponButton(CharacterData));
	}
	
	// Resonate (R) from active ring - always available for Resonator
	if (ActiveRing)
	{
		OutButtons.Add(CreateResonateRingButton(CharacterData));
	}
	
	// Switch Ring always available for Resonator
	OutButtons.Add(CreateChangeRingButton(CharacterData));
	
	// Items always
	OutButtons.Add(CreateItemsButton(CharacterData));
	
	// NO SWITCH WEAPON for Resonator
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
	/**
	 * CreateResonateButton - Plain "Resonate" (no modifier)
	 * Used when there's only ONE resonate source:
	 * - Caster with Ring (States 2, 4)
	 * - Caster with Evolved Weapon (State 3)
	 * - Generic with Evolved Weapon only (States 2, 4, 10, 12)
	 * 
	 * Must detect whether source is Ring or Evolved Weapon
	 */
	
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Resonate");
	Button.DisplayName = FText::FromString(TEXT("Resonate"));
	Button.Category = EPieMenuCategory::Resonate;
	
	// Detect the spell source - Ring or Evolved Weapon?
	bool bSourceIsRing = false;
	bool bSourceIsEvolvedWeapon = false;
	bool bHasBreakChance = false;
	int32 SpellCount = 0;
	FLinearColor TintColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	
	if (CharacterData->IsCaster())
	{
		// Caster: Check PrimarySlotType
		if (CharacterData->PrimarySlotType == EPrimarySlotType::Ring && CharacterData->PrimaryRing)
		{
			// Source is ring
			bSourceIsRing = true;
			bHasBreakChance = !CharacterData->PrimaryRing->IsEvolved();
			SpellCount = CharacterData->PrimaryRing->GetAvailableSpells().Num();
			TintColor = GetElementColor(static_cast<int32>(CharacterData->PrimaryRing->Element));
		}
		else if (CharacterData->PrimarySlotType == EPrimarySlotType::Weapon && 
				 CharacterData->PrimaryWeapon && CharacterData->PrimaryWeapon->IsEvolved())
		{
			// Source is evolved weapon
			bSourceIsEvolvedWeapon = true;
			bHasBreakChance = false; // Evolved weapon has no break
			// TODO: Get spell count from weapon evolution
			SpellCount = 0; // Placeholder until we have GetWeaponEvolutionSpells()
			TintColor = GetElementColor(static_cast<int32>(CharacterData->PrimaryWeapon->GetWeaponElement()));
		}
	}
	else if (CharacterData->IsGeneric())
	{
		// Generic: Source is always evolved weapon (ring uses CreateResonateRingButton in unified menu)
		UWeaponData* ActiveWeapon = CharacterData->bUsePrimary ? 
			CharacterData->PrimaryWeapon : CharacterData->SecondaryWeapon;
		
		if (ActiveWeapon && ActiveWeapon->IsEvolved())
		{
			bSourceIsEvolvedWeapon = true;
			bHasBreakChance = false;
			// TODO: Get spell count from weapon evolution
			SpellCount = 0; // Placeholder
			TintColor = GetElementColor(static_cast<int32>(ActiveWeapon->GetWeaponElement()));
		}
	}
	
	// Set description based on detected source
	if (bSourceIsRing)
	{
		if (bHasBreakChance)
		{
			Button.Description = FText::FromString(FString::Printf(TEXT("%d ring spells (break chance)"), SpellCount));
		}
		else
		{
			Button.Description = FText::FromString(FString::Printf(TEXT("%d ring spells (no break)"), SpellCount));
		}
	}
	else if (bSourceIsEvolvedWeapon)
	{
		Button.Description = FText::FromString(FString::Printf(TEXT("%d weapon spells (no break)"), SpellCount));
	}
	else
	{
		Button.Description = FText::FromString(TEXT("No resonate source"));
	}
	
	Button.bEnabled = (bSourceIsRing || bSourceIsEvolvedWeapon);
	Button.ButtonTint = TintColor;
	
	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}
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

	// Switch only appears for Generic with dual weapons
	// Show what we'll switch TO (the other weapon)
	UWeaponData* OtherWeapon = CharacterData->bUsePrimary ? 
		CharacterData->SecondaryWeapon : CharacterData->PrimaryWeapon;
	
	if (OtherWeapon)
	{
		Button.Description = FText::FromString(FString::Printf(TEXT("Switch to %s"), *OtherWeapon->WeaponName));
		Button.bEnabled = true;
	}
	else
	{
		// This shouldn't happen since we only add Switch when dual wielding
		Button.Description = FText::FromString(TEXT("No alternate weapon"));
		Button.bEnabled = false;
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateBreakthroughButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Breakthrough");
	Button.DisplayName = FText::FromString(TEXT("Breakthrough"));
	Button.Category = EPieMenuCategory::Breakthrough; // Character evolution spells
	Button.Description = FText::FromString(TEXT("Character Evolution spells (no break chance)"));
	Button.bEnabled = CharacterData->IsEvolved();
	
	// Tint with character's element
	Button.ButtonTint = GetElementColor(static_cast<int32>(CharacterData->InnateElement));
	
	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateResonateWeaponButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("ResonateWeapon");
	Button.DisplayName = FText::FromString(TEXT("Resonate (W)"));
	Button.Category = EPieMenuCategory::ResonateWeapon; // Evolved weapon spells
	Button.Description = FText::FromString(TEXT("Evolved weapon spells (no break chance)"));
	
	UWeaponData* Weapon = CharacterData->PrimaryWeapon;
	if (Weapon && Weapon->IsEvolved())
	{
		Button.bEnabled = true;
		Button.ButtonTint = GetElementColor(static_cast<int32>(Weapon->GetWeaponElement()));
	}
	else
	{
		Button.bEnabled = false;
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateResonateRingButton(UCharacterData* CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("ResonateRing");
	Button.DisplayName = FText::FromString(TEXT("Resonate (R)"));
	Button.Category = EPieMenuCategory::ResonateRing; // Ring spells
	
	// Get ring based on character class:
	// - Caster: PrimaryRing (single slot)
	// - Generic: SecondaryRing (if secondary slot is ring)
	// - Resonator: EquippedRings[ActiveIndex]
	URingData* Ring = nullptr;
	
	if (CharacterData->IsCaster())
	{
		Ring = CharacterData->PrimaryRing;
	}
	else if (CharacterData->IsGeneric())
	{
		Ring = CharacterData->SecondaryRing;
	}
	else if (CharacterData->IsResonator())
	{
		// TODO: Get active ring index from runtime state
		if (CharacterData->EquippedRings.Num() > 0)
		{
			Ring = CharacterData->EquippedRings[0];
		}
	}
	
	if (Ring)
	{
		bool bRingEvolved = Ring->IsEvolved();
		if (bRingEvolved)
		{
			Button.Description = FText::FromString(FString::Printf(TEXT("%s spells (no break chance)"), *Ring->RingName));
		}
		else
		{
			Button.Description = FText::FromString(FString::Printf(TEXT("%s spells (break chance)"), *Ring->RingName));
		}
		Button.bEnabled = true;
		Button.ButtonTint = GetElementColor(static_cast<int32>(Ring->Element));
	}
	else
	{
		Button.Description = FText::FromString(TEXT("No ring equipped"));
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
	case EPieMenuCategory::Breakthrough:		// Character evolution spells -> Schools
	case EPieMenuCategory::Resonate:			// Single source resonate -> Schools
	case EPieMenuCategory::ResonateWeapon:		// Evolved weapon spells -> Schools
	case EPieMenuCategory::ResonateRing:		// Ring spells -> Schools
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

	/**
	 * Per Combat_Menu_UI_Specification.md:
	 * - Generic: Switch ONLY when has two weapons (States 9-12) AND not evolved
	 * - Evolved Generic loses secondary slot (States 3-4)
	 * - Generic with Weapon + Ring = NO switch (unified menu, States 5-8)
	 * - Caster: NO switch (single equipment slot)
	 * - Resonator: NO switch weapon (has Switch Ring instead)
	 */
	
	if (CharacterData->IsGeneric() && !CharacterData->IsEvolved())
	{
		// Only allow switch when dual wielding WEAPONS (not weapon + ring)
		return CharacterData->SecondarySlotType == ESecondarySlotType::Weapon && 
			   CharacterData->PrimaryWeapon != nullptr && 
			   CharacterData->SecondaryWeapon != nullptr;
	}

	// Caster and Resonator don't have weapon switching
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
