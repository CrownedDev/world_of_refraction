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
#include "EInfusionSourceOption.h"
#include "CrystalType.h"
#include "WeaponManager.h"
#include "RingManager.h"

// ==================== SUBSYSTEM LIFECYCLE ====================

void UCombatMenuSubsystem::Initialize(FSubsystemCollectionBase &Collection)
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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetMainMenuButtons(UCharacterData *CharacterData)
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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetMainMenuButtonsForActor(AActor *Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatMenuSubsystem] GetMainMenuButtonsForActor: null Actor"));
		return TArray<FPieMenuButtonData>();
	}

	CurrentActor = Actor;

	// Get CharacterData from component
	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		return GetMainMenuButtons(CharComp->CharacterData);
	}

	UE_LOG(LogTemp, Warning, TEXT("[CombatMenuSubsystem] GetMainMenuButtonsForActor: no CharacterDataComponent"));
	return TArray<FPieMenuButtonData>();
}
// ==================== MAIN MENU BUILDERS ====================

void UCombatMenuSubsystem::BuildGenericMainMenu(UCharacterData *CharacterData, TArray<FPieMenuButtonData> &OutButtons)
{
	/**
	 * Generic Menu States based on Primary + Secondary slot combinations:
	 *
	 * PRIMARY WEAPON (States 1-6):
	 * 1. Weapon only                      -> Attack, Abilities, Items
	 * 2. Weapon (evolved) only            -> Attack, Abilities, Resonate, Items
	 * 3. Weapon + Weapon                  -> Attack, Abilities, Items, Switch
	 * 4. Weapon + Weapon (pri evolved)    -> Attack, Abilities, Resonate, Items, Switch
	 * 5. Weapon + Weapon (sec evolved)    -> Attack, Abilities, [Resonate if active], Items, Switch
	 * 6. Weapon + Weapon (both evolved)   -> Attack, Abilities, Resonate, Items, Switch
	 *
	 * PRIMARY RING (States 7-10):
	 * 7. Ring only                        -> Resonate (R), Items
	 * 8. Ring (evolved) only              -> Resonate (R), Items
	 * 9. Ring + Weapon                    -> Attack, Abilities, Resonate (R), Items
	 * 10. Ring + Weapon (evolved)         -> Attack, Abilities, Resonate (W), Resonate (R), Items
	 *
	 * PRIMARY EVOLUTION (States 11-14):
	 * 11. Evolution only                  -> Breakthrough, Items
	 * 12. Evolution + Weapon              -> Attack, Abilities, Breakthrough, Items
	 * 13. Evolution + Weapon (evolved)    -> Attack, Abilities, Breakthrough, Resonate, Items
	 *
	 * NOTE: Evolved rings have no break chance, normal rings do.
	 * NOTE: "Resonate (W)" and "Resonate (R)" modifiers only shown when both sources present.
	 */

	bool bCharacterEvolved = CharacterData->IsEvolved();

	// Check secondary weapon availability
	bool bHasSecondaryWeapon = !bCharacterEvolved &&
							   CharacterData->SecondarySlotType == ESecondarySlotType::Weapon &&
							   CharacterData->SecondaryWeapon != nullptr;

	// Check primary slot type
	bool bHasPrimaryRing = CharacterData->PrimarySlotType == EPrimarySlotType::Ring &&
						   CharacterData->PrimaryRing != nullptr;
	bool bHasPrimaryEvolution = CharacterData->PrimarySlotType == EPrimarySlotType::Evolution &&
								CharacterData->PrimaryEvolution != nullptr;
	bool bHasPrimaryWeapon = CharacterData->PrimarySlotType == EPrimarySlotType::Weapon &&
							 CharacterData->PrimaryWeapon != nullptr;

	// Determine active weapon
	UWeaponData *ActiveWeapon = nullptr;
	if (bHasPrimaryWeapon)
	{
		// Primary is weapon
		if (bHasSecondaryWeapon)
		{
			// Two weapons - use bUsePrimary to determine active
			ActiveWeapon = CharacterData->bUsePrimary ? CharacterData->PrimaryWeapon : CharacterData->SecondaryWeapon;
		}
		else
		{
			ActiveWeapon = CharacterData->PrimaryWeapon;
		}
	}
	else if (bHasSecondaryWeapon)
	{
		// Primary is Ring/Evolution, weapon comes from secondary
		ActiveWeapon = CharacterData->SecondaryWeapon;
	}
	// else: No weapon access

	bool bActiveWeaponEvolved = ActiveWeapon && ActiveWeapon->IsEvolved();

	// Attack + Abilities (if has weapon)
	if (ActiveWeapon)
	{
		OutButtons.Add(CreateAttackButton(CharacterData));
		OutButtons.Add(CreateAbilitiesButton(CharacterData));
	}

	// Breakthrough (if character has evolution in primary)
	if (bHasPrimaryEvolution)
	{
		OutButtons.Add(CreateBreakthroughButton(CharacterData));
	}

	// Resonate from evolved weapon
	if (bActiveWeaponEvolved)
	{
		if (bHasPrimaryRing)
		{
			OutButtons.Add(CreateResonateWeaponButton(CharacterData)); // "Resonate (W)"
		}
		else
		{
			OutButtons.Add(CreateResonateButton(CharacterData)); // "Resonate"
		}
	}

	// Resonate (R) from primary ring
	if (bHasPrimaryRing)
	{
		OutButtons.Add(CreateResonateRingButton(CharacterData));
	}

	// Items always
	OutButtons.Add(CreateItemsButton(CharacterData));

	// Switch only if Primary=Weapon + Secondary=Weapon
	if (bHasPrimaryWeapon && bHasSecondaryWeapon)
	{
		OutButtons.Add(CreateSwitchWeaponButton(CharacterData));
	}
}

