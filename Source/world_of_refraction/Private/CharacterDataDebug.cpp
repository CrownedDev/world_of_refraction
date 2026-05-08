// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterDataDebug.h"
#include "Engine/Engine.h"
#include "StatConstants.h"
#include "LoadoutData.h"

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

FString UCharacterDataDebug::GetCharacterStatsString(UCharacterData *Character)
{
	if (!Character)
	{
		return TEXT("ERROR: Invalid Character Data");
	}

	// Get class name as string
	FString ClassName = UEnum::GetValueAsString(Character->CharacterClass);
	ClassName.RemoveFromStart(TEXT("ECharacterClass::"));

	// Get element name as string (only relevant for Casters)
	FString ElementName = TEXT("N/A");
	if (Character->IsCaster())
	{
		ElementName = UEnum::GetValueAsString(Character->InnateElement);
		ElementName.RemoveFromStart(TEXT("ESpellElement::"));
	}

	FString Output = TEXT("");
	Output += TEXT("===================================\n");
	Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->CharacterName);
	Output += TEXT("===================================\n\n");

	// ==================== IDENTITY ====================
	Output += TEXT("IDENTITY:\n");
	Output += FString::Printf(TEXT("  Class: %s\n"), *ClassName);
	if (Character->IsCaster())
	{
		Output += FString::Printf(TEXT("  Innate Element: %s\n"), *ElementName);
	}
	Output += FString::Printf(TEXT("  AI Controlled: %s\n"), Character->bIsAIControlled ? TEXT("Yes") : TEXT("No"));
	Output += FString::Printf(TEXT("  Default Loadout: %s\n"),
							  Character->DefaultLoadout ? *Character->DefaultLoadout->GetName() : TEXT("None"));
	Output += TEXT("\n");

	// ==================== WORLD PROGRESSION ====================
	Output += TEXT("WORLD PROGRESSION:\n");
	Output += FString::Printf(TEXT("  Mind Level:   %d\n"), Character->WorldMindLevel);
	Output += FString::Printf(TEXT("  Body Level:   %d\n"), Character->WorldBodyLevel);
	Output += FString::Printf(TEXT("  Spirit Level: %d\n"), Character->WorldSpiritLevel);
	Output += TEXT("\n");

	// ==================== INITIAL STAT DISTRIBUTION ====================
	// Sub-Stats Pool
	Output += FString::Printf(TEXT("SUB-STATS (Pool: %d, Spent: %d, Remaining: %d) %s\n"),
							  Character->GetTotalPool(),
							  Character->GetTotalSpent(),
							  Character->GetPointsRemaining(),
							  Character->IsValidDistribution() ? TEXT("[OK]") : TEXT("[INVALID]"));

	Output += TEXT("  Mind:\n");
	Output += FString::Printf(TEXT("    Efficiency:     %d\n"), Character->Efficiency);
	Output += FString::Printf(TEXT("    Effect Damage:  %d\n"), Character->EffectDamage);
	Output += FString::Printf(TEXT("    Crit Chance:    %d\n"), Character->CritChance);
	Output += FString::Printf(TEXT("    Spell Speed:    %d\n"), Character->SpellSpeed);

	Output += TEXT("  Body:\n");
	Output += FString::Printf(TEXT("    Defense:        %d\n"), Character->Defense);
	Output += FString::Printf(TEXT("    Action Speed: %d\n"), Character->ActionSpeed);
	Output += FString::Printf(TEXT("    Raw Damage:     %d\n"), Character->RawDamage);
	Output += FString::Printf(TEXT("    Max Health:     %d\n"), Character->MaxHealth);

	Output += TEXT("  Spirit:\n");
	Output += FString::Printf(TEXT("    Max Energy:     %d\n"), Character->MaxEnergy);
	Output += FString::Printf(TEXT("    Resistance:     %d\n"), Character->Resistance);
	Output += FString::Printf(TEXT("    Turn Speed:     %d\n"), Character->TurnSpeed);
	Output += FString::Printf(TEXT("    Luck:           %d\n"), Character->Luck);

	// ==================== CALCULATED COMBAT STATS ====================
	Output += TEXT("CALCULATED STATS:\n");
	Output += FString::Printf(TEXT("  Max Health:    %d\n"), Character->CalculateMaxHealth());
	Output += FString::Printf(TEXT("  Max Energy:    %d\n"), Character->CalculateMaxEnergy());
	Output += FString::Printf(TEXT("  Raw Damage:    %.2fx\n"), Character->CalculateRawDamage());
	Output += FString::Printf(TEXT("  Effect Damage: %d\n"), Character->CalculateEffectDamage());
	Output += FString::Printf(TEXT("  Resistance:    %.1f%%\n"), Character->CalculateResistance() * 100.0f);
	Output += FString::Printf(TEXT("  Turn Speed:    %.1f\n"), Character->CalculateTurnSpeed());
	Output += TEXT("\n");

	// ==================== LOADOUT REMINDER ====================
	Output += TEXT("-----------------------------------\n");
	Output += TEXT("Equipment info: Use LoadoutComponent\n");
	Output += TEXT("===================================\n");

	return Output;
}