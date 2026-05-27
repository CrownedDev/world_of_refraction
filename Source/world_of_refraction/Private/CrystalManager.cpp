// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "EvolutionItemData.h"
#include "LoadoutComponent.h"
#include "FRuntimeAttachedItem.h"
#include "CrystalIdentity.h"
#include "BreakCalculator.h"
#include "BreakCalculatorDebug.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CombatConstants.h"
#include "TurnManager.h"
#include "WeaponData.h"
#include "Engine/GameInstance.h"

void UCrystalManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[CrystalManager] Initialized"));
}

void UCrystalManager::Deinitialize()
{
    Super::Deinitialize();
}

// ========================================
// WEAR
// ========================================

void UCrystalManager::ProcessPostCastWear(
    AActor *Actor,
    UObject *Holder,
    FRuntimeAttachedItem &Attachment,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    if (!Actor || Attachment.IsEmpty())
    {
        return;
    }

    // Only refined attachments wear/break. Evolution items default to unbreakable
    // (see FEvolutionAttachment::IsBroken — checks bCanBreak).
    if (!Attachment.IsRefined())
    {
        return;
    }

    // Defensive: don't double-process an already-broken attachment.
    if (Attachment.IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[CrystalManager] ProcessPostCastWear called on already-broken attachment for %s"),
               *Actor->GetName());
        return;
    }

    // Wear math reads the source crystal's tier. For refined: from FCrystalId.
    // Substat-modified path when the caster's UCharacterData is reachable —
    // wear tracks real power output (gear-inclusive via crystal-modified
    // pillar accessors). Falls back to the base formula if the asset is
    // missing rather than crashing.
    UCharacterDataComponent *CasterCharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    int32 Wear = 0;
    if (CasterCharComp && CasterCharComp->CharacterData)
    {
        const float SpellDmgFrac    = CasterCharComp->GetCrystalModifiedSpellDamage() - 1.0f;
        const float StatusMultFrac  = CasterCharComp->GetCrystalModifiedStatusMultiplier() - 1.0f;
        const float EfficiencyFrac  = 1.0f - CasterCharComp->GetCrystalModifiedEfficiencyMultiplier();
        const float ResistanceFrac  = CasterCharComp->GetCrystalModifiedResistance();
        Wear = UBreakCalculator::CalculateDurabilityWearWithSubstats(
            Attachment.Refined.Id.Tier,
            ActionTier,
            InfusionLevel,
            bIsSpell,
            SpellDmgFrac,
            StatusMultFrac,
            EfficiencyFrac,
            ResistanceFrac);
    }
    else
    {
        Wear = UBreakCalculator::CalculateDurabilityWear(
            Attachment.Refined.Id.Tier,
            ActionTier,
            InfusionLevel,
            bIsSpell);
    }

    if (Wear <= 0)
    {
        return;
    }

    // Luck-driven break skip. Roll the wielder's Luck before applying wear.
    // On success, skip the wear entirely (durability unchanged, no broadcast).
    if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
    {
        const float RawLuck = CharComp->GetEquipmentModifiedLuck();
        const float SkipChance = (RawLuck / CombatConstants::LUCK_RAW_MAX) * CombatConstants::LUCK_BREAK_SKIP_MAX;
        if (FMath::FRand() < SkipChance)
        {
            UE_LOG(LogTemp, Log,
                   TEXT("[CrystalManager] %s LUCKY break skip on crystal '%s' (would have applied %d wear, skip chance %.2f)"),
                   *Actor->GetName(),
                   *CrystalIdentity::GetDisplayName(Attachment.Refined.Id),
                   Wear, SkipChance);
            return;
        }
    }

    const bool bBroke = Attachment.ApplyWear(Wear);
    const int32 NewDur = Attachment.GetCurrentDurability();
    const int32 MaxDur = Attachment.GetMaxDurability();

    UE_LOG(LogTemp, Verbose,
           TEXT("[CrystalManager] %s applies %d wear to crystal '%s' (%d/%d) [ActionTier=%d L%d bIsSpell=%d]"),
           *Actor->GetName(), Wear,
           *CrystalIdentity::GetDisplayName(Attachment.Refined.Id),
           NewDur, MaxDur,
           static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

    // Broadcast post-wear durability for real-time UI updates.
    OnCrystalDurabilityChanged.Broadcast(Actor, Holder, NewDur, MaxDur);

    if (bBroke)
    {
        UE_LOG(LogTemp, Log,
               TEXT("[CrystalManager] Crystal '%s' broke on %s (holder: %s)"),
               *CrystalIdentity::GetDisplayName(Attachment.Refined.Id),
               *Actor->GetName(),
               Holder ? *Holder->GetName() : TEXT("Unknown"));

        FBrokenCrystalPayload Payload;
        Payload.Kind = EAttachedItemKind::Refined;
        Payload.RefinedId = Attachment.Refined.Id;
        OnCrystalBroken.Broadcast(Actor, Holder, Payload);
    }
}

