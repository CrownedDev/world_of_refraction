// AIDecisionManager.cpp

#include "AI/AIDecisionManager.h"
#include "AI/AIDecisionConstants.h"
#include "Combat/CombatOrchestrator.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "TimerManager.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Infusion/InfusionCostHelper.h"
#include "Infusion/InfusionConstants.h"
#include "Combat/CombatConstants.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Inventory/ItemEffectType.h"
#include "Combat/TurnManager.h"

USkillEffectManager *
UAIDecisionManager::GetSkillEffectManager() const
{
    UGameInstance *GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<USkillEffectManager>() : nullptr;
}

UActionExecutor *
UAIDecisionManager::GetActionExecutor() const
{
    UGameInstance *GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<UActionExecutor>() : nullptr;
}

void UAIDecisionManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);

    // Note: DefenseSystem may not be ready yet - will lazy-load when needed
    DefenseSystemRef = GetGameInstance()->GetSubsystem<UDefenseSystem>();

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Initialized"));
}

void UAIDecisionManager::Deinitialize()
{
    ClearCombatOrchestrator();
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Deinitialized"));
    Super::Deinitialize();
}

// ==================== COMBAT REGISTRATION ====================

void UAIDecisionManager::SetCombatOrchestrator(ACombatOrchestrator *Orchestrator)
{
    CurrentCombat = Orchestrator;
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Combat orchestrator set"));
}

void UAIDecisionManager::ClearCombatOrchestrator()
{
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ThinkingTimerHandle);
    }
    CurrentCombat = nullptr;
    PendingActor = nullptr;
    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Combat orchestrator cleared"));
}

// ==================== DECISION MAKING ====================

void UAIDecisionManager::RequestDecision(AActor *AIActor)
{
    if (!AIActor || !CurrentCombat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] RequestDecision failed - null actor or combat"));
        return;
    }

    PendingActor = AIActor;

    // Calculate thinking delay
    EAIDifficulty Difficulty = GetCurrentDifficulty();
    float Delay = CalculateThinkingDelay(Difficulty);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s thinking for %.2fs (Difficulty: %d)"),
           *AIActor->GetName(), Delay, static_cast<int32>(Difficulty));

    // Schedule decision after delay
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ThinkingTimerHandle,
            this,
            &UAIDecisionManager::ExecuteDecision,
            Delay,
            false);
    }
}

void UAIDecisionManager::ExecuteDecision()
{
    if (!PendingActor || !CurrentCombat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] ExecuteDecision - no pending actor or combat"));
        return;
    }

    // Capture locally BEFORE any submit (which may trigger next turn synchronously)
    AActor *Actor = PendingActor;
    PendingActor = nullptr; // Clear BEFORE submit

    // Validate turn ownership - turn may have advanced while we were thinking
    AActor *CurrentTurnActor = CurrentCombat->GetCurrentActor();
    if (Actor != CurrentTurnActor)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[AIDecisionManager] %s decision dropped - turn moved to %s while thinking"),
               *Actor->GetName(),
               CurrentTurnActor ? *CurrentTurnActor->GetName() : TEXT("none"));
        return;
    }

    FAction Action = BuildAction(Actor);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s executing action type %d"),
           *Actor->GetName(), static_cast<int32>(Action.ActionType));

    // Submit through orchestrator (may advance turn synchronously)
    CurrentCombat->SubmitAction(Action);
}

// ==================== QUERY ====================

EAIDifficulty UAIDecisionManager::GetCurrentDifficulty() const
{
    if (CurrentCombat)
    {
        return CurrentCombat->GetCombatDifficulty();
    }
    return EAIDifficulty::Medium;
}

// ==================== DECISION LOGIC ====================

FAction UAIDecisionManager::BuildAction(AActor *AIActor)
{
    FAction Action;
    Action.ActionType = EActionType::Defend; // Safe default

    if (!AIActor)
    {
        return Action;
    }

    // Get components
    ULoadoutComponent *Loadout = AIActor->FindComponentByClass<ULoadoutComponent>();
    UCharacterDataComponent *CharComp = AIActor->FindComponentByClass<UCharacterDataComponent>();

    if (!Loadout || !CharComp || !CharComp->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] %s missing Loadout or CharacterData"),
               *AIActor->GetName());
        return Action;
    }

    // Get living enemies - needed for all branches
    TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    if (Enemies.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] No living enemies - defending"));
        return Action;
    }

    // Difficulty determines decision quality
    EAIDifficulty Difficulty = GetCurrentDifficulty();

    if (Difficulty == EAIDifficulty::Easy)
    {
        // Easy: Random action, random target (old behavior)
        AActor *Target = Enemies[FMath::RandRange(0, Enemies.Num() - 1)];
        EAIActionChoice ChosenType = ChooseActionType(AIActor, Loadout);

        Action.Targets.Add(Target);

        // Populate data (simplified). Attacks fold into ActionType=Ability + SkillData (attack/ability
        // merge); EAIActionChoice keeps attack-vs-ability distinct for the random category pick above.
        switch (ChosenType)
        {
        case EAIActionChoice::Spell:
        {
            Action.ActionType = EActionType::Spell;
            TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
            if (Spells.Num() > 0)
            {
                Action.SpellData = Spells[FMath::RandRange(0, Spells.Num() - 1)];
            }
            break;
        }
        case EAIActionChoice::Ability:
        {
            Action.ActionType = EActionType::Ability;
            TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
            if (Abilities.Num() > 0)
            {
                Action.SkillData = Abilities[FMath::RandRange(0, Abilities.Num() - 1)];
            }
            break;
        }
        case EAIActionChoice::Attack:
        {
            Action.ActionType = EActionType::Ability; // attacks dispatch as Ability (SkillData + IsAttack)
            Action.SkillData = Loadout->GetCurrentAttack();
            break;
        }
        case EAIActionChoice::Defend:
        default:
            Action.ActionType = EActionType::Defend;
            break;
        }

        return Action;
    }

    // Medium/Hard/Expert: Use smart decision branches
    return BuildAction_Smart(AIActor, Loadout, CharComp);
}

EAIActionChoice UAIDecisionManager::ChooseActionType(AActor *AIActor, ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        return EAIActionChoice::Defend;
    }

    // Gather available options. Attacks stay a distinct category (EAIActionChoice) even though they
    // dispatch as EActionType::Ability — preserves the original equal-weight random distribution.
    TArray<EAIActionChoice> Options;

    if (Loadout->GetAllWeaponAttacks().Num() > 0)
    {
        Options.Add(EAIActionChoice::Attack);
    }

    if (Loadout->GetAvailableSpells().Num() > 0)
    {
        Options.Add(EAIActionChoice::Spell);
    }

    if (Loadout->GetAvailableAbilities().Num() > 0)
    {
        Options.Add(EAIActionChoice::Ability);
    }

    // Random selection
    if (Options.Num() > 0)
    {
        int32 Index = FMath::RandRange(0, Options.Num() - 1);
        return Options[Index];
    }

    return EAIActionChoice::Defend;
}

void UAIDecisionManager::GetThinkingDelayRange(EAIDifficulty Difficulty, float &OutMin, float &OutMax) const
{
    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        OutMin = AIConstants::EASY_THINK_MIN;
        OutMax = AIConstants::EASY_THINK_MAX;
        break;
    case EAIDifficulty::Medium:
        OutMin = AIConstants::MEDIUM_THINK_MIN;
        OutMax = AIConstants::MEDIUM_THINK_MAX;
        break;
    case EAIDifficulty::Hard:
        OutMin = AIConstants::HARD_THINK_MIN;
        OutMax = AIConstants::HARD_THINK_MAX;
        break;
    case EAIDifficulty::Expert:
        OutMin = AIConstants::EXPERT_THINK_MIN;
        OutMax = AIConstants::EXPERT_THINK_MAX;
        break;
    default:
        OutMin = AIConstants::MEDIUM_THINK_MIN;
        OutMax = AIConstants::MEDIUM_THINK_MAX;
        break;
    }
}

float UAIDecisionManager::CalculateThinkingDelay(EAIDifficulty Difficulty) const
{
    float Min, Max;
    GetThinkingDelayRange(Difficulty, Min, Max);
    return FMath::FRandRange(Min, Max);
}

// ==================== DEFENSE DECISIONS ====================

