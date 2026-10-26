// CombatResultRecorder.h
// Test-only listener for ACombatOrchestrator::OnCombatResultReady: a dynamic delegate needs a
// UFUNCTION target, so specs bind this and read back how the fight ended.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/CombatOrchestrator.h"
#include "CombatResultRecorder.generated.h"

UCLASS(Transient)
class UCombatResultRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<ECombatState> EndStates;

	void Listen(ACombatOrchestrator *Orchestrator)
	{
		Orchestrator->OnCombatResultReady.AddDynamic(this, &UCombatResultRecorder::Record);
	}

	UFUNCTION()
	void Record(const FCombatResult &Result) { EndStates.Add(Result.FinalState); }
};
