// StatModifierDebug.cpp

#include "Debug/Character/StatModifierDebug.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float STAT_DEBUG_SCREEN_SECONDS = 8.0f;

	/** Every sub-stat with a table row, in enum order (None excluded). */
	const ESubStat STAT_DEBUG_SUB_STATS[] = {
		ESubStat::Efficiency, ESubStat::SpellDamage, ESubStat::StatusMultiplier, ESubStat::CritDamage,
		ESubStat::SpellSpeed, ESubStat::Defense, ESubStat::ActionSpeed, ESubStat::RawDamage,
		ESubStat::Resistance, ESubStat::TurnSpeed, ESubStat::Luck, ESubStat::Reflex};

	/** Last snapshot per actor name, for CompareStats. Debug-only; lives for the editor session. */
	TMap<FString, FString> GStatDebugLastSnapshots;

	FAutoConsoleCommandWithWorld GCompareStatsCmd(
		TEXT("wor.CompareStats"),
		TEXT("DEBUG: snapshot every combatant's composed stats and list what changed since the last call."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UStatModifierDebug::CompareStats(World); }));

	FAutoConsoleCommandWithWorld GPrintStatModifiersCmd(
		TEXT("wor.PrintStatModifiers"),
		TEXT("DEBUG: print each combatant's net buff/debuff per sub-stat."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UStatModifierDebug::PrintStatModifiers(World); }));

	template <typename TSubsystem>
	TSubsystem *StatDebugSubsystem(const AActor *Actor)
	{
		const UWorld *World = Actor ? Actor->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	FString StatDebugName(ESubStat Stat)
	{
		return StaticEnum<ESubStat>()->GetDisplayNameTextByValue(static_cast<int64>(Stat)).ToString();
	}

	TArray<AActor *> StatDebugCombatants(const UObject *WorldContextObject)
	{
		TArray<AActor *> Result;
		UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		if (!World)
		{
			return Result;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->FindComponentByClass<UCharacterDataComponent>())
			{
				Result.Add(*It);
			}
		}
		return Result;
	}
}

FString UStatModifierDebug::GetStatModifierString(AActor *Actor)
{
	const USkillEffectManager *SEM = StatDebugSubsystem<USkillEffectManager>(Actor);
	if (!SEM)
	{
		return TEXT("no skill effect manager");
	}
	TArray<FString> Parts;
	for (const ESubStat Stat : STAT_DEBUG_SUB_STATS)
	{
		const float Net = SEM->GetNetStatModifier(Actor, Stat);
		if (!FMath::IsNearlyZero(Net))
		{
			Parts.Add(FString::Printf(TEXT("%s %+.1f%%"), *StatDebugName(Stat), Net));
		}
	}
	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("no active stat modifiers");
}

FString UStatModifierDebug::GetStatsSnapshotString(AActor *Actor)
{
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char)
	{
		return TEXT("no CharacterDataComponent");
	}
	TArray<FString> Lines;
	auto Add = [&Lines](const TCHAR *Name, float Value) { Lines.Add(FString::Printf(TEXT("%s=%.5f"), Name, Value)); };
	Add(TEXT("ActionSpeed"), Char->GetEffectiveActionSpeed());
	Add(TEXT("SpellSpeed"), Char->GetEffectiveSpellSpeed());
	Add(TEXT("SpellDamage"), Char->GetEffectiveSpellDamage());
	Add(TEXT("RawDamage"), Char->GetEffectiveRawDamage());
	Add(TEXT("Luck"), Char->GetEquipmentModifiedLuck());
	Add(TEXT("EfficiencyMultiplier"), Char->GetEffectiveEfficiencyMultiplier());
	Add(TEXT("StatusMultiplier"), Char->GetEffectiveStatusMultiplier());
	Add(TEXT("Resistance"), Char->GetEffectiveResistance());
	Add(TEXT("ReflexWindowFactor"), Char->ReflexWindowGearFactor());
	Add(TEXT("SpellWindowFactor"), Char->SpeedWindowGearFactor(EActionType::Spell));
	Add(TEXT("AbilityWindowFactor"), Char->SpeedWindowGearFactor(EActionType::Ability));
	if (const UDamageCalculator *Calc = StatDebugSubsystem<UDamageCalculator>(Actor))
	{
		Add(TEXT("Defense"), Calc->GetDefenderFlatDefense(Actor));
		Add(TEXT("CritChance"), Calc->GetCriticalChance(Actor));
		Add(TEXT("CritDamage"), Calc->GetCritDamageMultiplier(Actor));
	}
	if (const USkillEffectManager *SEM = StatDebugSubsystem<USkillEffectManager>(Actor))
	{
		for (const ESubStat Stat : STAT_DEBUG_SUB_STATS)
		{
			Add(*FString::Printf(TEXT("Net%s"), *UEnum::GetValueAsString(Stat).RightChop(FCString::Strlen(TEXT("ESubStat::")))),
				SEM->GetNetStatModifier(Actor, Stat));
		}
	}
	return FString::Join(Lines, TEXT("\n"));
}

void UStatModifierDebug::CompareStats(const UObject *WorldContextObject)
{
	const TArray<AActor *> Combatants = StatDebugCombatants(WorldContextObject);
	if (Combatants.Num() == 0)
	{
		UE_LOG(LogWoRCombat, Display, TEXT("[CompareStats] no combatants in this world"));
		return;
	}
	for (AActor *Actor : Combatants)
	{
		const FString Name = Actor->GetName();
		const FString Snapshot = GetStatsSnapshotString(Actor);
		const FString *Previous = GStatDebugLastSnapshots.Find(Name);
		if (!Previous)
		{
			UE_LOG(LogWoRCombat, Display, TEXT("[CompareStats] %s: first snapshot\n%s"), *Name, *Snapshot);
		}
		else
		{
			TArray<FString> Before, After;
			Previous->ParseIntoArrayLines(Before);
			Snapshot.ParseIntoArrayLines(After);
			int32 Changed = 0;
			for (int32 Index = 0; Index < After.Num(); ++Index)
			{
				const FString Old = Before.IsValidIndex(Index) ? Before[Index] : TEXT("(none)");
				if (Old != After[Index])
				{
					++Changed;
					UE_LOG(LogWoRCombat, Display, TEXT("[CompareStats] %s: %s -> %s"), *Name, *Old, *After[Index]);
				}
			}
			UE_LOG(LogWoRCombat, Display, TEXT("[CompareStats] %s: %s"), *Name,
				   Changed == 0 ? TEXT("identical") : *FString::Printf(TEXT("%d changed"), Changed));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(INDEX_NONE, STAT_DEBUG_SCREEN_SECONDS, Changed == 0 ? FColor::Green : FColor::Yellow,
												 FString::Printf(TEXT("CompareStats %s: %d changed"), *Name, Changed));
			}
		}
		GStatDebugLastSnapshots.Add(Name, Snapshot);
	}
}

void UStatModifierDebug::PrintStatModifiers(const UObject *WorldContextObject)
{
	for (AActor *Actor : StatDebugCombatants(WorldContextObject))
	{
		const FString Line = FString::Printf(TEXT("%s: %s"), *Actor->GetName(), *GetStatModifierString(Actor));
		UE_LOG(LogWoRCombat, Display, TEXT("[StatModifiers] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, STAT_DEBUG_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
}