void UAIDecisionManager::ScheduleDefenseDecision(AActor *Defender, float AttackSize, int32 BaseDamage, float WindowDuration)
{
    if (!Defender)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] ScheduleDefenseDecision - null Defender"));
        return;
    }

    // Lazy-load DefenseSystem (may not be ready at Initialize time)
    if (!DefenseSystemRef)
    {
        DefenseSystemRef = GetGameInstance()->GetSubsystem<UDefenseSystem>();
    }

    if (!DefenseSystemRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] ScheduleDefenseDecision - could not get DefenseSystem"));
        return;
    }

    EAIDifficulty Difficulty = GetCurrentDifficulty();

    // Roll for defense attempt
    float AttemptChance = GetDefenseAttemptChance(Difficulty);
    float Roll = FMath::FRand();

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s defense roll: %.2f vs %.2f (%.0f%% chance)"),
           *Defender->GetName(), Roll, AttemptChance, AttemptChance * 100.0f);

    if (Roll > AttemptChance)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose not to defend"),
               *Defender->GetName());
        return;
    }

    // Choose defense type
    EDefenseType Choice = ChooseDefenseType(Defender, AttackSize, BaseDamage, Difficulty);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose defense type: %d"),
           *Defender->GetName(), static_cast<int32>(Choice));

    if (Choice == EDefenseType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] %s - ChooseDefenseType returned None"),
               *Defender->GetName());
        return;
    }

    // Calculate reaction delay (must be within window)
    float ReactionDelay = CalculateDefenseReactionDelay(Difficulty, WindowDuration);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s will %s in %.2fs"),
           *Defender->GetName(),
           Choice == EDefenseType::Block ? TEXT("Block") : Choice == EDefenseType::Parry ? TEXT("Parry")
                                                                                         : TEXT("Dodge"),
           ReactionDelay);

    // Schedule defense input
    FTimerHandle &TimerHandle = DefenseTimerHandles.FindOrAdd(Defender);

    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerHandle);

        FTimerDelegate TimerDelegate;
        TimerDelegate.BindLambda([this, Defender, Choice]()
                                 {
            if (!Defender || !DefenseSystemRef)
            {
                return;
            }

            // Roll for timing accuracy
            EAIDifficulty Diff = GetCurrentDifficulty();
            float Accuracy = GetDefenseAccuracy(Diff);
            bool bGoodTiming = FMath::FRand() < Accuracy;

            if (bGoodTiming)
            {
                EDefenseDirection Direction = EDefenseDirection::None;
                if (Choice == EDefenseType::Dodge)
                {
                    Direction = FMath::RandBool() ? EDefenseDirection::Left : EDefenseDirection::Right;
                }

                DefenseSystemRef->SubmitDefenseInput(Defender, Choice, Direction);
                
                UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s executed %s defense"),
                    *Defender->GetName(),
                    Choice == EDefenseType::Block ? TEXT("Block") : 
                    Choice == EDefenseType::Parry ? TEXT("Parry") : TEXT("Dodge"));
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s mistimed defense (%.0f%% accuracy)"),
                    *Defender->GetName(), Accuracy * 100.0f);
            }

            DefenseTimerHandles.Remove(Defender); });

        World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, ReactionDelay, false);
    }
}

EDefenseType UAIDecisionManager::ChooseDefenseType(AActor *Defender, float AttackSize, int32 BaseDamage, EAIDifficulty Difficulty) const
{
    // Dodge is always viable — the attack-size gate was removed (Option B). The AI now dodges on
    // timing alone, exactly like the player; ChooseDefenseType may freely pick it. Block/Parry
    // remain the fallbacks below, so the AI is never stranded without a defense.
    const bool bCanDodge = true;

    // Lethality check — a hit that would kill always warrants a dodge attempt
    // (100% avoid), regardless of difficulty. Gated on bCanDodge: an undodgeable
    // lethal hit falls through to the Block/Parry logic below.
    if (Defender && bCanDodge)
    {
        if (UCharacterDataComponent *DefComp = Defender->FindComponentByClass<UCharacterDataComponent>())
        {
            if (BaseDamage >= DefComp->CurrentHP)
            {
                return EDefenseType::Dodge;
            }
        }
    }

    // Easy: Always block (safest)
    if (Difficulty == EAIDifficulty::Easy)
    {
        return EDefenseType::Block;
    }

    // Medium: Block or Dodge (no parry - too risky)
    if (Difficulty == EAIDifficulty::Medium)
    {
        if (bCanDodge && FMath::RandBool())
        {
            return EDefenseType::Dodge;
        }
        return EDefenseType::Block;
    }

    // Hard/Expert: Smart choice
    // Prioritize: Dodge (100% avoid) > Parry (70% + reflect) > Block (50%)
    if (bCanDodge)
    {
        return EDefenseType::Dodge;
    }

    // Parry vs Block: Higher difficulty = more parry attempts
    float ParryChance = (Difficulty == EAIDifficulty::Expert) ? AIConstants::EXPERT_PARRY_CHANCE : AIConstants::HARD_PARRY_CHANCE;
    if (FMath::FRand() < ParryChance)
    {
        return EDefenseType::Parry;
    }

    return EDefenseType::Block;
}

float UAIDecisionManager::GetDefenseAttemptChance(EAIDifficulty Difficulty) const
{
    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        return AIConstants::EASY_DEFENSE_ATTEMPT;
    case EAIDifficulty::Medium:
        return AIConstants::MEDIUM_DEFENSE_ATTEMPT;
    case EAIDifficulty::Hard:
        return AIConstants::HARD_DEFENSE_ATTEMPT;
    case EAIDifficulty::Expert:
        return AIConstants::EXPERT_DEFENSE_ATTEMPT;
    default:
        return AIConstants::MEDIUM_DEFENSE_ATTEMPT;
    }
}

float UAIDecisionManager::GetDefenseAccuracy(EAIDifficulty Difficulty) const
{
    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        return AIConstants::EASY_DEFENSE_ACCURACY;
    case EAIDifficulty::Medium:
        return AIConstants::MEDIUM_DEFENSE_ACCURACY;
    case EAIDifficulty::Hard:
        return AIConstants::HARD_DEFENSE_ACCURACY;
    case EAIDifficulty::Expert:
        return AIConstants::EXPERT_DEFENSE_ACCURACY;
    default:
        return AIConstants::MEDIUM_DEFENSE_ACCURACY;
    }
}

float UAIDecisionManager::CalculateDefenseReactionDelay(EAIDifficulty Difficulty, float WindowDuration) const
{
    // Reaction time as fraction of window
    // Easy: Late in window (70-90%)
    // Expert: Early in window (10-30%)
    float MinFraction, MaxFraction;

    switch (Difficulty)
    {
    case EAIDifficulty::Easy:
        MinFraction = AIConstants::EASY_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::EASY_REACTION_FRACTION_MAX;
        break;
    case EAIDifficulty::Medium:
        MinFraction = AIConstants::MEDIUM_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::MEDIUM_REACTION_FRACTION_MAX;
        break;
    case EAIDifficulty::Hard:
        MinFraction = AIConstants::HARD_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::HARD_REACTION_FRACTION_MAX;
        break;
    case EAIDifficulty::Expert:
        MinFraction = AIConstants::EXPERT_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::EXPERT_REACTION_FRACTION_MAX;
        break;
    default:
        MinFraction = AIConstants::MEDIUM_REACTION_FRACTION_MIN;
        MaxFraction = AIConstants::MEDIUM_REACTION_FRACTION_MAX;
    }

    float Fraction = FMath::FRandRange(MinFraction, MaxFraction);
    return WindowDuration * Fraction;
}

// ==================== PER-IMPACT DEFENSE SYNTHESIS (Cluster B) ====================

double UAIDecisionManager::CalculateDefenseDelta(EAIDifficulty AIDiff, EDefenseType ChosenType,
                                                 const FDefenseDifficultyTriple &ImpactDifficulty,
                                                 AActor *Defender, AActor *Attacker, EActionType AttackType) const
{
    if (!DefenseSystemRef)
    {
        return 0.0;
    }

    // Band tier from the IMPACT, keyed on the chosen press type — mirrors MatchAndConsumeInput's
    // TypeTier (DefenseSystem.cpp:328-338) EXACTLY: the tier is passed STRAIGHT to
    // DefenseDifficultyMultiplier with no ResolveInheritedDifficulty, so an Inherit tier resolves
    // to Easy (x1.0) via that function's terminal fallback (DefenseDifficulty.h:70-73). Doing the
    // identical thing here guarantees the AI aims at the SAME band the matcher judges against.
    const EDefenseDifficulty BandTier =
        (ChosenType == EDefenseType::Parry) ? ImpactDifficulty.Parry :
        (ChosenType == EDefenseType::Dodge) ? ImpactDifficulty.Dodge :
        (ChosenType == EDefenseType::Block) ? ImpactDifficulty.Block :
                                              EDefenseDifficulty::Easy;

    const float Mult = DefenseDifficultyMultiplier(BandTier);
    const float Window = DefenseSystemRef->GetEffectiveDefenseInputWindow(Defender, Attacker, AttackType);
    const float Band = DefenseSystemRef->PerfectThreshold * Mult; // SAME perfect half-band the matcher uses

    // AI skill governs only aim-within-band: lower tier aims further from the impact.
    float BandMult;
    switch (AIDiff)
    {
    case EAIDifficulty::Easy:
        BandMult = AIConstants::EASY_DELTA_BAND_MULT;
        break;
    case EAIDifficulty::Medium:
        BandMult = AIConstants::MEDIUM_DELTA_BAND_MULT;
        break;
    case EAIDifficulty::Hard:
        BandMult = AIConstants::HARD_DELTA_BAND_MULT;
        break;
    case EAIDifficulty::Expert:
        BandMult = AIConstants::EXPERT_DELTA_BAND_MULT;
        break;
    default:
        BandMult = AIConstants::MEDIUM_DELTA_BAND_MULT;
        break;
    }

    double Delta = static_cast<double>(Band) * BandMult; // base aim offset from impact
    Delta += Delta * FMath::FRandRange(-AIConstants::DEFENSE_DELTA_JITTER_FRAC, AIConstants::DEFENSE_DELTA_JITTER_FRAC);

    // Clamp: never negative (no overshoot past impact -> no "too late" entry that could bleed into
    // the next impact), never beyond the valid lead-in (Window x Mult, the matcher's BeforeWindow)
    // where it would be a guaranteed whiff with no chance.
    Delta = FMath::Clamp(Delta, 0.0, static_cast<double>(Window * Mult));
    return Delta; // seconds BEFORE impact
}

