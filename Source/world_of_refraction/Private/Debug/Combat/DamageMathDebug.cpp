// DamageMathDebug.cpp

#include "Debug/Combat/DamageMathDebug.h"
#include "Combat/Damage/DamageMath.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "WoRLog.h"

namespace
{
	constexpr float DAMAGE_MATH_DEBUG_SCREEN_SECONDS = 8.0f;
}

FString UDamageMathDebug::GetDamageMathString(const UObject *WorldContext, AActor *Attacker, AActor *Defender,
											  int32 BaseDamage, bool bIsSpell)
{
	const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	const UDamageCalculator *Calculator = GameInstance ? GameInstance->GetSubsystem<UDamageCalculator>() : nullptr;
	if (!Calculator)
	{
		return TEXT("ERROR: no DamageCalculator (needs a game world)");
	}

	FDamageCalculationInput Input;
	Input.BaseDamage = BaseDamage;
	Input.ActionType = bIsSpell ? EActionType::Spell : EActionType::Ability;
	Input.bCanCrit = false;
	const FDamageMathInputs Inputs = Calculator->GatherDamageMathInputs(Attacker, Defender, Input);
	const FDamageCalculationResult Result = WoRDamageMath::Compute(Inputs);

	return FString::Printf(TEXT("[DamageMath] %s -> %s\n%sResult: before defense %d, blocked %d, final %d (attacker x%.4f)\n"),
						   Attacker ? *Attacker->GetName() : TEXT("none"), Defender ? *Defender->GetName() : TEXT("none"),
						   *WoRDamageMath::GetInputsString(Inputs), Result.DamageBeforeDefense, Result.DamageBlockedByDefense,
						   Result.FinalDamage, Result.AttackerDamageMultiplier);
}

void UDamageMathDebug::PrintDamageMath(const UObject *WorldContext, AActor *Attacker, AActor *Defender, int32 BaseDamage,
									   bool bIsSpell)
{
	const FString Output = GetDamageMathString(WorldContext, Attacker, Defender, BaseDamage, bIsSpell);
	UE_LOG(LogWoRCombat, Log, TEXT("%s"), *Output);
	if (GEngine)
	{
		TArray<FString> Lines;
		Output.ParseIntoArrayLines(Lines);
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, DAMAGE_MATH_DEBUG_SCREEN_SECONDS, FColor::Cyan,
										 Lines.Num() > 0 ? Lines.Last() : Output);
	}
}
