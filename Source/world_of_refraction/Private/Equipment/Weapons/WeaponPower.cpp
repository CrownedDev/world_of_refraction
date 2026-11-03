// WeaponPower.cpp

#include "Equipment/Weapons/WeaponPower.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Tuning/CombatBaselineConstants.h"
#include "Combat/Tuning/CombatTuning.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/WorldStatRequirements.h"

namespace
{
	/** The scaling factor with no pillar bonus (B-326: an unmet requirement). */
	constexpr float NO_PILLAR_SCALING = 1.0f;

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
		// Past the cap the ladder is indexed, so the C++ curve (which clamps to 1) answers.
		return WoRTuning::GetLadderValue(ECombatLadder::PillarCurve, Level, CombatBaseline::CodePillarCurve(Level));
	}

	float GetReferencePower(EItemTier Tier)
	{
		const int32 TierIndex = TierHelpers::GetTierValue(Tier);
		return WoRTuning::GetLadderValue(ECombatLadder::WeaponReferencePower, TierIndex, CombatBaseline::CodeWeaponReferencePower(TierIndex));
	}

	int32 GetReferenceRequirement(EItemTier Tier)
	{
		const int32 TierIndex = TierHelpers::GetTierValue(Tier);
		return FMath::RoundToInt(WoRTuning::GetLadderValue(ECombatLadder::WeaponRequirementByTier, TierIndex,
														   static_cast<float>(CombatBaseline::CodeWeaponRequirement(TierIndex))));
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
		// B-326 (D-119): an unmet requirement also drops the weapon's pillar scaling.
		const float Scaling = WeaponRequirement::IsUnmetFactor(RequirementFactor) ? NO_PILLAR_SCALING
																				   : GetScalingFactor(Weapon.StatScaling, Levels);
		return GetScaledPower(Weapon, InstanceTier) * Scaling * RequirementFactor;
	}

	FString GetWeaponPartString(const UWeaponData &Weapon, EItemTier InstanceTier, const FWorldPillarLevels &Levels,
								float RequirementFactor)
	{
		FString Out = FString::Printf(TEXT("Power %d at %s (reference %.0f, requirement %d) -> %.2f at %s\n"), Weapon.Power,
									  *UEnum::GetValueAsString(Weapon.Tier), GetReferencePower(Weapon.Tier), GetReferenceRequirement(Weapon.Tier),
									  GetScaledPower(Weapon, InstanceTier), *UEnum::GetValueAsString(InstanceTier));
		for (const FPillarScaling &Grade : Weapon.StatScaling)
		{
			const int32 Level = LevelOf(Levels, Grade.Pillar);
			Out += FString::Printf(TEXT("  %s %s: x%.2f x curve(%d) %.2f\n"), *UEnum::GetDisplayValueAsText(Grade.Pillar).ToString(),
								   *GetScalingTierString(Grade.Tier), GetScalingTierCoefficient(Grade.Tier), Level, GetPillarCurve(Level));
		}
		Out += FString::Printf(TEXT("Scaling x%.3f%s  requirement x%.2f  weapon part %.2f"), GetScalingFactor(Weapon.StatScaling, Levels),
							   WeaponRequirement::IsUnmetFactor(RequirementFactor) ? TEXT(" (off: requirement unmet)") : TEXT(""),
							   RequirementFactor, GetWeaponPart(Weapon, InstanceTier, Levels, RequirementFactor));
		return Out;
	}
}
