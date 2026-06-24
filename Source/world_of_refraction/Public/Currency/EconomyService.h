// EconomyService.h
// Cross-system orchestrator for the dismantle / purchase economy. A GameInstanceSubsystem
// (matches the DamageCalculator / ActionExecutor pattern per CLAUDE.md) so it can be reached
// globally and coordinate an owner's inventory <-> wallet WITHOUT coupling those components to
// each other — it resolves both off the owner actor via FindComponentByClass and drives them.
//
// Yield math + essence-type resolution live in the stateless EconomyYield helper; this service
// owns the orchestration (authority gate, availability check, remove-then-grant ordering).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "EconomyService.generated.h"

class AActor;
class UCrystalInventoryComponent;
class UCurrencyComponent;
class UInventoryComponent;
class UEvolutionInventoryComponent;
class USpellData;
class UAbilityData;
class UWeaponData;

UCLASS()
class WORLD_OF_REFRACTION_API UEconomyService : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Dismantle Count crystals/stones of Id from Owner's chosen pool (Item vs Refined per
     * bRefined), granting typed essence per EconomyYield. Server-authoritative.
     *
     * Order is REMOVE-then-GRANT: a failed/insufficient removal never grants phantom essence,
     * and the grant is sized to what was actually removed.
     *
     * Returns false if: Owner null / Count<=0; no authority; CrystalInventoryComponent or
     * CurrencyComponent missing; insufficient count in the chosen pool; or removal returned 0.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count = 1, bool bRefined = false);

    /**
     * Merge crystals/stones UP to a TARGET tier (§4.5, value-based): produce 1 of {Type, TargetTier}
     * by consuming same-Type crystals from BELOW it, lowest-first, until their summed F-unit value
     * (GetCrystalValue) meets-or-exceeds the target's value. Keyed by Type + TargetTier — the player
     * asks for "a [TargetTier] [Type]" and the merge picks which lower crystals to spend. Same Type
     * (element+variant preserved), same pool. Costs Prisms (GetMergeCostForTier(TargetTier) — half
     * the §5 buy price), no essence/Gold. Item-crystals + stones only; evolution crystals are
     * structurally unrepresentable as FCrystalId so they can never reach this pool (only invalid
     * Type is None).
     *
     * Server-authoritative; spend+remove-first then add, with full refund if the add fails. Returns
     * false if: Owner null / no authority; CrystalInventory or Currency missing; Type is None;
     * TargetTier is F (the floor — not a merge target); total available value below the target is
     * insufficient; Prisms unaffordable; or the target tier is at cap.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool MergeCrystals(AActor *Owner, ECrystalType Type, EItemTier TargetTier, bool bRefined = false);

    /**
     * Dismantle the owned WEAPON instance identified by PersistentID, granting GEAR essence
     * (weapons/rings → Gear, per category routing). Server-authoritative; remove-then-grant.
     * Yield is from the item's ASSET tier (leveled-tier deferred until tier-on-instance exists).
     * Returns false if Owner null / no authority; InventoryComponent or CurrencyComponent
     * missing; or no owned weapon carries that GUID.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleWeapon(AActor *Owner, FGuid PersistentID);

    /** Dismantle the owned RING instance by PersistentID → GEAR essence. See DismantleWeapon. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleRing(AActor *Owner, FGuid PersistentID);

    /**
     * Dismantle the owned EVOLUTION instance by InstanceID (NOT PersistentID — evolution identity).
     * HYBRID yield: the evolution's ELEMENT essence TYPE (GetAssociatedElement → ElementToEssenceType)
     * at the GEAR leveling AMOUNT (GetLevelingEssenceYieldForTier on the INSTANCE tier — the §3 gear
     * curve F5..S145, NOT the §4.2 crystal yield). Server-authoritative; remove-then-grant via
     * UEvolutionInventoryComponent::RemoveInstance. Also the destroy path reused by primary-removal
     * and (future) break-on-zero-durability. False if Owner null / no authority; Evolution-inventory
     * or Currency component missing; or no owned evolution carries that InstanceID.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleEvolution(AActor *Owner, FGuid InstanceID);

    /**
     * Dismantle a known SPELL, granting SKILL essence (abilities/spells → Skill, per category
     * routing — NOT Gear). Server-authoritative; remove-then-grant (UnlearnSpell's bool return
     * is the success signal — a not-known spell grants nothing). Yield from the spell's ASSET
     * tier (leveled-tier deferred). False if Owner/Spell null; no authority; components missing;
     * or the spell was not known.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleSpell(AActor *Owner, USpellData *Spell);

    /** Dismantle a known ABILITY → SKILL essence. See DismantleSpell. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleAbility(AActor *Owner, UAbilityData *Ability);

    // ==================== PURCHASE (spend-side) ====================

    /**
     * Buy a SPELL into Owner's inventory. Cost (§4.4 + §5): Prisms base (spell tier) + Prisms
     * scaling surcharge (50 × Σ grade-number) + typed essence (element @ spell tier + Σ pillar @
     * each scaling grade). Canonical flow: CanAfford ALL → Spend ALL → LearnSpell → refund
     * everything on grant-failure. Server-authoritative. False if Owner/Spell null; no authority;
     * components missing; any component unaffordable (spends nothing); or the grant failed.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool PurchaseSpell(AActor *Owner, USpellData *Spell);

    /**
     * Buy a WEAPON into Owner's inventory. Cost: Prisms base (weapon tier) only — no essence, no
     * surcharge (equipment pricing). Spend → AddWeapon → refund on grant-failure. See PurchaseSpell.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool PurchaseWeapon(AActor *Owner, UWeaponData *Weapon);

    // ==================== LEVELING (instance tier-up, spend-side) ====================

    /**
     * Tier-up the owned WEAPON instance identified by PersistentID by ONE step, writing its
     * INSTANCE tier (Entry.Tier) — the first thing that WRITES the per-instance tier, so the
     * leveled weapon immediately dismantles for more, gets bigger loadout budgets, and hits
     * harder / wears differently in combat (all downstream reads already use instance tier).
     *
     * Cost (§5.3): Gear essence = GetTierUpCostForTier(currentTier); Reality essence = half that.
     * NO Gold. Canonical spend shape: CanAfford BOTH → Spend BOTH → write next tier → refund both
     * if the write cannot land. Server-authoritative.
     *
     * Returns false if: Owner null / no authority; Inventory or Currency component missing; no
     * owned weapon carries that GUID; the weapon is already S_Tier (max — can't level past S); or
     * either essence component is unaffordable (spends nothing).
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool LevelUpWeapon(AActor *Owner, FGuid PersistentID);

    /** Tier-up the owned RING instance by PersistentID by one step → writes Entry.Tier. See LevelUpWeapon. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool LevelUpRing(AActor *Owner, FGuid PersistentID);

    /**
     * Tier-up an owned EVOLUTION instance by one step → writes the FEvolutionInventoryEntry.Tier
     * on UEvolutionInventoryComponent (identity is InstanceID, not PersistentID). Levelable while
     * in INVENTORY or in the PRIMARY slot — the inventory entry persists when primary-slotted (the
     * slot is re-inflated from it by PrimaryEvolutionInstance), so leveling the entry propagates to
     * the slotted attachment on the next inflation. Gear-attached evolution is NOT covered (no owned
     * instance source yet). Same Gear + ½ Reality cost as weapons/rings via the shared core; see
     * LevelUpWeapon for the full contract. False if no owned evolution carries that InstanceID.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool LevelUpEvolution(AActor *Owner, FGuid InstanceID);

private:
    /**
     * Shared tier-up core — type-agnostic. Knows only the wallet + a reference to the instance's
     * tier field, so any per-instance struct that carries an EItemTier (weapon, ring, and later
     * evolution) reuses ALL the cost/cap/spend/write logic. The wrappers resolve their entry and
     * pass &Entry.Tier here.
     *
     * Reads InOutTier; if already S_Tier, returns false (S is max — can't level past S). Else
     * computes Gear cost (GetTierUpCostForTier) + half-cost Reality, requires BOTH affordable
     * (spends nothing if either short), spends both, then writes the next tier up into InOutTier.
     * Returns true only when the spend+write happened.
     */
    bool TryLevelUpEntry(UCurrencyComponent *Currency, EItemTier &InOutTier) const;
};
