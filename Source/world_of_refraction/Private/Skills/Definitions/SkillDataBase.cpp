// SkillDataBase.cpp
// Shared base class implementation.

#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SkillCastEntry.h"
#include "Skills/Effects/SkillTriggerUtils.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"

TArray<float> ResolveDamageSplit(int32 HitCount, const TArray<FDamageSplitEntry> &Split)
{
    const int32 N = FMath::Max(1, HitCount);

    // Empty split = even split — byte-identical to the legacy behavior.
    TArray<float> Table;
    if (Split.Num() == 0)
    {
        Table.Init(100.0f / N, N);
        return Table;
    }

    // Walk authored entries; invalid ones warn and are skipped.
    Table.Init(0.0f, N);
    TArray<bool> bAuthored;
    bAuthored.Init(false, N);
    float AuthoredSum = 0.0f;
    int32 AuthoredCount = 0;

    for (const FDamageSplitEntry &Entry : Split)
    {
        if (Entry.HitNumber < 1 || Entry.HitNumber > N)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ResolveDamageSplit] HitNumber %d out of range [1,%d] — entry skipped"),
                   Entry.HitNumber, N);
            continue;
        }
        if (Entry.Percent <= 0.0f)
        {
            UE_LOG(LogTemp, Verbose,
                   TEXT("[ResolveDamageSplit] Hit %d Percent %.1f <= 0 — treated as unauthored (remainder split evenly among unassigned hits), not lost"),
                   Entry.HitNumber, Entry.Percent);
            continue;
        }
        if (bAuthored[Entry.HitNumber - 1])
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ResolveDamageSplit] Duplicate entry for hit %d — entry skipped (first wins)"),
                   Entry.HitNumber);
            continue;
        }

        Table[Entry.HitNumber - 1] = Entry.Percent;
        bAuthored[Entry.HitNumber - 1] = true;
        AuthoredSum += Entry.Percent;
        ++AuthoredCount;
    }

    // Unassigned hits share what's left of 100.
    const int32 UnassignedCount = N - AuthoredCount;
    if (UnassignedCount > 0)
    {
        const float PerUnassigned = FMath::Max(0.0f, 100.0f - AuthoredSum) / UnassignedCount;
        for (int32 i = 0; i < N; ++i)
        {
            if (!bAuthored[i])
            {
                Table[i] = PerUnassigned;
            }
        }
    }

    // Normalize when the table can't sum to 100 (over-authored, or all hits
    // authored to a non-100 total). Ratios preserved, total damage invariant —
    // redistribute, never amplify.
    float Total = 0.0f;
    for (const float Pct : Table)
    {
        Total += Pct;
    }
    if (!FMath::IsNearlyEqual(Total, 100.0f, 0.01f) && Total > 0.0f)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[ResolveDamageSplit] Authored split sums to %.1f%% — normalizing to 100%% (ratios preserved)"),
               Total);
        const float Scale = 100.0f / Total;
        for (float &Pct : Table)
        {
            Pct *= Scale;
        }
    }

    return Table;
}

TArray<FDefenseDifficultyTriple> ResolveImpactDifficulty(
    int32 HitCount, const TArray<FDamageSplitEntry> &Split, const FDefenseDifficultyTriple &Default)
{
    const int32 N = FMath::Max(1, HitCount);

    // Start every impact at the skill-level default; per-hit Difficulty (carried on the DamageSplit
    // entries — same array ResolveDamageSplit reads) replaces per HitNumber below. .Percent is ignored
    // here; difficulty is keyed purely by HitNumber, independent of whether the hit carries damage.
    TArray<FDefenseDifficultyTriple> Table;
    Table.Init(Default, N);
    TArray<bool> bAuthored;
    bAuthored.Init(false, N);

    for (const FDamageSplitEntry &Entry : Split)
    {
        if (Entry.HitNumber < 1 || Entry.HitNumber > N)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ResolveImpactDifficulty] HitNumber %d out of range [1,%d] — entry skipped"),
                   Entry.HitNumber, N);
            continue;
        }
        if (bAuthored[Entry.HitNumber - 1])
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ResolveImpactDifficulty] Duplicate entry for impact %d — entry skipped (first wins)"),
                   Entry.HitNumber);
            continue;
        }

        Table[Entry.HitNumber - 1] = Entry.Difficulty;
        bAuthored[Entry.HitNumber - 1] = true;
    }

    // Collapse Inherit per field: impact's own tier → Default → Easy. The resolved table
    // carries NO Inherit values (every field concrete) — clean for cluster 3/4 to consume.
    for (FDefenseDifficultyTriple &Triple : Table)
    {
        Triple.Parry = ResolveInheritedDifficulty(Triple.Parry, Default.Parry);
        Triple.Dodge = ResolveInheritedDifficulty(Triple.Dodge, Default.Dodge);
        Triple.Block = ResolveInheritedDifficulty(Triple.Block, Default.Block);
    }

    return Table;
}

