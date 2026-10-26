// EncounterData.cpp

#include "Combat/Encounter/EncounterData.h"

#include "Character/CharacterData.h"
#include "WoRLog.h"

TArray<UCharacterData *> UEncounterData::GetEnemyRoster() const
{
	TArray<UCharacterData *> Roster;
	for (UCharacterData *Enemy : Enemies)
	{
		if (Enemy && Roster.Num() < EncounterConstants::ENCOUNTER_MAX_ENEMIES)
		{
			Roster.Add(Enemy);
		}
	}
	return Roster;
}

TArray<UCharacterData *> UEncounterData::BuildOpposingRoster(const UEncounterData *Encounter, UCharacterData *Fallback)
{
	TArray<UCharacterData *> Roster = Encounter ? Encounter->GetEnemyRoster() : TArray<UCharacterData *>();
	if (Roster.Num() == 0 && Fallback)
	{
		Roster.Add(Fallback);
	}
	return Roster;
}

FString UEncounterData::GetEncounterString() const
{
	const TArray<UCharacterData *> Roster = GetEnemyRoster();
	FString Output = FString::Printf(TEXT("Encounter %s: %d of %d enemies take the field\n"), *GetName(), Roster.Num(), Enemies.Num());
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		Output += FString::Printf(TEXT("  [%d] %s\n"), i, *Roster[i]->GetName());
	}
	return Output;
}

void UEncounterData::PrintEncounter() const
{
	UE_LOG(LogWoRCombat, Log, TEXT("%s"), *GetEncounterString());
}