void UCombatMenuSubsystem::BuildCasterMainMenu(UCharacterData *CharacterData, TArray<FPieMenuButtonData> &OutButtons)
{
	/**
	 * Caster Menu States based on Primary slot:
	 *
	 * PRIMARY WEAPON (States 1-2):
	 * 1. Weapon only                -> Attack, Abilities, Refractions, Items
	 * 2. Weapon (evolved)           -> Attack, Abilities, Refractions, Resonate, Items
	 *
	 * PRIMARY RING (States 3-4):
	 * 3. Ring only                  -> Refractions, Resonate (R), Items
	 * 4. Ring (evolved)             -> Refractions, Resonate (R), Items
	 *
	 * PRIMARY EVOLUTION (State 5):
	 * 5. Evolution                  -> Refractions, Breakthrough, Items
	 *
	 * NOTE: Caster ALWAYS has Refractions (innate spells from InnateElement).
	 * NOTE: Caster has NO Switch button (single equipment slot).
	 * NOTE: Caster with Evolution loses all weapon/ring access.
	 * NOTE: Ring break chance only applies to non-evolved rings.
	 */

	// Check primary slot type
	bool bHasPrimaryWeapon = CharacterData->PrimarySlotType == EPrimarySlotType::Weapon &&
							 CharacterData->PrimaryWeapon != nullptr;
	bool bHasPrimaryRing = CharacterData->PrimarySlotType == EPrimarySlotType::Ring &&
						   CharacterData->PrimaryRing != nullptr;
	bool bHasPrimaryEvolution = CharacterData->PrimarySlotType == EPrimarySlotType::Evolution &&
								CharacterData->PrimaryEvolution != nullptr;

	bool bWeaponEvolved = bHasPrimaryWeapon && CharacterData->PrimaryWeapon->IsEvolved();
	bool bRingEvolved = bHasPrimaryRing && CharacterData->PrimaryRing->IsEvolved();

	// Attack + Abilities (only if primary is weapon)
	if (bHasPrimaryWeapon)
	{
		OutButtons.Add(CreateAttackButton(CharacterData));
		OutButtons.Add(CreateAbilitiesButton(CharacterData));
	}

	// Refractions (innate spells) - ALWAYS available for Caster
	OutButtons.Add(CreateRefractionsButton(CharacterData));

	// Breakthrough (if primary is evolution)
	if (bHasPrimaryEvolution)
	{
		OutButtons.Add(CreateBreakthroughButton(CharacterData));
	}

	// Resonate from evolved weapon (primary weapon only)
	if (bWeaponEvolved)
	{
		OutButtons.Add(CreateResonateButton(CharacterData)); // No modifier needed, single source
	}

	// Resonate from primary ring
	if (bHasPrimaryRing)
	{
		OutButtons.Add(CreateResonateButton(CharacterData)); // No modifier needed, single source
	}

	// Items always
	OutButtons.Add(CreateItemsButton(CharacterData));

	// Caster has NO Switch button
}