TArray<FDefenseDifficultyTriple> ResolveCastDifficulty(
    const TArray<FSkillCastEntry> &CastArray, const FDefenseDifficultyTriple &Default)
{
    // Index IS array position — no HitNumber lookup, no dedup/range warnings (unlike the melee
    // ResolveImpactDifficulty). Just walk each cast-entry and collapse Inherit per field:
    // entry tier -> Default -> Easy. Empty CastArray -> empty table (the cluster-4 reader guards).
    TArray<FDefenseDifficultyTriple> Table;
    Table.Reserve(CastArray.Num());
    for (const FSkillCastEntry &E : CastArray)
    {
        FDefenseDifficultyTriple T;
        T.Parry = ResolveInheritedDifficulty(E.Difficulty.Parry, Default.Parry);
        T.Dodge = ResolveInheritedDifficulty(E.Difficulty.Dodge, Default.Dodge);
        T.Block = ResolveInheritedDifficulty(E.Difficulty.Block, Default.Block);
        Table.Add(T);
    }
    return Table;
}

bool USkillDataBase::HasDrainEffect() const
{
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsDrain())
        {
            return true;
        }
    }
    return false;
}

bool USkillDataBase::HasBuffEffects() const
{
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsBuff())
        {
            return true;
        }
    }
    return false;
}

bool USkillDataBase::HasDebuffEffects() const
{
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsDebuff())
        {
            return true;
        }
    }
    return false;
}

// Folded from UCastableSkillDataBase at the base merge — read the requirement/tier
// fields now living on this class.

bool USkillDataBase::MeetsRequirements(const UCharacterData *Character) const
{
    if (!Character)
    {
        return false;
    }
    return Requirements.MeetsRequirements(Character);
}

int32 USkillDataBase::GetTotalDeficit(const UCharacterData *Character) const
{
    if (!Character)
    {
        return 0;
    }
    return Requirements.GetTotalDeficit(Character);
}

float USkillDataBase::CalculateRequirementPenalty(const UCharacterData *Character) const
{
    if (!Character)
    {
        return 0.0f;
    }
    return Requirements.CalculatePenalty(Character);
}

// Damage / energy — hoisted from UAbilityData at the attack/ability merge (Cluster 1) so the
// merged FAction.SkillData path can compute these for both attacks and abilities. Bodies are
// byte-identical to UAbilityData::CalculateDamage / CalculateNormalEnergyCost+CalculateInfusedEnergyCost
// and to the inline attack computation in ExecuteAttackAsync — lossless across all leaves.

int32 USkillDataBase::CalculateDamage(const UCharacterData *Character, bool bIsInfused) const
{
    if (!Character)
    {
        return 0;
    }

    // Attacker-side base only — the RawDamage multiplier is applied once downstream by
    // DamageCalculator. bIsInfused no longer affects damage (element-infusion penalty removed).
    float Damage = BaseDamage;
    Damage *= (1.0f - CalculateRequirementPenalty(Character));
    return FMath::RoundToInt(Damage);
}

int32 USkillDataBase::CalculateEnergyCost(const UCharacterData *Character, bool bIsInfused) const
{
    if (!Character)
    {
        return BaseEnergyCost;
    }

    float Cost = BaseEnergyCost;
    Cost *= (1.0f + CalculateRequirementPenalty(Character));
    if (bIsInfused)
    {
        Cost *= CombatConstants::INFUSION_ENERGY_MULTIPLIER;
    }
    return FMath::RoundToInt(Cost);
}

