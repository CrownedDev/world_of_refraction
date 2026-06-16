// SpellData.cpp
// Spell Data Asset implementation.

#include "Skills/Definitions/SpellData.h"
#include "Character/CharacterData.h"

// ==================== MIGRATION ====================

void USpellData::PostLoad()
{
    Super::PostLoad();

    // D2: mirror the legacy montage into the unified base field. Triggered only
    // while SkillMontage is unauthored; CastAnimation stays the runtime source
    // of truth until the Stage 12 reader switch. Transient until resaved.
    if (!SkillMontage && CastAnimation)
    {
        SkillMontage = CastAnimation;
    }

    // D5: mirror the loose VFX fields into the role-classified array. Triggered
    // only while VFXArray is unauthored; the loose fields stay the runtime
    // source of truth until the Stage 12 reader switch. SpellVFX does NOT
    // migrate — it becomes the Cast-entry Trail at Stage 11. Attach modes match
    // today's spawn behavior exactly: muzzle spawns at caster origin (no socket,
    // OnSpellAnimNotify), impact bursts on the target. Transient until resaved.
    if (VFXArray.IsEmpty())
    {
        if (MuzzleVFX)
        {
            FSkillVFXEntry &Entry = VFXArray.AddDefaulted_GetRef();
            Entry.Label = TEXT("Muzzle (migrated)");
            Entry.Role = EVFXRole::Muzzle;
            Entry.VFX = MuzzleVFX;
            Entry.Attach = EVFXAttach::Caster;
        }
        if (ImpactVFX)
        {
            FSkillVFXEntry &Entry = VFXArray.AddDefaulted_GetRef();
            Entry.Label = TEXT("Impact (migrated)");
            Entry.Role = EVFXRole::Impact;
            Entry.VFX = ImpactVFX;
            Entry.Attach = EVFXAttach::Target;
        }
    }

    // D6: mirror the loose delivery fields into ONE Cast entry. Triggered only
    // while CastArray is unauthored AND any delivery field differs from the
    // class default (delta serialization can't tell authored-default from
    // untouched — a fully-default spell stays empty until Stage 12 treats
    // empty CastArray as "loose defaults"). Size carries today's ACTUAL
    // hitbox (BaseSize × HitboxRatio) and VisualScale the visual scale —
    // straight BaseSize→Size would inflate every hitbox by 1/HitboxRatio.
    // SpellVFX migrates HERE as the entry's Trail (deferred from Stage 10).
    // Loose fields stay runtime-authoritative until the Stage 12 reader
    // switch. Transient until resaved.
    if (CastArray.IsEmpty())
    {
        const USpellData *Defaults = GetDefault<USpellData>(GetClass());
        const bool bHasDeliveryAuthoring =
            SpellVFX != Defaults->SpellVFX ||
            DeliveryType != Defaults->DeliveryType ||
            ProjectileSpeed != Defaults->ProjectileSpeed ||
            BaseSize != Defaults->BaseSize ||
            HitboxRatio != Defaults->HitboxRatio ||
            HomingStrength != Defaults->HomingStrength ||
            BeamDuration != Defaults->BeamDuration ||
            BeamTickInterval != Defaults->BeamTickInterval;

        if (bHasDeliveryAuthoring)
        {
            FSkillCastEntry &Entry = CastArray.AddDefaulted_GetRef();
            Entry.Label = TEXT("Migrated");
            Entry.DeliveryType = DeliveryType;
            Entry.ProjectileSpeed = ProjectileSpeed;
            Entry.Size = BaseSize * HitboxRatio;
            Entry.VisualScale = BaseSize;
            Entry.Trail = SpellVFX;
            Entry.HomingStrength = HomingStrength;
            Entry.BeamDuration = BeamDuration;
            Entry.BeamTickInterval = BeamTickInterval;
            // ProjectileClass stays null (executor's DefaultProjectileClass,
            // as today); Count/BurstInterval stay defaults (single delivery).
        }
    }
}

// ==================== DAMAGE CALCULATIONS ====================