void UCombatMenuSubsystem::BuildResonatorMainMenu(UCharacterData *CharacterData, TArray<FPieMenuButtonData> &OutButtons)
{
	/**
	 * Resonator Menu States based on Primary slot + Ring Loadout:
	 *
	 * PRIMARY WEAPON (States 1-4):
	 * 1. Weapon + Rings                 -> Attack, Abilities, Resonate, Items, Switch Ring
	 * 2. Weapon (evolved) + Rings       -> Attack, Abilities, Resonate (W), Resonate (R), Items, Switch Ring
	 * 3. Weapon + No Rings              -> Attack, Abilities, Items
	 * 4. Weapon (evolved) + No Rings    -> Attack, Abilities, Resonate, Items
	 *
	 * PRIMARY EVOLUTION (States 5-6):
	 * 5. Evolution + Rings              -> Breakthrough, Resonate, Items, Switch Ring
	 * 6. Evolution + No Rings           -> Breakthrough, Items
	 *
	 * NOTE: Resonator CANNOT have Ring in primary slot (uses RingLoadout instead).
	 * NOTE: Resonator has NO secondary slot.
	 * NOTE: RingLoadout: 5 rings normal, 3 rings if evolved, max 2 evolved rings.
	 * NOTE: Switch Ring cycles through RingLoadout (not weapon switch).
	 * NOTE: "Resonate (W)" and "Resonate (R)" modifiers only shown when both sources present.
	 */

	// Check primary slot type
	bool bHasPrimaryWeapon = CharacterData->PrimarySlotType == EPrimarySlotType::Weapon &&
							 CharacterData->PrimaryWeapon != nullptr;
	bool bHasPrimaryEvolution = CharacterData->PrimarySlotType == EPrimarySlotType::Evolution &&
								CharacterData->PrimaryEvolution != nullptr;

	bool bWeaponEvolved = bHasPrimaryWeapon && CharacterData->PrimaryWeapon->IsEvolved();

	// Check ring loadout
	bool bHasRings = CharacterData->EquippedRings.Num() > 0;
	bool bHasMultipleRings = CharacterData->EquippedRings.Num() > 1;

	// Attack + Abilities (only if primary is weapon)
	if (bHasPrimaryWeapon)
	{
		OutButtons.Add(CreateAttackButton(CharacterData));
		OutButtons.Add(CreateAbilitiesButton(CharacterData));
	}

	// Breakthrough (if primary is evolution)
	if (bHasPrimaryEvolution)
	{
		OutButtons.Add(CreateBreakthroughButton(CharacterData));
	}

	// Resonate from evolved weapon
	if (bWeaponEvolved)
	{
		if (bHasRings)
		{
			OutButtons.Add(CreateResonateWeaponButton(CharacterData)); // "Resonate (W)"
		}
		else
		{
			OutButtons.Add(CreateResonateButton(CharacterData)); // "Resonate"
		}
	}

	// Resonate from active ring in loadout
	if (bHasRings)
	{
		if (bWeaponEvolved)
		{
			OutButtons.Add(CreateResonateRingButton(CharacterData)); // "Resonate (R)"
		}
		else
		{
			OutButtons.Add(CreateResonateButton(CharacterData)); // "Resonate"
		}
	}

	// Items always
	OutButtons.Add(CreateItemsButton(CharacterData));

	// Switch Ring (only if multiple rings in loadout)
	if (bHasMultipleRings)
	{
		OutButtons.Add(CreateChangeRingButton(CharacterData));
	}
}