// ========================================
// HELPERS
// ========================================

ULoadoutComponent *UCrystalManager::GetLoadoutComponent(AActor *Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }
    return Actor->FindComponentByClass<ULoadoutComponent>();
}

// ========================================
// DEBUG
// ========================================

void UCrystalManager::DebugBreakActiveCrystal()
{
    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug.BreakActiveCrystal] No TurnManager subsystem."));
        return;
    }

    AActor *Actor = TurnMgr->GetCurrentActor();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug.BreakActiveCrystal] No active character (combat not active?)."));
        return;
    }

    ULoadoutComponent *LC = GetLoadoutComponent(Actor);
    if (!LC)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] No LoadoutComponent on %s."),
               *Actor->GetName());
        return;
    }

    UWeaponData *Weapon = LC->GetPrimaryWeapon();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s has no primary weapon equipped."),
               *Actor->GetName());
        return;
    }

    FRuntimeAttachedItem *Attachment = LC->FindAttachedItemByHolder(Weapon);
    if (!Attachment || Attachment->IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon has no attached crystal."),
               *Actor->GetName());
        return;
    }

    const FString CrystalName = Attachment->IsRefined()
                                    ? CrystalIdentity::GetDisplayName(Attachment->Refined.Id)
                                    : (Attachment->Evolution.Item ? Attachment->Evolution.Item->GetFullItemName() : TEXT("(null)"));

    if (Attachment->IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon crystal '%s' is already broken."),
               *Actor->GetName(), *CrystalName);
        return;
    }
    if (!Attachment->IsRefined())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon crystal '%s' is not refined (evolution / immune — does not wear/break)."),
               *Actor->GetName(), *CrystalName);
        return;
    }

    // Drain durability down to 1 directly on the attachment so the next
    // non-skipped wear breaks it. ApplyWear on partial wear does not broadcast
    // OnCrystalBroken; the unified broadcast lives in ProcessPostCastWear's
    // bBroke branch, so this drain stays silent.
    const int32 DrainAmount = Attachment->GetCurrentDurability() - 1;
    if (DrainAmount > 0)
    {
        Attachment->ApplyWear(DrainAmount);
    }

    // Route through the real pipeline. Luck-skip is probabilistic, so retry
    // until the attachment transitions to broken. S-Tier + L2 + spell parameters
    // produce the worst-case wear, guaranteeing >0 wear on any non-skipped roll.
    static constexpr int32 MAX_ATTEMPTS = 8;
    for (int32 Attempt = 1; Attempt <= MAX_ATTEMPTS; ++Attempt)
    {
        ProcessPostCastWear(Actor, Weapon, *Attachment,
                            EItemTier::S_Tier, /*InfusionLevel*/ 2, /*bIsSpell*/ true);

        if (Attachment->IsBroken())
        {
            UE_LOG(LogTemp, Display,
                   TEXT("[Debug.BreakActiveCrystal] Broke %s's primary weapon crystal '%s' on attempt %d/%d."),
                   *Actor->GetName(), *CrystalName, Attempt, MAX_ATTEMPTS);
            return;
        }
    }

    // All attempts luck-skipped. Surface the luck stat so we know whether the
    // cap needs raising for high-luck actors.
    float RawLuck = -1.0f;
    if (UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>())
    {
        RawLuck = CharComp->GetEquipmentModifiedLuck();
    }
    UE_LOG(LogTemp, Warning,
           TEXT("[Debug.BreakActiveCrystal] All %d attempts luck-skipped on %s's crystal '%s' (EquipmentModifiedLuck=%.2f). Try again, or raise MAX_ATTEMPTS if this recurs."),
           MAX_ATTEMPTS, *Actor->GetName(), *CrystalName, RawLuck);
}

