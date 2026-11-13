// FlawlessParryRecorder.h
// B-431: test-only listener for UDefenseSystem::OnFlawlessParry (one broadcast per defender per attack). A dynamic
// delegate needs a UFUNCTION target, so specs bind this and read back who was reported.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Defense/DefenseSystem.h"
#include "FlawlessParryRecorder.generated.h"

UCLASS(Transient)
class UFlawlessParryRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<TWeakObjectPtr<AActor>> Defenders;

	void Listen(UDefenseSystem *Defense)
	{
		if (Defense)
		{
			Defense->OnFlawlessParry.AddDynamic(this, &UFlawlessParryRecorder::RecordFlawless);
		}
	}

	int32 CountFor(const AActor *Defender) const
	{
		return Defenders.FilterByPredicate([Defender](const TWeakObjectPtr<AActor> &Each) { return Each.Get() == Defender; }).Num();
	}

	UFUNCTION()
	void RecordFlawless(AActor *Defender, AActor *Attacker)
	{
		Defenders.Add(Defender);
	}
};