void UAIDecisionManager::TrySynthesizeImpactDefense(AActor *Defender, AActor *Attacker, EActionType AttackType,
                                                    int32 BaseDamage, float AttackSize,
                                                    const FDefenseDifficultyTriple &ImpactDifficulty, double ImpactTime)
{
    if (!Defender)
    {
        return;
    }

    // Lazy-load DefenseSystem (mirrors ScheduleDefenseDecision).
    if (!DefenseSystemRef)
    {
        DefenseSystemRef = GetGameInstance()->GetSubsystem<UDefenseSystem>();
    }
    if (!DefenseSystemRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIDecisionManager] TrySynthesizeImpactDefense - no DefenseSystem"));
        return;
    }

    const EAIDifficulty Diff = GetCurrentDifficulty();

    // Per-impact attempt roll — each impact is independently defended or eaten.
    if (FMath::FRand() > GetDefenseAttemptChance(Diff))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[AIDecisionManager] %s eats impact (attempt roll failed)"),
               *Defender->GetName());
        return;
    }

    const EDefenseType Choice = ChooseDefenseType(Defender, AttackSize, BaseDamage, Diff);
    if (Choice == EDefenseType::None)
    {
        return;
    }

    EDefenseDirection Dir = EDefenseDirection::None;
    if (Choice == EDefenseType::Dodge)
    {
        Dir = FMath::RandBool() ? EDefenseDirection::Left : EDefenseDirection::Right;
    }

    const double Delta = CalculateDefenseDelta(Diff, Choice, ImpactDifficulty, Defender, Attacker, AttackType);
    const double InputTime = ImpactTime - Delta;

    DefenseSystemRef->SubmitDefenseInput(Defender, Choice, Dir, InputTime);

    UE_LOG(LogTemp, Log,
           TEXT("[AIDecisionManager] %s synthesized defense type %d (tier %d, Delta %.3fs, InputTime %.3f, ImpactTime %.3f)"),
           *Defender->GetName(), static_cast<int32>(Choice), static_cast<int32>(Diff), Delta, InputTime, ImpactTime);
}

// ==================== TARGET SCORING ====================

int32 UAIDecisionManager::ScoreTarget(AActor *Attacker, AActor *Target, int32 EstimatedDamage)
{
    if (!Target)
    {
        return 0;
    }

    int32 Score = 0;
    int32 TargetHP = GetCurrentHP(Target);
    int32 TargetMaxHP = GetMaxHP(Target);

    // Kill potential - highest priority
    if (CanKillTarget(Attacker, Target, EstimatedDamage))
    {
        Score += AIConstants::KILL_POTENTIAL_SCORE;

        UE_LOG(LogTemp, Verbose, TEXT("[AIDecisionManager] %s can kill %s (+%d)"),
               *Attacker->GetName(), *Target->GetName(), AIConstants::KILL_POTENTIAL_SCORE);
    }

    // Low HP bonus - prioritize wounded targets
    float HPPercent = GetHPPercent(Target);
    int32 HPMissingScore = FMath::RoundToInt((1.0f - HPPercent) * AIConstants::HP_MISSING_WEIGHT);
    Score += HPMissingScore;

    // Threat assessment - prioritize dangerous enemies
    int32 ThreatScore = FMath::RoundToInt(CalculateThreatLevel(Target) * AIConstants::THREAT_WEIGHT);
    Score += ThreatScore;

    UE_LOG(LogTemp, Verbose, TEXT("[AIDecisionManager] Score for %s: %d (Kill:%s, HP:%.0f%%, Threat:%d)"),
           *Target->GetName(), Score,
           CanKillTarget(Attacker, Target, EstimatedDamage) ? TEXT("Yes") : TEXT("No"),
           HPPercent * 100.0f, ThreatScore);

    return Score;
}

AActor *UAIDecisionManager::SelectBestTarget(AActor *Attacker, const TArray<AActor *> &Enemies)
{
    if (Enemies.Num() == 0)
    {
        return nullptr;
    }

    // Easy difficulty: random target
    EAIDifficulty Difficulty = GetCurrentDifficulty();
    if (Difficulty == EAIDifficulty::Easy)
    {
        int32 RandomIndex = FMath::RandRange(0, Enemies.Num() - 1);
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Easy difficulty - random target: %s"),
               *Enemies[RandomIndex]->GetName());
        return Enemies[RandomIndex];
    }

    // Medium+ difficulty: score and pick best
    AActor *BestTarget = nullptr;
    int32 BestScore = -1;

    for (AActor *Enemy : Enemies)
    {
        int32 EstimatedDamage = EstimateBestDamage(Attacker, Enemy);
        int32 Score = ScoreTarget(Attacker, Enemy, EstimatedDamage);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestTarget = Enemy;
        }
    }

    if (BestTarget)
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] Best target: %s (Score: %d)"),
               *BestTarget->GetName(), BestScore);
    }

    return BestTarget ? BestTarget : Enemies[0];
}

int32 UAIDecisionManager::EstimateBestDamage(AActor *Attacker, AActor *Target)
{
    // Get components
    ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>();
    UCharacterDataComponent *CharComp = Attacker->FindComponentByClass<UCharacterDataComponent>();

    if (!Loadout || !CharComp || !CharComp->CharacterData)
    {
        return 50; // Default estimate
    }

    int32 BestDamage = 0;

    // Check spells
    TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
    for (USpellData *Spell : Spells)
    {
        if (Spell)
        {
            int32 SpellDamage = EstimateSpellDamage(Attacker, Target, Spell);
            BestDamage = FMath::Max(BestDamage, SpellDamage);
        }
    }

    // Check abilities
    TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
    for (UAbilityData *Ability : Abilities)
    {
        if (Ability)
        {
            int32 AbilityDamage = EstimateAbilityDamage(Attacker, Target, Ability);
            BestDamage = FMath::Max(BestDamage, AbilityDamage);
        }
    }

    // Check weapon attack
    USkillDataBase *Attack = Loadout->GetCurrentAttack();
    if (Attack)
    {
        UDamageCalculator *DamageCalc = GetGameInstance()->GetSubsystem<UDamageCalculator>();
        if (DamageCalc)
        {
            FDamageCalculationResult DamageResult = DamageCalc->CalculateAttackDamage(Attacker, Target, Attack, false);
            int32 AttackDamage = DamageResult.FinalDamage;
            BestDamage = FMath::Max(BestDamage, AttackDamage);
        }
        else
        {
            // Fallback if DamageCalculator unavailable
            BestDamage = FMath::Max(BestDamage, 50);
        }
    }

    return BestDamage > 0 ? BestDamage : 50;
}

int32 UAIDecisionManager::EstimateSpellDamage(AActor *Attacker, AActor *Target, USpellData *Spell, int32 InfusionLevel, EInfusionSourceOption InfusionSource) const
{
    if (!Attacker || !Target || !Spell)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    UDamageCalculator *DamageCalc = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDamageCalculator>() : nullptr;
    UActionExecutor *ActionExec = GetActionExecutor();

    // Fall back to the raw asset value if the subsystems are unavailable.
    if (!DamageCalc || !ActionExec)
    {
        return Spell->CalculateDamage(AttackerComp->CharacterData);
    }

    // Build a complete action so ComputeActionStatModifiers can walk the
    // Reality/Evolution sources for this specific spell + infusion level.
    // Source is resolved off the attacker's active loadout so the modifier walk
    // sees the real provenance (ring/weapon/evolution casts compute differently
    // from innate). Falls back to Innate if no loadout is reachable.
    ULoadoutComponent *AttackerLoadout = Attacker->FindComponentByClass<ULoadoutComponent>();
    FAction Action;
    Action.ActionType = EActionType::Spell;
    Action.SpellData = Spell;
    Action.SpellSource = AttackerLoadout ? AttackerLoadout->ResolveSpellSource(Spell) : ESpellSource::Innate;
    Action.SpellInfusionLevel = InfusionLevel;
    Action.Targets.Add(Target);

    const FActionStatModifiers ActionMods = ActionExec->ComputeActionStatModifiers(Action, Attacker);

    // BaseDamage is the attacker-side base only; DamageCalculator applies the
    // SpellDamage stat, ActionMods and defender defense once, downstream.
    FDamageCalculationInput Input;
    Input.BaseDamage = Spell->CalculateDamage(AttackerComp->CharacterData);
    Input.ActionType = EActionType::Spell;
    Input.Element = Spell->Element;
    Input.ActionMods = ActionMods;
    Input.bCanCrit = false;
    // Parity with the live path (ApplyHit): feed the skill's authored scaling tiers so the AI's predicted
    // damage includes the StatScaling bonus the live damage applies. Empty array → +0 (byte-identical score).
    Input.StatScaling = Spell->StatScaling;

    const FDamageCalculationResult Result = DamageCalc->CalculateDamage(Attacker, Target, Input);

    // CalculateDamage ran with bCanCrit=false — fold expected crit value back in
    // so the estimate matches average execution damage. AI parity (5e-D): value a crit by the
    // attacker's ACTUAL crit-damage multiplier (x1.0-x2.0 via GetCritDamageMultiplier), not a fixed
    // x1.5 — a low-CritDamage attacker weights crits near x1.0 (no uplift), a maxed one toward x2.0.
    const float CritChance = DamageCalc->GetCriticalChance(Attacker);
    const float CritMult = DamageCalc->GetCritDamageMultiplier(Attacker);
    float Estimate = Result.FinalDamage * (1.0f + CritChance * (CritMult - 1.0f));

    // 6-5-d: per-mode, stat-scaled charge damage — matches execution (GetChargeDamageMultiplier).
    // L0 returns x1.0; L1/L2 apply the resolved mode's value (L1 now carries a damage bonus too).
    // Mode from the (level-independent) infusion source the AI will use; None -> Balanced fallback.
    if (InfusionLevel > 0)
    {
        const EInfusionMode Mode = ActionExec->ResolveInfusionMode(InfusionSource, Attacker);
        Estimate *= ActionExec->GetChargeDamageMultiplier(InfusionLevel, Mode, /*bIsSpell*/ true, AttackerComp);
    }

    // Tier-gap parity (Cluster D): the estimate sees the same multiplier
    // execution applies at assembly (B2), resolved per-candidate from this
    // spell's own SpellSource/catalyst. Non-logging accessor — no per-candidate
    // Output Log spam.
    Estimate *= ActionExec->GetTierGapDamageMultiplier(Attacker, Action);

    return FMath::RoundToInt(Estimate);
}

