// Source/world_of_refraction/Private/CrystalManager.cpp
#include "Equipment/Crystals/CrystalManager.h"

#include "Equipment/Crystals/EvolutionItemData.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Durability/BreakCalculator.h"
#include "Equipment/Durability/BreakCalculatorDebug.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Combat/TurnManager.h"
#include "Equipment/Weapons/WeaponData.h"
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
    if (!Attachment.IsCrystal())
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
        // Snapshot the wear-input stats in one call. All four substats are the FULL
        // composed values (innate + equipment + stone + transient) via GetEffectiveStats;
        // wear is a POWER term, so a geared/buffed caster wears crystals faster — more
        // spell power / status amp / control = more strain.
        const FEffectiveStats Stats = CasterCharComp->GetEffectiveStats();
        const float SpellDmgFrac    = Stats.SpellDamage - 1.0f;
        const float StatusMultFrac  = Stats.StatusMultiplier - 1.0f;
        const float EfficiencyFrac  = 1.0f - Stats.EfficiencyMultiplier;
        const float ResistanceFrac  = Stats.Resistance;
        Wear = UBreakCalculator::CalculateDurabilityWearWithSubstats(
            Attachment.Crystal.Id.Tier,
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
            Attachment.Crystal.Id.Tier,
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
        // Upper-clamp only (positive luck plateaus at LUCK_BREAK_SKIP_MAX); negative
        // luck (curse) yields a negative SkipChance, and FRand() in [0,1) is never
        // < negative, so a cursed wielder never lucky-skips — no lower clamp needed.
        const float SkipChance = FMath::Min(RawLuck / CombatConstants::LUCK_RAW_MAX, 1.0f) * CombatConstants::LUCK_BREAK_SKIP_MAX;
        if (FMath::FRand() < SkipChance)
        {
            UE_LOG(LogTemp, Log,
                   TEXT("[CrystalManager] %s LUCKY break skip on crystal '%s' (would have applied %d wear, skip chance %.2f)"),
                   *Actor->GetName(),
                   *ItemIdentity::GetDisplayName(Attachment.Crystal.Id),
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
           *ItemIdentity::GetDisplayName(Attachment.Crystal.Id),
           NewDur, MaxDur,
           static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

    // Broadcast post-wear durability for real-time UI updates.
    OnCrystalDurabilityChanged.Broadcast(Actor, Holder, NewDur, MaxDur);

    if (bBroke)
    {
        UE_LOG(LogTemp, Log,
               TEXT("[CrystalManager] Crystal '%s' broke on %s (holder: %s)"),
               *ItemIdentity::GetDisplayName(Attachment.Crystal.Id),
               *Actor->GetName(),
               Holder ? *Holder->GetName() : TEXT("Unknown"));

        FBrokenCrystalPayload Payload;
        Payload.Kind = EAttachedItemKind::Crystal;
        Payload.CrystalId = Attachment.Crystal.Id;
        OnCrystalBroken.Broadcast(Actor, Holder, Payload);
    }
}

void UCrystalManager::ProcessPostCastEvolutionWear(
    AActor *Actor,
    ULoadoutComponent *LC,
    EItemTier ActionTier,
    int32 InfusionLevel,
    bool bIsSpell)
{
    if (!Actor || !LC)
    {
        return;
    }

    const FCombatLoadout Loadout = LC->GetActiveLoadout();
    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution || !Loadout.PrimaryEvolution.Item)
    {
        return;
    }

    const EItemTier EvoTier = Loadout.PrimaryEvolution.Item->Tier;

    UCharacterDataComponent *CasterCharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
    int32 Wear = 0;
    if (CasterCharComp && CasterCharComp->CharacterData)
    {
        // Snapshot the wear-input stats in one call. All four substats are the FULL
        // composed values (innate + equipment + stone + transient) via GetEffectiveStats;
        // wear is a POWER term, so a geared/buffed caster wears crystals faster — more
        // spell power / status amp / control = more strain.
        const FEffectiveStats Stats = CasterCharComp->GetEffectiveStats();
        const float SpellDmgFrac    = Stats.SpellDamage - 1.0f;
        const float StatusMultFrac  = Stats.StatusMultiplier - 1.0f;
        const float EfficiencyFrac  = 1.0f - Stats.EfficiencyMultiplier;
        const float ResistanceFrac  = Stats.Resistance;
        Wear = UBreakCalculator::CalculateDurabilityWearWithSubstats(
            EvoTier,
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
            EvoTier,
            ActionTier,
            InfusionLevel,
            bIsSpell);
    }

    if (Wear <= 0)
    {
        return;
    }

    const int32 BeforeDur = Loadout.PrimaryEvolution.CurrentDurability;
    const int32 MaxDur = Loadout.PrimaryEvolution.Item->MaxDurability;
    const FString EvoName = Loadout.PrimaryEvolution.Item->GetFullItemName();

    // BD's mechanic is intrinsic — bypass the per-asset bCanBreak gate so the
    // evolution can wear even when the asset wasn't explicitly opted-in. This
    // function is only called from the BD branch of ApplyCommitCosts, so the
    // force flag is safe to hard-code here. FEvolutionAttachment itself stays
    // BD-agnostic; the override is expressed via the bForceWear parameter.
    const bool bBroke = LC->ApplyWearToActivePrimaryEvolution(Wear, /*bForceWear=*/true);

    const FCombatLoadout AfterLoadout = LC->GetActiveLoadout();
    const int32 AfterDur = AfterLoadout.PrimaryEvolution.CurrentDurability;

    UE_LOG(LogTemp, Verbose,
           TEXT("[CrystalManager] %s applies %d evolution wear to '%s' (%d -> %d / %d)%s [ActionTier=%d L%d bIsSpell=%d]"),
           *Actor->GetName(), Wear, *EvoName,
           BeforeDur, AfterDur, MaxDur,
           bBroke ? TEXT(" BROKE") : TEXT(""),
           static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

    if (bBroke)
    {
        UE_LOG(LogTemp, Log,
               TEXT("[CrystalManager] Evolution '%s' broke on %s (between-combat sweep will clear slot)"),
               *EvoName, *Actor->GetName());
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

    const FString CrystalName = Attachment->IsCrystal()
                                    ? ItemIdentity::GetDisplayName(Attachment->Crystal.Id)
                                    : (Attachment->Evolution.Item ? Attachment->Evolution.Item->GetFullItemName() : TEXT("(null)"));

    if (Attachment->IsBroken())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Debug.BreakActiveCrystal] %s's primary weapon crystal '%s' is already broken."),
               *Actor->GetName(), *CrystalName);
        return;
    }
    if (!Attachment->IsCrystal())
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
    const TCHAR *BranchLabel = bIsEvolutionBranch ? TEXT("EVOLUTION") : TEXT("CRYSTAL");
    const FString CrystalName = bIsEvolutionBranch
                                    ? (Attachment->Evolution.Item ? Attachment->Evolution.Item->GetFullItemName() : TEXT("(null)"))
                                    : ItemIdentity::GetDisplayName(Attachment->Crystal.Id);

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

    // Wear-input snapshot (one call). SpellDamage = the FULL composed scalar (innate +
    // equipment + stone + transient); wear is a POWER term, so more spell power = more wear.
    const FEffectiveStats Stats = CharComp->GetEffectiveStats();
    const float SpellDmgFrac   = Stats.SpellDamage            - 1.0f;
    const float StatusMultFrac = Stats.StatusMultiplier       - 1.0f;
    const float EfficiencyFrac = 1.0f - Stats.EfficiencyMultiplier;
    const float ResistanceFrac = Stats.Resistance;

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
    if (!Attachment->IsCrystal())
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
    // Wear-input snapshot. A null CharComp yields a default FEffectiveStats
    // (SpellDamage/StatusMultiplier/EfficiencyMultiplier = 1, Resistance = 0), so the
    // fracs below resolve to 0/0/0/0 — the same no-substat baseline as the prior
    // `: 0.0f` branch. SpellDamage = the FULL composed scalar (wear is a POWER term).
    const FEffectiveStats Stats = CharComp ? CharComp->GetEffectiveStats() : FEffectiveStats();
    const float SpellDmgFrac   = Stats.SpellDamage - 1.0f;
    const float StatusMultFrac = Stats.StatusMultiplier - 1.0f;
    const float EfficiencyFrac = 1.0f - Stats.EfficiencyMultiplier;
    const float ResistanceFrac = Stats.Resistance;

    const FDurabilityWearWithSubstatsResult Predict =
        UBreakCalculator::CalculateDurabilityWearWithSubstatsDetailed(
            Attachment->Crystal.Id.Tier, ActionTierE, InfClamped, bIsSpell,
            SpellDmgFrac, StatusMultFrac, EfficiencyFrac, ResistanceFrac);

    const FString CrystalName = ItemIdentity::GetDisplayName(Attachment->Crystal.Id);
    const int32 BeforeDur = Attachment->GetCurrentDurability();
    const int32 MaxDur    = Attachment->GetMaxDurability();

    UE_LOG(LogTemp, Display,
           TEXT("[WOR_SimCast] BEFORE — Actor=%s, Crystal='%s' (Tier=%s), Action=%s L%d Spell, Dur=%d/%d"),
           *Actor->GetName(), *CrystalName,
           *TierHelpers::GetTierName(Attachment->Crystal.Id.Tier),
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
                               : (Attachment->IsCrystal() ? TEXT("Crystal") : TEXT("None"));
    const FString Name = bEvolution
                             ? (Attachment->Evolution.Item ? Attachment->Evolution.Item->GetFullItemName() : TEXT("(null)"))
                             : ItemIdentity::GetDisplayName(Attachment->Crystal.Id);
    const FString TierStr = bEvolution
                                ? FString(TEXT("n/a"))
                                : TierHelpers::GetTierName(Attachment->Crystal.Id.Tier);
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
