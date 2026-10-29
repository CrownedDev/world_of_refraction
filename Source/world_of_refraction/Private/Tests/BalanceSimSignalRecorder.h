// BalanceSimSignalRecorder.h
// Test-only listener for the B-291 headless seams: impact frames (UActionExecutor::OnImpactFrame), new rounds
// (UTurnManager::OnRoundAdded) and status breaks (UStatusBuildupManager::OnStatusTriggered). Dynamic delegates need a
// UFUNCTION target, so specs bind this. The balance simulator's recorder (B-292) grows from it.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "BalanceSimSignalRecorder.generated.h"

UCLASS(Transient)
class UBalanceSimSignalRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<int32> ImpactIndices;
	int32 Rounds = 0;
	int32 StatusBreaks = 0;

	void Listen(UActionExecutor *Executor, UTurnManager *Turns, UStatusBuildupManager *Buildup)
	{
		if (Executor)
		{
			Executor->OnImpactFrame.AddDynamic(this, &UBalanceSimSignalRecorder::RecordImpact);
		}
		if (Turns)
		{
			Turns->OnRoundAdded.AddDynamic(this, &UBalanceSimSignalRecorder::RecordRound);
		}
		if (Buildup)
		{
			Buildup->OnStatusTriggered.AddDynamic(this, &UBalanceSimSignalRecorder::RecordStatusBreak);
		}
	}

	UFUNCTION()
	void RecordImpact(AActor *Executor, int32 ImpactIndex) { ImpactIndices.Add(ImpactIndex); }

	UFUNCTION()
	void RecordRound() { ++Rounds; }

	UFUNCTION()
	void RecordStatusBreak(AActor *Target, AActor *Source, ESkillEffectType StatusType, ESpellElement Element) { ++StatusBreaks; }
};