int32 UAIDecisionManager::EstimateAbilityDamage(AActor *Attacker, AActor *Target, UAbilityData *Ability, int32 InfusionLevel, EInfusionSourceOption InfusionSource) const
{
    if (!Attacker || !Target || !Ability)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    UDamageCalculator *DamageCalc = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDamageCalculator>() : nullptr;
    UActionExecutor *ActionExec = GetActionExecutor();

    // Fall back to the raw asset value if the subsystems are unavailable.
    if (!DamageCalc || !ActionExec)
    {
        return Ability->CalculateDamage(AttackerComp->CharacterData, false);
    }

    FAction Action;
    Action.ActionType = EActionType::Ability;
    Action.SkillData = Ability;
    Action.AbilityInfusionLevel = InfusionLevel;
    Action.Targets.Add(Target);

    const FActionStatModifiers ActionMods = ActionExec->ComputeActionStatModifiers(Action, Attacker);

    // BaseDamage is the attacker-side base only; DamageCalculator applies the
    // RawDamage stat, ActionMods and defender defense once, downstream.
    FDamageCalculationInput Input;
    Input.BaseDamage = Ability->CalculateDamage(AttackerComp->CharacterData, false);
    Input.ActionType = EActionType::Ability;
    Input.Element = ESpellElement::Generic;
    Input.ActionMods = ActionMods;
    Input.bCanCrit = false;
    // Parity with the live path (ApplyHit): feed the skill's authored scaling tiers so the AI's predicted
    // damage includes the StatScaling bonus the live damage applies. Empty array → +0 (byte-identical score).
    Input.StatScaling = Ability->StatScaling;

    const FDamageCalculationResult Result = DamageCalc->CalculateDamage(Attacker, Target, Input);

    // CalculateDamage ran with bCanCrit=false — fold expected crit value back in
    // so the estimate matches average execution damage. AI parity (5e-D): value a crit by the
    // attacker's ACTUAL crit-damage multiplier (x1.0-x2.0 via GetCritDamageMultiplier), not a fixed
    // x1.5 — a low-CritDamage attacker weights crits near x1.0 (no uplift), a maxed one toward x2.0.
    const float CritChance = DamageCalc->GetCriticalChance(Attacker);
    const float CritMult = DamageCalc->GetCritDamageMultiplier(Attacker);
    float Estimate = Result.FinalDamage * (1.0f + CritChance * (CritMult - 1.0f));

    // 6-5-d: per-mode, stat-scaled charge damage — matches execution (GetChargeDamageMultiplier).
    // L0 returns x1.0; L1/L2 apply the resolved mode's value (L1 now carries a damage bonus too).
    if (InfusionLevel > 0)
    {
        const EInfusionMode Mode = ActionExec->ResolveInfusionMode(InfusionSource, Attacker);
        Estimate *= ActionExec->GetChargeDamageMultiplier(InfusionLevel, Mode, /*bIsSpell*/ false, AttackerComp);
    }

    // Tier-gap parity (Cluster D): no-op today — abilities borrow the active
    // weapon's tier for BOTH action and channel, so the gap is always 0. Kept so
    // the estimate self-heals if abilities ever get their own tier.
    Estimate *= ActionExec->GetTierGapDamageMultiplier(Attacker, Action);

    return FMath::RoundToInt(Estimate);
}

float UAIDecisionManager::EstimateStatusScore(AActor *Attacker, AActor *Target, USpellData *Spell)
{
    if (!Attacker || !Target || !Spell)
    {
        return 0.0f;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0.0f;
    }

    const int32 Buildup = Spell->CalculateStatusBuildup(AttackerComp->CharacterData);
    if (Buildup <= 0)
    {
        return 0.0f;
    }

    // Target already carrying a dangerous status — extra buildup is low value.
    if (HasDangerousDebuff(Target))
    {
        return AIConstants::STATUS_SCORE_REDUNDANT;
    }

    // This hit would tip the bar, or the bar is already close — high value.
    if (WouldTriggerStatusBar(Attacker, Target, static_cast<float>(Buildup)) ||
        IsStatusBarNearTrigger(Target))
    {
        return AIConstants::STATUS_SCORE_TRIGGER;
    }

    // Otherwise a modest value for contributing buildup.
    return AIConstants::STATUS_SCORE_CONTRIBUTE;
}

float UAIDecisionManager::EstimateStatusScore(AActor *Attacker, AActor *Target, UAbilityData *Ability)
{
    if (!Attacker || !Target || !Ability)
    {
        return 0.0f;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !AttackerComp->CharacterData)
    {
        return 0.0f;
    }

    const int32 Buildup = Ability->CalculateStatusBuildup(AttackerComp->CharacterData);
    if (Buildup <= 0)
    {
        return 0.0f;
    }

    if (HasDangerousDebuff(Target))
    {
        return AIConstants::STATUS_SCORE_REDUNDANT;
    }

    if (WouldTriggerStatusBar(Attacker, Target, static_cast<float>(Buildup)) ||
        IsStatusBarNearTrigger(Target))
    {
        return AIConstants::STATUS_SCORE_TRIGGER;
    }

    return AIConstants::STATUS_SCORE_CONTRIBUTE;
}

bool UAIDecisionManager::CanAffordSpell(AActor *Actor, USpellData *Spell, int32 InfusionLevel) const
{
    if (!Actor || !Spell)
    {
        return false;
    }

    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec)
    {
        return true; // Cannot compute cost — don't block the AI.
    }

    // Source is resolved off the actor's loadout — CalculateActionEnergyCost
    // returns 0 for ring/weapon crystal sources and varies for BD evolution
    // with the Innate-conversion carve-out, so a wrong default ESpellSource
    // would mis-gate affordability.
    ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
    FAction Probe;
    Probe.ActionType = EActionType::Spell;
    Probe.SpellData = Spell;
    Probe.SpellSource = Loadout ? Loadout->ResolveSpellSource(Spell) : ESpellSource::Innate;
    Probe.SpellInfusionLevel = InfusionLevel;

    const int32 Cost = ActionExec->CalculateActionEnergyCost(Actor, Probe);
    return GetCurrentEP(Actor) >= Cost;
}

bool UAIDecisionManager::CanAffordAbility(AActor *Actor, UAbilityData *Ability, int32 InfusionLevel) const
{
    if (!Actor || !Ability)
    {
        return false;
    }

    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec)
    {
        return true; // Cannot compute cost — don't block the AI.
    }

    FAction Probe;
    Probe.ActionType = EActionType::Ability;
    Probe.SkillData = Ability;
    Probe.AbilityInfusionLevel = InfusionLevel;

    const int32 Cost = ActionExec->CalculateActionEnergyCost(Actor, Probe);
    return GetCurrentEP(Actor) >= Cost;
}

int32 UAIDecisionManager::CalculateThreatLevel(AActor *Actor)
{
    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->CharacterData)
    {
        return 50; // Default threat
    }

    UCharacterData *Data = CharComp->CharacterData;
    int32 Threat = 0;

    // Raw damage — FULL composed RawDamage (innate + equipment + stone + transient), capped
    // [0,2], so a geared/buffed combatant self-scores its physical threat accurately. Consistent
    // with the SpellDamage threat read below. AI heuristic, non-critical.
    Threat += FMath::RoundToInt(CharComp->GetEffectiveRawDamage() * AIConstants::RAW_DAMAGE_THREAT_MULT);

    // StatusMultiplier stat points (Spirit-side — status buildup strength).
    Threat += FMath::RoundToInt(Data->GetTotalStatusMultiplier() * AIConstants::STATUS_MULTIPLIER_THREAT_MULT);

    // Spell power — FULL composed SpellDamage (innate + equipment + stone + transient), so a
    // spell-geared/buffed combatant self-scores its threat more accurately. AI heuristic,
    // non-critical. (The RawDamage read above stays pillar — no composed RawDamage getter.)
    Threat += FMath::RoundToInt(CharComp->GetEffectiveSpellDamage() * AIConstants::SPELL_POWER_THREAT_MULT);

    return Threat;
}

bool UAIDecisionManager::CanKillTarget(AActor *Attacker, AActor *Target, int32 Damage)
{
    int32 TargetHP = GetCurrentHP(Target);
    return Damage >= TargetHP && TargetHP > 0;
}

float UAIDecisionManager::GetHPPercent(AActor *Actor)
{
    int32 Current = GetCurrentHP(Actor);
    int32 Max = GetMaxHP(Actor);

    if (Max <= 0)
        return 1.0f;
    return FMath::Clamp((float)Current / (float)Max, 0.0f, 1.0f);
}

int32 UAIDecisionManager::GetCurrentHP(AActor *Actor)
{
    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    return CharComp ? CharComp->CurrentHP : 0;
}

int32 UAIDecisionManager::GetMaxHP(AActor *Actor)
{
    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp)
    {
        return CharComp->MaxHP;
    }
    return 100;
}

// ==================== SMART DECISION BUILDING ====================

FAction UAIDecisionManager::BuildAction_Smart(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp)
{
    FAction Action;

    // Branch 1: Survival - heal or defend if low HP
    if (TrySurvivalBranch(AIActor, Loadout, Action))
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose survival action"), *AIActor->GetName());
        return Action;
    }

    // Branch 2: Cleanse - remove dangerous debuffs
    if (TryCleanseBranch(AIActor, Loadout, Action))
    {
        UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose cleanse action"), *AIActor->GetName());
        return Action;
    }

    // Branch 3: Offensive - attack best target
    Action = BuildOffensiveAction(AIActor, Loadout, CharComp);

    UE_LOG(LogTemp, Log, TEXT("[AIDecisionManager] %s chose offensive action"), *AIActor->GetName());
    return Action;
}

// ==================== SURVIVAL BRANCH ====================