void UCrystalManager::DebugForceWearActiveCrystal(int32 Amount)
{
    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug.ForceWearActiveCrystal] No TurnManager subsystem."));
        return;
    }

    AActor *Actor = TurnMgr->GetCurrentActor();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Debug.ForceWearActiveCrystal] No active character (combat not active?)."));
        return;
    }

    ULoadoutComponent *LC = GetLoadoutComponent(Actor);
    if (!LC)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.ForceWearActiveCrystal] No LoadoutComponent on %s."),
               *Actor->GetName());
        return;
    }

    UWeaponData *Weapon = LC->GetPrimaryWeapon();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.ForceWearActiveCrystal] %s has no primary weapon equipped."),
               *Actor->GetName());
        return;
    }

    FRuntimeAttachedItem *Attachment = LC->FindAttachedItemByHolder(Weapon);
    if (!Attachment || Attachment->IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.ForceWearActiveCrystal] %s's primary weapon has no attached crystal."),
               *Actor->GetName());
        return;
    }

    // Identify the branch + the crystal — used in every log line below so the
    // bCanBreak flip is unambiguous in the Output Log.
    const bool bIsEvolutionBranch = Attachment->IsEvolution();
    const TCHAR *BranchLabel = bIsEvolutionBranch ? TEXT("EVOLUTION") : TEXT("REFINED");
    const FString CrystalName = bIsEvolutionBranch
                                    ? (Attachment->Evolution.Item ? Attachment->Evolution.Item->GetFullItemName() : TEXT("(null)"))
                                    : CrystalIdentity::GetDisplayName(Attachment->Refined.Id);

    // Evolution-only: bCanBreak is what the refactor flipped. Refined branch
    // is always wearable (no gate), so log "n/a" there for clarity.
    const FString CanBreakStr = bIsEvolutionBranch
                                    ? (Attachment->Evolution.Item
                                           ? (Attachment->Evolution.Item->bCanBreak ? TEXT("true") : TEXT("false"))
                                           : TEXT("(null item)"))
                                    : TEXT("n/a (refined — always wearable)");

    const int32 BeforeDur = Attachment->GetCurrentDurability();
    const int32 MaxDur = Attachment->GetMaxDurability();
    const bool bBeforeBroken = Attachment->IsBroken();

    UE_LOG(LogTemp, Display,
           TEXT("[Debug.ForceWearActiveCrystal] BEFORE — Actor=%s, Branch=%s, Crystal='%s', bCanBreak=%s, Durability=%d/%d, IsBroken=%d, Amount=%d"),
           *Actor->GetName(), BranchLabel, *CrystalName, *CanBreakStr,
           BeforeDur, MaxDur, bBeforeBroken ? 1 : 0, Amount);

    // Direct ApplyWear — no BreakCalculator math, no Luck skip. The point of
    // this hook is to exercise the bCanBreak gate and the durability
    // transition only.
    const bool bBrokeThisWear = Attachment->ApplyWear(Amount);
    const int32 AfterDur = Attachment->GetCurrentDurability();
    const bool bAfterBroken = Attachment->IsBroken();
    const bool bWearApplied = AfterDur != BeforeDur;

    UE_LOG(LogTemp, Display,
           TEXT("[Debug.ForceWearActiveCrystal] AFTER  — Durability=%d/%d, WearApplied=%d, BrokeThisCall=%d, IsBroken=%d"),
           AfterDur, MaxDur, bWearApplied ? 1 : 0, bBrokeThisWear ? 1 : 0, bAfterBroken ? 1 : 0);

    if (bIsEvolutionBranch && !bWearApplied && !bBeforeBroken)
    {
        UE_LOG(LogTemp, Display,
               TEXT("[Debug.ForceWearActiveCrystal] Evolution wear was a no-op — expected when bCanBreak=false on '%s'."),
               *CrystalName);
    }
}

