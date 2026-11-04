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
#include "Merchant/MerchantData.h" // FMerchantStockEntry — the Purchase cart element
#include "Currency/CurrencyTypes.h" // ECurrencyType for the leveling-essence param on TryLevelUpEntry
#include "Character/EWorldPillar.h" // BuyWorldStat (§7 C4)
#include "EconomyService.generated.h"

class AActor;
class UCrystalInventoryComponent;
class UCurrencyComponent;
class UInventoryComponent;
class UEvolutionInventoryComponent;
class ULoadoutComponent;
class USpellData;
class UAbilityData;
class UWeaponData;

namespace PurchaseCostBalances
{
    /** B-333: the balances a purchase can charge, in display order. */
    inline constexpr ECurrencyType CHARGED[] = {ECurrencyType::Prisms, ECurrencyType::SkillEssence,
                                                ECurrencyType::ElementalEssence, ECurrencyType::PillarEssence};

    /** B-366c: the balance a cost line is paid from. The money line (listed as Prisms) is paid in the currency
     *  of the place (UEconomyService::GetPlaceCurrency); the essences are paid as listed. */
    inline ECurrencyType PaidFrom(ECurrencyType Listed, ECurrencyType Place)
    {
        return Listed == ECurrencyType::Prisms ? Place : Listed;
    }
}

/** Per-currency cost of a Purchase cart. B-333 (D-125): Prisms + Skill / Elemental / Pillar Essence,
 *  all scalar balances; Typed is OBSOLETE (B-333) and stays empty. Returned by PreviewCartCost so the
 *  shop UI displays exactly what Purchase will charge — one cost source. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPurchaseCost
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Economy")
    int32 Prisms = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy")
    int32 SkillEssence = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy")
    int32 ElementalEssence = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy")
    int32 PillarEssence = 0;

    /** OBSOLETE (B-333): nothing fills it now (D-125). Delete in the sweep (D-124). */
    UPROPERTY(BlueprintReadOnly, Category = "Economy")
    TMap<EEssenceType, int32> Typed;

    /** OBSOLETE (B-333). */
    void AddTyped(EEssenceType Type, int32 Amount) { Typed.FindOrAdd(Type) += Amount; }

    /** B-333: this cost in Currency; 0 for a balance a purchase never charges. */
    int32 Get(ECurrencyType Currency) const
    {
        switch (Currency)
        {
        case ECurrencyType::Prisms: return Prisms;
        case ECurrencyType::SkillEssence: return SkillEssence;
        case ECurrencyType::ElementalEssence: return ElementalEssence;
        case ECurrencyType::PillarEssence: return PillarEssence;
        default: return 0;
        }
    }

    /** B-333: add Amount of an essence (Skill / Elemental / Pillar) to this cost. */
    void AddEssence(ECurrencyType Currency, int32 Amount)
    {
        switch (Currency)
        {
        case ECurrencyType::SkillEssence: SkillEssence += Amount; break;
        case ECurrencyType::ElementalEssence: ElementalEssence += Amount; break;
        case ECurrencyType::PillarEssence: PillarEssence += Amount; break;
        default: break;
        }
    }
};