bool UAIDecisionManager::TrySurvivalBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction)
{
    if (!AIActor || !Loadout)
    {
        return false;
    }

    UCharacterDataComponent *CharComp = AIActor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->CharacterData)
    {
        return false;
    }

    float HPPercent = static_cast<float>(CharComp->CurrentHP) / CharComp->MaxHP;
    int32 CurrentEnergy = GetCurrentEP(AIActor); // CurrentEP — unified BD/non-BD pool
    int32 MaxEnergy = CharComp->MaxEP;
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // HP threshold based on difficulty
    EAIDifficulty Difficulty = GetCurrentDifficulty();
    float HealThreshold = (Difficulty == EAIDifficulty::Hard || Difficulty == EAIDifficulty::Expert) ? AIConstants::HARDEXPERT_HEAL_HP_THRESHOLD : AIConstants::EASYMED_HEAL_HP_THRESHOLD;

    // Priority 1: Low HP - need healing
    if (HPPercent <= HealThreshold)
    {
        // Try healing spell first (if we have energy)
        USpellData *HealSpell = FindHealingSpell(Loadout);
        if (HealSpell)
        {
            // Energy cost via ActionExecutor so efficiency + infusion multipliers apply.
            // Source resolved once so the probe and the OutAction agree.
            const ESpellSource HealSource = Loadout->ResolveSpellSource(HealSpell);
            FAction HealProbe;
            HealProbe.ActionType = EActionType::Spell;
            HealProbe.SpellData = HealSpell;
            HealProbe.SpellSource = HealSource;
            HealProbe.Targets.Add(AIActor);

            UActionExecutor *ActionExec = GetActionExecutor();
            const int32 HealCost = ActionExec
                                       ? ActionExec->CalculateActionEnergyCost(AIActor, HealProbe)
                                       : HealSpell->CalculateEnergyCost(CharComp->CharacterData);

            if (CurrentEnergy >= HealCost)
            {
                OutAction.ActionType = EActionType::Spell;
                OutAction.SpellData = HealSpell;
                OutAction.Targets.Add(AIActor); // Self-target
                OutAction.SpellSource = HealSource;
                UE_LOG(LogTemp, Log, TEXT("[AI Survival] Using healing spell"));
                return true;
            }
        }

        // Fallback: Use healing item (Sapphire)
        bool bFoundHeal = false;
        const FCrystalId HealId = FindHealingItem(Loadout, bFoundHeal);
        if (bFoundHeal)
        {
            OutAction.ActionType = EActionType::Item;
            OutAction.ItemData = HealId;
            OutAction.Targets.Add(AIActor); // Self-target
            UE_LOG(LogTemp, Log, TEXT("[AI Survival] Using healing item (Sapphire)"));
            return true;
        }

        // Last resort: Defend
        OutAction.ActionType = EActionType::Defend;
        UE_LOG(LogTemp, Log, TEXT("[AI Survival] No healing available - defending"));
        return true;
    }

    // Priority 2: Low energy - need energy restoration
    if (EnergyPercent < AIConstants::ENERGY_CONSERVATION_THRESHOLD) // 30%
    {
        bool bFoundEnergy = false;
        const FCrystalId EnergyId = FindEnergyItem(Loadout, bFoundEnergy);
        if (bFoundEnergy)
        {
            OutAction.ActionType = EActionType::Item;
            OutAction.ItemData = EnergyId;
            OutAction.Targets.Add(AIActor); // Self-target
            UE_LOG(LogTemp, Log, TEXT("[AI Survival] Using energy item (Citrine)"));
            return true;
        }
    }

    // Not in danger
    return false;
}

// ==================== CLEANSE BRANCH ====================

bool UAIDecisionManager::TryCleanseBranch(AActor *AIActor, ULoadoutComponent *Loadout, FAction &OutAction)
{
    if (!AIActor || !Loadout)
    {
        return false;
    }

    // Check for dangerous debuffs
    if (!HasDangerousDebuff(AIActor))
    {
        return false;
    }

    UCharacterDataComponent *CharComp = AIActor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return false;
    }

    int32 CurrentEnergy = GetCurrentEP(AIActor); // CurrentEP — unified BD/non-BD pool

    // Try cleanse spell first (if we have energy)
    USpellData *CleanseSpell = FindCleanseSpell(Loadout);
    if (CleanseSpell)
    {
        // Energy cost via ActionExecutor so efficiency + infusion multipliers apply.
        // Source resolved once so probe and OutAction agree.
        const ESpellSource CleanseSource = Loadout->ResolveSpellSource(CleanseSpell);
        FAction CleanseProbe;
        CleanseProbe.ActionType = EActionType::Spell;
        CleanseProbe.SpellData = CleanseSpell;
        CleanseProbe.SpellSource = CleanseSource;
        CleanseProbe.Targets.Add(AIActor);

        UActionExecutor *ActionExec = GetActionExecutor();
        const int32 Cost = ActionExec
                               ? ActionExec->CalculateActionEnergyCost(AIActor, CleanseProbe)
                               : CleanseSpell->CalculateEnergyCost(CharComp->CharacterData);
        if (CurrentEnergy >= Cost)
        {
            OutAction.ActionType = EActionType::Spell;
            OutAction.SpellData = CleanseSpell;
            OutAction.Targets.Add(AIActor); // Self-target
            OutAction.SpellSource = CleanseSource;
            UE_LOG(LogTemp, Log, TEXT("[AI Cleanse] Using cleanse spell"));
            return true;
        }
    }

    // Fallback: Use cleanse item (Iolite)
    bool bFoundCleanse = false;
    const FCrystalId CleanseId = FindCleanseItem(Loadout, bFoundCleanse);
    if (bFoundCleanse)
    {
        OutAction.ActionType = EActionType::Item;
        OutAction.ItemData = CleanseId;
        OutAction.Targets.Add(AIActor); // Self-target
        UE_LOG(LogTemp, Log, TEXT("[AI Cleanse] Using cleanse item (Iolite)"));
        return true;
    }

    // Can't cleanse - continue to offensive
    return false;
}

// ==================== ITEM DETECTION ====================

FCrystalId UAIDecisionManager::FindHealingItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Healing)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

FCrystalId UAIDecisionManager::FindCleanseItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    // Cleanse-only (Iolite). NOTE: deliberately does NOT match StatusClear
    // (Quartz) — Quartz reduces the status *buildup bar*, it does not remove an
    // already-applied debuff, so it can't satisfy TryCleanseBranch's intent.
    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Cleanse)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

FCrystalId UAIDecisionManager::FindEnergyItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::EnergyRestore)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

// ==================== OFFENSIVE ACTION ====================