// ========================================
// WOR_ CONSOLE EXEC SUITE
// ========================================

void UCrystalManager::WOR_WearTable()
{
    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_WearTable] No TurnManager subsystem."));
        return;
    }

    AActor *Actor = TurnMgr->GetCurrentActor();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_WearTable] No active character (combat not active?)."));
        return;
    }

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->CharacterData)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_WearTable] %s has no CharacterDataComponent / CharacterData — cannot read crystal-modified pillars."),
               *Actor->GetName());
        return;
    }

    const float SpellDmgFrac   = CharComp->GetCrystalModifiedSpellDamage()           - 1.0f;
    const float StatusMultFrac = CharComp->GetCrystalModifiedStatusMultiplier()      - 1.0f;
    const float EfficiencyFrac = 1.0f - CharComp->GetCrystalModifiedEfficiencyMultiplier();
    const float ResistanceFrac = CharComp->GetCrystalModifiedResistance();

    UE_LOG(LogTemp, Display,
           TEXT("[WOR_WearTable] Actor=%s — substat-modified wear prediction (worst-case envelope: Action=S L2 Spell)"),
           *Actor->GetName());

    UBreakCalculatorDebug::PrintWearTableWithSubstats(
        SpellDmgFrac, StatusMultFrac, EfficiencyFrac, ResistanceFrac,
        EItemTier::S_Tier, /*InfusionLevel*/ 2, /*bIsSpell*/ true);
}

void UCrystalManager::WOR_SimCast(int32 ActionTier, int32 InfusionLevel)
{
    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_SimCast] No TurnManager subsystem."));
        return;
    }

    AActor *Actor = TurnMgr->GetCurrentActor();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_SimCast] No active character (combat not active?)."));
        return;
    }

    ULoadoutComponent *LC = GetLoadoutComponent(Actor);
    if (!LC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_SimCast] No LoadoutComponent on %s."), *Actor->GetName());
        return;
    }

    UWeaponData *Weapon = LC->GetPrimaryWeapon();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_SimCast] %s has no primary weapon equipped."),
               *Actor->GetName());
        return;
    }

    FRuntimeAttachedItem *Attachment = LC->FindAttachedItemByHolder(Weapon);
    if (!Attachment || Attachment->IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_SimCast] %s's primary weapon has no attached crystal."),
               *Actor->GetName());
        return;
    }
    if (!Attachment->IsRefined())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_SimCast] %s's primary weapon crystal is not refined (evolution / immune — does not wear)."),
               *Actor->GetName());
        return;
    }
    if (Attachment->IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_SimCast] %s's primary weapon crystal is already broken."),
               *Actor->GetName());
        return;
    }

    const int32 TierClamped = FMath::Clamp(ActionTier, 0, static_cast<int32>(EItemTier::S_Tier));
    const int32 InfClamped  = FMath::Clamp(InfusionLevel, 0, 2);
    const EItemTier ActionTierE = static_cast<EItemTier>(TierClamped);
    constexpr bool bIsSpell = true;

    UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    const float SpellDmgFrac   = CharComp ? (CharComp->GetCrystalModifiedSpellDamage()      - 1.0f) : 0.0f;
    const float StatusMultFrac = CharComp ? (CharComp->GetCrystalModifiedStatusMultiplier() - 1.0f) : 0.0f;
    const float EfficiencyFrac = CharComp ? (1.0f - CharComp->GetCrystalModifiedEfficiencyMultiplier()) : 0.0f;
    const float ResistanceFrac = CharComp ? CharComp->GetCrystalModifiedResistance() : 0.0f;

    const FDurabilityWearWithSubstatsResult Predict =
        UBreakCalculator::CalculateDurabilityWearWithSubstatsDetailed(
            Attachment->Refined.Id.Tier, ActionTierE, InfClamped, bIsSpell,
            SpellDmgFrac, StatusMultFrac, EfficiencyFrac, ResistanceFrac);

    const FString CrystalName = CrystalIdentity::GetDisplayName(Attachment->Refined.Id);
    const int32 BeforeDur = Attachment->GetCurrentDurability();
    const int32 MaxDur    = Attachment->GetMaxDurability();

    UE_LOG(LogTemp, Display,
           TEXT("[WOR_SimCast] BEFORE — Actor=%s, Crystal='%s' (Tier=%s), Action=%s L%d Spell, Dur=%d/%d"),
           *Actor->GetName(), *CrystalName,
           *TierHelpers::GetTierName(Attachment->Refined.Id.Tier),
           *TierHelpers::GetTierName(ActionTierE),
           InfClamped, BeforeDur, MaxDur);

    UE_LOG(LogTemp, Display,
           TEXT("[WOR_SimCast] PREDICT — Base=%d, PowerF=%.2f, CtrlF=%.2f, TierGap=%d, Final=%d (substats: SpellDmg=%+.2f StatusMult=%+.2f Efficiency=%+.2f Resistance=%+.2f)"),
           Predict.BaseWear, Predict.PowerFactor, Predict.ControlFactor,
           Predict.TierGap, Predict.FinalWear,
           SpellDmgFrac, StatusMultFrac, EfficiencyFrac, ResistanceFrac);

    ProcessPostCastWear(Actor, Weapon, *Attachment, ActionTierE, InfClamped, bIsSpell);

    const int32 AfterDur = Attachment->GetCurrentDurability();
    const int32 WearApplied = BeforeDur - AfterDur;

    UE_LOG(LogTemp, Display,
           TEXT("[WOR_SimCast] AFTER  — Dur=%d/%d, WearApplied=%d, IsBroken=%d"),
           AfterDur, MaxDur, WearApplied, Attachment->IsBroken() ? 1 : 0);

    if (WearApplied == 0 && Predict.FinalWear > 0)
    {
        UE_LOG(LogTemp, Display,
               TEXT("[WOR_SimCast] Note — predicted %d wear but live path applied 0 (luck-skip inside ProcessPostCastWear)."),
               Predict.FinalWear);
    }
}