// ==================== BUTTON CREATORS ====================

FPieMenuButtonData UCombatMenuSubsystem::CreateAttackButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Attack");
	Button.DisplayName = FText::FromString(TEXT("Attack"));
	Button.Category = EPieMenuCategory::Attack;

	// Get current weapon for description
	UWeaponData *CurrentWeapon = CharacterData->bUsePrimary ? CharacterData->PrimaryWeapon : CharacterData->SecondaryWeapon;

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

FPieMenuButtonData UCombatMenuSubsystem::CreateAbilitiesButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Abilities");
	Button.DisplayName = FText::FromString(TEXT("Abilities"));
	Button.Category = EPieMenuCategory::Abilities;

	TArray<UAbilityData *> Abilities = GetCurrentWeaponAbilities(CharacterData);
	int32 Count = Abilities.Num();

	Button.Description = FText::FromString(FString::Printf(TEXT("%d abilities"), Count));
	Button.bEnabled = Count > 0;

	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateRefractionsButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Refractions");
	Button.DisplayName = FText::FromString(TEXT("Refractions"));
	Button.Category = EPieMenuCategory::Refractions;

	TArray<USpellData *> Spells = GetAllSpells(CharacterData);
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

FPieMenuButtonData UCombatMenuSubsystem::CreateChangeRingButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("ChangeRing");
	Button.DisplayName = FText::FromString(TEXT("Change Ring"));
	Button.Category = EPieMenuCategory::ChangeRing;

	URingManager *RM = GetRingManager();
	TArray<URingData *> Rings = RM ? RM->GetEquippedRings(CurrentActor.Get()) : TArray<URingData *>();
	int32 Count = FMath::Min(Rings.Num(), MAX_RINGS);
	Button.Description = FText::FromString(FString::Printf(TEXT("%d rings"), Count));
	Button.bEnabled = Count > 1; // Need at least 2 rings to switch

	if (!Button.bEnabled)
	{
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	}

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateResonateButton(UCharacterData *CharacterData)
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
		URingManager *RM = GetRingManager();
		URingData *PriRing = RM ? RM->GetPrimaryRing(CurrentActor.Get()) : nullptr;
		if (PriRing)
		{
			// Source is ring
			bSourceIsRing = true;
			bHasBreakChance = !PriRing->IsEvolved();
			SpellCount = PriRing->GetAvailableSpells().Num();
			TintColor = GetElementColor(static_cast<int32>(PriRing->GetRingElement()));
		}
		else
		{
			UWeaponManager *WM = GetWeaponManager();
			UWeaponData *PriWeapon = WM ? WM->GetActiveWeapon(CurrentActor.Get()) : nullptr;
			if (PriWeapon && PriWeapon->IsEvolved())
			{
				// Source is evolved weapon
				bSourceIsEvolvedWeapon = true;
				bHasBreakChance = false;
				SpellCount = 0;
				TintColor = GetElementColor(static_cast<int32>(PriWeapon->GetWeaponElement()));
			}
		}
	}
	else if (CharacterData->IsGeneric())
	{
		// Generic: Source is always evolved weapon (ring uses CreateResonateRingButton in unified menu)
		UWeaponManager *WM = GetWeaponManager();
		UWeaponData *ActiveWeapon = WM ? WM->GetActiveWeapon(CurrentActor.Get()) : nullptr;

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

FPieMenuButtonData UCombatMenuSubsystem::CreateItemsButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("Items");
	Button.DisplayName = FText::FromString(TEXT("Items"));
	Button.Category = EPieMenuCategory::Items;
	Button.Description = FText::FromString(TEXT("Use items"));
	Button.bEnabled = true; // Always enabled, sub-menu shows actual availability

	return Button;
}

