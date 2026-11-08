// DamageScopeDebug.cpp

#include "Debug/Combat/DamageScopeDebug.h"

#include "Combat/WoRFightSubsystem.h"
#include "EngineUtils.h"
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
	for (const FActiveSkillEffect &Effect : Effects->GetActiveEffects(Actor))
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
		}
	}
}

static FAutoConsoleCommandWithWorld GDamagePrintScopesCommand(
	TEXT("wor.Damage.Scopes"),
	TEXT("B-383: every damage / defence boost on each actor, with its scope (All, Physical, Spell or one element)."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&WoRDamageScopeDebug::PrintScopes));
