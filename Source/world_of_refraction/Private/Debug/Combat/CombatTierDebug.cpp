// CombatTierDebug.cpp

#include "Debug/Combat/CombatTierDebug.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Tier/CombatTier.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float COMBAT_TIER_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintCombatTiersCmd(
		TEXT("wor.Combat.PrintTiers"),
		TEXT("DEBUG: print every combatant's Combat Tier (B-325): the tiers averaged, the fallback and the tier-power HP."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UCombatTierDebug::PrintCombatTiers(World);
			}));
}

FString UCombatTierDebug::GetCombatTierString(const AActor *Actor)
{
	if (!Actor)
	{
		return TEXT("<no actor>");
	}
	const EItemTier Tier = WoRCombatTier::GetCombatTier(Actor);
	TArray<EItemTier> Tiers;
	WoRCombatTier::GatherTiers(Actor, Tiers);
	FString Source;
	if (Tiers.Num() == 0)
	{
		Source = TEXT("no gear or skills, fallback (EnemyTier)");
	}
	else
	{
		TArray<FString> Letters;
		for (const EItemTier Each : Tiers)
		{
			Letters.Add(TierHelpers::GetTierName(Each));
		}
		Source = FString::Printf(TEXT("from %d (%s)"), Tiers.Num(), *FString::Join(Letters, TEXT(" ")));
	}
	return FString::Printf(TEXT("%s: Tier %s %s, tier-power HP x%.2f if an enemy"), *Actor->GetName(),
						   *TierHelpers::GetTierName(Tier), *Source, TierPowerScaling::GetTierPowerMultiplier(Tier));
}

void UCombatTierDebug::PrintCombatTiers(const UObject *WorldContextObject)
{
	const UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->FindComponentByClass<UCharacterDataComponent>())
		{
			continue;
		}
		const FString Line = GetCombatTierString(*It);
		UE_LOG(LogWoRCombat, Display, TEXT("[CombatTier] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, COMBAT_TIER_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
}
