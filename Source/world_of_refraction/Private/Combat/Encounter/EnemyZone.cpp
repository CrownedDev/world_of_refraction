// EnemyZone.cpp

#include "Combat/Encounter/EnemyZone.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace EnemyZoneInternal
{
	// Q-186: one named multiplier per tier. TODO: tune (Q-186).
	constexpr float TIER_STRENGTH[EnemyZoneConstants::MAX_ZONE_TIER + 1] = {1.0f, 1.15f, 1.3f, 1.5f, 1.75f};
	/** Q-328: the lowest Combat Tier for a zone's enemies, two letters a zone at first, then one. */
	constexpr EItemTier ENEMY_TIER_GUIDE[EnemyZoneConstants::MAX_ZONE_TIER + 1] = {EItemTier::F_Tier, EItemTier::D_Tier,
																				 EItemTier::C_Tier, EItemTier::B_Tier,
																				 EItemTier::A_Tier};

	const FVector DEFAULT_ZONE_EXTENT(1000.0f, 1000.0f, 500.0f);
}

// FEncounterZoneInfo::Tier's ClampMax must be a literal for UHT; keep it equal to this.
static_assert(EnemyZoneConstants::MAX_ZONE_TIER == 4, "Update FEncounterZoneInfo::Tier ClampMax and TIER_STRENGTH");

AEnemyZone::AEnemyZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	RootComponent = Area;
	Area->SetBoxExtent(EnemyZoneInternal::DEFAULT_ZONE_EXTENT);
	// A marker only: fights ask it whether they started inside.
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool AEnemyZone::Contains(const FVector &Location) const
{
	if (!Area)
	{
		return false;
	}
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FEncounterZoneInfo AEnemyZone::FindZoneAt(const UWorld *World, const FVector &Location)
{
	FEncounterZoneInfo Best;
	if (!World)
	{
		return Best;
	}
	bool bFound = false;
	for (TActorIterator<AEnemyZone> It(World); It; ++It)
	{
		if (It->Contains(Location) && (!bFound || It->Zone.Tier > Best.Tier))
		{
			Best = It->Zone;
			bFound = true;
		}
	}
	return Best;
}

float AEnemyZone::GetTierStrengthMultiplier(int32 Tier)
{
	return EnemyZoneInternal::TIER_STRENGTH[FMath::Clamp(Tier, 0, EnemyZoneConstants::MAX_ZONE_TIER)];
}

EItemTier AEnemyZone::GetEnemyTierGuide(int32 Tier)
{
	return EnemyZoneInternal::ENEMY_TIER_GUIDE[FMath::Clamp(Tier, 0, EnemyZoneConstants::MAX_ZONE_TIER)];
}

FString AEnemyZone::GetEnemyZoneString() const
{
	return FString::Printf(TEXT("[EnemyZone] %s: tier %d (enemies Tier %s or higher), loot pool %s"), *GetName(),
						   Zone.Tier, *TierHelpers::GetTierName(GetEnemyTierGuide(Zone.Tier)),
						   Zone.LootPool.IsValid() ? *Zone.LootPool.ToString() : TEXT("<none>"));
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorld GPrintEnemyZonesCmd(
		TEXT("wor.Trial.PrintZones"),
		TEXT("Print every enemy zone on this map: its tier, strength and loot pool (B-186)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				for (TActorIterator<AEnemyZone> It(World); It; ++It)
				{
					UE_LOG(LogTemp, Display, TEXT("%s"), *It->GetEnemyZoneString());
				}
			}));
}
#endif
