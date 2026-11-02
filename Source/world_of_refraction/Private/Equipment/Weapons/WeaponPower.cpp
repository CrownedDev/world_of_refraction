// WeaponPower.cpp

#include "Equipment/Weapons/WeaponPower.h"
#include "Character/StatConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/WorldStatRequirements.h"

namespace
{
	int32 LevelOf(const FWorldPillarLevels &Levels, EWorldPillar Pillar)
	{
		switch (Pillar)
		{
		case EWorldPillar::Mind: return Levels.Mind;
		case EWorldPillar::Body: return Levels.Body;
		case EWorldPillar::Spirit: return Levels.Spirit;
		default: return 0;
		}
	}
}

namespace WoRWeaponPower
{
	float GetPillarCurve(int32 Level)
	{
		const float Max = static_cast<float>(StatConstants::MAX_WORLD_STAT_LEVEL);
		const float Short = 1.0f - FMath::Clamp(static_cast<float>(Level), 0.0f, Max) / Max;
		return 1.0f - Short * Short;
	}

	float GetScaledPower(const UWeaponData &Weapon, EItemTier InstanceTier)
	{
		const float AssetTierPower = TierPowerScaling::GetTierPowerMultiplier(Weapon.Tier);
		const float InstanceTierPower = TierPowerScaling::GetTierPowerMultiplier(InstanceTier);
		return AssetTierPower > KINDA_SMALL_NUMBER ? Weapon.Power * InstanceTierPower / AssetTierPower : static_cast<float>(Weapon.Power);
	}

	float GetScalingFactor(const TArray<FPillarScaling> &Scaling, const FWorldPillarLevels &Levels)
	{
		float Factor = 1.0f;
		for (const FPillarScaling &Grade : Scaling)
		{
			Factor += GetScalingTierCoefficient(Grade.Tier) * GetPillarCurve(LevelOf(Levels, Grade.Pillar));
		}
		return Factor;
	}

	float GetWeaponPart(const UWeaponData &Weapon, EItemTier InstanceTier, const FWorldPillarLevels &Levels, float RequirementFactor)
	{
		if (Weapon.Power <= 0)
		{
			return 0.0f;
		}
		return GetScaledPower(Weapon, InstanceTier) * GetScalingFactor(Weapon.StatScaling, Levels) * RequirementFactor;
	}

	FString GetWeaponPartString(const UWeaponData &Weapon, EItemTier InstanceTier, const FWorldPillarLevels &Levels,
								float RequirementFactor)
	{
		FString Out = FString::Printf(TEXT("Power %d at %s -> %.2f at %s\n"), Weapon.Power, *UEnum::GetValueAsString(Weapon.Tier),
									  GetScaledPower(Weapon, InstanceTier), *UEnum::GetValueAsString(InstanceTier));
		for (const FPillarScaling &Grade : Weapon.StatScaling)
		{
			const int32 Level = LevelOf(Levels, Grade.Pillar);
			Out += FString::Printf(TEXT("  %s %s: x%.2f x curve(%d) %.2f\n"), *UEnum::GetDisplayValueAsText(Grade.Pillar).ToString(),
								   *GetScalingTierString(Grade.Tier), GetScalingTierCoefficient(Grade.Tier), Level, GetPillarCurve(Level));
		}
		Out += FString::Printf(TEXT("Scaling x%.3f  requirement x%.2f  weapon part %.2f"), GetScalingFactor(Weapon.StatScaling, Levels),
							   RequirementFactor, GetWeaponPart(Weapon, InstanceTier, Levels, RequirementFactor));
		return Out;
	}
}