FPieMenuButtonData UCombatMenuSubsystem::CreateSwitchWeaponButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("SwitchWeapon");
	Button.DisplayName = FText::FromString(TEXT("Switch"));
	Button.Category = EPieMenuCategory::SwitchWeapon;

	// Switch only appears for Generic with dual weapons
	// Show what we'll switch TO (the other weapon)
	UWeaponData *OtherWeapon = CharacterData->bUsePrimary ? CharacterData->SecondaryWeapon : CharacterData->PrimaryWeapon;

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

FPieMenuButtonData UCombatMenuSubsystem::CreateBreakthroughButton(UCharacterData *CharacterData)
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

FPieMenuButtonData UCombatMenuSubsystem::CreateResonateWeaponButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("ResonateWeapon");
	Button.DisplayName = FText::FromString(TEXT("Resonate (W)"));
	Button.Category = EPieMenuCategory::ResonateWeapon; // Evolved weapon spells
	Button.Description = FText::FromString(TEXT("Evolved weapon spells (no break chance)"));

	UWeaponData *Weapon = CharacterData->PrimaryWeapon;
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

FPieMenuButtonData UCombatMenuSubsystem::CreateResonateRingButton(UCharacterData *CharacterData)
{
	FPieMenuButtonData Button;
	Button.ButtonID = TEXT("ResonateRing");
	Button.DisplayName = FText::FromString(TEXT("Resonate (R)"));
	Button.Category = EPieMenuCategory::ResonateRing;

	URingData *Ring = nullptr;
	URingManager *RM = GetRingManager();

	if (CharacterData->IsCaster() || CharacterData->IsGeneric())
	{
		// Caster/Generic: Get primary ring via RingManager
		Ring = RM ? RM->GetPrimaryRing(CurrentActor.Get()) : nullptr;
	}
	else if (CharacterData->IsResonator())
	{
		// Resonator: Get active ring from ring loadout
		Ring = RM ? RM->GetActiveRing(CurrentActor.Get()) : nullptr;
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
		Button.ButtonTint = GetElementColor(static_cast<int32>(Ring->GetRingElement()));
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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetSchoolsButtons(UCharacterData *CharacterData)
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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetSpellsForSchool(UCharacterData *CharacterData, EPieMenuSpellSchool School)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SelectedSchool = School;
	SetMenuState(EPieMenuState::SpellGrid);

	TArray<USpellData *> Spells = GetSpellsBySchool(CharacterData, School);

	// Cap at MAX_SPELLS_PER_SCHOOL
	int32 Count = FMath::Min(Spells.Num(), MAX_SPELLS_PER_SCHOOL);

	for (int32 i = 0; i < Count; ++i)
	{
		USpellData *Spell = Spells[i];
		if (!Spell)
			continue;

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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetAbilitiesButtons(UCharacterData *CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::AbilityGrid);

	TArray<UAbilityData *> Abilities = GetCurrentWeaponAbilities(CharacterData);

	for (int32 i = 0; i < Abilities.Num(); ++i)
	{
		UAbilityData *Ability = Abilities[i];
		if (!Ability)
			continue;

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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetRingsButtons(UCharacterData *CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData || !CharacterData->IsResonator())
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::RingGrid);

	// Cap at MAX_RINGS
	URingManager *RM = GetRingManager();
	TArray<URingData *> Rings = RM ? RM->GetEquippedRings(CurrentActor.Get()) : TArray<URingData *>();
	int32 Count = FMath::Min(Rings.Num(), MAX_RINGS);

	for (int32 i = 0; i < Count; ++i)
	{
		URingData *Ring = CharacterData->EquippedRings[i];
		if (!Ring)
			continue;

		FPieMenuButtonData Button = FPieMenuButtonData::MakeDataButton(
			FString::Printf(TEXT("Ring_%d"), i),
			FText::FromString(Ring->RingName),
			EPieMenuCategory::Ring,
			Ring,
			i,
			nullptr); // TODO: Add Icon property to RingData

		Button.Description = FText::FromString(Ring->Description);
		Button.ButtonTint = GetElementColor(static_cast<int32>(Ring->GetRingElement()));
		Button.bEnabled = true;

		// TODO: Mark currently active ring
		// if (i == ActiveRingIndex) { ... }

		Buttons.Add(Button);
	}

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d ring buttons"), Buttons.Num() - 1);
	return Buttons;
}

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetItemsButtons(UCharacterData *CharacterData)
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

TArray<FPieMenuButtonData> UCombatMenuSubsystem::GetInfusionSourceButtons(UCharacterData *CharacterData)
{
	TArray<FPieMenuButtonData> Buttons;

	if (!CharacterData)
	{
		return Buttons;
	}

	SetMenuState(EPieMenuState::InfusionGrid);

	// None (Physical) - always available
	{
		FPieMenuButtonData Button;
		Button.ButtonID = TEXT("Infusion_None");
		Button.DisplayName = FText::FromString(TEXT("Physical"));
		Button.Description = FText::FromString(TEXT("No infusion - weapon stats apply"));
		Button.Category = EPieMenuCategory::InfusionSource;
		Button.DataIndex = static_cast<int32>(EInfusionSourceOption::None);
		Button.bEnabled = true;
		Button.ButtonTint = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
		Buttons.Add(Button);
	}

	// Innate (Caster only)
	if (CharacterData->IsCaster())
	{
		FPieMenuButtonData Button;
		Button.ButtonID = TEXT("Infusion_Innate");
		Button.DisplayName = FText::FromString(TEXT("Innate"));
		Button.Description = FText::FromString(TEXT("Use innate element (costs HP)"));
		Button.Category = EPieMenuCategory::InfusionSource;
		Button.DataIndex = static_cast<int32>(EInfusionSourceOption::Innate);
		Button.bEnabled = true;
		Button.ButtonTint = GetElementColor(static_cast<int32>(CharacterData->InnateElement));
		Buttons.Add(Button);
	}

	// ActiveRing (Resonator only)
	if (CharacterData->IsResonator() && CharacterData->EquippedRings.Num() > 0)
	{
		URingData *ActiveRing = CharacterData->EquippedRings[0]; // TODO: Use actual active ring index
		if (ActiveRing)
		{
			FPieMenuButtonData Button;
			Button.ButtonID = TEXT("Infusion_ActiveRing");
			Button.DisplayName = FText::FromString(TEXT("Active Ring"));
			Button.Description = FText::FromString(FString::Printf(TEXT("Use %s element (ring may break)"), *ActiveRing->RingName));
			Button.Category = EPieMenuCategory::InfusionSource;
			Button.DataIndex = static_cast<int32>(EInfusionSourceOption::ActiveRing);
			Button.bEnabled = true;
			Button.ButtonTint = GetElementColor(static_cast<int32>(ActiveRing->GetRingElement()));
			Buttons.Add(Button);
		}
	}

	// PrimaryRing (Generic/Caster with ring in primary slot)
	URingManager *RM = GetRingManager();
	URingData *PriRing = RM ? RM->GetPrimaryRing(CurrentActor.Get()) : nullptr;
	if (PriRing)
	{
		FPieMenuButtonData Button;
		Button.ButtonID = TEXT("Infusion_PrimaryRing");
		Button.DisplayName = FText::FromString(TEXT("Primary Ring"));
		Button.Description = FText::FromString(FString::Printf(TEXT("Use %s element (ring may break)"), *PriRing->RingName));
		Button.Category = EPieMenuCategory::InfusionSource;
		Button.DataIndex = static_cast<int32>(EInfusionSourceOption::PrimaryRing);
		Button.bEnabled = true;
		Button.ButtonTint = GetElementColor(static_cast<int32>(PriRing->GetRingElement()));
		Buttons.Add(Button);
	}

	// WeaponCrystal (any class with non-Ilodite crystal)
	UWeaponData *ActiveWeapon = CharacterData->GetActiveWeapon();
	if (ActiveWeapon && ActiveWeapon->IsCrystalFunctional() && !ActiveWeapon->HasIloditeEquipped())
	{
		ESpellElement CrystalElement = ActiveWeapon->GetWeaponElement();
		if (CrystalElement != ESpellElement::Generic)
		{
			FPieMenuButtonData Button;
			Button.ButtonID = TEXT("Infusion_WeaponCrystal");
			Button.DisplayName = FText::FromString(TEXT("Weapon Crystal"));
			Button.Description = FText::FromString(TEXT("Use crystal element (crystal may break)"));
			Button.Category = EPieMenuCategory::InfusionSource;
			Button.DataIndex = static_cast<int32>(EInfusionSourceOption::WeaponCrystal);
			Button.bEnabled = true;
			Button.ButtonTint = GetElementColor(static_cast<int32>(CrystalElement));
			Buttons.Add(Button);
		}
	}

	// Evolution (any evolved character)
	if (CharacterData->IsEvolved())
	{
		FPieMenuButtonData Button;
		Button.ButtonID = TEXT("Infusion_Evolution");
		Button.DisplayName = FText::FromString(TEXT("Evolution"));
		Button.Description = FText::FromString(TEXT("Use evolved element (costs HP + status)"));
		Button.Category = EPieMenuCategory::InfusionSource;
		Button.DataIndex = static_cast<int32>(EInfusionSourceOption::Evolution);
		Button.bEnabled = true;
		Button.ButtonTint = GetElementColor(static_cast<int32>(CharacterData->GetSecondaryElement()));
		Buttons.Add(Button);
	}

	Buttons.Add(FPieMenuButtonData::MakeBackButton());

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Built %d infusion source buttons"), Buttons.Num() - 1);
	return Buttons;
}

// ==================== ACTIONS ====================

bool UCombatMenuSubsystem::ExecuteSwitchWeapon(UCharacterData *CharacterData)
{
	if (!CharacterData || !CanSwitchWeapon(CharacterData))
	{
		return false;
	}

	// Toggle via LoadoutComponent
	ULoadoutComponent *LoadoutComp = GetLoadoutComponent();
	if (LoadoutComp)
	{
		LoadoutComp->ToggleEquipment();
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Switched weapon. Now using %s"),
		   CharacterData->bUsePrimary ? TEXT("Primary") : TEXT("Secondary"));

	// Request menu refresh
	OnMenuRefreshRequested.Broadcast();

	return true;
}

bool UCombatMenuSubsystem::ExecuteRingSwitch(UCharacterData *CharacterData, int32 RingIndex)
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
	URingData *Ring = CharacterData->EquippedRings[RingIndex];
	UE_LOG(LogTemp, Log, TEXT("[CombatMenuSubsystem] Switched to ring: %s"),
		   Ring ? *Ring->RingName : TEXT("None"));

	// Request menu refresh (Resonate spells change)
	OnMenuRefreshRequested.Broadcast();

	return true;
}

// ==================== SELECTION HANDLING ====================

void UCombatMenuSubsystem::HandleButtonSelection(const FPieMenuButtonData &ButtonData)
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

		// Find the switch statement and add this case before default:
	case EPieMenuCategory::InfusionSource:
		// Store selected source and execute action
		OnExecuteAction.Broadcast(ButtonData);
		OnMenuCloseRequested.Broadcast();
		break;

	// === OPEN SUB-MENUS ===
	case EPieMenuCategory::Abilities:
	case EPieMenuCategory::Refractions:
	case EPieMenuCategory::Breakthrough:   // Character evolution spells -> Schools
	case EPieMenuCategory::Resonate:	   // Single source resonate -> Schools
	case EPieMenuCategory::ResonateWeapon: // Evolved weapon spells -> Schools
	case EPieMenuCategory::ResonateRing:   // Ring spells -> Schools
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
	case EPieMenuState::InfusionGrid:
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

ULoadoutComponent *UCombatMenuSubsystem::GetLoadoutComponent() const
{
	if (CurrentActor.IsValid())
	{
		return CurrentActor->FindComponentByClass<ULoadoutComponent>();
	}
	return nullptr;
}

TArray<USpellData *> UCombatMenuSubsystem::GetSpellsBySchool(UCharacterData *CharacterData, EPieMenuSpellSchool School) const
{
	TArray<USpellData *> Result;
	TArray<USpellData *> AllSpells = GetAllSpells(CharacterData);

	for (USpellData *Spell : AllSpells)
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

TArray<USpellData *> UCombatMenuSubsystem::GetAllSpells(UCharacterData *CharacterData) const
{
	ULoadoutComponent *Loadout = GetLoadoutComponent();
	if (Loadout && Loadout->IsReadyForBattle())
	{
		return Loadout->GetAvailableSpells();
	}

	return TArray<USpellData *>();
}

TArray<UAbilityData *> UCombatMenuSubsystem::GetCurrentWeaponAbilities(UCharacterData *CharacterData) const
{
	ULoadoutComponent *Loadout = GetLoadoutComponent();
	if (Loadout && Loadout->IsReadyForBattle())
	{
		return Loadout->GetAvailableAbilities();
	}

	return TArray<UAbilityData *>();
}

int32 UCombatMenuSubsystem::CountSpellsInSchool(UCharacterData *CharacterData, EPieMenuSpellSchool School) const
{
	TArray<USpellData *> AllSpells = GetAllSpells(CharacterData);
	int32 Count = 0;

	for (USpellData *Spell : AllSpells)
	{
		if (Spell && static_cast<int32>(Spell->School) == static_cast<int32>(School))
		{
			Count++;
		}
	}

	return FMath::Min(Count, MAX_SPELLS_PER_SCHOOL);
}

bool UCombatMenuSubsystem::CanSwitchWeapon(UCharacterData *CharacterData) const
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
		FLinearColor(1.0f, 0.3f, 0.1f, 1.0f), // 0: Fire
		FLinearColor(0.2f, 0.5f, 1.0f, 1.0f), // 1: Water
		FLinearColor(0.6f, 0.4f, 0.2f, 1.0f), // 2: Earth
		FLinearColor(0.7f, 1.0f, 0.7f, 1.0f), // 3: Wind
		FLinearColor(1.0f, 1.0f, 0.8f, 1.0f), // 4: Light
		FLinearColor(0.3f, 0.1f, 0.4f, 1.0f), // 5: Darkness
		FLinearColor(1.0f, 1.0f, 0.3f, 1.0f), // 6: Lightning
		FLinearColor(0.1f, 0.1f, 0.2f, 1.0f), // 7: Void
		FLinearColor(1.0f, 0.5f, 1.0f, 1.0f), // 8: Reality
		FLinearColor(0.7f, 0.7f, 0.7f, 1.0f), // 9+: Generic
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
	FString ClassStr = CurrentCharacter.IsValid() ? UEnum::GetValueAsString(CurrentCharacter->CharacterClass) : TEXT("N/A");
	ClassStr.RemoveFromStart(TEXT("ECharacterClass::"));

	UE_LOG(LogTemp, Log, TEXT("========== Combat Menu State =========="));
	UE_LOG(LogTemp, Log, TEXT("  State: %s"), *StateStr);
	UE_LOG(LogTemp, Log, TEXT("  Character: %s"), *CharName);
	UE_LOG(LogTemp, Log, TEXT("  Class: %s"), *ClassStr);
	UE_LOG(LogTemp, Log, TEXT("  Selected School: %d"), static_cast<int32>(SelectedSchool));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

UWeaponManager *UCombatMenuSubsystem::GetWeaponManager() const
{
	return GetGameInstance()->GetSubsystem<UWeaponManager>();
}

URingManager *UCombatMenuSubsystem::GetRingManager() const
{
	return GetGameInstance()->GetSubsystem<URingManager>();
}