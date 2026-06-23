// EconomyService.cpp

#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/ItemIdentity.h"
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
