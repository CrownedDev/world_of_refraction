// ScalingDebug.cpp

#include "Combat/Damage/ScalingDebug.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Actions/EActionType.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/CombatOrchestrator.h"
#include "Loadout/LoadoutComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

namespace
{
    // Build the isolated snapshot input: feed the skill's raw BaseDamage and neutralize the
    // defender-side terms (defense / resistance / crit) so FinalDamage reflects ONLY the
    // attacker-side scaling chain — the exact Step-1 term stage b2 edits. Element is forced
    // None so no weakness/resistance multiplier perturbs the number. ActionType is resolved
    // from the leaf type (spell -> SpellDamage stat, else ability -> RawDamage stat), matching
    // DamageCalculator's stat selection.
    FDamageCalculationInput BuildSnapshotInput(const USkillDataBase *Skill)
    {
        FDamageCalculationInput Input;
        Input.BaseDamage = Skill->BaseDamage;
        Input.ActionType = Cast<USpellData>(Skill) ? EActionType::Spell : EActionType::Ability;
        Input.Element = ESpellElement::None;
        Input.bCanCrit = false;
        Input.bIgnoreDefense = true;
        Input.bIgnoreResistance = true;
        Input.HitCount = Skill->HitCount;
        // Stage b2: feed the skill's authored scaling tiers so the snapshot reflects the tier term
        // (empty array → no change → matches the pre-b2 baseline; the byte-identical regression proof).
        Input.StatScaling = Skill->StatScaling;
        return Input;
    }

    UDamageCalculator *ResolveDamageCalculator(const AActor *Actor)
    {
        if (!Actor || !Actor->GetWorld() || !Actor->GetWorld()->GetGameInstance())
        {
            return nullptr;
        }
        return Actor->GetWorld()->GetGameInstance()->GetSubsystem<UDamageCalculator>();
    }
}

void UScalingDebug::CompareScalingSnapshot(AActor *Attacker, AActor *Defender, const USkillDataBase *Skill)
{
    if (!Attacker || !Defender || !Skill)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] CompareScalingSnapshot: null attacker/defender/skill"));
        return;
    }

    UDamageCalculator *DamageCalc = ResolveDamageCalculator(Attacker);
    if (!DamageCalc)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] CompareScalingSnapshot: no DamageCalculator subsystem"));
        return;
    }

    const FDamageCalculationInput Input = BuildSnapshotInput(Skill);
    const FDamageCalculationResult Result = DamageCalc->CalculateDamage(Attacker, Defender, Input);

    UE_LOG(LogTemp, Display, TEXT("=== ScalingSnapshot: %s (%s) ==="),
           *Skill->Name,
           Input.ActionType == EActionType::Spell ? TEXT("Spell/SpellDamage") : TEXT("Ability/RawDamage"));
    UE_LOG(LogTemp, Display, TEXT("  StatScaling entries: %d (empty = no tier scaling)"), Skill->StatScaling.Num());
    UE_LOG(LogTemp, Display, TEXT("  BaseDamage in: %d"), Input.BaseDamage);
    UE_LOG(LogTemp, Display, TEXT("  AttackerMultiplier: %.4fx"), Result.AttackerDamageMultiplier);
    UE_LOG(LogTemp, Display, TEXT("  FINAL DAMAGE: %d"), Result.FinalDamage);
    UE_LOG(LogTemp, Display, TEXT("==============================="));
}

FString UScalingDebug::GetScalingSnapshotString(AActor *Attacker, AActor *Defender, const USkillDataBase *Skill)
{
    if (!Attacker || !Defender || !Skill)
    {
        return TEXT("(null attacker/defender/skill)");
    }

    UDamageCalculator *DamageCalc = ResolveDamageCalculator(Attacker);
    if (!DamageCalc)
    {
        return TEXT("(no DamageCalculator subsystem)");
    }

    const FDamageCalculationInput Input = BuildSnapshotInput(Skill);
    const FDamageCalculationResult Result = DamageCalc->CalculateDamage(Attacker, Defender, Input);

    return FString::Printf(TEXT("%s [%s] base %d x %.4f = %d (scaling entries %d)"),
                           *Skill->Name,
                           Input.ActionType == EActionType::Spell ? TEXT("Spell") : TEXT("Ability"),
                           Input.BaseDamage,
                           Result.AttackerDamageMultiplier,
                           Result.FinalDamage,
                           Skill->StatScaling.Num());
}

// ========================================
// CONSOLE COMMAND — frictionless baseline capture
// ========================================

namespace
{
    // Auto-resolve the active combat's attacker (current-turn actor) + defender (first living enemy)
    // and snapshot every available ability the attacker has. No args / no actor refs by hand — run it
    // mid-PIE-combat. Reads the CURRENT formula (b1, no tier term), so it captures today's baseline;
    // re-run after stage b2 with empty StatScaling arrays and the numbers must match.
    void RunScalingSnapshotCommand(UWorld *World)
    {
        if (!World)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] WoR.ScalingSnapshot: no world"));
            return;
        }

        ACombatOrchestrator *Orchestrator =
            Cast<ACombatOrchestrator>(UGameplayStatics::GetActorOfClass(World, ACombatOrchestrator::StaticClass()));
        if (!Orchestrator)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] WoR.ScalingSnapshot: no CombatOrchestrator (run during a PIE combat)"));
            return;
        }

        AActor *Attacker = Orchestrator->GetCurrentActor();
        if (!Attacker)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] WoR.ScalingSnapshot: no current-turn actor"));
            return;
        }

        const TArray<AActor *> Enemies = Orchestrator->GetLivingEnemies(Attacker);
        AActor *Defender = Enemies.Num() > 0 ? Enemies[0] : nullptr;
        if (!Defender)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] WoR.ScalingSnapshot: no living enemy for %s"), *Attacker->GetName());
            return;
        }

        ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>();
        if (!Loadout)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ScalingDebug] WoR.ScalingSnapshot: %s has no LoadoutComponent"), *Attacker->GetName());
            return;
        }

        const TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
        UE_LOG(LogTemp, Display, TEXT("[ScalingDebug] WoR.ScalingSnapshot: %s vs %s — %d abilit%s"),
               *Attacker->GetName(), *Defender->GetName(), Abilities.Num(),
               Abilities.Num() == 1 ? TEXT("y") : TEXT("ies"));

        for (UAbilityData *Ability : Abilities)
        {
            if (Ability)
            {
                UScalingDebug::CompareScalingSnapshot(Attacker, Defender, Ability);
            }
        }
    }
}

static FAutoConsoleCommandWithWorld GScalingSnapshotCommand(
    TEXT("WoR.ScalingSnapshot"),
    TEXT("Snapshot the attacker-side scaling (AttackerMultiplier + FinalDamage) for the current-turn actor's ")
    TEXT("available abilities vs the first living enemy. Baseline-capture for the unified-scaling-tiers arc — ")
    TEXT("run before stage b2, re-run after with empty StatScaling to verify byte-identical."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&RunScalingSnapshotCommand));
