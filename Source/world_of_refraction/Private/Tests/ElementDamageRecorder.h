// ElementDamageRecorder.h
// Test-only listener for UActionExecutor::OnElementDamageDealt (B-064): a dynamic delegate needs a
// UFUNCTION target, so specs bind this and read back each hit's damage and element.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Actions/ActionExecutor.h"
#include "ElementDamageRecorder.generated.h"

UCLASS(Transient)
class UElementDamageRecorder : public UObject
{
	GENERATED_BODY()

public:
	struct FEntry
	{
		int32 Damage = 0;
		ESpellElement Element = ESpellElement::None;
	};

	TArray<FEntry> Entries;

	void Listen(UActionExecutor *Executor)
	{
		Executor->OnElementDamageDealt.AddDynamic(this, &UElementDamageRecorder::Record);
	}

	UFUNCTION()
	void Record(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical, ESpellElement Element)
	{
		Entries.Add({Damage, Element});
	}
};
