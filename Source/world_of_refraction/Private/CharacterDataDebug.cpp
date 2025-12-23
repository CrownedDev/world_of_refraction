// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterDataDebug.h"
#include "Engine/Engine.h"
#include "WeaponData.h"
#include "WeaponAttackData.h"
#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "LoadoutConstants.h"
#include "ItemData.h"

void UCharacterDataDebug::PrintCharacterStats(UCharacterData *Character, float Duration, FLinearColor TextColor)
{
	if (!Character)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: CharacterData is NULL"));
		}
		return;
	}

	FString StatsString = GetCharacterStatsString(Character);

	if (GEngine)
	{
		// Print each line separately for better readability
		TArray<FString> Lines;
		StatsString.ParseIntoArray(Lines, TEXT("\n"));

		for (int32 i = Lines.Num() - 1; i >= 0; --i) // Reverse order so it reads top to bottom
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, TextColor.ToFColor(true), Lines[i]);
		}
	}
}

void UCharacterDataDebug::LogCharacterStats(UCharacterData *Character)
{
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("ERROR: CharacterData is NULL"));
		return;
	}

	FString StatsString = GetCharacterStatsString(Character);
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

// FString UCharacterDataDebug::GetCharacterStatsString(UCharacterData *Character)
// {
// 	if (!Character)
// 	{
// 		return TEXT("ERROR: Invalid Character Data");
// 	}

// 	// Get class name as string
// 	FString ClassName = UEnum::GetValueAsString(Character->CharacterClass);
// 	ClassName.RemoveFromStart(TEXT("ECharacterClass::"));

// 	// Get element name as string (only relevant for Casters)
// 	FString ElementName = TEXT("N/A");
// 	if (Character->HasInnateElement())
// 	{
// 		ElementName = UEnum::GetValueAsString(Character->InnateElement);
// 		ElementName.RemoveFromStart(TEXT("ESpellElement::"));
// 	}

// 	FString Output = TEXT("");
// 	Output += TEXT("===================================\n");
// 	Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->CharacterName);
// 	Output += TEXT("===================================\n\n");

// 	// Identity
// 	Output += TEXT("IDENTITY:\n");
// 	Output += FString::Printf(TEXT("  Class: %s\n"), *ClassName);
// 	if (Character->HasInnateElement())
// 	{
// 		Output += FString::Printf(TEXT("  Element: %s\n"), *ElementName);
// 	}
// 	Output += FString::Printf(TEXT("  Description: %s\n\n"), *Character->Description);

// 	// Initial Budget Validation
// 	Output += TEXT("INITIAL DISTRIBUTION:\n");
// 	Output += FString::Printf(TEXT("  Budget: %d\n"), StatConstants::INITIAL_STAT_BUDGET);
// 	Output += FString::Printf(TEXT("  Used:   %d %s\n\n"),
// 							  Character->GetInitialSubStatSum(),
// 							  Character->IsValidInitialDistribution() ? TEXT("[OK]") : TEXT("[X] INVALID"));

// 	// World Progression Validation
// 	Output += TEXT("WORLD PROGRESSION:\n");
// 	Output += FString::Printf(TEXT("  Expected: %d points\n"), Character->GetExpectedWorldPoints());
// 	Output += FString::Printf(TEXT("  Used:     %d %s\n\n"),
// 							  Character->GetWorldSubStatSum(),
// 							  Character->IsValidWorldDistribution() ? TEXT("[OK]") : TEXT("[X] INVALID"));

// 	// Class-specific loadout
// 	Output += TEXT("LOADOUT:\n");

// 	switch (Character->CharacterClass)
// 	{
// 	case ECharacterClass::Generic:
// 		if (Character->IsEvolved())
// 		{
// 			Output += FString::Printf(TEXT("  Secondary Element: %s\n"),
// 									  *Character->PrimaryEvolution->GetCrystalName());
// 			Output += FString::Printf(TEXT("  Evolution: %s\n"),
// 									  *Character->PrimaryEvolution->GetFullItemName());

