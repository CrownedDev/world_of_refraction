// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterDataDebug.h"
#include "Engine/Engine.h"
#include "WeaponData.h"
#include "WeaponAttackData.h"

#include "LoadoutConstants.h"

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

	// Initial Budget Validation
	Output += TEXT("INITIAL DISTRIBUTION:\n");
	Output += FString::Printf(TEXT("  Budget: %d\n"), Character->InitialStatBudget);
	Output += FString::Printf(TEXT("  Used:   %d %s\n\n"),
							  Character->GetInitialSubStatSum(),
							  Character->IsValidInitialDistribution() ? TEXT("[OK]") : TEXT("[X] INVALID"));

	// World Progression Validation
	Output += TEXT("WORLD PROGRESSION:\n");
	Output += FString::Printf(TEXT("  Expected: %d points\n"), Character->GetExpectedWorldPoints());
	Output += FString::Printf(TEXT("  Used:     %d %s\n\n"),
							  Character->GetWorldSubStatSum(),
							  Character->IsValidWorldDistribution() ? TEXT("[OK]") : TEXT("[X] INVALID"));

	// Weapon Loadout
	Output += TEXT("WEAPON LOADOUT:\n");
	if (Character->InnateElement == ERefractionElement::Generic)
	{
		Output += FString::Printf(TEXT("  Starts With: %s\n"), Character->bUsePrimary ? TEXT("Primary") : TEXT("Secondary"));
	}
	else
	{
		Output += FString::Printf(TEXT("  Starts: %s\n"), Character->bUsePrimary ? TEXT("Armed") : TEXT("Unarmed"));
	}

	if (Character->PrimaryWeapon)
	{
		Output += FString::Printf(TEXT("  Primary: %s (%s)\n"),
								  *Character->PrimaryWeapon->WeaponName,
								  *Character->PrimaryWeapon->GetWeaponTypeName());

		if (Character->PrimaryWeapon->WeaponAttack)
		{
			Output += FString::Printf(TEXT("    Attack: %s [%s] (Buildup: %d)\n"),
									  *Character->PrimaryWeapon->WeaponAttack->AttackName,
									  *Character->PrimaryWeapon->WeaponAttack->GetDamageTypeName(),
									  Character->PrimaryWeapon->WeaponAttack->StatusBuildup);
		}

		// Requirements
		if (Character->PrimaryWeapon->Requirements.HasRequirements())
		{
			Output += TEXT("    Requirements: ") + Character->PrimaryWeapon->Requirements.GetSimpleString() + TEXT("\n");
		}

		// Stat Bonuses
		TArray<FString> Bonuses;
		if (Character->PrimaryWeapon->BonusAttack != 0)
			Bonuses.Add(FString::Printf(TEXT("Atk %+d"), Character->PrimaryWeapon->BonusAttack));
		if (Character->PrimaryWeapon->BonusDefense != 0)
			Bonuses.Add(FString::Printf(TEXT("Def %+d"), Character->PrimaryWeapon->BonusDefense));
		if (Character->PrimaryWeapon->BonusMagicPower != 0)
			Bonuses.Add(FString::Printf(TEXT("Mag %+d"), Character->PrimaryWeapon->BonusMagicPower));
		if (Character->PrimaryWeapon->BonusSpeed != 0)
			Bonuses.Add(FString::Printf(TEXT("Spd %+d"), Character->PrimaryWeapon->BonusSpeed));
		if (Character->PrimaryWeapon->BonusCritChance != 0.0f)
			Bonuses.Add(FString::Printf(TEXT("Crit %+.1f%%"), Character->PrimaryWeapon->BonusCritChance));
		if (Character->PrimaryWeapon->BonusCritDamage != 0.0f)
			Bonuses.Add(FString::Printf(TEXT("CritDmg %+.1f%%"), Character->PrimaryWeapon->BonusCritDamage));
		if (Character->PrimaryWeapon->BonusMaxHP != 0)
			Bonuses.Add(FString::Printf(TEXT("HP %+d"), Character->PrimaryWeapon->BonusMaxHP));
		if (Character->PrimaryWeapon->BonusMaxMP != 0)
			Bonuses.Add(FString::Printf(TEXT("MP %+d"), Character->PrimaryWeapon->BonusMaxMP));

		if (Bonuses.Num() > 0)
		{
			Output += TEXT("    Bonuses: ") + FString::Join(Bonuses, TEXT(", ")) + TEXT("\n");
		}

		Output += FString::Printf(TEXT("    Abilities: %d/%d %s\n"),
								  Character->PrimaryWeapon->GetAbilityCount(),
								  LoadoutConstants::MAX_WEAPON_ABILITIES,
								  Character->PrimaryWeapon->bAbilitiesLocked ? TEXT("[LOCKED]") : TEXT(""));

		if (Character->PrimaryWeapon->bCanBeInfused)
		{
			Output += FString::Printf(TEXT("    Infusion: %.1fx multiplier\n"),
									  Character->PrimaryWeapon->InfusionStatusMultiplier);
		}
	}
	else
	{
		Output += TEXT("  Primary: None\n");
	}

	if (Character->InnateElement == ERefractionElement::Generic)
	{
		if (Character->SecondaryWeapon)
		{
			Output += FString::Printf(TEXT("  Secondary: %s (%s)\n"),
									  *Character->SecondaryWeapon->WeaponName,
									  *Character->SecondaryWeapon->GetWeaponTypeName());

			if (Character->SecondaryWeapon->WeaponAttack)
			{
				Output += FString::Printf(TEXT("    Attack: %s [%s] (Buildup: %d)\n"),
										  *Character->SecondaryWeapon->WeaponAttack->AttackName,
										  *Character->SecondaryWeapon->WeaponAttack->GetDamageTypeName(),
										  Character->SecondaryWeapon->WeaponAttack->StatusBuildup);
			}

			Output += FString::Printf(TEXT("    Abilities: %d/%d %s\n"),
									  Character->SecondaryWeapon->GetAbilityCount(),
									  LoadoutConstants::MAX_WEAPON_ABILITIES,
									  Character->SecondaryWeapon->bAbilitiesLocked ? TEXT("[LOCKED]") : TEXT(""));
		}
		else
		{
			Output += TEXT("  Secondary: None\n");
		}
	}

	Output += TEXT("\n");

	// Base Stats
	Output += TEXT("BASE STATS (Sum of Sub-Stats):\n");
	Output += FString::Printf(TEXT("  Mind:   %d\n"), Character->GetBaseMind());
	Output += FString::Printf(TEXT("  Body:   %d\n"), Character->GetBaseBody());
	Output += FString::Printf(TEXT("  Spirit: %d\n\n"), Character->GetBaseSpirit());

	// World Bonuses
	Output += TEXT("WORLD STAT LEVELS:\n");
	Output += FString::Printf(TEXT("  Mind:   Level %d (+%d pts, +%.0f%% scaling)\n"),
							  Character->WorldMindLevel,
							  Character->WorldMindLevel * 3,
							  Character->WorldMindLevel * 3.0f);
	Output += FString::Printf(TEXT("  Body:   Level %d (+%d pts, +%.0f%% scaling)\n"),
							  Character->WorldBodyLevel,
							  Character->WorldBodyLevel * 3,
							  Character->WorldBodyLevel * 3.0f);
	Output += FString::Printf(TEXT("  Spirit: Level %d (+%d pts, +%.0f%% scaling)\n\n"),
							  Character->WorldSpiritLevel,
							  Character->WorldSpiritLevel * 3,
							  Character->WorldSpiritLevel * 3.0f);

	// Effective Stats
	Output += TEXT("EFFECTIVE STATS (With Scaling):\n");
	Output += FString::Printf(TEXT("  Mind:   %.2f\n"), Character->GetEffectiveMind());
	Output += FString::Printf(TEXT("  Body:   %.2f\n"), Character->GetEffectiveBody());
	Output += FString::Printf(TEXT("  Spirit: %.2f\n\n"), Character->GetEffectiveSpirit());

	// Sub-Stat Breakdown with Split
	Output += TEXT("SUB-STAT BREAKDOWN:\n");
	Output += TEXT("  Mind:\n");
	Output += FString::Printf(TEXT("    Cost Reduction: %d (%d + %d)\n"),
							  Character->GetTotalCostReduction(),
							  Character->InitialCostReductionPoints,
							  Character->WorldCostEfficencyPoints);
	Output += FString::Printf(TEXT("    Turn Speed:     %d (%d + %d)\n"),
							  Character->GetTotalTurnSpeed(),
							  Character->InitialTurnSpeedPoints,
							  Character->WorldTurnSpeedPoints);
	Output += FString::Printf(TEXT("    Crit Chance:    %d (%d + %d)\n"),
							  Character->GetTotalCritChance(),
							  Character->InitialCritChancePoints,
							  Character->WorldCritChancePoints);

	Output += TEXT("  Body:\n");
	Output += FString::Printf(TEXT("    Defense:        %d (%d + %d)\n"),
							  Character->GetTotalDefense(),
							  Character->InitialDefensePoints,
							  Character->WorldDefensePoints);
	Output += FString::Printf(TEXT("    Attack Speed:   %d (%d + %d)\n"),
							  Character->GetTotalAttackSpeed(),
							  Character->InitialAttackSpeedPoints,
							  Character->WorldAttackSpeedPoints);
	Output += FString::Printf(TEXT("    Raw Damage:     %d (%d + %d)\n"),
							  Character->GetTotalRawDamage(),
							  Character->InitialRawDamagePoints,
							  Character->WorldRawDamagePoints);

	Output += TEXT("  Spirit:\n");
	Output += FString::Printf(TEXT("    Effect Damage:  %d (%d + %d)\n"),
							  Character->GetTotalEffectDamage(),
							  Character->InitialEffectDamagePoints,
							  Character->WorldEffectDamagePoints);
	Output += FString::Printf(TEXT("    Resistance:     %d (%d + %d)\n"),
							  Character->GetTotalResistance(),
							  Character->InitialResistancePoints,
							  Character->WorldResistancePoints);
	Output += FString::Printf(TEXT("    Ability Size:   %d (%d + %d)\n\n"),
							  Character->GetTotalAbilitySize(),
							  Character->InitialAbilitySizePoints,
							  Character->WorldAbilitySizePoints);

	// Combat Calculations
	Output += TEXT("COMBAT STATS:\n");
	Output += FString::Printf(TEXT("  Cost Reduction: %.1f%%\n"),
							  Character->CalculateSpellCostReduction() * 100.0f);
	Output += FString::Printf(TEXT("  Turn Speed:     %.2f\n"),
							  Character->CalculateTurnSpeed());
	Output += FString::Printf(TEXT("  Crit Chance:    %.1f%%\n"),
							  Character->CalculateCriticalChance() * 100.0f);
	Output += FString::Printf(TEXT("  Flat Defense:   %d\n"),
							  Character->CalculateFlatDefense());
	Output += FString::Printf(TEXT("  Attack Speed:   %.2fx\n"),
							  Character->CalculateAttackSpeed());
	Output += FString::Printf(TEXT("  Raw Damage:     %.2fx\n"),
							  Character->CalculateRawDamageMultiplier());
	Output += FString::Printf(TEXT("  Effect Damage:  %.2fx\n"),
							  Character->CalculateEffectDamageMultiplier());
	Output += FString::Printf(TEXT("  Resistance:     %.1f%%\n"),
							  Character->CalculateElementalResistance() * 100.0f);
	Output += FString::Printf(TEXT("  Ability Size:   %.2fx\n\n"),
							  Character->CalculateAbilitySizeMultiplier());

	Output += TEXT("===================================\n");

	return Output;
}