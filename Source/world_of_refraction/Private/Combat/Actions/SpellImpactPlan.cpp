// SpellImpactPlan.cpp
// B-032d (Q-032d): see SpellImpactPlan.h.

#include "Combat/Actions/SpellImpactPlan.h"
#include "Combat/CombatConstants.h"
#include "Skills/Definitions/SkillCastEntry.h"
#include "Skills/Definitions/SpellData.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/** An AOE or Instant entry, or a projectile entry's least Count, lands once per target. */
	constexpr int32 SINGLE_ARRIVAL = 1;
	/** A spell with no cast entries delivers once per target (SpawnSpellDelivery). */
	constexpr int32 LOOSE_PATH_ARRIVALS = SINGLE_ARRIVAL;
	/** The B-032 shape: one cast entry, one asset hit. */
	constexpr int32 SINGLE_CAST_ENTRY = 1;
	constexpr int32 SINGLE_HIT = 1;
	/** Absorbs even-fraction float error (100/3 = 33.333332) so an exact share doesn't floor one short; the same
	 *  epsilon as ApplyOneImpact's lumped slice, so both paths cut a hit the same way. */
	constexpr float SPLIT_FLOOR_EPSILON = 0.01f;
}

int32 SpellImpactPlan::ArrivalsOf(const FSkillCastEntry &Entry)
{
	return Entry.DeliveryType == ESpellDeliveryType::Projectile ? FMath::Max(SINGLE_ARRIVAL, Entry.Count) : SINGLE_ARRIVAL;
}

FSpellImpactPlan SpellImpactPlan::Make(const USpellData &Spell)
{
	FSpellImpactPlan Plan;
	Plan.AuthoredHitCount = FMath::Max(SINGLE_HIT, Spell.HitCount);
	Plan.bOffensive = Spell.TargetType == ETargetType::Enemy;

	int32 Arrivals = 0;
	for (const FSkillCastEntry &Entry : Spell.CastArray)
	{
		Arrivals += ArrivalsOf(Entry);
	}
	Plan.ImpactsPerTarget = Spell.CastArray.IsEmpty() ? LOOSE_PATH_ARRIVALS : FMath::Max(SINGLE_ARRIVAL, Arrivals);
	Plan.bUsesDamageSplit = Plan.AuthoredHitCount == Plan.ImpactsPerTarget;
	return Plan;
}

bool SpellImpactPlan::IsSingleEntrySingleHit(const USpellData &Spell)
{
	return Spell.CastArray.Num() == SINGLE_CAST_ENTRY && Spell.HitCount == SINGLE_HIT;
}

bool SpellImpactPlan::IsWidened(const USpellData &Spell)
{
	return Spell.TargetType == ETargetType::Enemy && !IsSingleEntrySingleHit(Spell);
}

int32 SpellImpactPlan::ImpactBaseSlice(int32 BaseDamage, int32 ImpactCount, const TArray<float> &SplitPercents, int32 ImpactIndex)
{
	const int32 Count = FMath::Max(SINGLE_ARRIVAL, ImpactCount);
	const bool bUseSplit = (SplitPercents.Num() == Count) && SplitPercents.IsValidIndex(ImpactIndex);
	if (bUseSplit)
	{
		return FMath::FloorToInt(BaseDamage * (SplitPercents[ImpactIndex] / CombatConstants::STAT_PERCENT_DIVISOR) + SPLIT_FLOOR_EPSILON);
	}
	const int32 EvenRemainder = (ImpactIndex < (BaseDamage % Count)) ? 1 : 0;
	return (BaseDamage / Count) + EvenRemainder;
}

FString SpellImpactPlan::GetPlanString(const USpellData &Spell, bool bPerImpactDefenseOn)
{
	const FSpellImpactPlan Plan = Make(Spell);
	const bool bPerImpact = IsSingleEntrySingleHit(Spell) || (bPerImpactDefenseOn && IsWidened(Spell));
	return FString::Printf(TEXT("%s: %d impact(s) per target (%d cast entries, HitCount %d, %s), %s%s"),
						   *Spell.Name, Plan.ImpactsPerTarget, Spell.CastArray.Num(), Plan.AuthoredHitCount,
						   Plan.bUsesDamageSplit ? TEXT("DamageSplit") : TEXT("even split: HitCount does not match"),
						   bPerImpact ? TEXT("per impact") : TEXT("lumped"),
						   Plan.bOffensive ? TEXT("") : TEXT(" (support)"));
}

namespace
{
	FAutoConsoleCommand PrintSpellImpactsCommand(
		TEXT("wor.Combat.PrintSpellImpacts"),
		TEXT("B-032d: logs every loaded spell's impact plan (impacts per target, split, per impact or lumped) under ")
		TEXT("the current wor.Combat.PerImpactDefense."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const IConsoleVariable *PerImpact = IConsoleManager::Get().FindConsoleVariable(TEXT("wor.Combat.PerImpactDefense"));
			const bool bOn = PerImpact && PerImpact->GetBool();
			int32 Listed = 0;
			for (TObjectIterator<USpellData> It; It; ++It)
			{
				if (It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
				{
					continue;
				}
				UE_LOG(LogTemp, Log, TEXT("[SpellImpacts] %s"), *SpellImpactPlan::GetPlanString(**It, bOn));
				++Listed;
			}
			UE_LOG(LogTemp, Log, TEXT("[SpellImpacts] %d loaded spell(s), wor.Combat.PerImpactDefense %d"), Listed, bOn ? 1 : 0);
		}));
}
