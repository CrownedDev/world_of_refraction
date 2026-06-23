// EconomyService.cpp

#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "GameFramework/Actor.h"

bool UEconomyService::DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count, bool bRefined)
{
    if (!Owner || Count <= 0)
    {
        return false;
    }

    // Server-authoritative action. A standalone (PIE) actor already reports authority, so this
    // single check mirrors the HasServerAuthority pattern (NM_Standalone || HasAuthority).
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    // Resolve the two components off the owner — the service couples them, they don't couple to
    // each other.
    UCrystalInventoryComponent *CrystalInv = Owner->FindComponentByClass<UCrystalInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!CrystalInv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: %s missing %s%s"),
               *Owner->GetName(),
               CrystalInv ? TEXT("") : TEXT("CrystalInventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Availability in the chosen pool (Item vs Refined).
    const int32 Available = bRefined ? CrystalInv->GetRefinedCount(Id) : CrystalInv->GetItemCount(Id);
    if (Available < Count)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: %s has %d/%d %s (%s pool) — insufficient"),
               *Owner->GetName(), Available, Count, *ItemIdentity::GetDisplayName(Id),
               bRefined ? TEXT("refined") : TEXT("item"));
        return false;
    }

    const EEssenceType EssenceType = EconomyYield::ResolveEssenceType(Id);

    // REMOVE FIRST — a failed/partial removal must never grant phantom essence.
    const int32 Removed = bRefined ? CrystalInv->RemoveRefinedCount(Id, Count)
                                   : CrystalInv->RemoveItemCount(Id, Count);
    if (Removed <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleCrystal: removal returned %d for %s — no grant"),
               Removed, *ItemIdentity::GetDisplayName(Id));
        return false;
    }

    // Grant essence sized to what was ACTUALLY removed (== Count after the availability check;
    // using Removed keeps the grant honest if removal ever clamps below Count).
    const int32 Yield = EconomyYield::GetTypedEssenceYieldForTier(Id.Tier) * Removed;
    Currency->AddEssenceType(EssenceType, Yield);

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled %dx %s (%s pool) -> %d essence (type %s)"),
           Removed, *ItemIdentity::GetDisplayName(Id), bRefined ? TEXT("refined") : TEXT("item"),
           Yield, *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(EssenceType)));
    return true;
}

bool UEconomyService::DismantleWeapon(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Find the owned instance and capture its (asset) tier BEFORE removal — the entry pointer
    // dangles once the array is mutated, so nothing past the remove dereferences it. Leveled-tier
    // is deferred: there is no per-instance tier yet, so dismantle yields off the base asset tier.
    const FWeaponInventoryEntry *Entry = Inv->Weapons.FindByPredicate(
        [&PersistentID](const FWeaponInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: %s has no weapon instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }
    const EItemTier Tier = Entry->Weapon->Tier;
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — a failed removal must never grant phantom essence.
    if (!Inv->RemoveWeaponByPersistentID(PersistentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleWeapon: removal failed for GUID %s — no grant"),
               *PersistentID.ToString());
        return false;
    }

    Currency->AddGearEssence(Yield); // weapons/rings → Gear essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled weapon (GUID %s, tier %d) -> %d Gear essence"),
           *PersistentID.ToString(), static_cast<int32>(Tier), Yield);
    return true;
}

bool UEconomyService::DismantleRing(AActor *Owner, FGuid PersistentID)
{
    if (!Owner)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const FRingInventoryEntry *Entry = Inv->Rings.FindByPredicate(
        [&PersistentID](const FRingInventoryEntry &E) { return E.PersistentID == PersistentID; });
    if (!Entry || !Entry->Ring)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: %s has no ring instance for GUID %s (or null asset)"),
               *Owner->GetName(), *PersistentID.ToString());
        return false;
    }
    const EItemTier Tier = Entry->Ring->Tier;
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    if (!Inv->RemoveRingByPersistentID(PersistentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleRing: removal failed for GUID %s — no grant"),
               *PersistentID.ToString());
        return false;
    }

    Currency->AddGearEssence(Yield); // weapons/rings → Gear essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled ring (GUID %s, tier %d) -> %d Gear essence"),
           *PersistentID.ToString(), static_cast<int32>(Tier), Yield);
    return true;
}

bool UEconomyService::DismantleSpell(AActor *Owner, USpellData *Spell)
{
    if (!Owner || !Spell)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleSpell: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleSpell: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const EItemTier Tier = Spell->Tier; // asset tier (leveled-tier deferred — no tier-on-instance)
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — UnlearnSpell's bool return IS the success signal: false = the spell was not
    // known, so a not-owned spell never grants phantom essence (no HasSpell pre-check needed).
    if (!Inv->UnlearnSpell(Spell))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleSpell: %s did not know %s — no grant"),
               *Owner->GetName(), *Spell->GetName());
        return false;
    }

    Currency->AddSkillEssence(Yield); // spells/abilities → Skill essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled spell %s (tier %d) -> %d Skill essence"),
           *Spell->GetName(), static_cast<int32>(Tier), Yield);
    return true;
}

