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
	TArray<int32> Unused;
	return BuildCaughtRoster(CaughtTeams, Unused);
}

TArray<UCharacterData *> UEncounterData::BuildCaughtRoster(const TArray<TArray<UCharacterData *>> &CaughtTeams, TArray<int32> &OutTeamIndices)
{
	OutTeamIndices.Reset();
	TArray<int32> Teams; // indices of the non-empty teams
	for (int32 Index = 0; Index < CaughtTeams.Num(); ++Index)
	{
		if (CaughtTeams[Index].Num() > 0)
		{
			Teams.Add(Index);
		}
	}
	TArray<UCharacterData *> Roster;
	if (Teams.Num() == 1)
	{
		Roster = CaughtTeams[Teams[0]];
		Roster.SetNum(FMath::Min(Roster.Num(), EncounterConstants::ENCOUNTER_MAX_ENEMIES));
		OutTeamIndices.Init(Teams[0], Roster.Num());
		return Roster;
	}

	for (const int32 TeamIndex : Teams)
	{
		if (Roster.Num() >= EncounterConstants::ENCOUNTER_MAX_ENEMIES)
		{
			break;
		}
		UCharacterData *Strongest = nullptr;
		for (UCharacterData *Member : CaughtTeams[TeamIndex])
		{
			if (Member && (!Strongest || Member->GetTotalPool() > Strongest->GetTotalPool()))
			{
				Strongest = Member;
			}
		}
		if (Strongest)
		{
			Roster.Add(Strongest);
			OutTeamIndices.Add(TeamIndex);
		}
	}
	return Roster;
}

EEncounterRank UEncounterData::GetMemberRank(const UEncounterData *Encounter, EEncounterRank TeamRank, const UCharacterData *Member)
{
	const bool bIsForm = Encounter && Encounter->MiniBossForm && Member && Encounter->MiniBossForm->Character == Member;
	return bIsForm ? HarderEncounterRank(TeamRank, EEncounterRank::MiniBoss) : TeamRank;
}

EEncounterRank UEncounterData::GetFightRank(const UEncounterData *Encounter, EEncounterRank PlacedRank, const TArray<UCharacterData *> &Fielded)
{
	const bool bFormFights = Encounter && Encounter->MiniBossForm && Fielded.Contains(Encounter->MiniBossForm->Character);
	return bFormFights ? HarderEncounterRank(PlacedRank, EEncounterRank::MiniBoss) : PlacedRank;
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
