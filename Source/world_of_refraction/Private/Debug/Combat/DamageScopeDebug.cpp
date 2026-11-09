// DamageScopeDebug.cpp

#include "Debug/Combat/DamageScopeDebug.h"

#include "Combat/Actions/EActionType.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/WoRFightSubsystem.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "WoRLog.h"

bool WoRDamageScopeDebug::IsScopedType(ESkillEffectType Type)
{
	return WoRDamageScope::IsScopedType(Type);
}

FString WoRDamageScopeDebug::GetScopesString(AActor *Actor)
{
	const USkillEffectManager *Effects = Actor ? WoRFight::Get<USkillEffectManager>(Actor) : nullptr;
	if (!Effects)
	{
		return FString::Printf(TEXT("%s: no effect manager"), *GetNameSafe(Actor));
	}
	TArray<FString> Lines;
	// B-385: the held effects (the enum store, or GAS at UseGAS 1), the same set the damage reads see.
	for (const FActiveSkillEffect &Effect : Effects->GetHeldEffects(Actor))
	{
		if (IsScopedType(Effect.EffectType))
		{
			Lines.Add(FString::Printf(TEXT("%s %+.0f%% [%s]"), *UEnum::GetDisplayValueAsText(Effect.EffectType).ToString(),
									  Effect.GetStackedValue(), *WoRDamageScope::ToString(Effect.DamageScope, Effect.Element)));
		}
	}
	return FString::Printf(TEXT("%s: %s"), *GetNameSafe(Actor),
						   Lines.Num() > 0 ? *FString::Join(Lines, TEXT("; ")) : TEXT("no damage / defence boosts"));
}

FString WoRDamageScopeDebug::GetAllDamageString(AActor *Actor, ESpellElement SampleElement)
{
	const UGameInstance *GI = Actor ? Actor->GetGameInstance() : nullptr;
	const UDamageCalculator *Calculator = GI ? GI->GetSubsystem<UDamageCalculator>() : nullptr; // a game-instance subsystem
	if (!Calculator)
	{
		return FString::Printf(TEXT("%s: no damage calculator"), *GetNameSafe(Actor));
	}
	return FString::Printf(TEXT("%s: All-damage %+.0f%% (%s spell), %+.0f%% (physical)"), *GetNameSafe(Actor),
						   Calculator->GetAllDamagePercent(Actor, EActionType::Spell, SampleElement),
						   *UEnum::GetDisplayValueAsText(SampleElement).ToString(),
						   Calculator->GetAllDamagePercent(Actor, EActionType::Ability, ESpellElement::None));
}

void WoRDamageScopeDebug::PrintScopes(UWorld *World)
{
	const USkillEffectManager *Effects = World ? WoRFight::Get<USkillEffectManager>(World) : nullptr;
	if (!Effects)
	{
		UE_LOG(LogWoR, Display, TEXT("[DamageScope] no fight running (no effect manager)"));
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (Effects->GetActiveEffects(*It).Num() > 0)
		{
			UE_LOG(LogWoR, Display, TEXT("[DamageScope] %s"), *GetScopesString(*It));
			UE_LOG(LogWoR, Display, TEXT("[DamageScope] %s"), *GetAllDamageString(*It));
		}
	}
}

static FAutoConsoleCommandWithWorld GDamagePrintScopesCommand(
	TEXT("wor.Damage.Scopes"),
	TEXT("B-383 / B-384: every damage / defence boost on each actor with its scope, and its one All-damage percent."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&WoRDamageScopeDebug::PrintScopes));
