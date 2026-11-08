// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/Skills/SpellDataDebug.h"
#include "AI/AIDecisionManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Skills/Definitions/SupportSkillRules.h"

void USpellDataDebug::PrintSpellStats(USpellData *Spell, UCharacterData *Character, float Duration)
{
    if (!Spell || !Character)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Spell or Character is NULL"));
        }
        return;
    }

    FString StatsString = GetSpellStatsString(Spell, Character);

    if (GEngine)
    {
        TArray<FString> Lines;
        StatsString.ParseIntoArray(Lines, TEXT("\n"));

        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Magenta, Lines[i]);
        }
    }
}

void USpellDataDebug::LogSpellStats(USpellData *Spell, UCharacterData *Character)
{
    if (!Spell || !Character)
    {
        UE_LOG(LogTemp, Error, TEXT("ERROR: Spell or Character is NULL"));
        return;
    }

    FString StatsString = GetSpellStatsString(Spell, Character);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *StatsString);
}

FString USpellDataDebug::GetSpellStatsString(USpellData *Spell, UCharacterData *Character)
{
    if (!Spell || !Character)
    {
        return TEXT("ERROR: Invalid Spell or Character Data");
    }

    FString Output = TEXT("");
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("SPELL: %s\n"), *Spell->GetDisplayName(Character));
    Output += FString::Printf(TEXT("CHARACTER: %s\n"), *Character->Name);
    Output += TEXT("===================================\n\n");

    // Element & School
    // Element & School
    FString ElementName = UEnum::GetValueAsString(Spell->Element);
    ElementName.RemoveFromStart(TEXT("ESpellElement::"));

    Output += TEXT("SPELL TYPE:\n");
    Output += FString::Printf(TEXT("  Element: %s\n"), *ElementName);
    Output += FString::Printf(TEXT("  School:  %s\n"), *GetSchoolString(Spell)); // B-356b: the shown school

    // Check if character can cast
    // NOTE: As of Reality element unlock, CanCharacterCast always returns
    // true for valid characters. This branch is effectively unreachable —
    // kept for historical reference / future per-spell gates.
    bool bCanCast = Spell->CanCharacterCast(Character);
    if (!bCanCast)
    {
        Output += TEXT("⚠️  CANNOT CAST (legacy check unreachable)\n");
        Output += TEXT("===================================\n");
        return Output;
    }

    // Requirements
    Output += TEXT("REQUIREMENTS:\n");
    if (Spell->Requirements.HasRequirements())
    {
        Output += Spell->Requirements.GetRequirementsSummary(Character);
    }
    else
    {
        Output += TEXT("  None\n");
    }

    Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character));
    Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character));

    // D8: 0 = fires this turn; N = arms now, fires N turns ahead (8b/8c mechanism).
    if (Spell->ActivationDelay > 0)
    {
        Output += FString::Printf(TEXT("  Activation Delay: %d turn(s)\n"), Spell->ActivationDelay);
    }

    if (SupportSkillRules::IsSupportSkill(Spell))
    {
        Output += TEXT("  Support skill: builds no status\n");
    }
    int32 Buildup = Spell->CalculateStatusBuildup(Character);
    if (Buildup > 0)
    {
        Output += FString::Printf(TEXT("  Status Buildup: %d\n"), Buildup);
        Output += FString::Printf(TEXT("  Triggers Status: %s\n\n"),
                                  Buildup >= CombatConstants::STATUS_EFFECT_THRESHOLD ? TEXT("YES!") : TEXT("No"));
    }
    else
    {
        Output += TEXT("  Status Buildup: None\n\n");
    }

    // Effects
    const TArray<FSkillEffect> All = Spell->GetAllEffects();
    if (All.Num() > 0)
    {
        Output += TEXT("EFFECTS:\n");
        for (int32 i = 0; i < All.Num(); ++i)
        {
            const FSkillEffect &Effect = All[i];
            Output += FString::Printf(TEXT("  [%d] %s\n"), i + 1, *Effect.GetDescription());
        }
        Output += TEXT("\n");
    }
    else
    {
        Output += TEXT("EFFECTS: None\n\n");
    }

    Output += TEXT("===================================\n");

    return Output;
}