// 			// Combat spells from evolution
// 			TArray<USpellData *> CombatSpells = Character->GetCombatSpells();
// 			Output += FString::Printf(TEXT("  Combat Spells: %d/%d\n"),
// 									  CombatSpells.Num(), CrystalSpellConstants::MAX_SPELL_SLOTS);
// 			for (int32 i = 0; i < CombatSpells.Num(); ++i)
// 			{
// 				if (CombatSpells[i])
// 				{
// 					Output += FString::Printf(TEXT("    [%d] %s (%s)\n"),
// 											  i + 1, *CombatSpells[i]->SpellName, *CombatSpells[i]->GetElementName());
// 				}
// 			}

// 			// Primary weapon (still accessible)
// 			if (Character->PrimaryWeapon)
// 			{
// 				Output += FString::Printf(TEXT("  Primary: %s (%s)\n"),
// 										  *Character->PrimaryWeapon->WeaponName,
// 										  *Character->PrimaryWeapon->GetWeaponTypeName());
// 				PrintWeaponDetails(Output, Character->PrimaryWeapon);
// 			}
// 			else
// 			{
// 				Output += TEXT("  Primary: None\n");
// 			}

// 			Output += TEXT("  Secondary: DISABLED (Evolved)\n");
// 		}
// 		else
// 		{
// 			Output += FString::Printf(TEXT("  Starts With: %s\n"),
// 									  Character->bUsePrimary ? TEXT("Primary Weapon") : TEXT("Secondary Weapon"));

// 			// Primary weapon
// 			if (Character->PrimaryWeapon)
// 			{
// 				Output += FString::Printf(TEXT("  Primary: %s (%s)\n"),
// 										  *Character->PrimaryWeapon->WeaponName,
// 										  *Character->PrimaryWeapon->GetWeaponTypeName());
// 				PrintWeaponDetails(Output, Character->PrimaryWeapon);
// 			}
// 			else
// 			{
// 				Output += TEXT("  Primary: None\n");
// 			}

// 			// Secondary weapon
// 			if (Character->SecondaryWeapon)
// 			{
// 				Output += FString::Printf(TEXT("  Secondary: %s (%s)\n"),
// 										  *Character->SecondaryWeapon->WeaponName,
// 										  *Character->SecondaryWeapon->GetWeaponTypeName());
// 				PrintWeaponDetails(Output, Character->SecondaryWeapon);
// 			}
// 			else
// 			{
// 				Output += TEXT("  Secondary: None\n");
// 			}
// 		}
// 		break;

// 	case ECharacterClass::Caster:
// 		if (Character->IsEvolved())
// 		{
// 			Output += TEXT("  [EVOLVED - Dual Element Caster]\n");
// 			Output += FString::Printf(TEXT("  Primary Element: %s\n"), *ElementName);
// 			Output += FString::Printf(TEXT("  Secondary Element: %s\n"),
// 									  *Character->PrimaryEvolution->GetCrystalName());
// 			Output += FString::Printf(TEXT("  Evolution: %s\n"),
// 									  *Character->PrimaryEvolution->GetFullItemName());

// 			// Combat spells from evolution
// 			TArray<USpellData *> CombatSpells = Character->GetCombatSpells();
// 			Output += FString::Printf(TEXT("  Combat Spells: %d/%d\n"),
// 									  CombatSpells.Num(), CrystalSpellConstants::MAX_SPELL_SLOTS);
// 			for (int32 i = 0; i < CombatSpells.Num(); ++i)
// 			{
// 				if (CombatSpells[i])
// 				{
// 					Output += FString::Printf(TEXT("    [%d] %s (%s)\n"),
// 											  i + 1, *CombatSpells[i]->SpellName, *CombatSpells[i]->GetElementName());
// 				}
// 			}

// 			Output += TEXT("  Weapons: DISABLED (Evolved)\n");
// 		}
// 		else
// 		{
// 			Output += FString::Printf(TEXT("  Starts: %s\n"),
// 									  Character->bUsePrimary ? TEXT("Armed") : TEXT("Unarmed"));
// 			Output += FString::Printf(TEXT("  Innate Element: %s\n"), *ElementName);
// 			Output += FString::Printf(TEXT("  Innate Spells: %d\n"), Character->InnateSpells.Num());

