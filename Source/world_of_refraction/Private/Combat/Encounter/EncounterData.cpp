// EncounterData.cpp

#include "Combat/Encounter/EncounterData.h"

#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "WoRLog.h"

TArray<UCharacterData *> UEncounterData::GetEnemyRoster() const
{
	TArray<UCharacterData *> Roster;
	if (MiniBossForm && MiniBossForm->Character)
	{
		Roster.Add(MiniBossForm->Character);
	}
	for (UCharacterData *Enemy : Enemies)
	{
		// The form's character takes the field once, even when it is also listed.
		if (Enemy && !Roster.Contains(Enemy) && Roster.Num() < EncounterConstants::ENCOUNTER_MAX_ENEMIES)
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

TArray<UCharacterData *> UEncounterData::BuildCaughtRoster(const TArray<TArray<UCharacterData *>> &CaughtTeams)
{
	TArray<const TArray<UCharacterData *> *> Teams;
	for (const TArray<UCharacterData *> &Team : CaughtTeams)
	{
		if (Team.Num() > 0)
		{
			Teams.Add(&Team);
		}
	}
	if (Teams.Num() == 1)
	{
		TArray<UCharacterData *> Roster = *Teams[0];
		Roster.SetNum(FMath::Min(Roster.Num(), EncounterConstants::ENCOUNTER_MAX_ENEMIES));
		return Roster;
	}

	TArray<UCharacterData *> Roster;
	for (const TArray<UCharacterData *> *Team : Teams)
	{
		if (Roster.Num() >= EncounterConstants::ENCOUNTER_MAX_ENEMIES)
		{
			break;
		}
		UCharacterData *Strongest = nullptr;
		for (UCharacterData *Member : *Team)
		{
			if (Member && (!Strongest || Member->GetTotalPool() > Strongest->GetTotalPool()))
			{
				Strongest = Member;
			}
		}
		if (Strongest)
		{
			Roster.Add(Strongest);
		}
	}
	return Roster;
}

EEncounterRank UEncounterData::GetFightRank(const UEncounterData *Encounter, EEncounterRank PlacedRank, const TArray<UCharacterData *> &Fielded)
{
	const bool bFormFights = Encounter && Encounter->MiniBossForm && Fielded.Contains(Encounter->MiniBossForm->Character);
	return bFormFights ? FMath::Max(PlacedRank, EEncounterRank::MiniBoss) : PlacedRank;
}

FString UEncounterData::GetEncounterString() const
{
	const TArray<UCharacterData *> Roster = GetEnemyRoster();
	FString Output = FString::Printf(TEXT("Encounter %s: %d of %d enemies take the field%s\n"), *GetName(), Roster.Num(), Enemies.Num() + (MiniBossForm ? 1 : 0),
									 MiniBossForm ? *FString::Printf(TEXT(", led by form %s as a mini-boss"), *MiniBossForm->GetName()) : TEXT(""));
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