bool UEconomyService::DismantleAbility(AActor *Owner, UAbilityData *Ability)
{
    if (!Owner || !Ability)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleAbility: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleAbility: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    const EItemTier Tier = Ability->Tier; // asset tier (leveled-tier deferred — no tier-on-instance)
    const int32 Yield = EconomyYield::GetLevelingEssenceYieldForTier(Tier);

    // REMOVE FIRST — UnlearnAbility's bool return IS the success signal (false = not known).
    if (!Inv->UnlearnAbility(Ability))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] DismantleAbility: %s did not know %s — no grant"),
               *Owner->GetName(), *Ability->GetName());
        return false;
    }

    Currency->AddSkillEssence(Yield); // spells/abilities → Skill essence
    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Dismantled ability %s (tier %d) -> %d Skill essence"),
           *Ability->GetName(), static_cast<int32>(Tier), Yield);
    return true;
}

// ==================== PURCHASE (spend-side) ====================

bool UEconomyService::PurchaseSpell(AActor *Owner, USpellData *Spell)
{
    if (!Owner || !Spell)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // ---- Compute cost (typed essence accumulated per type; Prisms = base + scaling surcharge) ----
    TMap<EEssenceType, int32> EssenceCost;
    // (a1) element essence at the spell's own tier.
    EssenceCost.FindOrAdd(EconomyYield::ElementToEssenceType(Spell->Element)) +=
        EconomyYield::GetTypedEssencePurchaseCostForTier(Spell->Tier);

    // (b) Prisms base by spell tier.
    int32 PrismsCost = EconomyYield::GetPrismsBaseForTier(Spell->Tier);

    for (const FStatScaling &Entry : Spell->StatScaling)
    {
        if (Entry.Stat == ESubStat::None)
        {
            continue;
        }
        // (a2) pillar essence at each scaling grade (same numbers as the tier buy row, §4.3) —
        //      entries sharing a pillar accumulate.
        EssenceCost.FindOrAdd(EconomyYield::SubStatToPillarEssence(Entry.Stat)) +=
            EconomyYield::GetTypedEssencePurchaseCostForTier(EconomyYield::ScalingGradeToItemTier(Entry.Tier));
        // (c) Prisms scaling surcharge: 50 × grade-number.
        PrismsCost += EconomyYield::Constants::PRISMS_SCALING_SURCHARGE_PER_GRADE *
                      EconomyYield::GetScalingGradeNumber(Entry.Tier);
    }

    // ---- CanAfford ALL components — spend nothing if any is short ----
    for (const TPair<EEssenceType, int32> &Pair : EssenceCost)
    {
        if (!Currency->CanAfford(ECurrencyType::EssenceTyped, Pair.Value, static_cast<uint8>(Pair.Key)))
        {
            UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: %s cannot afford %d %s essence for %s"),
                   *Owner->GetName(), Pair.Value,
                   *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key)),
                   *Spell->GetName());
            return false;
        }
    }
    if (!Currency->CanAfford(ECurrencyType::Prisms, PrismsCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: %s cannot afford %d Prisms for %s"),
               *Owner->GetName(), PrismsCost, *Spell->GetName());
        return false;
    }

    // ---- Spend ALL (CanAfford already cleared every component) ----
    for (const TPair<EEssenceType, int32> &Pair : EssenceCost)
    {
        Currency->SpendEssenceType(Pair.Key, Pair.Value);
    }
    Currency->Spend(ECurrencyType::Prisms, PrismsCost);

    // ---- Grant; refund EVERYTHING on grant-failure (e.g. at spell capacity) ----
    if (!Inv->LearnSpell(Spell))
    {
        for (const TPair<EEssenceType, int32> &Pair : EssenceCost)
        {
            Currency->AddEssenceType(Pair.Key, Pair.Value);
        }
        Currency->Add(ECurrencyType::Prisms, PrismsCost);
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseSpell: LearnSpell failed for %s — refunded %d Prisms + essence"),
               *Spell->GetName(), PrismsCost);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Purchased spell %s (tier %d) for %d Prisms + typed essence"),
           *Spell->GetName(), static_cast<int32>(Spell->Tier), PrismsCost);
    return true;
}

bool UEconomyService::PurchaseWeapon(AActor *Owner, UWeaponData *Weapon)
{
    if (!Owner || !Weapon)
    {
        return false;
    }
    if (!Owner->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: no authority on %s — ignored"),
               *Owner->GetName());
        return false;
    }

    UInventoryComponent *Inv = Owner->FindComponentByClass<UInventoryComponent>();
    UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();
    if (!Inv || !Currency)
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: %s missing %s%s"),
               *Owner->GetName(),
               Inv ? TEXT("") : TEXT("InventoryComponent "),
               Currency ? TEXT("") : TEXT("CurrencyComponent"));
        return false;
    }

    // Equipment pricing: Prisms base by tier only (no essence, no surcharge).
    const int32 PrismsCost = EconomyYield::GetPrismsBaseForTier(Weapon->Tier);
    if (!Currency->CanAfford(ECurrencyType::Prisms, PrismsCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: %s cannot afford %d Prisms for %s"),
               *Owner->GetName(), PrismsCost, *Weapon->GetName());
        return false;
    }

    Currency->Spend(ECurrencyType::Prisms, PrismsCost);
    if (!Inv->AddWeapon(Weapon))
    {
        Currency->Add(ECurrencyType::Prisms, PrismsCost); // refund on grant-failure (e.g. at capacity)
        UE_LOG(LogTemp, Warning, TEXT("[EconomyService] PurchaseWeapon: AddWeapon failed for %s — refunded %d Prisms"),
               *Weapon->GetName(), PrismsCost);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[EconomyService] Purchased weapon %s (tier %d) for %d Prisms"),
           *Weapon->GetName(), static_cast<int32>(Weapon->Tier), PrismsCost);
    return true;
}
