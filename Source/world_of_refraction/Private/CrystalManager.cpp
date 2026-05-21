// Source/world_of_refraction/Private/CrystalManager.cpp
#include "CrystalManager.h"

#include "EvolutionItemData.h"
#include "LoadoutComponent.h"
#include "FRuntimeAttachedItem.h"
#include "CrystalIdentity.h"
#include "BreakCalculator.h"
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

    // Only refined attachments wear/break. Evolution items are immune by design
    // (see FEvolutionAttachment::IsBroken — checks bImmuneToBreaking).
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
    const int32 Wear = UBreakCalculator::CalculateDurabilityWear(
        Attachment.Refined.Id.Tier,
        ActionTier,
        InfusionLevel,
        bIsSpell);

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
