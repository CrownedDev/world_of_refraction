// DefenseOutcomeRecorder.h
// B-032d: test-only listener for UDefenseSystem::OnDefenseResolved, one broadcast per impact resolved per impact (a
// guard's at its resolve, an unguarded hit's once it lands). A dynamic delegate needs a UFUNCTION target, so specs
// bind this and read back the watched defender's impacts in order.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Defense/DefenseSystem.h"
#include "DefenseOutcomeRecorder.generated.h"

UCLASS(Transient)
class UDefenseOutcomeRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<int32> ImpactIndices;
	TArray<EDefenseType> DefenseTypes;

	void Listen(UDefenseSystem *Defense, AActor *Defender)
	{
		Watched = Defender;
		if (Defense)
		{
			Defense->OnDefenseResolved.AddDynamic(this, &UDefenseOutcomeRecorder::RecordResolved);
		}
	}

	UFUNCTION()
	void RecordResolved(AActor *Defender, AActor *Attacker, EDefenseType DefenseType, bool bPerfect, int32 ImpactIndex, float AttackEnergyCost)
	{
		if (Defender && Defender == Watched.Get())
		{
			ImpactIndices.Add(ImpactIndex);
			DefenseTypes.Add(DefenseType);
		}
	}

private:
	TWeakObjectPtr<AActor> Watched;
};