void USpellDataDebug::CompareSpellEffectiveness(USpellData *Spell, UCharacterData *Character1, UCharacterData *Character2)
{
    if (!Spell || !Character1 || !Character2)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("ERROR: Missing data for comparison"));
        }
        return;
    }

    FString Output = TEXT("");
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("SPELL COMPARISON: %s\n"), *Spell->Name);
    Output += TEXT("===================================\n\n");

    // Character 1
    Output += FString::Printf(TEXT("%s:\n"), *Character1->Name);
    if (Spell->CanCharacterCast(Character1))
    {
        Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character1));
        Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character1));
    }
    else
    {
        Output += TEXT("  Cannot cast (legacy check unreachable)\n");
    }
    Output += TEXT("\n");

    // Character 2
    Output += FString::Printf(TEXT("%s:\n"), *Character2->Name);
    if (Spell->CanCharacterCast(Character2))
    {
        Output += FString::Printf(TEXT("  Damage: %d\n"), Spell->CalculateDamage(Character2));
        Output += FString::Printf(TEXT("  Energy: %d\n"), Spell->CalculateEnergyCost(Character2));
    }
    else
    {
        Output += TEXT("  Cannot cast (legacy check unreachable)\n");
    }

    Output += TEXT("===================================\n");

    UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);

    if (GEngine)
    {
        TArray<FString> Lines;
        Output.ParseIntoArray(Lines, TEXT("\n"));
        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Magenta, Lines[i]);
        }
    }
}

namespace
{
    FString SchoolShortName(ESpellSchool School)
    {
        FString Name = UEnum::GetValueAsString(School);
        Name.RemoveFromStart(TEXT("ESpellSchool::"));
        return Name;
    }
}

FString USpellDataDebug::GetSchoolString(const USpellData *Spell)
{
    if (!Spell)
    {
        return TEXT("<null spell>");
    }
    const FString Shown = SchoolShortName(Spell->GetSchool());
    if (SpellSchools::IsRetired(Spell->School))
    {
        return FString::Printf(TEXT("%s (authored %s, retired)"), *Shown, *SchoolShortName(Spell->School));
    }
    return Shown;
}

