// ActionExecutedRecorder.h
// Test-only listener for ACombatOrchestrator::OnActionExecuted: a dynamic delegate needs a
// UFUNCTION target, so specs bind this and read back who did what, and whether it worked.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/CombatOrchestrator.h"
#include "ActionExecutedRecorder.generated.h"

UCLASS(Transient)
class UActionExecutedRecorder : public UObject
{
	GENERATED_BODY()

public:
	struct FEntry
	{
		TWeakObjectPtr<AActor> Actor;
		EActionType ActionType = EActionType::None;
		bool bSuccess = false;
	};

	TArray<FEntry> Entries;

	void Listen(ACombatOrchestrator *Orchestrator)
	{
		Orchestrator->OnActionExecuted.AddDynamic(this, &UActionExecutedRecorder::Record);
	}

	bool Saw(const AActor *Actor, EActionType ActionType, bool bSuccess) const
	{
		return Entries.ContainsByPredicate([&](const FEntry &Entry)
		{
			return Entry.Actor.Get() == Actor && Entry.ActionType == ActionType && Entry.bSuccess == bSuccess;
		});
	}

	UFUNCTION()
	void Record(AActor *Actor, const FActionResult &Result) { Entries.Add({Actor, Result.ActionType, Result.bSuccess}); }
};
