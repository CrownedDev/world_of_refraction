// TrialPool.cpp

#include "Trial/TrialPool.h"

#include "Combat/Encounter/EnemyZone.h"
#include "EngineUtils.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Trial/TrialData.h"
#include "Trial/TrialLoot.h"

namespace
{
	bool IsOwnElement(ECrystalType Type, ESpellElement TrialElement)
	{
		const bool bLeans = TrialElement != ESpellElement::Generic && TrialElement != ESpellElement::None;
		return bLeans && CrystalTypeHelpers::GetElement(Type) == TrialElement;
	}

	FString GetRangeString(const FTrialTierRange &Range)
	{
		return FString::Printf(TEXT("%s to %s"), *ItemIdentity::GetTierLetter(Range.Min), *ItemIdentity::GetTierLetter(Range.Max));
	}
}

FTrialTierRange TrialPool::GetZoneTierRange(int32 ZoneTier)
{
	const int32 Zone = FMath::Clamp(ZoneTier, 0, EnemyZoneConstants::MAX_ZONE_TIER);
	const int32 Top = FMath::Min(Zone + ZONE_TIER_SPAN, static_cast<int32>(EItemTier::S_Tier));
	return {EItemTier::F_Tier, static_cast<EItemTier>(Top)};
}

FGameplayTag TrialPool::GetPoolTag(const UTrialData *Trial, const FEncounterZoneInfo &Zone)
{
	if (Zone.LootPool.IsValid())
	{
		return Zone.LootPool;
	}
	return Trial ? Trial->Pool : FGameplayTag();
}

const TArray<ECrystalType> &TrialPool::GetPoolCrystals()
{
	// Q-566 (brain for Crown, D-225 "every element's crystals"): each element's gem. Quartz is no element's and no trial
	// pickup gives it today (a merchant may stock it as a consumable), so it stays out.
	static const TArray<ECrystalType> Crystals = {ECrystalType::Garnet, ECrystalType::Sapphire, ECrystalType::Citrine, ECrystalType::Emerald,
												  ECrystalType::Amber,	ECrystalType::Opal,		ECrystalType::Onyx,	   ECrystalType::Amethyst,
												  ECrystalType::Iolite};
	return Crystals;
}

float TrialPool::GetCrystalWeight(ECrystalType Type, ESpellElement TrialElement)
{
	return IsOwnElement(Type, TrialElement) ? OWN_ELEMENT_WEIGHT : OTHER_ELEMENT_WEIGHT;
}

float TrialPool::GetCrystalTierWeight(EItemTier Tier, const FTrialTierRange &Range, bool bOwnElement)
{
	if (!Range.Contains(Tier))
	{
		return 0.0f;
	}
	const int32 StepsUp = static_cast<int32>(Tier) - static_cast<int32>(Range.Min);
	return 1.0f + (bOwnElement ? OWN_ELEMENT_TIER_STEP * StepsUp : 0.0f);
}

namespace
{
	/** Index into Weights, picked by weight; 0 when every weight is 0. */
	int32 PickWeighted(TConstArrayView<float> Weights, FRandomStream &Roll)
	{
		float Total = 0.0f;
		for (const float Weight : Weights)
		{
			Total += Weight;
		}
		float Point = Roll.FRandRange(0.0f, Total);
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			if (Weights[Index] > 0.0f && Point < Weights[Index])
			{
				return Index;
			}
			Point -= Weights[Index];
		}
		return FMath::Max(0, Weights.FindLastByPredicate([](float Weight) { return Weight > 0.0f; }));
	}
}

FCrystalId TrialPool::PickCrystal(ESpellElement TrialElement, const FTrialTierRange &Range, FRandomStream &Roll)
{
	const TArray<ECrystalType> &Crystals = GetPoolCrystals();
	TArray<float> TypeWeights;
	for (const ECrystalType Type : Crystals)
	{
		TypeWeights.Add(GetCrystalWeight(Type, TrialElement));
	}
	const ECrystalType Type = Crystals[PickWeighted(TypeWeights, Roll)];
	TArray<float> TierWeights;
	for (int32 Tier = static_cast<int32>(Range.Min); Tier <= static_cast<int32>(Range.Max); ++Tier)
	{
		TierWeights.Add(GetCrystalTierWeight(static_cast<EItemTier>(Tier), Range, IsOwnElement(Type, TrialElement)));
	}
	return FCrystalId(Type, static_cast<EItemTier>(static_cast<int32>(Range.Min) + PickWeighted(TierWeights, Roll)));
}

FString TrialPool::GetTrialPoolString(const UTrialData *Trial, const UWorld *World)
{
	if (!Trial)
	{
		return TEXT("[TrialPool] No trial");
	}
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("[TrialPool] %s (%s): %s"), *Trial->GetName(), *UEnum::GetValueAsString(Trial->Element),
							  *TrialLoot::GetPoolString(Trial->Pool)));
	TArray<FString> Weights;
	for (const ECrystalType Type : GetPoolCrystals())
	{
		Weights.Add(FString::Printf(TEXT("%s %.0f"), *ItemIdentity::GetTypeName(Type), GetCrystalWeight(Type, Trial->Element)));
	}
	Lines.Add(FString::Printf(TEXT("  Crystals (weight): %s"), *FString::Join(Weights, TEXT(", "))));

	TArray<const AEnemyZone *> Zones;
	if (World)
	{
		for (TActorIterator<AEnemyZone> It(World); It; ++It)
		{
			Zones.Add(*It);
		}
	}
	Zones.Sort([](const AEnemyZone &A, const AEnemyZone &B) { return A.Zone.Tier < B.Zone.Tier; });
	Lines.Add(FString::Printf(TEXT("  Outside every zone: tiers %s, the trial's pool"), *GetRangeString(GetZoneTierRange(0))));
	for (const AEnemyZone *Zone : Zones)
	{
		const bool bOwnPool = Zone->Zone.LootPool.IsValid();
		Lines.Add(FString::Printf(TEXT("  %s (zone %d): tiers %s, %s"), *Zone->GetActorNameOrLabel(), Zone->Zone.Tier,
								  *GetRangeString(GetZoneTierRange(Zone->Zone.Tier)),
								  bOwnPool ? *TrialLoot::GetPoolString(Zone->Zone.LootPool) : TEXT("the trial's pool")));
	}
	return FString::Join(Lines, LINE_TERMINATOR);
}