UCLASS()
class WORLD_OF_REFRACTION_API UEconomyService : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Q-252: false (and says "Not during a fight." on screen) while a fight is on. Every economy action
     *  checks it; paid repair (GearRepair::RepairParty) too. Op names the action in the log. */
    static bool IsOutOfCombat(const AActor *Owner, const TCHAR *Op);

    /**
     * Dismantle Count crystals/stones of Id from Owner's inventory, granting its essence (B-333: gem Elemental, stat stone Pillar, ability stone Skill) per
     * EconomyYield. Server-authoritative. The pool (gem vs stone) is dispatched by Id.Type inside
     * the component — there is no item/refined axis anymore (the gem-merge collapsed it).
     *
     * Order is REMOVE-then-GRANT: a failed/insufficient removal never grants phantom essence,
     * and the grant is sized to what was actually removed.
     *
     * Returns false if: Owner null / Count<=0; no authority; CrystalInventoryComponent or
     * CurrencyComponent missing; insufficient count; or removal returned 0.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleCrystal(AActor *Owner, const FCrystalId &Id, int32 Count = 1);

    /**
     * Merge crystals/stones UP to a TARGET tier (§4.5, value-based): produce 1 of {Type, TargetTier}
     * by consuming same-Type crystals from BELOW it, lowest-first, until their summed F-unit value
     * (GetCrystalValue) meets-or-exceeds the target's value. Keyed by Type + TargetTier — the player
     * asks for "a [TargetTier] [Type]" and the merge picks which lower crystals to spend. Same Type
     * (element+variant preserved), one pool. Costs Prisms (GetMergeCostForTier(TargetTier) — half
     * the §5 buy price), no essence/Gold. Crystals + stones only; the pool (Crystals vs Stones) is dispatched
     * by Type inside the component — no item/refined axis (the gem-merge collapsed it). Evolution
     * crystals are structurally unrepresentable as FCrystalId so they can never reach here (only
     * invalid Type is None). Any overshoot comes back as change: the fewest same-Type crystals below
     * the target (EconomyYield::GetMergeChange, ER-28).
     *
     * Server-authoritative; spend+remove-first then add, with full refund if the add fails. Returns
     * false if: Owner null / no authority; CrystalInventory or Currency missing; Type is None;
     * TargetTier is F (the floor — not a merge target); total available value below the target is
     * insufficient; Prisms unaffordable; the target tier is at cap; or the change doesn't fit (a
     * change tier at cap — the whole merge is refunded, Q-177).
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool MergeCrystals(AActor *Owner, ECrystalType Type, EItemTier TargetTier);

    /**
     * Dismantle the owned WEAPON instance identified by PersistentID, granting GEAR essence
     * (weapons/rings → Gear, per category routing). Server-authoritative; remove-then-grant.
     * Yield is from the item's INSTANCE (leveled) tier — Entry.Tier, mutated by LevelUp* / Downgrade*.
     * Returns false if Owner null / no authority; InventoryComponent or CurrencyComponent
     * missing; or no owned weapon carries that GUID.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleWeapon(AActor *Owner, FGuid PersistentID);

    /** Dismantle the owned RING instance by PersistentID → GEAR essence. See DismantleWeapon. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleRing(AActor *Owner, FGuid PersistentID);

    /** D-016: the confirm text to show before dismantling an EQUIPPED weapon or ring: which
     *  loadouts hold it, that its upgrades are lost, and (non-Caster) that the character can't
     *  fight when the active loadout is left with no gear. Empty when no loadout holds it, so the
     *  caller can dismantle without asking. DismantleWeapon / DismantleRing take it out of every
     *  loadout themselves. */
    UFUNCTION(BlueprintPure, Category = "Economy")
    static FText GetDismantleWarning(const AActor *Owner, FGuid PersistentID);

    /** B-319 (D-115): the owner keeps one copy of a weapon or ring. WeakerID must be a weaker copy from
     *  UInventoryComponent::FindDuplicateGear; its loadout slots move to the kept copy first, then it is
     *  converted to Gold (half the tier's buy price, Q-319) or dismantled for Gear essence. Out of a
     *  fight, server only; false when WeakerID is no weaker copy or the removal is refused.
     *  B-366b (Q-366): the convert pays the currency of the place (GetPlaceCurrency). */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool ResolveDuplicate(AActor *Owner, FGuid WeakerID, bool bConvertToGold);

    /** B-319 (Q-319): Gold for converting a duplicate of this tier (half its buy price). */
    UFUNCTION(BlueprintPure, Category = "Economy")
    static int32 GetDuplicateGoldForTier(EItemTier Tier);

    /** B-366b (Q-366, the shop rule ER-21; shops still charge the hub currency everywhere, ER-21's Conflict): the
     *  currency money earned here pays in: the run currency
     *  (ECurrencyType::Gold, shown as Prisms, D-159) while a trial run is under way, else the hub currency
     *  (ECurrencyType::Prisms, shown as Gold). Name it with WoRGameNames::GetCurrencyName. */
    static ECurrencyType GetPlaceCurrency(const UObject *WorldContext);

    /**
     * Dismantle the owned EVOLUTION instance by InstanceID (NOT PersistentID — evolution identity).
     * HYBRID yield: Elemental Essence (B-333, D-125)
     * at the GEAR leveling AMOUNT (GetLevelingEssenceYieldForTier on the INSTANCE tier — the §3 gear
     * curve F5..S145, NOT the §4.2 crystal yield). Server-authoritative; remove-then-grant via
     * UEvolutionInventoryComponent::RemoveInstance. Also the destroy path reused by primary-removal
     * and (future) break-on-zero-durability. False if Owner null / no authority; Evolution-inventory
     * or Currency component missing; or no owned evolution carries that InstanceID.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleEvolution(AActor *Owner, FGuid InstanceID);

    /**
     * Player-initiated "remove primary evolution → destroy + dust" (the Spiritualist action; NPC
     * gating is the UI layer). SELF-RESOLVES the slotted owned-entry identity from the runtime
     * loadout (GetActivePrimaryEvolutionInstance, iii-b) — no InstanceID param needed. Reads that
     * GUID, clears the primary slot (so nothing references the entry), then DismantleEvolution
     * destroys the owned entry, yields the hybrid essence (element type, gear amount, leveled tier),
     * and frees an evolution cap slot. Server-authoritative. False if Owner null / no authority;
     * LoadoutComponent missing; the primary slot is not an instance-resolved evolution (invalid
     * GUID); or DismantleEvolution fails.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool RemovePrimaryEvolution(AActor *Owner);

    /**
     * Dismantle a known SPELL, granting SKILL essence (abilities/spells → Skill, per category
     * routing — NOT Gear). Server-authoritative; remove-then-grant (UnlearnSpell's bool return
     * is the success signal — a not-known spell grants nothing). Yield from the spell's INSTANCE
     * (leveled) tier via UInventoryComponent::ResolveSpellTier. False if Owner/Spell null; no authority; components missing;
     * or the spell was not known.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleSpell(AActor *Owner, USpellData *Spell);

    /** Dismantle a known ABILITY → SKILL essence. See DismantleSpell. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DismantleAbility(AActor *Owner, UAbilityData *Ability);

    // ==================== PURCHASE (spend-side) ====================

    /**
     * Buy a CART of merchant stock into Owner's inventory in ONE atomic, all-or-nothing
     * transaction — the single spend-side entry point for every sellable type (weapon, ring,
     * spell, ability, evolution, crystal). Replaces the old per-type PurchaseWeapon/PurchaseSpell.
     *
     * Flow: resolve components (authority-gated) → VALIDATE up-front (per-entry cost +
     * cumulative-per-type grant capacity) → CanAfford EVERY currency once → Spend once → grant
     * loop. Capacity + affordability are fully pre-validated, so the grant loop cannot fail on
     * valid stock; a defensive grant failure rolls back every grant and refunds every currency.
     *
     * Per-type cost (symmetric with the dismantle yield):
     *   - Weapon / Ring : (Prisms base by tier + attached-item surcharge + bundled-skill
     *                     surcharge) × Count. Attachment: crystal/stone → its tier base;
     *                     evolution → 2× its tier base; fusion → 1.5× summed half bases.
     *                     Bundled skills (3l): each non-null skill prices at its tier base / 3
     *                     + 10, Prisms-only (no essence) — weapon PresetAbilities always;
     *                     weapon/ring DefaultSpells only when a gem crystal is attached
     *                     (Kind == Crystal); weapon DefaultAbilities only when the attachment
     *                     can grant stone abilities (augment stone, or fusion with an
     *                     AbilityStone half). WeaponAttack is never priced (weapon identity).
     *                     [quality is the C placeholder until shop-roll]
     *   B-333 (D-125, Q-333): essence @ tier is the TYPED_ESSENCE_BUY row.
     *   - Spell         : Prisms base + 50×Σ scaling-grade + Skill @ tier + Elemental @ tier (any
     *                     element but None; Generic counts — Quartz folded into Elemental).
     *   - Ability       : Prisms base + Skill @ tier (abilities carry no element).
     *   - Evolution     : 2× Prisms base (premium item class) + Elemental @ tier + ½ that again.
     *   - Crystal       : (Prisms base + its essence @ tier) × Count — a gem Elemental, a stat
     *                     stone Pillar, an ability stone Skill (EconomyYield::GetCrystalEssence).
     *
     * Count is honoured for weapons / rings / crystals; clamped to 1 for spells / abilities /
     * evolutions (spells/abilities can't be owned twice; evolution is per-instance). Buying a
     * spell/ability already owned — or duplicated within the cart — fails the whole purchase.
     *
     * Server-authoritative. Returns false (spending nothing) if: Owner null / no authority; Items
     * empty; a required component is missing; an entry is empty / not a sellable type / already
     * owned; cumulative capacity is exceeded for any type; or any currency is short.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool Purchase(AActor *Owner, const TArray<FMerchantStockEntry> &Items);

    /** B-316 (D-111 buy-for, D-117, Q-316): Payer's wallet buys the cart for the party member ForKey (a character key,
     *  UPoolSubsystem::GetCharacterKey; AI-controlled members included). The goods arrive with ForKey: weapons and
     *  adapters become its own (WoRGive::TryGiveGear, so its duplicate choice fires, D-115); crystals and stones go to
     *  its pouch (what doesn't fit stays where the purchase put it: the payer's pouch or the shared storage, D-097).
     *  B-316b: spells and abilities are learned into ForKey's own skills in the store (the payer's are untouched, so
     *  the payer may already know one). ForKey none or the payer's own key = Purchase. Refused up front (nothing
     *  spent) when ForKey isn't in the local party, or the cart holds an evolution (no evolution give yet, Q-323), or
     *  gear or a skill for a run-only recruit (it owns nothing, Q-321, Q-316b), or a skill for a member whose skills
     *  aren't in the store yet (never drawn). */
    bool PurchaseFor(AActor *Payer, FName ForKey, const TArray<FMerchantStockEntry> &Items);

    /** Price a cart WITHOUT purchasing — the exact per-currency totals Purchase would charge
     *  (same shared builder, so shop-UI display and the charge can never drift). Empty/unknown
     *  entries price at 0 (Purchase itself rejects them); spell/ability/evolution Count is
     *  clamped to 1, mirroring Purchase. Pure — reads no owner state. */
    UFUNCTION(BlueprintPure, Category = "Economy")
    FPurchaseCost PreviewCartCost(const TArray<FMerchantStockEntry> &Items) const;

    /**
     * Buy +1 World Stat point in Pillar for Owner (§7 C4). Always buyable at any run vendor; cost is
     * GOLD and ESCALATES per buy this run: WORLDSTAT_BUY_BASE_COST + (purchaseCount * STEP), where the
     * count comes from the runtime component (reset per run by ResetRunWorldStats). Server-authoritative.
     * Cap-safe: rejects (no charge) when the pillar is already at LIVE_MAX. On success: spend Gold,
     * grant the point, bump the purchase count (escalates the next buy). False if: Owner null / no
     * authority; CharacterDataComponent or CurrencyComponent missing; pillar at cap; or Gold unaffordable.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool BuyWorldStat(AActor *Owner, EWorldPillar Pillar);

    // ==================== LEVELING (instance tier-up, spend-side) ====================

    /**
     * Tier-up the owned WEAPON instance identified by PersistentID by ONE step, writing its
     * INSTANCE tier (Entry.Tier) — the first thing that WRITES the per-instance tier, so the
     * leveled weapon immediately dismantles for more, gets bigger loadout budgets, and hits
     * harder / wears differently in combat (all downstream reads already use instance tier).
     *
     * Cost (§5.3): Gear essence = GetTierUpCostForTier(currentTier). B-333 (Q-333): no co-cost (a weapon has no element).
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
     * instance source yet). Gear + ½ Elemental (an evolution has an element, Q-333) via the shared core; see
     * LevelUpWeapon for the full contract. False if no owned evolution carries that InstanceID.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool LevelUpEvolution(AActor *Owner, FGuid InstanceID);

    /**
     * Tier-up an owned SPELL by one step → writes the caster's FSpellInstance.Tier. Identified by
     * ASSET (owners hold ≤1 instance per asset — duplicates rejected at learn time), consistent with
     * the cast-tier resolver. Pays SKILL essence, + ½ Elemental when the spell has an element (Q-333) (§3 — spells/abilities level on Skill,
     * not Gear) via the shared TryLevelUpEntry core. S-capped. False if Owner null / no authority;
     * components missing; or the spell isn't owned (can't level what you don't have).
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool LevelUpSpell(AActor *Owner, const USpellData *Spell);

    /** Tier-up an owned ABILITY by one step → writes the owned FAbilityInstance.Tier. Skill-essence
     *  twin of LevelUpSpell (asset-keyed; basic attacks aren't owned, so they reject). */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool LevelUpAbility(AActor *Owner, const UAbilityData *Ability);

    // ==================== DOWNGRADE (instance tier-down / respec, refund-side) ====================

    /**
     * Tier-DOWN the owned WEAPON instance by ONE step (the inverse of LevelUpWeapon — Face C respec).
     * Refunds HALF the reverted step's leveling cost in Gear essence ONLY; the ½-Elemental co-cost paid
     * at level-up is NOT refunded (reverting always costs the reshape currency). Floored at the item's
     * AUTHORED base tier (you can only revert tiers you leveled UP to, never below what it shipped at).
     * Server-authoritative. False if: Owner null / no authority; components missing; no owned weapon
     * for that GUID; or already at/below base.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DowngradeWeapon(AActor *Owner, FGuid PersistentID);

    /** Tier-down the owned RING instance by one step (Gear refund). See DowngradeWeapon. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DowngradeRing(AActor *Owner, FGuid PersistentID);

    /** Tier-down the owned EVOLUTION instance by one step (Gear refund; identity is InstanceID). */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DowngradeEvolution(AActor *Owner, FGuid InstanceID);

    /** Tier-down an owned SPELL by one step (Skill refund; asset-keyed). See DowngradeWeapon. */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DowngradeSpell(AActor *Owner, const USpellData *Spell);

    /** Tier-down an owned ABILITY by one step (Skill refund; asset-keyed). */
    UFUNCTION(BlueprintCallable, Category = "Economy")
    bool DowngradeAbility(AActor *Owner, const UAbilityData *Ability);

    // ==================== DEBUG ====================

    /** Read-only economy snapshot for Owner: authority, the economy-relevant component
     *  checklist (Currency / CrystalInventory / Inventory / EvolutionInventory / Loadout — a
     *  missing one is the usual reason a Dismantle / Level / Downgrade returns false), and the
     *  full wallet (via UCurrencyComponentDebug::GetWalletString). Pure inspection — resolves
     *  components read-only and drives nothing. */
    UFUNCTION(BlueprintPure, Category = "Economy|Debug")
    FString GetEconomyString(AActor *Owner) const;

    /** Log + on-screen GetEconomyString(Owner). Console trigger: wor.PrintEconomy (in the .cpp;
     *  resolves the played character). No Exec / CallInEditor — a UGameInstanceSubsystem is not
     *  on the engine's Exec chain and has no Details panel for a button. */
    UFUNCTION(BlueprintCallable, Category = "Economy|Debug")
    void PrintEconomy(AActor *Owner) const;

