// TeamOfThreeDebug.cpp
// QA (B-595d follow-up): wor.Debug.TeamOfThree gives every encounter on the current map that has no team of its own a
// team of three copies of its own enemy, so a skill that hits every enemy (WhirlWind) can be seen on three. Run it on
// the trial map before walking into a fight. For this PIE session only: the trial map reloads its authored encounters.

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/Encounter/EncounterData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	/** Encounters given a team of three; an encounter with its own team, or with no enemy on its pawn, is left alone. */
	int32 GiveTeamsOfThree(UWorld *World)
	{
		int32 Given = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			UEncounterComponent *Encounter = It->FindComponentByClass<UEncounterComponent>();
			const UCharacterDataComponent *Pawn = It->FindComponentByClass<UCharacterDataComponent>();
			if (!Encounter || Encounter->EncounterData || !Pawn || !Pawn->CharacterData)
			{
				continue;
			}
			UEncounterData *Team = NewObject<UEncounterData>(Encounter);
			Team->Enemies.Init(Pawn->CharacterData, EncounterConstants::ENCOUNTER_MAX_ENEMIES);
			Encounter->EncounterData = Team;
			++Given;
			UE_LOG(LogWoR, Display, TEXT("[TeamOfThree] %s now fights as three %s"), *It->GetName(), *Pawn->CharacterData->GetName());
		}
		return Given;
	}

	FAutoConsoleCommandWithWorld GTeamOfThreeCmd(
		TEXT("wor.Debug.TeamOfThree"),
		TEXT("QA: every encounter on this map with no team of its own fights as three copies of its enemy (this PIE session). ")
			TEXT("For seeing a skill that hits every enemy, e.g. WhirlWind."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			UE_LOG(LogWoR, Display, TEXT("[TeamOfThree] %d encounter(s) given a team of three"), World ? GiveTeamsOfThree(World) : 0);
		}));
}
