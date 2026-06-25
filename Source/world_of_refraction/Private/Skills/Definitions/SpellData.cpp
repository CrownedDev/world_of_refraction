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
            HitboxRatio != Defaults->HitboxRatio;

        if (bHasDeliveryAuthoring)
        {
            FSkillCastEntry &Entry = CastArray.AddDefaulted_GetRef();
            Entry.Label = TEXT("Migrated");
            Entry.DeliveryType = DeliveryType;
            Entry.ProjectileSpeed = ProjectileSpeed;
            Entry.Size = BaseSize * HitboxRatio;
            Entry.VisualScale = BaseSize;
            Entry.Trail = SpellVFX;
            // ProjectileClass stays null (executor's DefaultProjectileClass,
            // as today); Count/BurstInterval stay defaults (single delivery).
        }
    }
}

// ==================== DAMAGE CALCULATIONS ====================

int32 USpellData::CalculateDamage(UCharacterData *Character, const FActionStatModifiers &ActionMods) const
{
    if (!Character)
        return 0;

    // Attacker-side base: the skill-level BaseDamage. The SpellDamage/Mind/element scaling runs
    // downstream at ApplyHit (ActionType=Spell). Requirement proficiency now flows through
    // ComputeActionStatModifiers (per-pillar), not a flat penalty here.
    const int32 EffectiveBase = BaseDamage;

    // Attacker-side base only. SpellDamage multiplier is applied once downstream
    // by DamageCalculator::CalculateDamage via GetAttackerDamageMultiplier; the
    // ActionMods.SpellDamage modifier is applied there too. StatusMultiplier is
    // no longer multiplied into damage — it drives status buildup exclusively
    // (StatusBuildupManager::AddStatusBuildup + CalculateStatusBuildup).
    float FinalDamage = EffectiveBase;

    return FMath::RoundToInt(FinalDamage);
}

int32 USpellData::CalculateStatusBuildup(UCharacterData *Character, const FActionStatModifiers &ActionMods) const
{
    if (!Character)
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
    // Thin fallback for callers without resolve context (e.g. SpellDataDebug) — raw Name.
    return Name;
}

FString USpellData::GetDisplayName(ESpellElement ResolvedElement, bool bIsBrokenDarkness) const
{
    // Only Generic spells take a resolved-element prefix; fixed-element spells keep
    // their authored Name ("Fireball" stays "Fireball").
    if (Element != ESpellElement::Generic)
    {
        return Name;
    }

    // Unresolvable (None) or still-Generic (the cast-boundary safety net) — no prefix.
    if (ResolvedElement == ESpellElement::None || ResolvedElement == ESpellElement::Generic)
    {
        return Name;
    }

    const UEnum *ElementEnum = StaticEnum<ESpellElement>();
    const FString ElementName = ElementEnum
                                    ? ElementEnum->GetDisplayNameTextByValue(static_cast<int64>(ResolvedElement)).ToString()
                                    : FString();
    if (ElementName.IsEmpty())
    {
        return Name;
    }

    // Reality is its own identity — "Reality Ball", never "Dark Reality".
    if (ResolvedElement == ESpellElement::Reality)
    {
        return FString::Printf(TEXT("Reality %s"), *Name);
    }

    if (bIsBrokenDarkness)
    {
        // BD: "Dark [Element] [Name]" — but Darkness never doubles ("Darkness Ball").
        if (ResolvedElement == ESpellElement::Darkness)
        {
            return FString::Printf(TEXT("Darkness %s"), *Name);
        }
        return FString::Printf(TEXT("Dark %s %s"), *ElementName, *Name);
    }

    // Non-BD: "[Element] [Name]" ("Fire Ball").
    return FString::Printf(TEXT("%s %s"), *ElementName, *Name);
}

// ==================== DEFENSE HELPERS ====================

bool USpellData::CanBeBlocked() const
{
    // All current deliveries are blockable. Instant is defendable per-impact (Hard
    // difficulty) like AOE — no delivery is undefendable anymore.
    return true;
}

bool USpellData::CanBeParried() const
{
    return DeliveryType == ESpellDeliveryType::Projectile ||
           DeliveryType == ESpellDeliveryType::AOE ||
           DeliveryType == ESpellDeliveryType::Instant;
}

bool USpellData::CanBeDodgedByMoving() const
{
    return DeliveryType == ESpellDeliveryType::Projectile;
}

bool USpellData::CanBeDodgedByTiming() const
{
    return DeliveryType == ESpellDeliveryType::Projectile ||
           DeliveryType == ESpellDeliveryType::Instant;
}

TArray<EDefenseType> USpellData::GetAvailableDefenses() const
{
    TArray<EDefenseType> Options;

    // Every delivery is blockable now (Instant included — defendable per-impact at Hard).
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

    // Validate status buildup (sign handled by ClampMin on base)
    if (StatusBuildup < 0)
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