FAction UAIDecisionManager::BuildOffensiveAction(AActor *AIActor, ULoadoutComponent *Loadout, UCharacterDataComponent *CharComp)
{
    FAction Action;

    if (!CurrentCombat)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    // Get all enemies
    TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    if (Enemies.Num() == 0)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    // Select best target
    AActor *BestTarget = SelectBestTarget(AIActor, Enemies);
    if (!BestTarget)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    Action.Targets.Add(BestTarget);

    // Evaluate options and pick best. AI categories stay distinct (EAIActionChoice): attacks dispatch
    // as EActionType::Ability but keep damage-only scoring, separate from full ability scoring.
    TArray<EAIActionChoice> AvailableActions;
    TMap<EAIActionChoice, int32> ActionScores;

    // Gather available actions
    TArray<USkillDataBase *> AllAttacks = Loadout->GetAllWeaponAttacks();
    if (AllAttacks.Num() > 0)
    {
        AvailableActions.Add(EAIActionChoice::Attack);
    }

    if (Loadout->GetAvailableSpells().Num() > 0)
    {
        AvailableActions.Add(EAIActionChoice::Spell);
    }

    if (Loadout->GetAvailableAbilities().Num() > 0)
    {
        AvailableActions.Add(EAIActionChoice::Ability);
    }

    if (AvailableActions.Num() == 0)
    {
        Action.ActionType = EActionType::Defend;
        return Action;
    }

    // Track the best UNAFFORDABLE spell/ability score (and its EP cost) for the
    // self-target Emerald valuation — "I hold a strong action I can't pay for now".
    float UnaffordableBest = 0.0f;
    int32 UnaffordableBestCost = 0;

    // Score each action type
    for (EAIActionChoice ActionType : AvailableActions)
    {
        int32 Score = 0;

        switch (ActionType)
        {
        case EAIActionChoice::Attack:
        {
            // Score best attack from all weapons
            UDamageCalculator *DamageCalc = GetGameInstance()->GetSubsystem<UDamageCalculator>();
            if (DamageCalc)
            {
                for (USkillDataBase *Attack : AllAttacks)
                {
                    if (Attack)
                    {
                        FDamageCalculationResult Result = DamageCalc->CalculateAttackDamage(AIActor, BestTarget, Attack, false);
                        Score = FMath::Max(Score, Result.FinalDamage);
                    }
                }
            }
            break;
        }
        case EAIActionChoice::Spell:
        {
            // Best combined damage + status score among affordable spells.
            TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
            float BestSpellScore = 0.0f;
            for (USpellData *Spell : Spells)
            {
                if (!Spell || Spell->School != ESpellSchool::Destruction)
                {
                    continue;
                }
                const float SpellScore = EstimateSpellDamage(AIActor, BestTarget, Spell) +
                                         EstimateStatusScore(AIActor, BestTarget, Spell) * AIConstants::STATUS_SCORE_WEIGHT;
                if (CanAffordSpell(AIActor, Spell))
                {
                    BestSpellScore = FMath::Max(BestSpellScore, SpellScore);
                }
                else if (SpellScore > UnaffordableBest)
                {
                    // Strong action the AI can't pay for this turn — candidate to hold for an Emerald bonus turn.
                    UnaffordableBest = SpellScore;
                    if (UActionExecutor *Exec = GetActionExecutor())
                    {
                        FAction CostProbe;
                        CostProbe.ActionType = EActionType::Spell;
                        CostProbe.SpellData = Spell;
                        CostProbe.SpellSource = Loadout->ResolveSpellSource(Spell);
                        UnaffordableBestCost = Exec->CalculateActionEnergyCost(AIActor, CostProbe);
                    }
                }
            }
            Score = FMath::RoundToInt(BestSpellScore);
            break;
        }
        case EAIActionChoice::Ability:
        {
            // Best combined damage + status score among affordable abilities.
            TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
            float BestAbilityScore = 0.0f;
            for (UAbilityData *Ability : Abilities)
            {
                if (!Ability)
                {
                    continue;
                }
                const float AbilityScore = EstimateAbilityDamage(AIActor, BestTarget, Ability) +
                                           EstimateStatusScore(AIActor, BestTarget, Ability) * AIConstants::STATUS_SCORE_WEIGHT;
                if (CanAffordAbility(AIActor, Ability))
                {
                    BestAbilityScore = FMath::Max(BestAbilityScore, AbilityScore);
                }
                else if (AbilityScore > UnaffordableBest)
                {
                    // Strong action the AI can't pay for this turn — candidate to hold for an Emerald bonus turn.
                    UnaffordableBest = AbilityScore;
                    if (UActionExecutor *Exec = GetActionExecutor())
                    {
                        FAction CostProbe;
                        CostProbe.ActionType = EActionType::Ability;
                        CostProbe.SkillData = Ability;
                        UnaffordableBestCost = Exec->CalculateActionEnergyCost(AIActor, CostProbe);
                    }
                }
            }
            Score = FMath::RoundToInt(BestAbilityScore);
            break;
        }
        default:
            break;
        }

        ActionScores.Add(ActionType, Score);
    }

    // Pick action with highest score
    EAIActionChoice BestActionType = EAIActionChoice::Defend;
    int32 BestScore = -1;

    for (const auto &Pair : ActionScores)
    {
        if (Pair.Value > BestScore)
        {
            BestScore = Pair.Value;
            BestActionType = Pair.Key;
        }
    }

    // ===== Emerald (bonus-turn item) valuation — Medium+ only =====
    // Slots in HERE because this is the only point where BestScore (best AFFORDABLE
    // action this turn, in HP-damage units) and the chosen BestTarget both exist.
    // Every Emerald score below is in the SAME HP-damage units as BestScore.
    if (GetCurrentDifficulty() != EAIDifficulty::Easy)
    {
        bool bFoundEmerald = false;
        const FCrystalId EmeraldId = FindBonusTurnItem(Loadout, bFoundEmerald);
        if (bFoundEmerald)
        {
            const int32 EmeraldDelay = CrystalEffectTable::GetEmeraldBonusTurnDelay(EmeraldId);

            // --- ENEMY-TARGET: force a guaranteed one-tick-lethal DoT kill now, before
            //     the target's team gets turns to heal/cleanse it away. ---
            const float LethalPerTick = GetLethalDoTPerTick(BestTarget);          // GATE 1
            const bool bAlreadyKillable = CanKillTarget(AIActor, BestTarget, BestScore); // GATE 2
            if (LethalPerTick > 0.0f && !bAlreadyKillable)
            {
                // Target's best expected damage against the AI — its per-turn threat,
                // natively in the same HP-damage units as ActionScores (no scale factor).
                const float ThreatPerTurnHP =
                    static_cast<float>(EstimateBestDamage(BestTarget, AIActor));
                const int32 ExposureTurns = GetRescueExposureTurns(AIActor, BestTarget);
                const float EmeraldEnemyScore =
                    GetCurrentHP(BestTarget) * AIConstants::KILL_SECURE_FACTOR +
                    ThreatPerTurnHP * ExposureTurns -
                    ThreatPerTurnHP * AIConstants::FREE_ACTION_FACTOR;

                if (EmeraldEnemyScore > BestScore)
                {
                    FAction Emerald;
                    Emerald.ActionType = EActionType::Item;
                    Emerald.ItemData = EmeraldId;
                    Emerald.Targets.Add(BestTarget);
                    return Emerald;
                }
            }

            // --- SELF-TARGET: EP-starved — hold a stronger action for the bonus turn. ---
            if (UnaffordableBest > BestScore * AIConstants::STARVE_MARGIN)
            {
                const int32 ProjectedEP =
                    GetCurrentEP(AIActor) + AIConstants::ESTIMATED_EP_REGEN_PER_TURN * EmeraldDelay;
                if (ProjectedEP >= UnaffordableBestCost)
                {
                    const float DelayDiscount = 1.0f / (1.0f + AIConstants::DELAY_DECAY * EmeraldDelay);
                    const float EmeraldSelfScore = UnaffordableBest * DelayDiscount - BestScore;
                    if (EmeraldSelfScore > BestScore)
                    {
                        FAction Emerald;
                        Emerald.ActionType = EActionType::Item;
                        Emerald.ItemData = EmeraldId;
                        Emerald.Targets.Add(AIActor);
                        return Emerald;
                    }
                }
            }
        }
    }

    // Build final action. Map the AI category → dispatched action type: attacks dispatch as Ability
    // (carried on SkillData; IsAttack() distinguishes them downstream).
    Action.ActionType = (BestActionType == EAIActionChoice::Spell)    ? EActionType::Spell
                        : (BestActionType == EAIActionChoice::Defend) ? EActionType::Defend
                                                                      : EActionType::Ability;

    switch (BestActionType)
    {
    case EAIActionChoice::Attack:
    {
        // Pick best attack from all weapons
        USkillDataBase *BestAttack = nullptr;
        int32 BestAttackDamage = 0;
        UDamageCalculator *DamageCalc = GetGameInstance()->GetSubsystem<UDamageCalculator>();

        for (USkillDataBase *Attack : Loadout->GetAllWeaponAttacks())
        {
            if (Attack && DamageCalc)
            {
                FDamageCalculationResult Result = DamageCalc->CalculateAttackDamage(AIActor, Action.Targets[0], Attack, false);
                if (Result.FinalDamage > BestAttackDamage)
                {
                    BestAttackDamage = Result.FinalDamage;
                    BestAttack = Attack;
                }
            }
            else if (Attack && !BestAttack)
            {
                BestAttack = Attack; // Fallback if no DamageCalc
            }
        }

        Action.SkillData = BestAttack;
        break;
    }
    case EAIActionChoice::Spell:
    {
        // Pick the affordable spell with the best combined damage + status score.
        TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
        USpellData *BestSpell = nullptr;
        float BestSpellScore = -1.0f;

        for (USpellData *Spell : Spells)
        {
            if (!Spell || Spell->School != ESpellSchool::Destruction)
            {
                continue;
            }
            if (!CanAffordSpell(AIActor, Spell))
            {
                continue;
            }
            const float SpellScore = EstimateSpellDamage(AIActor, BestTarget, Spell) +
                                     EstimateStatusScore(AIActor, BestTarget, Spell) * AIConstants::STATUS_SCORE_WEIGHT;
            if (SpellScore > BestSpellScore)
            {
                BestSpellScore = SpellScore;
                BestSpell = Spell;
            }
        }

        // No affordable spell — fall through to Defend.
        if (!BestSpell)
        {
            Action.ActionType = EActionType::Defend;
            break;
        }

        Action.SpellData = BestSpell;
        Action.SpellSource = Loadout->ResolveSpellSource(BestSpell);

        // Decide infusion, then drop to L0 if the infused cost is unaffordable.
        int32 SpellInfusion = DecideSpellInfusionLevel(AIActor, Action.Targets[0], BestSpell);
        if (SpellInfusion > 0 && !CanAffordSpell(AIActor, BestSpell, SpellInfusion))
        {
            SpellInfusion = 0;
        }

        // 6-5-b: spells are origin-bound. Pick the 1:1 allowed source (Evolution preferred for the
        // BD/Reality {Evolution, Innate} case). Empty (Item/non-infusable) -> cast uninfused. Then HP-guard.
        if (SpellInfusion > 0)
        {
            const EInfusionSourceOption SpellSrc = DecideSpellInfusionSource(AIActor, BestSpell);
            if (SpellSrc == EInfusionSourceOption::None)
            {
                SpellInfusion = 0; // no legal source -> don't infuse
            }
            else
            {
                const int32 BaseEP = BestSpell->CalculateEnergyCost(CharComp ? CharComp->CharacterData : nullptr);
                SpellInfusion = ClampInfusionLevelForHP(AIActor, CharComp, BaseEP, /*bIsSpell*/ true,
                                                        SpellSrc, SpellInfusion);
                if (SpellInfusion > 0)
                {
                    Action.SelectedSource = SpellSrc;
                }
            }
        }
        Action.SpellInfusionLevel = SpellInfusion;
        break;
    }
    case EAIActionChoice::Ability:
    {
        // Pick the affordable ability with the best combined damage + status score.
        TArray<UAbilityData *> Abilities = Loadout->GetAvailableAbilities();
        UAbilityData *BestAbility = nullptr;
        float BestAbilityScore = -1.0f;

        for (UAbilityData *Ability : Abilities)
        {
            if (!Ability)
            {
                continue;
            }
            if (!CanAffordAbility(AIActor, Ability))
            {
                continue;
            }
            const float AbilityScore = EstimateAbilityDamage(AIActor, BestTarget, Ability) +
                                       EstimateStatusScore(AIActor, BestTarget, Ability) * AIConstants::STATUS_SCORE_WEIGHT;
            if (AbilityScore > BestAbilityScore)
            {
                BestAbilityScore = AbilityScore;
                BestAbility = Ability;
            }
        }

        // No affordable ability — fall through to Defend.
        if (!BestAbility)
        {
            Action.ActionType = EActionType::Defend;
            break;
        }

        Action.SkillData = BestAbility;

        // Decide infusion, then drop to L0 if the infused cost is unaffordable.
        int32 AbilityInfusion = DecideAbilityInfusionLevel(AIActor, Action.Targets[0], BestAbility);
        if (AbilityInfusion > 0 && !CanAffordAbility(AIActor, BestAbility, AbilityInfusion))
        {
            AbilityInfusion = 0;
        }

        // 6-5-b: abilities are not origin-bound — pick the source via heuristic, then HP-guard.
        if (AbilityInfusion > 0)
        {
            const EInfusionSourceOption AbilitySrc = DecideAbilityInfusionSource(AIActor);
            const int32 BaseEP = BestAbility->CalculateEnergyCost(CharComp ? CharComp->CharacterData : nullptr, /*bIsInfused*/ true);
            AbilityInfusion = ClampInfusionLevelForHP(AIActor, CharComp, BaseEP, /*bIsSpell*/ false,
                                                      AbilitySrc, AbilityInfusion);
            if (AbilityInfusion > 0)
            {
                Action.SelectedSource = AbilitySrc;
            }
        }
        Action.AbilityInfusionLevel = AbilityInfusion;
        break;
    }
    default:
        Action.ActionType = EActionType::Defend;
        break;
    }

    return Action;
}