// 			// List spells
// 			for (int32 i = 0; i < Character->InnateSpells.Num(); ++i)
// 			{
// 				USpellData *Spell = Character->InnateSpells[i];
// 				if (Spell)
// 				{
// 					Output += FString::Printf(TEXT("    [%d] %s\n"), i + 1, *Spell->SpellName);
// 				}
// 			}

// 			// Weapon
// 			if (Character->PrimaryWeapon)
// 			{
// 				Output += FString::Printf(TEXT("  Weapon: %s (%s)\n"),
// 										  *Character->PrimaryWeapon->WeaponName,
// 										  *Character->PrimaryWeapon->GetWeaponTypeName());
// 				PrintWeaponDetails(Output, Character->PrimaryWeapon);
// 			}
// 			else
// 			{
// 				Output += TEXT("  Weapon: None\n");
// 			}
// 		}
// 		break;

// 	case ECharacterClass::Resonator:
// 		Output += FString::Printf(TEXT("  Starts: %s\n"),
// 								  Character->bUsePrimary ? TEXT("Armed") : TEXT("Unarmed"));
// 		Output += FString::Printf(TEXT("  Equipped Rings: %d/6\n"), Character->EquippedRings.Num());

// 		// List rings
// 		for (int32 i = 0; i < Character->EquippedRings.Num(); ++i)
// 		{
// 			URingData *Ring = Character->EquippedRings[i];
// 			if (Ring)
// 			{
// 				FString RingElement = UEnum::GetValueAsString(Ring->GetRingElement());
// 				RingElement.RemoveFromStart(TEXT("ESpellElement::"));

// 				Output += FString::Printf(TEXT("    [%d] %s (%s) - %d spells\n"),
// 										  i + 1,
// 										  *Ring->RingName,
// 										  *RingElement,
// 										  Ring->GetAvailableSpells().Num());
// 			}
// 			else
// 			{
// 				Output += FString::Printf(TEXT("    [%d] Empty Slot\n"), i + 1);
// 			}
// 		}

// 		// Weapon (optional for Resonator)
// 		if (Character->PrimaryWeapon)
// 		{
// 			Output += FString::Printf(TEXT("  Weapon: %s (%s)\n"),
// 									  *Character->PrimaryWeapon->WeaponName,
// 									  *Character->PrimaryWeapon->GetWeaponTypeName());
// 		}
// 		else
// 		{
// 			Output += TEXT("  Weapon: None\n");
// 		}
// 		break;
// 	}

// 	Output += TEXT("\n");

// 	// Mind Stats (4): Efficiency, EffectDamage, CritChance, SpellSpeed
// 	Output += TEXT("MIND STATS:\n");
// 	Output += FString::Printf(TEXT("  Base Mind: %d (World Level: %d)\n"),
// 							  Character->GetBaseMind(), Character->WorldMindLevel);
// 	Output += FString::Printf(TEXT("  Effective Mind: %.1f\n"), Character->GetEffectiveMind());
// 	Output += FString::Printf(TEXT("  Efficiency: x%.2f (%.0f%% EP reduction)\n"),
// 							  Character->CalculateEfficiencyMultiplier(),
// 							  (1.0f - Character->CalculateEfficiencyMultiplier()) * 100.0f);
// 	Output += FString::Printf(TEXT("  Effect Damage: x%.2f\n"), Character->CalculateEffectDamageMultiplier());
// 	Output += FString::Printf(TEXT("  Crit Chance: %.1f%%\n"), Character->CalculateCritChance() * 100.0f);
// 	Output += FString::Printf(TEXT("  Spell Speed: x%.2f\n\n"), Character->CalculateSpellSpeed());

// 	// Body Stats (3): Defense, MovementSpeed, RawDamage
// 	Output += TEXT("BODY STATS:\n");
// 	Output += FString::Printf(TEXT("  Base Body: %d (World Level: %d)\n"),
// 							  Character->GetBaseBody(), Character->WorldBodyLevel);
// 	Output += FString::Printf(TEXT("  Effective Body: %.1f\n"), Character->GetEffectiveBody());
// 	Output += FString::Printf(TEXT("  Defense: %d\n"), Character->CalculateFlatDefense());
// 	Output += FString::Printf(TEXT("  Movement Speed: %.1f\n"), Character->CalculateMovementSpeed());
// 	Output += FString::Printf(TEXT("  Animation Speed: x%.2f\n"), Character->CalculateAnimationSpeed());
// 	Output += FString::Printf(TEXT("  Raw Damage: %d\n\n"), Character->CalculateRawDamage());