namespace
{
    bool HasPayloadWhere(const USpellData &Spell, TFunctionRef<bool(ESkillEffectType)> Predicate)
    {
        for (const FSkillEffect &Effect : Spell.GetAllEffects())
        {
            for (const FSkillEffectPayload &Payload : Effect.Payloads)
            {
                if (Predicate(Payload.EffectType))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool IsCleansePayload(ESkillEffectType Type)
    {
        switch (Type)
        {
        case ESkillEffectType::Cleanse:
        case ESkillEffectType::CleanseSelf:
        case ESkillEffectType::CleanseAllies:
        case ESkillEffectType::RemoveSpeedDebuff:
        case ESkillEffectType::RemoveDamageDebuff:
        case ESkillEffectType::RemoveDefenseDebuff:
        case ESkillEffectType::StatusDecrease:
            return true;
        default:
            return false;
        }
    }

    /** Changing what the target does (D-163 Manipulation, Crown 2026-11-08): confusion (Random Skill) today; taunt,
     *  forced target, row swap and charm when they exist. */
    bool IsManipulationPayload(ESkillEffectType Type)
    {
        return Type == ESkillEffectType::RandomSkill;
    }

    /** Stopping or weakening the target (Impairment, Crown 2026-11-08): stun, apply stun, skip turn, silence. */
    bool IsImpairmentPayload(ESkillEffectType Type)
    {
        switch (Type)
        {
        case ESkillEffectType::Stun:
        case ESkillEffectType::ApplyStunToTarget:
        case ESkillEffectType::SkipTurn:
        case ESkillEffectType::Silenced:
            return true;
        default:
            return false;
        }
    }
}

bool USpellDataDebug::ProposeSchool(const USpellData *Spell, ESpellSchool &OutSchool)
{
    if (!Spell)
    {
        return false;
    }
    const bool bAimsAtEnemies = Spell->TargetType == ETargetType::Enemy || Spell->TargetType == ETargetType::Anyone;
    if (bAimsAtEnemies && Spell->BaseDamage > 0)
    {
        OutSchool = ESpellSchool::Destruction; // a drain too (Q-356c: a drain is an attack)
        return true;
    }
    if (UAIDecisionManager::IsHealPick(*Spell) || HasPayloadWhere(*Spell, [](ESkillEffectType Type) { return IsCleansePayload(Type); }))
    {
        OutSchool = ESpellSchool::Restoration;
        return true;
    }
    if (bAimsAtEnemies && HasPayloadWhere(*Spell, [](ESkillEffectType Type) { return IsManipulationPayload(Type); }))
    {
        OutSchool = ESpellSchool::Manipulation;
        return true;
    }
    if (bAimsAtEnemies && (HasPayloadWhere(*Spell, [](ESkillEffectType Type) { return IsImpairmentPayload(Type); }) || Spell->HasDebuffEffects() ||
                           SupportSkillRules::GetAuthoredStatusBuildup(Spell) > 0))
    {
        OutSchool = ESpellSchool::Impairment;
        return true;
    }
    if (Spell->HasBuffEffects())
    {
        OutSchool = ESpellSchool::Enhancement;
        return true;
    }
    return false;
}

bool USpellDataDebug::IsSchoolMismatch(const USpellData *Spell)
{
    ESpellSchool Proposed = ESpellSchool::Destruction;
    if (!Spell || Spell->GetSchool() == ESpellSchool::Conjuration || !ProposeSchool(Spell, Proposed))
    {
        return false;
    }
    return Spell->GetSchool() != Proposed;
}

FString USpellDataDebug::GetSchoolEffectMismatchString()
{
#if !WITH_EDITOR
    return TEXT("(editor only)");
#else
    IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.WaitForCompletion(); // a half-scanned registry would read as "all agree"
    TArray<FAssetData> Assets;
    Registry.GetAssetsByClass(USpellData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

    FString Out = FString::Printf(TEXT("%d spell assets scanned\n"), Assets.Num());
    for (const FAssetData &Asset : Assets)
    {
        const USpellData *Spell = Cast<USpellData>(Asset.GetAsset());
        if (!Spell)
        {
            continue;
        }
        ESpellSchool Proposed = ESpellSchool::Destruction;
        const bool bProposed = ProposeSchool(Spell, Proposed);
        // B-374 review: Conjuration was the damage school until D-163, so a Conjuration damage spell may be a
        // conjured one or an old Destruction one; it is listed as a question, never as a mismatch.
        const bool bConjuredOrDestruction = bProposed && Spell->GetSchool() == ESpellSchool::Conjuration
                                            && Proposed == ESpellSchool::Destruction;
        if (!IsSchoolMismatch(Spell) && !bConjuredOrDestruction)
        {
            continue;
        }
        Out += FString::Printf(TEXT("%s: %s, %s %s (target %s, damage %d, buildup %d, debuff %s, heal %s)\n"),
                               *Asset.GetObjectPathString(), *GetSchoolString(Spell),
                               bConjuredOrDestruction ? TEXT("check: conjured, or") : TEXT("effects propose"), *SchoolShortName(Proposed),
                               *UEnum::GetValueAsString(Spell->TargetType), Spell->BaseDamage, Spell->StatusBuildup,
                               Spell->HasDebuffEffects() ? TEXT("yes") : TEXT("no"),
                               UAIDecisionManager::IsHealingSpell(*Spell) ? TEXT("yes") : TEXT("no"));
    }
    return Out;
#endif
}