// ==================== HELPER FUNCTIONS ====================

bool UAIDecisionManager::HasDangerousDebuff(AActor *Actor)
{
    if (!Actor)
    {
        return false;
    }

    USkillEffectManager *StatusManager = GetSkillEffectManager();
    if (!StatusManager)
    {
        return false;
    }

    TArray<FActiveSkillEffect> Effects = StatusManager->GetActiveEffects(Actor);
    for (const FActiveSkillEffect &Effect : Effects)
    {
        switch (Effect.EffectType)
        {
        case ESkillEffectType::SkipTurn: // Stun
            return true;
        case ESkillEffectType::DOT:
            // Check if DOT will kill us
            if (Effect.EffectValue * Effect.RemainingTurns >= GetCurrentHP(Actor))
            {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

// ==================== EMERALD (BONUS-TURN ITEM) VALUATION ====================

FCrystalId UAIDecisionManager::FindBonusTurnItem(ULoadoutComponent *Loadout, bool &bOutFound)
{
    bOutFound = false;
    if (!Loadout)
    {
        return FCrystalId{};
    }

    for (const FItemLoadoutSlot &Slot : Loadout->GetUsableItems())
    {
        if (Slot.IsEmpty())
        {
            continue;
        }
        if (ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::GrantBonusTurn)
        {
            bOutFound = true;
            return Slot.CrystalId;
        }
    }

    return FCrystalId{};
}

float UAIDecisionManager::GetLethalDoTPerTick(AActor *Target)
{
    if (!Target)
    {
        return 0.0f;
    }

    USkillEffectManager *StatusManager = GetSkillEffectManager();
    if (!StatusManager)
    {
        return 0.0f;
    }

    const int32 TargetHP = GetCurrentHP(Target);
    if (TargetHP <= 0)
    {
        return 0.0f;
    }

    // One-tick-lethal: a SINGLE DoT tick (EffectValue) alone reaches/exceeds current
    // HP. Only then does forcing ONE bonus turn guarantee the kill at that turn's
    // end-of-turn tick. (Accumulated-lethal — EffectValue*RemainingTurns — is the
    // self-preservation test in HasDangerousDebuff; NOT enough here, since one bonus
    // turn triggers only ONE tick.)
    float BestPerTick = 0.0f;
    for (const FActiveSkillEffect &Effect : StatusManager->GetActiveEffects(Target))
    {
        if (Effect.EffectType == ESkillEffectType::DOT &&
            Effect.EffectValue >= static_cast<float>(TargetHP) &&
            Effect.EffectValue > BestPerTick)
        {
            BestPerTick = Effect.EffectValue;
        }
    }

    return BestPerTick;
}

int32 UAIDecisionManager::GetRescueExposureTurns(AActor *AIActor, AActor *Target) const
{
    if (!AIActor || !Target || !CurrentCombat)
    {
        return 0;
    }

    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        return 0;
    }

    // The AI's enemies == the target's team — the combatants who could heal/cleanse
    // the target before the DoT kills it. Count their turn-slots that fall BEFORE the
    // target's own next slot (when the DoT ticks and the kill resolves).
    const TArray<AActor *> Enemies = CurrentCombat->GetLivingEnemies(AIActor);
    const TSet<AActor *> EnemySet(Enemies);

    int32 Exposure = 0;
    for (const FPreviewTurnEntry &Entry : TurnMgr->PreviewTurnOrder(AIConstants::EMERALD_EXPOSURE_LOOKAHEAD))
    {
        if (Entry.Actor == Target)
        {
            break; // reached the target's next turn — rescue window closes here
        }
        if (Entry.Actor && Entry.Actor != Target && EnemySet.Contains(Entry.Actor))
        {
            ++Exposure;
        }
    }

    return Exposure;
}

USpellData *UAIDecisionManager::FindHealingSpell(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        return nullptr;
    }

    TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
    for (USpellData *Spell : Spells)
    {
        if (!Spell || Spell->School != ESpellSchool::Restoration)
        {
            continue;
        }

        for (const FSkillEffect &Effect : Spell->Effects)
        {
            if (Effect.EffectType == ESkillEffectType::Heal ||
                Effect.EffectType == ESkillEffectType::HealthRestore)
            {
                return Spell;
            }
        }
    }

    return nullptr;
}

USpellData *UAIDecisionManager::FindCleanseSpell(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        return nullptr;
    }

    TArray<USpellData *> Spells = Loadout->GetAvailableSpells();
    for (USpellData *Spell : Spells)
    {
        if (!Spell)
        {
            continue;
        }

        for (const FSkillEffect &Effect : Spell->Effects)
        {
            if (Effect.EffectType == ESkillEffectType::Cleanse)
            {
                return Spell;
            }
        }
    }

    return nullptr;
}

// ==================== STATUS BAR QUERIES ====================

bool UAIDecisionManager::IsStatusBarNearTrigger(AActor *Target, float Threshold) const
{
    if (!Target)
    {
        return false;
    }

    UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr;
    if (!BuildupManager)
    {
        return false;
    }

    // Get current bar percentage (0.0 - 1.0)
    float BarPercent = BuildupManager->GetStatusBarPercent(Target);

    // Check if bar is near trigger threshold
    return BarPercent >= Threshold; // Default threshold is 0.70 (70%)
}

bool UAIDecisionManager::WouldTriggerStatusBar(AActor *Attacker, AActor *Target, float BuildupAmount) const
{
    if (!Attacker || !Target)
    {
        return false;
    }

    UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr;
    if (!BuildupManager)
    {
        return false;
    }

    // Get remaining buildup needed to trigger
    float RemainingBuildup = BuildupManager->GetBuildupToTrigger(Target);

    // TODO (vulnerability awareness): BuildupAmount here is the RAW attacker-side
    // buildup — the target's resistance shave is NOT applied. Since resistance can now
    // go negative (amplification) or positive (reduction), this scorer won't perceive
    // a vulnerability-debuffed target taking up to 2x buildup, nor a resistant target
    // taking less. To exploit/avoid it, project BuildupAmount through the target's
    // resistance before comparing (mirror StatusBuildupManager's resistance aggregate).
    // Check if this hit would trigger the bar
    return BuildupAmount >= RemainingBuildup;
}

bool UAIDecisionManager::IsValuableStatus(ESkillEffectType StatusType, AActor *Target) const
{
    // Skip turn statuses are always valuable
    if (StatusType == ESkillEffectType::SkipTurn)
    {
        return true;
    }

    USkillEffectManager *StatusManager = GetGameInstance()->GetSubsystem<USkillEffectManager>();
    if (!StatusManager)
    {
        return true; // Assume valuable if we can't check
    }

    // DOTs are valuable if target doesn't already have one
    if (StatusType == ESkillEffectType::DOT)
    {
        return !StatusManager->HasActiveDOT(Target);
    }

    // Debuffs are valuable if target doesn't have too many already
    int32 DebuffCount = StatusManager->GetDebuffCount(Target);
    return DebuffCount < 3;
}

// ==================== INFUSION DECISIONS ====================

int32 UAIDecisionManager::DecideSpellInfusionLevel(AActor *Attacker, AActor *Target, USpellData *Spell) const
{
    if (!Attacker || !Target || !Spell)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !TargetComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    // Get difficulty
    EAIDifficulty Difficulty = GetCurrentDifficulty();

    // Easy: Never infuse
    if (Difficulty == EAIDifficulty::Easy)
    {
        return 0;
    }

    // Get energy state
    int32 CurrentEnergy = GetCurrentEP(Attacker);
    int32 MaxEnergy = GetMaxEP(Attacker);
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // 6-5-d: resolve the (level-independent) infusion source so estimates use the real mode.
    const EInfusionSourceOption Source = DecideSpellInfusionSource(Attacker, Spell);

    // Calculate damage at each level. The estimator applies per-mode charge damage internally
    // (single application point — no hand re-multiply), so L2Damage matches execution.
    int32 L0Damage = EstimateSpellDamage(Attacker, Target, Spell, 0, Source);
    int32 L2Damage = EstimateSpellDamage(Attacker, Target, Spell, 2, Source);

    int32 TargetHP = TargetComp->CurrentHP;

    // Priority 1: Can we kill with L0? Don't waste energy
    if (L0Damage >= TargetHP)
    {
        return 0;
    }

    // Priority 2: Can we kill with L2? Use it if we have energy
    if (L2Damage >= TargetHP && EnergyPercent > AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 2;
    }

    // Priority 3: Status bar considerations (Medium+)
    UStatusBuildupManager *BuildupManager = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
    if (BuildupManager)
    {
        // Calculate L1 buildup — per-mode, stat-scaled status (matches execution's GetChargeStatusMultiplier).
        float BaseBuildup = Spell->StatusBuildup;
        UActionExecutor *StatusExec = GetActionExecutor();
        const float L1StatusMult = StatusExec
                                       ? StatusExec->GetChargeStatusMultiplier(1, StatusExec->ResolveInfusionMode(Source, Attacker), AttackerComp)
                                       : 1.0f;
        float L1Buildup = BaseBuildup * L1StatusMult;

        // Would L1 trigger the bar?
        bool bL1WouldTrigger = WouldTriggerStatusBar(Attacker, Target, L1Buildup);
        bool bL0WouldTrigger = WouldTriggerStatusBar(Attacker, Target, BaseBuildup);

        // Get pending status type (abilities apply physical status, not specific types)
        ESkillEffectType PendingTrigger = BuildupManager->GetPendingTrigger(Target);
        if (PendingTrigger == ESkillEffectType::None)
        {
            // Default to DOT for abilities (they apply status via infusion)
            PendingTrigger = ESkillEffectType::DOT;
        }

        // Use L1 if it triggers valuable status and L0 wouldn't
        if (bL1WouldTrigger && !bL0WouldTrigger && IsValuableStatus(PendingTrigger, Target))
        {
            return 1;
        }

        // Hard+: Use L1 if bar is high (>70%) and we have good energy
        if (Difficulty >= EAIDifficulty::Hard &&
            IsStatusBarNearTrigger(Target) &&
            EnergyPercent > AIConstants::ENERGY_ABUNDANT_THRESHOLD &&
            IsValuableStatus(PendingTrigger, Target))
        {
            return 1;
        }
    }

    // Conserve energy if low
    if (EnergyPercent < AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 0;
    }

    // Default: No infusion
    return 0;
}

int32 UAIDecisionManager::DecideAbilityInfusionLevel(AActor *Attacker, AActor *Target, UAbilityData *Ability) const
{
    if (!Attacker || !Target || !Ability)
    {
        return 0;
    }

    UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
    UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
    if (!AttackerComp || !TargetComp || !AttackerComp->CharacterData)
    {
        return 0;
    }

    EAIDifficulty Difficulty = GetCurrentDifficulty();
    if (Difficulty == EAIDifficulty::Easy)
    {
        return 0;
    }

    int32 CurrentEnergy = GetCurrentEP(Attacker);
    int32 MaxEnergy = GetMaxEP(Attacker);
    float EnergyPercent = static_cast<float>(CurrentEnergy) / MaxEnergy;

    // 6-5-d: resolve the (level-independent) ability infusion source for accurate estimates.
    const EInfusionSourceOption Source = DecideAbilityInfusionSource(Attacker);

    // Calculate damage at each level. The estimator applies per-mode charge damage internally
    // (single application point — no hand re-multiply), so L2Damage matches execution.
    int32 L0Damage = EstimateAbilityDamage(Attacker, Target, Ability, 0, Source);
    int32 L2Damage = EstimateAbilityDamage(Attacker, Target, Ability, 2, Source);
    int32 TargetHP = TargetComp->CurrentHP;

    // Priority 1: Can we kill with L0? Don't waste
    if (L0Damage >= TargetHP)
    {
        return 0;
    }

    // Priority 2: Can we kill with L2? Use it
    if (L2Damage >= TargetHP && EnergyPercent > AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 2;
    }

    // Priority 3: Status bar considerations (Medium+)
    UStatusBuildupManager *BuildupManager = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
    if (BuildupManager && Difficulty >= EAIDifficulty::Medium)
    {
        // Calculate L1 status buildup
        int32 BaseBuildup = Ability->CalculateStatusBuildup(AttackerComp->CharacterData);
        // 6-5-d: per-mode, stat-scaled status (matches execution's GetChargeStatusMultiplier).
        UActionExecutor *StatusExec = GetActionExecutor();
        const float L1StatusMult = StatusExec
                                       ? StatusExec->GetChargeStatusMultiplier(1, StatusExec->ResolveInfusionMode(Source, Attacker), AttackerComp)
                                       : 1.0f;
        float L1Buildup = BaseBuildup * L1StatusMult;

        // Would L1 trigger the bar?
        bool bL1WouldTrigger = WouldTriggerStatusBar(Attacker, Target, L1Buildup);
        bool bL0WouldTrigger = WouldTriggerStatusBar(Attacker, Target, BaseBuildup);

        // Get pending status type
        ESkillEffectType PendingTrigger = BuildupManager->GetPendingTrigger(Target);
        if (PendingTrigger == ESkillEffectType::None)
        {
            PendingTrigger = ESkillEffectType::DOT;
        }

        // Use L1 if it triggers valuable status and L0 wouldn't
        if (bL1WouldTrigger && !bL0WouldTrigger && IsValuableStatus(PendingTrigger, Target))
        {
            return 1;
        }

        // Hard+: Use L1 if bar is high (>70%) and we have good energy
        if (Difficulty >= EAIDifficulty::Hard &&
            IsStatusBarNearTrigger(Target) &&
            EnergyPercent > AIConstants::ENERGY_ABUNDANT_THRESHOLD &&
            IsValuableStatus(PendingTrigger, Target))
        {
            return 1;
        }
    }

    // Conserve energy if low
    if (EnergyPercent < AIConstants::ENERGY_CONSERVATION_THRESHOLD)
    {
        return 0;
    }

    // Default: No infusion
    return 0;
}

EInfusionSourceOption UAIDecisionManager::DecideSpellInfusionSource(AActor *Attacker, USpellData *Spell) const
{
    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec)
    {
        return EInfusionSourceOption::None;
    }

    const TArray<EInfusionSourceOption> Allowed = ActionExec->GetAllowedInfusionSourcesForSpell(Attacker, Spell);
    if (Allowed.Num() == 0)
    {
        return EInfusionSourceOption::None; // Item / non-infusable -> no source
    }

    // Spells are 1:1 origin-bound; the BD/Reality evolution case returns {Evolution, Innate} —
    // prefer Evolution this stage (richer BD/Reality choice deferred).
    return Allowed.Contains(EInfusionSourceOption::Evolution)
               ? EInfusionSourceOption::Evolution
               : Allowed[0];
}