// 	// Spirit Stats (4): MaxEnergy, MaxHealth, Resistance, TurnSpeed
// 	Output += TEXT("SPIRIT STATS:\n");
// 	Output += FString::Printf(TEXT("  Base Spirit: %d (World Level: %d)\n"),
// 							  Character->GetBaseSpirit(), Character->WorldSpiritLevel);
// 	Output += FString::Printf(TEXT("  Effective Spirit: %.1f\n"), Character->GetEffectiveSpirit());
// 	Output += FString::Printf(TEXT("  Max HP: %d\n"), Character->CalculateMaxHealth());
// 	Output += FString::Printf(TEXT("  Max EP: %d\n"), Character->CalculateMaxEnergy());
// 	Output += FString::Printf(TEXT("  Status Resistance: %.1f%%\n"), Character->CalculateResistance() * 100.0f);
// 	Output += FString::Printf(TEXT("  Turn Speed: %.1f\n"), Character->CalculateTurnSpeed());

// 	return Output;
// }

// void UCharacterDataDebug::PrintWeaponDetails(FString &Output, UWeaponData *Weapon)
// {
// 	if (!Weapon)
// 		return;

// 	if (Weapon->WeaponAttack)
// 	{
// 		Output += FString::Printf(TEXT("    Attack: %s [%s] (Buildup: %d)\n"),
// 								  *Weapon->WeaponAttack->AttackName,
// 								  *Weapon->WeaponAttack->GetDamageTypeName(),
// 								  Weapon->WeaponAttack->StatusBuildup);
// 	}

// 	// Requirements
// 	if (Weapon->Requirements.HasRequirements())
// 	{
// 		Output += TEXT("    Requirements: ") + Weapon->Requirements.GetSimpleString() + TEXT("\n");
// 	}

// 	// Stat Bonuses
// 	TArray<FString> Bonuses;
// 	if (Weapon->BonusAttack != 0)
// 		Bonuses.Add(FString::Printf(TEXT("Atk %+d"), Weapon->BonusAttack));
// 	if (Weapon->BonusDefense != 0)
// 		Bonuses.Add(FString::Printf(TEXT("Def %+d"), Weapon->BonusDefense));
// 	if (Weapon->BonusMagicPower != 0)
// 		Bonuses.Add(FString::Printf(TEXT("Mag %+d"), Weapon->BonusMagicPower));
// 	if (Weapon->BonusSpeed != 0)
// 		Bonuses.Add(FString::Printf(TEXT("Spd %+d"), Weapon->BonusSpeed));
// 	if (Weapon->BonusCritChance != 0.0f)
// 		Bonuses.Add(FString::Printf(TEXT("Crit %+.1f%%"), Weapon->BonusCritChance));
// 	if (Weapon->BonusCritDamage != 0.0f)
// 		Bonuses.Add(FString::Printf(TEXT("CritDmg %+.1f%%"), Weapon->BonusCritDamage));
// 	if (Weapon->BonusMaxHP != 0)
// 		Bonuses.Add(FString::Printf(TEXT("HP %+d"), Weapon->BonusMaxHP));
// 	if (Weapon->BonusMaxMP != 0)
// 		Bonuses.Add(FString::Printf(TEXT("MP %+d"), Weapon->BonusMaxMP));

// 	if (Bonuses.Num() > 0)
// 	{
// 		Output += TEXT("    Bonuses: ") + FString::Join(Bonuses, TEXT(", ")) + TEXT("\n");
// 	}

// 	Output += FString::Printf(TEXT("    Abilities: %d/%d %s\n"),
// 							  Weapon->GetAbilityCount(),
// 							  LoadoutConstants::MAX_WEAPON_ABILITIES,
// 							  Weapon->bAbilitiesLocked ? TEXT("[Locked]") : TEXT("[Unlocked]"));
// }