private:
    /** Purchase's body. B-316b: SkillReceiver (a character key, none = Owner) is who learns the cart's spells and
     *  abilities: its skills in the store are checked and changed instead of Owner's. */
    bool PurchaseImpl(AActor *Owner, const TArray<FMerchantStockEntry> &Items, FName SkillReceiver);

    /** B-252b: one body each for the weapon / ring (adapter) verbs; TEntry is FWeaponInventoryEntry or
     *  FRingInventoryEntry. Verb names the public call in the logs and the authority / component checks. */
    template <class TEntry>
    bool DismantleOwnedGear(AActor *Owner, FGuid PersistentID, const TCHAR *Verb);
    template <class TEntry>
    bool LevelUpOwnedGear(AActor *Owner, FGuid PersistentID, const TCHAR *Verb);
    template <class TEntry>
    bool DowngradeOwnedGear(AActor *Owner, FGuid PersistentID, const TCHAR *Verb);

    /** Cost-only walk of a cart — the single pricing source behind PreviewCartCost AND
     *  Purchase's validation pass. Prices skip empty/unknown entries (Purchase rejects those
     *  separately); spell/ability/evolution Count clamps to 1. */
    FPurchaseCost BuildCartCost(const TArray<FMerchantStockEntry> &Items) const;

    /**
     * Shared tier-up core — type-agnostic. Knows only the wallet + a reference to the instance's
     * tier field, so any per-instance struct that carries an EItemTier (weapon, ring, evolution,
     * spell, ability) reuses ALL the cost/cap/spend/write logic. The wrappers resolve their entry
     * and pass &Entry.Tier here.
     *
     * Reads InOutTier; if already S_Tier, returns false (S is max — can't level past S). Else
     * computes the leveling-essence cost (GetTierUpCostForTier) + the ½ Elemental co-cost when bHasElement, requires BOTH
     * affordable (spends nothing if either short), spends both, then writes the next tier up into
     * InOutTier. Returns true only when the spend+write happened.
     *
     * LevelingEssence selects the primary essence: GearEssence (default — weapons/rings/evolution)
     * or SkillEssence (spells/abilities), per §3's category split. B-333 (Q-333): the ½ co-cost is
     * Elemental Essence and is charged only when bHasElement (evolutions, spells with an element).
     */
    bool TryLevelUpEntry(UCurrencyComponent *Currency, EItemTier &InOutTier,
                         ECurrencyType LevelingEssence = ECurrencyType::GearEssence, bool bHasElement = false) const;

    /**
     * Shared tier-DOWN core — the inverse of TryLevelUpEntry. Lowers InOutTier one step (toward
     * FloorTier, the item's authored base) and refunds HALF the reverted step's cost
     * (GetTierUpCostForTier(downTier)) in LevelingEssence ONLY — the ½-Elemental co-cost is NOT
     * refunded. Returns false (no change) when already at/below FloorTier. The write + refund cannot
     * fail, so no rollback path. Type-agnostic: any per-instance EItemTier reuses it.
     */
    bool TryDowngradeEntry(UCurrencyComponent *Currency, EItemTier &InOutTier, EItemTier FloorTier,
                           ECurrencyType LevelingEssence = ECurrencyType::GearEssence) const;
};