FString USkillDataBase::GetTierString() const
{
    return TierHelpers::GetTierDisplayString(Tier);
}

// Hoisted from UWeaponAttackData (step 5a) — reads only base fields, so it serves every skill leaf.
FString USkillDataBase::GetAttackSummary() const
{
    FString Summary;

    if (HitCount == 1)
    {
        Summary = TEXT("Single Hit");
    }
    else
    {
        // Per-hit distribution is authored via DamageSplit on the base (D1).
        Summary = FString::Printf(TEXT("%d Hits"), HitCount);
    }

    Summary += FString::Printf(TEXT(" | Buildup: %d"), StatusBuildup);
    Summary += FString::Printf(TEXT(" | Energy: %d"), BaseEnergyCost);
    Summary += FString::Printf(TEXT(" | Speed: %.2fx"), BaseAnimSpeed);

    return Summary;
}

void USkillDataBase::PostLoad()
{
    Super::PostLoad();

    if (bTargetAxisMigrated)
        return; // already migrated — don't corrupt re-saved assets

    // Migrate the legacy combined field to the two-axis model.
    // LegacyTargetType still holds the old serialized name (UE resolves by name).
    switch (LegacyTargetType)
    {
    case ETargetType_Legacy::Self:
        TargetType = ETargetType::Self;   TargetCount = ETargetCount::Single; break;
    case ETargetType_Legacy::SingleEnemy:
        TargetType = ETargetType::Enemy;  TargetCount = ETargetCount::Single; break;
    case ETargetType_Legacy::AllEnemies:
        TargetType = ETargetType::Enemy;  TargetCount = ETargetCount::All;    break;
    case ETargetType_Legacy::SingleAlly:
        TargetType = ETargetType::Ally;   TargetCount = ETargetCount::Single; break;
    case ETargetType_Legacy::AllAllies:
        TargetType = ETargetType::Ally;   TargetCount = ETargetCount::All;    break;
    case ETargetType_Legacy::SingleAnyone:
        TargetType = ETargetType::Anyone; TargetCount = ETargetCount::Single; break;
    case ETargetType_Legacy::Everyone:
        TargetType = ETargetType::Anyone; TargetCount = ETargetCount::All;    break;
    }
    bTargetAxisMigrated = true;
    // PostLoad does NOT dirty the package — asset must be re-saved to bake this.
}

#if WITH_EDITOR
EDataValidationResult USkillDataBase::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Name.IsEmpty())
    {
        Context.AddWarning(FText::FromString(TEXT("Skill has empty Name")));
    }

    if (Effects.Num() > LoadoutConstants::MAX_SKILL_EFFECTS)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Too many effects (%d). Maximum is %d"),
            Effects.Num(), LoadoutConstants::MAX_SKILL_EFFECTS)));
        Result = EDataValidationResult::Invalid;
    }

    // Cluster D1: payload count per effect is bounded by the cast EffectID packing.
    for (int32 EffIdx = 0; EffIdx < Effects.Num(); ++EffIdx)
    {
        if (Effects[EffIdx].Payloads.Num() > LoadoutConstants::MAX_PAYLOADS)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Effect %d has too many payloads (%d). Maximum is %d"),
                EffIdx, Effects[EffIdx].Payloads.Num(), LoadoutConstants::MAX_PAYLOADS)));
            Result = EDataValidationResult::Invalid;
        }
    }

    // Folded from UCastableSkillDataBase::IsDataValid at the base merge.
    if (BaseDamage < 0)
    {
        Context.AddError(FText::FromString(TEXT("BaseDamage cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    if (BaseEnergyCost < 0)
    {
        Context.AddError(FText::FromString(TEXT("BaseEnergyCost cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}

void USkillDataBase::PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    // Refresh threshold-visibility flags on every effect so EditCondition gating
    // for ConditionThreshold / SecondaryThreshold / TargetThreshold reacts live
    // to in-editor edits of the matching ESkillTrigger field.
    for (FSkillEffect &Effect : Effects)
    {
        Effect.SyncThresholdFlags();
    }
}
#endif
