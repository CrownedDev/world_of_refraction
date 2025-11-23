// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterDataDebug.h"
#include "Engine/Engine.h"

void UCharacterDataDebug::PrintCharacterStats(UCharacterData* Character, float Duration, FLinearColor TextColor)
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

		for (int32 i = Lines.Num() - 1; i >= 0; --i)  // Reverse order so it reads top to bottom
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, TextColor.ToFColor(true), Lines[i]);
		}
	}
}

void UCharacterDataDebug::LogCharacterStats(UCharacterData* Character)
{
	if (!Character)
	{
		UE_LOG(LogTemp, Error, TEXT("ERROR: CharacterData is NULL"));
		return;
	}

	FString StatsString = GetCharacterStatsString(Character);
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

FString UCharacterDataDebug::GetCharacterStatsString(UCharacterData* Character)
{
	if (!Character)
	{
		return TEXT("ERROR: Invalid Character Data");
	}

	// Get element name as string
	FString ElementName = UEnum::GetValueAsString(Character->InnateElement);
	ElementName.RemoveFromStart(TEXT("ERefractionElement::"));

	FString Output = TEXT("");
	Output += TEXT("===================================\n");
	Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->CharacterName);
	Output += TEXT("===================================\n\n");

	// Identity
	Output += TEXT("IDENTITY:\n");
	Output += FString::Printf(TEXT("  Element: %s\n"), *ElementName);
	Output += FString::Printf(TEXT("  Description: %s\n\n"), *Character->Description);

	// Base Stats
	Output += TEXT("BASE STATS (Distributed):\n");
	Output += FString::Printf(TEXT("  Mind:   %d\n"), Character->DistributedMind);
	Output += FString::Printf(TEXT("  Body:   %d\n"), Character->DistributedBody);
	Output += FString::Printf(TEXT("  Spirit: %d\n"), Character->DistributedSpirit);
	Output += FString::Printf(TEXT("  Total:  %d / %d %s\n\n"),
		Character->GetTotalDistributedPoints(),
		Character->DistributablePoints,
		Character->IsValidDistribution() ? TEXT("?") : TEXT("? INVALID"));

	// World Bonuses
	Output += TEXT("WORLD STAT BONUSES:\n");
	Output += FString::Printf(TEXT("  Mind Level:   %d (+%.0f%%)\n"),
		Character->WorldMindLevel,
		Character->WorldMindLevel * 5.0f);
	Output += FString::Printf(TEXT("  Body Level:   %d (+%.0f%%)\n"),
		Character->WorldBodyLevel,
		Character->WorldBodyLevel * 5.0f);
	Output += FString::Printf(TEXT("  Spirit Level: %d (+%.0f%%)\n\n"),
		Character->WorldSpiritLevel,
		Character->WorldSpiritLevel * 5.0f);

	// Effective Stats
	Output += TEXT("EFFECTIVE STATS:\n");
	Output += FString::Printf(TEXT("  Mind:   %.2f\n"), Character->GetEffectiveMind());
	Output += FString::Printf(TEXT("  Body:   %.2f\n"), Character->GetEffectiveBody());
	Output += FString::Printf(TEXT("  Spirit: %.2f\n\n"), Character->GetEffectiveSpirit());

	// Mind Calculations
	Output += TEXT("MIND CALCULATIONS:\n");
	Output += FString::Printf(TEXT("  Cost Reduction: %.1f%%\n"),
		Character->CalculateSpellCostReduction() * 100.0f);
	Output += FString::Printf(TEXT("  Turn Speed:     %.2f\n"),
		Character->CalculateTurnSpeed());
	Output += FString::Printf(TEXT("  Crit Chance:    %.1f%%\n\n"),
		Character->CalculateCriticalChance() * 100.0f);

	// Body Calculations
	Output += TEXT("BODY CALCULATIONS:\n");
	Output += FString::Printf(TEXT("  Flat Defense:   %d\n"),
		Character->CalculateFlatDefense());
	Output += FString::Printf(TEXT("  Attack Speed:   %.2fx\n"),
		Character->CalculateAttackSpeed());
	Output += FString::Printf(TEXT("  Raw Damage:     %.2fx\n\n"),
		Character->CalculateRawDamageMultiplier());

	// Spirit Calculations
	Output += TEXT("SPIRIT CALCULATIONS:\n");
	Output += FString::Printf(TEXT("  Effect Damage:  %.2fx\n"),
		Character->CalculateEffectDamageMultiplier());
	Output += FString::Printf(TEXT("  Resistance:     %.1f%%\n"),
		Character->CalculateElementalResistance() * 100.0f);
	Output += FString::Printf(TEXT("  Ability Size:   %.2fx\n\n"),
		Character->CalculateAbilitySizeMultiplier());

	// Sub-Stat Distribution
	Output += TEXT("SUB-STAT DISTRIBUTION:\n");
	Output += FString::Printf(TEXT("  Mind:   %d / %d %s (Cost:%d, Speed:%d, Crit:%d)\n"),
		Character->GetUsedMindPoints(),
		Character->GetAvailableMindPoints(),
		Character->IsValidMindDistribution() ? TEXT("?") : TEXT("?"),
		Character->CostReductionPoints,
		Character->TurnSpeedPoints,
		Character->CritChancePoints);
	Output += FString::Printf(TEXT("  Body:   %d / %d %s (Def:%d, AtkSpd:%d, RawDmg:%d)\n"),
		Character->GetUsedBodyPoints(),
		Character->GetAvailableBodyPoints(),
		Character->IsValidBodyDistribution() ? TEXT("?") : TEXT("?"),
		Character->DefensePoints,
		Character->AttackSpeedPoints,
		Character->RawDamagePoints);
	Output += FString::Printf(TEXT("  Spirit: %d / %d %s (EffDmg:%d, Res:%d, Size:%d)\n\n"),
		Character->GetUsedSpiritPoints(),
		Character->GetAvailableSpiritPoints(),
		Character->IsValidSpiritDistribution() ? TEXT("?") : TEXT("?"),
		Character->EffectDamagePoints,
		Character->ResistancePoints,
		Character->AbilitySizePoints);

	// Validation Summary
	Output += TEXT("VALIDATION:\n");
	Output += FString::Printf(TEXT("  Base Stats: %s\n"),
		Character->IsValidDistribution() ? TEXT("[OK] VALID") : TEXT("[X] INVALID"));
	Output += FString::Printf(TEXT("  Mind:       %s\n"),
		Character->IsValidMindDistribution() ? TEXT("[OK] VALID") : TEXT("[X] INVALID"));
	Output += FString::Printf(TEXT("  Body:       %s\n"),
		Character->IsValidBodyDistribution() ? TEXT("[OK] VALID") : TEXT("[X] INVALID"));
	Output += FString::Printf(TEXT("  Spirit:     %s\n"),
		Character->IsValidSpiritDistribution() ? TEXT("[OK] VALID") : TEXT("[X] INVALID"));

	Output += TEXT("===================================\n");

	return Output;
}