EInfusionSourceOption UAIDecisionManager::DecideAbilityInfusionSource(AActor *Attacker) const
{
    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec)
    {
        return EInfusionSourceOption::Raw; // always-available fallback
    }

    const TArray<EInfusionSourceOption> Available = ActionExec->GetAvailableInfusionSources(Attacker);

    UCharacterDataComponent *Comp = Attacker ? Attacker->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    const UCharacterData *Data = Comp ? Comp->CharacterData : nullptr;

    // 1. Caster -> Innate (innate-on-ability pays no HP).
    if (Data && Data->IsCaster() && Available.Contains(EInfusionSourceOption::Innate))
    {
        return EInfusionSourceOption::Innate;
    }

    // 2. Resonator -> ActiveRing.
    if (Data && Data->IsResonator() && Available.Contains(EInfusionSourceOption::ActiveRing))
    {
        return EInfusionSourceOption::ActiveRing;
    }

    // 3. First available crystal source (durability-paying, no HP).
    for (const EInfusionSourceOption Crystal : {EInfusionSourceOption::PrimaryRing,
                                                EInfusionSourceOption::WeaponCrystal,
                                                EInfusionSourceOption::Evolution})
    {
        if (Available.Contains(Crystal))
        {
            return Crystal;
        }
    }

    // 4. Fallback: Raw (always available; HP-paying, so the caller's HP guard applies).
    return EInfusionSourceOption::Raw;
}

int32 UAIDecisionManager::ClampInfusionLevelForHP(AActor *Attacker, UCharacterDataComponent *Comp, int32 BaseEnergyCost,
                                                  bool bIsSpell, EInfusionSourceOption Source, int32 Level) const
{
    if (Level <= 0)
    {
        return Level;
    }

    // Only Raw, Innate-on-SPELL, and Evolution pay HP (innate-on-ability pays none; crystal sources
    // pay durability). Mirrors ActionExecutor::ApplyCommitCosts' HP-cost routing.
    const bool bHPPaying =
        (Source == EInfusionSourceOption::Raw) ||
        (Source == EInfusionSourceOption::Innate && bIsSpell) ||
        (Source == EInfusionSourceOption::Evolution);
    if (!bHPPaying)
    {
        return Level;
    }

    UActionExecutor *ActionExec = GetActionExecutor();
    if (!ActionExec || !Comp)
    {
        return Level; // can't compute — fail open, matching CanAfford*'s convention (null is unreachable in combat)
    }

    // Drop the charge until the HP cost is survivable. PreEffEP matches the executor's basis exactly:
    // BaseEnergyCost x ComputeInfusionCostMultiplier (pre-Efficiency, stat-scaled) -> WouldKill.
    while (Level > 0)
    {
        const int32 PreEffEP = FMath::RoundToInt(
            BaseEnergyCost * ActionExec->ComputeInfusionCostMultiplier(Level, bIsSpell, Comp));
        if (UInfusionCostHelper::WouldKill(Attacker, PreEffEP))
        {
            Level--;
        }
        else
        {
            break;
        }
    }
    return Level;
}

// ==================== HELPER FUNCTIONS ====================

int32 UAIDecisionManager::GetCurrentEP(AActor *Actor) const
{
    if (!Actor)
    {
        return 0;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp)
    {
        return 0;
    }

    // CurrentEP is the unified spend pool — Broken Darkness (absorption) and
    // non-BD (regenerating EP) characters alike.
    return CharComp->CurrentEP;
}

int32 UAIDecisionManager::GetMaxEP(AActor *Actor) const
{
    if (!Actor)
    {
        return 1; // Avoid division by zero
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp)
    {
        return CharComp->MaxEP;
    }

    return 1; // Avoid division by zero
}

UCharacterData *UAIDecisionManager::GetCharacterData(AActor *Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp)
    {
        return CharComp->CharacterData;
    }

    return nullptr;
}