int32 USpellData::CalculateDamage(UCharacterData *Character, const FActionStatModifiers &ActionMods, int32 BaseDamageOverride) const
{
    if (!Character)
        return 0;

    // Per-cast-entry SPELL damage (Stage 6 cluster 5): swap the raw base when overridden (>= 0),
    // else use the skill-level BaseDamage as before. Only the BASE changes — the bIsRawMode mult +
    // requirement penalty below still apply on it, and the SpellDamage/Mind/element scaling runs
    // downstream at ApplyHit (ActionType=Spell branch). So per-entry damage scales as a spell.
    const int32 EffectiveBase = (BaseDamageOverride >= 0) ? BaseDamageOverride : BaseDamage;

    // Attacker-side base only. SpellDamage multiplier is applied once downstream
    // by DamageCalculator::CalculateDamage via GetAttackerDamageMultiplier; the
    // ActionMods.SpellDamage modifier is applied there too. StatusMultiplier is
    // no longer multiplied into damage — it drives status buildup exclusively
    // (StatusBuildupManager::AddStatusBuildup + CalculateStatusBuildup).
    float FinalDamage = EffectiveBase;

    if (bIsRawMode)
    {
        FinalDamage *= CombatConstants::RAW_MODE_DAMAGE_MULTIPLIER;
    }

    const float RequirementPenalty = CalculateRequirementPenalty(Character);
    FinalDamage *= (1.0f - RequirementPenalty);

    return FMath::RoundToInt(FinalDamage);
}

int32 USpellData::CalculateStatusBuildup(UCharacterData *Character, const FActionStatModifiers &ActionMods) const
{
    if (!Character)
        return 0;

    // Raw mode: no status buildup
    if (bIsRawMode)
        return 0;

    // Buffs/heals don't build status
    if (School == ESpellSchool::Enhancement || School == ESpellSchool::Restoration)
        return 0;

    // Base buildup per hit
    float BuildupPerHit = StatusBuildup;

    // Scale with character's StatusMultiplier (Spirit-driven post pillar move).
    // ActionMods.StatusMultiplier stacks per-action stat buffs onto the multiplier.
    float Multiplier = Character->CalculateStatusMultiplier();
    Multiplier = ActionMods.ApplyTo(Multiplier, ESubStat::StatusMultiplier);
    BuildupPerHit *= Multiplier;

    // Multiply by hit count
    float TotalBuildup = BuildupPerHit * HitCount;

    return FMath::RoundToInt(TotalBuildup);
}

// ==================== ENERGY CALCULATIONS ====================

int32 USpellData::CalculateEnergyCost(UCharacterData *Character) const
{
    if (!Character)
        return BaseEnergyCost;

    float Cost = BaseEnergyCost;

    // Requirement penalty increases cost
    float RequirementPenalty = CalculateRequirementPenalty(Character);
    Cost *= (1.0f + RequirementPenalty);

    return FMath::RoundToInt(Cost);
}

// ==================== HELPER FUNCTIONS ====================

bool USpellData::CanCharacterCast(UCharacterData *Character) const
{
    if (!Character)
        return false;

    // The element-match gate (formerly here, Caster-only) now lives in
    // UActionExecutor::ValidateAction where the active infusion source is
    // available — Reality source bypasses the gate per locked design.
    // This function now answers "is this character of a class that can cast
    // spells at all" — true for all current classes (Generic / Caster / Resonator).
    return true;
}

FString USpellData::GetDisplayName(UCharacterData *Caster) const
{
    return Name;
}

// ==================== DEFENSE HELPERS ====================

bool USpellData::CanBeBlocked() const
{
    return DeliveryType != ESpellDeliveryType::Instant;
}

bool USpellData::CanBeParried() const
{
    return DeliveryType == ESpellDeliveryType::Projectile ||
           DeliveryType == ESpellDeliveryType::Homing;
}

bool USpellData::CanBeDodgedByMoving() const
{
    return DeliveryType == ESpellDeliveryType::Projectile;
}

bool USpellData::CanBeDodgedByTiming() const
{
    return DeliveryType == ESpellDeliveryType::Projectile ||
           DeliveryType == ESpellDeliveryType::Homing ||
           DeliveryType == ESpellDeliveryType::Beam;
}

TArray<EDefenseType> USpellData::GetAvailableDefenses() const
{
    TArray<EDefenseType> Options;

    if (DeliveryType == ESpellDeliveryType::Instant)
    {
        return Options; // Empty - unavoidable
    }

    Options.Add(EDefenseType::Block);

    if (CanBeParried())
    {
        Options.Add(EDefenseType::Parry);
    }

    if (CanBeDodgedByMoving() || CanBeDodgedByTiming())
    {
        Options.Add(EDefenseType::Dodge);
    }

    return Options;
}

// ==================== EDITOR VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult USpellData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Validate status buildup (only relevant for elemental mode; sign handled by ClampMin on base)
    if (!bIsRawMode && StatusBuildup < 0)
    {
        Context.AddError(FText::FromString(TEXT("Status Buildup cannot be negative")));
        Result = EDataValidationResult::Invalid;
    }

    // Validate construct
    if (bIsConstruct)
    {
        if (ConstructedWeapon == nullptr)
        {
            Context.AddError(FText::FromString(TEXT("Construct spell must have a weapon assigned")));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif
