#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "SkillEffectBlueprintLibrary.generated.h"

UCLASS()
class WORLD_OF_REFRACTION_API USkillEffectBlueprintLibrary
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Skill Effect")
	static FString GetEffectDurationString(
		const FActiveSkillEffect &Effect)
	{
		return Effect.GetDurationString();
	}

	UFUNCTION(BlueprintPure, Category = "Skill Effect")
	static FString GetEffectStackString(
		const FActiveSkillEffect &Effect)
	{
		return Effect.GetStackString();
	}

	UFUNCTION(BlueprintPure, Category = "Skill Effect")
	static bool IsEffectBuff(const FActiveSkillEffect &Effect)
	{
		return Effect.IsBuff();
	}

	UFUNCTION(BlueprintPure, Category = "Skill Effect")
	static bool IsEffectDebuff(const FActiveSkillEffect &Effect)
	{
		return Effect.IsDebuff();
	}

	UFUNCTION(BlueprintPure, Category = "Skill Effect")
	static FString GetEffectDisplayName(
		const FActiveSkillEffect &Effect)
	{
		return Effect.EffectName;
	}
};
