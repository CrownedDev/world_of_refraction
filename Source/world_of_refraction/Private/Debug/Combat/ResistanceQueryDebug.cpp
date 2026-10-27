// ResistanceQueryDebug.cpp

#include "Debug/Combat/ResistanceQueryDebug.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/CombatConstants.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float RESIST_DEBUG_SCREEN_SECONDS = 8.0f;
	/** Float noise from summing the same terms in another order; far below one buildup point. */
	constexpr float RESIST_DEBUG_TOLERANCE = 1.0e-5f;
	constexpr float RESIST_DEBUG_PERCENT = 100.0f;

	/** Generic is left out on purpose: it is resolved before a hit, and a leftover Generic now reads
	 *  the non-elemental bucket (Q-216), so it differs from the old composition by design. */
	const ESpellElement RESIST_DEBUG_ELEMENTS[] = {
		ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth, ESpellElement::Wind,
		ESpellElement::Light, ESpellElement::Darkness, ESpellElement::Lightning, ESpellElement::Void,
		ESpellElement::Reality, ESpellElement::None};
	const EPhysicalDamageType RESIST_DEBUG_PHYSICAL[] = {
		EPhysicalDamageType::None, EPhysicalDamageType::Slash, EPhysicalDamageType::Pierce, EPhysicalDamageType::Impact};

	FAutoConsoleCommandWithWorld GCompareResistanceCmd(
		TEXT("wor.CompareResistance"),
		TEXT("DEBUG: buildup resistance per damage type (B-216) vs the old composition, every combatant x every hit."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UResistanceQueryDebug::CompareResistance(World); }));

	FAutoConsoleCommandWithWorld GPrintResistanceTypesCmd(
		TEXT("wor.PrintResistanceTypes"),
		TEXT("DEBUG: print each combatant's resistance per damage type."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UResistanceQueryDebug::PrintResistanceTypes(World); }));

	template <typename TSubsystem>
	TSubsystem *ResistDebugSubsystem(const AActor *Actor)
	{
		const UWorld *World = Actor ? Actor->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	template <typename TEnum>
	FString ResistDebugName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	TArray<AActor *> ResistDebugCombatants(const UObject *WorldContextObject)
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

FString UResistanceQueryDebug::GetResistanceTypesString(AActor *Actor)
{
	if (!Actor)
	{
		return TEXT("no actor");
	}
	FString Out = FString::Printf(TEXT("=== Resistance by damage type: %s ===\n"), *Actor->GetName());
	for (const EWoRDamageType DamageType : WoRDamageType::All())
	{
		Out += FString::Printf(TEXT("  %-10s %+6.1f%%\n"), *ResistDebugName(DamageType),
							   WoRResistance::GetTypeResistance(Actor, DamageType) * RESIST_DEBUG_PERCENT);
	}
	Out += FString::Printf(TEXT("  StatusResistance  %.1f%%   StatusResistBonus %+.1f%%"),
						   WoRResistance::GetGeneralStatusResistance(Actor) * RESIST_DEBUG_PERCENT,
						   WoRResistance::GetStatusResistBonus(Actor) * RESIST_DEBUG_PERCENT);
	return Out;
}

float UResistanceQueryDebug::GetLegacyTotalStatusResistance(AActor *Target, ESpellElement Element, EPhysicalDamageType PhysicalType)
{
	// Verbatim pre-B-216 UStatusBuildupManager::GetTotalStatusResistance (comments trimmed).
	if (!Target)
	{
		return 0.0f;
	}
	UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	if (!TargetComp || !TargetComp->CharacterData)
	{
		return 0.0f;
	}
	UStatusBuildupManager *SBM = ResistDebugSubsystem<UStatusBuildupManager>(Target);
	USkillEffectManager *EffectMgr = ResistDebugSubsystem<USkillEffectManager>(Target);

	float General = TargetComp->GetCrystalResistanceStatCapped();
	if (ULoadoutComponent *TargetLoadout = Target->FindComponentByClass<ULoadoutComponent>())
	{
		const FEquipmentStatBonus TargetBonus = TargetLoadout->GetActiveStatBonus(Target);
		General *= (1.0f + TargetBonus.BonusResistance * CombatConstants::RESISTANCE_PER_POINT);
		General *= (1.0f + TargetLoadout->GetAttachedStonePercent(ESubStat::Resistance) / CombatConstants::STAT_PERCENT_DIVISOR);
	}
	General = FMath::Clamp(General, 0.0f, CombatConstants::RESISTANCE_MAX);

	float Matched = 0.0f;
	Matched += SBM ? SBM->GetTotalElementResistance(Target, Element, PhysicalType) : 0.0f;
	Matched += ClassInnateResistanceTable::GetClassInnateResistance(Target, Element, PhysicalType);
	if (ULoadoutComponent *GearLoadout = Target->FindComponentByClass<ULoadoutComponent>())
	{
		const FResistanceBonus GearResist = GearLoadout->GetActiveResistanceBonus(Target);
		const ClassInnateResistanceTable::FResistanceRow GearRow{
			GearResist.Fire, GearResist.Water, GearResist.Earth, GearResist.Wind,
			GearResist.Light, GearResist.Darkness, GearResist.Lightning, GearResist.Void,
			GearResist.Reality, GearResist.Slash, GearResist.Pierce, GearResist.Impact};
		Matched += (ClassInnateResistanceTable::GetElementColumn(GearRow, Element) +
					ClassInnateResistanceTable::GetPhysicalColumn(GearRow, PhysicalType)) /
				   ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}
	if (EffectMgr)
	{
		Matched += EffectMgr->GetTotalStatModifier(Target, ESkillEffectType::ModifyStatusResist) / CombatConstants::STAT_PERCENT_DIVISOR;
	}
	if (const UBossPhaseComponent *Boss = Target->FindComponentByClass<UBossPhaseComponent>())
	{
		Matched += Boss->GetStatusResistanceBonus();
	}
	return FMath::Clamp(General + Matched, CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX);
}

int32 UResistanceQueryDebug::CompareActorResistance(AActor *Actor, bool bLogDifferences)
{
	int32 Differ = 0;
	for (const ESpellElement Element : RESIST_DEBUG_ELEMENTS)
	{
		for (const EPhysicalDamageType Physical : RESIST_DEBUG_PHYSICAL)
		{
			const float New = WoRResistance::GetTotalStatusResistance(Actor, Element, Physical);
			const float Legacy = GetLegacyTotalStatusResistance(Actor, Element, Physical);
			if (FMath::Abs(New - Legacy) > RESIST_DEBUG_TOLERANCE)
			{
				++Differ;
				if (bLogDifferences)
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CompareResistance] %s %s/%s: new %.5f, legacy %.5f"),
						   *GetNameSafe(Actor), *ResistDebugName(Element), *ResistDebugName(Physical), New, Legacy);
				}
			}
		}
	}
	return Differ;
}

int32 UResistanceQueryDebug::CompareResistance(const UObject *WorldContextObject)
{
	int32 Differ = 0;
	const TArray<AActor *> Combatants = ResistDebugCombatants(WorldContextObject);
	for (AActor *Actor : Combatants)
	{
		Differ += CompareActorResistance(Actor, true);
	}
	const FString Line = FString::Printf(TEXT("[CompareResistance] %d combatant(s), %d hit(s) differ"), Combatants.Num(), Differ);
	UE_LOG(LogWoRCombat, Log, TEXT("%s"), *Line);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, RESIST_DEBUG_SCREEN_SECONDS, Differ == 0 ? FColor::Green : FColor::Yellow, Line);
	}
	return Differ;
}

void UResistanceQueryDebug::PrintResistanceTypes(const UObject *WorldContextObject)
{
	for (AActor *Actor : ResistDebugCombatants(WorldContextObject))
	{
		const FString Text = GetResistanceTypesString(Actor);
		UE_LOG(LogWoRCombat, Log, TEXT("%s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, RESIST_DEBUG_SCREEN_SECONDS, FColor::Cyan, Text);
		}
	}
}