void UCrystalManager::WOR_CrystalState()
{
    UTurnManager *TurnMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTurnManager>() : nullptr;
    if (!TurnMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_CrystalState] No TurnManager subsystem."));
        return;
    }

    AActor *Actor = TurnMgr->GetCurrentActor();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_CrystalState] No active character (combat not active?)."));
        return;
    }

    ULoadoutComponent *LC = GetLoadoutComponent(Actor);
    if (!LC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WOR_CrystalState] No LoadoutComponent on %s."), *Actor->GetName());
        return;
    }

    UWeaponData *Weapon = LC->GetPrimaryWeapon();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_CrystalState] %s has no primary weapon equipped."),
               *Actor->GetName());
        return;
    }

    FRuntimeAttachedItem *Attachment = LC->FindAttachedItemByHolder(Weapon);
    if (!Attachment || Attachment->IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WOR_CrystalState] %s's primary weapon has no attached crystal."),
               *Actor->GetName());
        return;
    }

    const bool bEvolution = Attachment->IsEvolution();
    const TCHAR *TypeStr = bEvolution
                               ? TEXT("Evolution")
                               : (Attachment->IsRefined() ? TEXT("Refined") : TEXT("None"));
    const FString Name = bEvolution
                             ? (Attachment->Evolution.Item ? Attachment->Evolution.Item->GetFullItemName() : TEXT("(null)"))
                             : CrystalIdentity::GetDisplayName(Attachment->Refined.Id);
    const FString TierStr = bEvolution
                                ? FString(TEXT("n/a"))
                                : TierHelpers::GetTierName(Attachment->Refined.Id.Tier);
    const FString CanBreakStr = bEvolution
                                    ? (Attachment->Evolution.Item
                                           ? (Attachment->Evolution.Item->bCanBreak ? TEXT("true") : TEXT("false"))
                                           : TEXT("(null item)"))
                                    : TEXT("true (refined — always wearable)");

    UE_LOG(LogTemp, Display,
           TEXT("[WOR_CrystalState] Actor=%s, Type=%s, Name='%s', Tier=%s, bCanBreak=%s, Durability=%d/%d, IsBroken=%d"),
           *Actor->GetName(), TypeStr, *Name, *TierStr, *CanBreakStr,
           Attachment->GetCurrentDurability(), Attachment->GetMaxDurability(),
           Attachment->IsBroken() ? 1 : 0);
}
