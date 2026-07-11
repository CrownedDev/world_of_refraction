# Economy System

## Overview

The Economy System is the **orchestration layer** for the dismantle / merge / purchase /
leveling economy. It sits above the per-owner wallet (`UCurrencyComponent`, see
[`CurrencySystem.md`](./CurrencySystem.md)) and the per-instance item data
(see [`TierOnInstance.md`](./TierOnInstance.md)), and drives them without coupling them
to each other.

Two pieces:
- **`UEconomyService`** (`UGameInstanceSubsystem`) — the operations: dismantle, merge,
  purchase, level-up, downgrade. Resolves an owner's inventory **and** wallet off the
  owner actor (`FindComponentByClass`) and coordinates them. Authority-gated.
- **`EconomyYield`** (stateless namespace of inline functions) — all the curves + the
  essence-type resolution. No state, no reflection; callers decide what to do with the result.

Design reference: `docs/Design/Resources_Design.md` (§3 leveling essence, §4 typed essence,
§4.5 merge, §5 prices, §5.3 leveling cost).

## Architecture

### `UEconomyService` (`UGameInstanceSubsystem`)

Reached via `GetGameInstance()->GetSubsystem<UEconomyService>()`. It owns no state — every
operation takes the `Owner` actor and resolves the components it needs:
`UInventoryComponent` (weapons/rings + the `FSpellCollection`/`FAbilityCollection`),
`UEvolutionInventoryComponent` (evolutions), `UCrystalInventoryComponent` (crystals/stones),
`UCurrencyComponent` (the wallet), and `ULoadoutComponent` (for primary-evolution resolution).

**Canonical operation shape:** authority-gate (`Owner->HasAuthority()`) → resolve components
→ guard (availability / affordability / cap / floor) → **mutate** → grant/refund. Spend-side
ops check affordability **before** spending (spend-nothing-on-shortfall); grant-side ops
**remove-then-grant** (a failed removal never grants phantom essence); the two-component spends
(leveling) refund on failure.

#### Dismantle (scrap → essence)
| Method | Identity | Yield type | Yield amount | Notes |
|---|---|---|---|---|
| `DismantleCrystal(Owner, FCrystalId Id, Count=1, bRefined=false)` | `{Type,Tier}` + pool | **typed** essence via `ResolveEssenceType(Id)` | `GetTypedEssenceYieldForTier(Id.Tier) × Removed` (§4.2 crystal curve) | remove-then-grant; sized to what actually removed |
| `DismantleWeapon(Owner, FGuid PersistentID)` | weapon `PersistentID` | **Gear** essence | `GetLevelingEssenceYieldForTier(Entry.Tier)` (§3) | reads the **instance** `Entry.Tier` (leveled) |
| `DismantleRing(Owner, FGuid PersistentID)` | ring `PersistentID` | **Gear** essence | as weapon | reads instance `Entry.Tier` |
| `DismantleEvolution(Owner, FGuid InstanceID)` | evo `InstanceID` | **HYBRID**: element-essence *type* | gear *amount* `GetLevelingEssenceYieldForTier(Entry.Tier)` | type = `Item->GetAssociatedElement()` → `ElementToEssenceType`. Also the destroy path reused by primary-removal + break. |
| `DismantleSpell(Owner, USpellData*)` | known spell | **Skill** essence | `GetLevelingEssenceYieldForTier(resolved tier)` | reads the owned instance tier via `UInventoryComponent::ResolveSpellTier` |
| `DismantleAbility(Owner, UAbilityData*)` | known ability | **Skill** essence | as spell | via `ResolveAbilityTier` |

Category routing (§3): weapons/rings/evolution → **Gear**; spells/abilities → **Skill**;
crystals/stones → **typed** (element/pillar/ability). Evolution is the one hybrid (element
*type*, gear *amount*).

#### Merge (crystals/stones up a tier — `MergeCrystals(Owner, ECrystalType, EItemTier TargetTier, bRefined)`)
Value-based (§4.5): sum same-`Type` crystals **lowest-first** until their F-unit value
(`GetCrystalValue`) meets-or-exceeds the target's value, consume that set, produce 1 of
`{Type, TargetTier}` in the same pool. Costs **Prisms** = `GetMergeCostForTier(TargetTier)`
(half the §5 buy price). Item-crystals + stones only (evolution is unrepresentable as
`FCrystalId`). Spend Prisms + remove-first, full refund if the produce fails.

#### Purchase (spend-side) — `Purchase(Owner, TArray<FMerchantStockEntry>)`

The **single spend-side entry point** for every sellable type — one atomic, all-or-nothing
CART transaction (the old per-type `PurchaseWeapon`/`PurchaseSpell` are deleted). The cart
element is `FMerchantStockEntry` (`MerchantData.h` — exactly one of `Asset`/`Crystal` + `Count`;
see [`MerchantShopSystem.md`](./MerchantShopSystem.md)).

**Flow:** authority gate → resolve components (`UCrystalInventoryComponent` /
`UEvolutionInventoryComponent` required only when the cart carries their goods) → **validation
pass** (per-entry sellability + cumulative-per-type grant capacity: weapon/ring slot costs,
spell/ability collection capacity, evolution cap, per-`FCrystalId` `CanAddCount`; spells and
abilities already owned — or duplicated within the cart — fail the whole purchase) →
**CanAfford every currency once** → **Spend once** → grant loop. Capacity + affordability are
fully pre-validated so the grant loop cannot fail on valid stock; a defensive grant failure
rolls back every prior grant (`TFunction` undo-thunks) and refunds every currency.

**Cost is built by one shared builder** — private `BuildCartCost`, public as
`PreviewCartCost(Items) → FPurchaseCost` (`{Prisms, SkillEssence, TMap<EEssenceType,int32> Typed}`,
BlueprintPure, reads no owner state). The shop UI displays `PreviewCartCost` and `Purchase`
charges from the same builder, so display and charge can never drift. Empty/unknown entries
price at 0 (Purchase itself rejects them).

**Per-type cost:**

| Type | Prisms | Essence | Count |
|---|---|---|---|
| Weapon / Ring | (base by tier + attached-item surcharge **+ bundled-skill surcharge**) × Count | — | honoured |
| Spell (element-typed) | base + `50 × Σ scaling-grade` surcharge | element @ tier + Σ pillar @ each scaling grade | clamped 1 |
| Spell (**Generic** element) | base | **SkillEssence** @ tier (prices like an ability — no surcharge, no typed; Generic would otherwise charge "Quartz essence") | clamped 1 |
| Ability | base | SkillEssence @ tier | clamped 1 |
| Evolution | **2× base** (premium item class; multiplier lives in `BuildCartCost`, not the shared tier-base helper, so a future merge-cost reuse can't double it) | element @ tier + ½ Reality | clamped 1 |
| Crystal / stone | base by `Crystal.Tier` × Count — **Prisms-only** | none — essence stays a dismantle-side currency for crystal stock | honoured |

**Attached-item surcharge** (anonymous-namespace `AttachmentPrisms`, keyed on
`FAttachedItem::Kind`): crystal/stone → its tier base; evolution → **2×** the evolution's tier
base (premium attachment; null-guarded — a mis-authored empty Evolution slot prices at 0);
fusion → **1.5×** the summed half-tier bases (rounded).

**Bundled-skill surcharge** (3l — anonymous-namespace template `BundledSkillsPrisms<TSkill>`,
`Tier` read via the shared `USkillDataBase`): each non-null bundled skill prices at
**`GetPrismsBaseForTier(Tier) / 3 + 10`** (integer floor; E skill = 26), Prisms-only — bundled
skills never charge essence. Gates: weapon `PresetAbilities` **always** (baked into the weapon);
weapon/ring `DefaultSpells` only when a gem crystal is attached (`Kind == Crystal` — not
stone/evolution/fusion); weapon `DefaultAbilities` only when `HasAbilityStoneAttachment`
(augment stone, or a fusion carrying an AbilityStone half — deliberately **tighter** than the
runtime `GetAugmentStoneAbilities` gate, which passes any fusion and lets the slot cap resolve
to 0; pricing must not charge for abilities the attachment can't grant). `WeaponAttack` is
never priced — weapon identity, not added value. Drives the Themed-Ring price row
(`Resources_Design.md` §5.1c): themed rings at E = 126 (1 spell) / 152 (2 spells).

- **Purchase reads the ASSET tier** (you're buying, not owned yet — no instance exists). This
  is the deliberate asset/instance split vs dismantle (which reads the leveled instance tier).
- **Quality is a C placeholder** in equipment pricing until the shop-roll generator lands
  (the §5.1b tier-half + quality-half split is not implemented yet).

#### Leveling (`LevelUp*`) + Downgrade (`Downgrade*`)
The level up/down pair across all five instance types — see [`TierOnInstance.md`](./TierOnInstance.md)
and the player-facing [`../Mechanics/Economy/Upgrading.md`](../Mechanics/Economy/Upgrading.md). Both route through
two shared, type-agnostic private cores:

- **`TryLevelUpEntry(Currency, EItemTier& InOutTier, ECurrencyType LevelingEssence = GearEssence)`**
  — S-cap; cost `GetTierUpCostForTier(CurrentTier)` in `LevelingEssence` + half that in **Reality**
  (`EEssenceType::Reality`); CanAfford BOTH → Spend BOTH → write `InOutTier = next tier`.
- **`TryDowngradeEntry(Currency, EItemTier& InOutTier, EItemTier FloorTier, ECurrencyType LevelingEssence = GearEssence)`**
  — floor guard (can't revert below `FloorTier` = the authored base); lower one step; refund **half**
  `GetTierUpCostForTier(downTier)` in `LevelingEssence` **only** (the ½-Reality co-cost is **not**
  refunded).

Public wrappers resolve the mutable instance tier + pass the right essence type:
`LevelUpWeapon`/`Ring`(PersistentID), `LevelUpEvolution`(InstanceID), `LevelUpSpell`/`Ability`(asset)
→ Gear for weapons/rings/evo, **Skill** for spells/abilities. `Downgrade*` mirror them and read the
authored base (`Entry->Weapon->Tier` / `Instance->Spell->Tier` / …) as the floor.

### `EconomyYield` (stateless namespace — `EconomyYield.h`)
All inline; named PIE-tunable constants in `EconomyYield::Constants`.

| Function | Returns |
|---|---|
| `GetTypedEssenceYieldForTier(EItemTier)` | crystal **dismantle** yield (§4.2 sell row: F5…S24) |
| `GetTypedEssencePurchaseCostForTier(EItemTier)` | typed-essence **buy** cost (§4.2 buy row: F10…S48) |
| `GetLevelingEssenceYieldForTier(EItemTier)` | gear/skill **dismantle** yield (§3: F5…S145) |
| `GetTierUpCostForTier(EItemTier)` | gear/skill **level-up** cost, that tier → next (§5.3: F→E 10 … A→S 70; S→0) |
| `GetPrismsBaseForTier(EItemTier)` | Prisms base price (§5 doubling: F25…S1600) |
| `GetMergeCostForTier(EItemTier)` | merge Prisms cost = `GetPrismsBaseForTier/2` |
| `GetCrystalValue(EItemTier)` | merge F-unit value (F1 E2 D4 C8 B16 A32 **S96**) |
| `RollQuality(float NormalizedLuck = 0)` | weighted drop quality (§11 curve F26…S4, Luck-tilted around C) |
| `ResolveEssenceType(const FCrystalId&)` | gem→element, AbilityStone→Ability, stat-stone→pillar (via the fold) |
| `ElementToEssenceType(ESpellElement)` | element → `EEssenceType` (1:1 by name; Generic/None→Generic) |
| `SubStatToPillarEssence(ESubStat)` | sub-stat → Mind/Body/Spirit essence (StatusMultiplier is Spirit) |
| `ScalingGradeToItemTier` / `GetScalingGradeNumber(EScalingTier)` | spell scaling-grade → tier / 1..7 number |

### The wallet (`UCurrencyComponent`) — summary
The owner's currencies, behind a unified `Add`/`Spend`/`CanAfford`/`GetBalance(ECurrencyType, SubKey)`
API: **Gold** (run-volatile), **Prisms**, **Diamond**, **GearEssence**, **SkillEssence** (scalars),
and the 14-key **typed Essence** (`EssenceTyped`, FastArray). Server-gated, replication-aware,
`SaveGame`-tagged (no save system yet). Full detail in [`CurrencySystem.md`](./CurrencySystem.md).

## Integration Points

- **Subsystem access:** `GetGameInstance()->GetSubsystem<UEconomyService>()` (the
  `GameInstanceSubsystem` pattern, like `DamageCalculator`/`ActionExecutor`).
- **Components resolved off `Owner`:** `UCurrencyComponent`, `UInventoryComponent`,
  `UEvolutionInventoryComponent`, `UCrystalInventoryComponent`, `ULoadoutComponent`. No
  hard references — pure `FindComponentByClass`.
- **Callers today:** combat reads consume `EconomyYield`/the resolvers indirectly (tier reads);
  `LevelUp*`/`Downgrade*`/`Dismantle*`/`Merge*` are authority-gated backend ops awaiting their
  hub/NPC UI. **`Purchase`/`PreviewCartCost` ARE wired:** `UShopWindowWidget` prices its cart and
  rows with `PreviewCartCost` and Confirm calls `Purchase` (see
  [`MerchantShopSystem.md`](./MerchantShopSystem.md)). The combat-end break sweep
  (`ACombatOrchestrator::ApplyBetweenCombatCrystalDestruction`) calls `DismantleEvolution` and grants
  crystal/fusion break essence directly via `UCurrencyComponent::AddEssenceType`.
- **`EconomyYield`** is included wherever a yield/cost/essence-type is needed (combat tier-gap,
  wear, the service).

## How to test

- **Wallet inspection:** `UCurrencyComponent::PrintWallet()` (CallInEditor button) →
  `FCurrencyComponentDebug::GetWalletString` one-line dump. Watch balances move across an op.
- **PIE path:** purchases run end-to-end in the hub (merchant cube → shop window → Confirm);
  the other ops still need a test harness / direct `UFUNCTION` call. Print Wallet to confirm
  the spend/grant + the instance-tier change. `UShopWindowWidget::GetShopString()` snapshots
  cart totals + affordability without clicking through the UI.
- **Dismantle/level parity:** at base tier the dismantle yield = asset-tier yield; after a
  `LevelUp*`, dismantle yields the leveled tier's value — confirms instance-tier reads.

## Known Limitations / TODOs

- **No reward/faucet sources.** Nothing awards currency in C++ yet (post-defeat reward hook is
  greenfield — `Resources_Design.md` BUILD-STATE NOTE). Faucets (`AddGearEssence`/`AddSkillEssence`)
  are public + ready but uncalled outside dismantle/break.
- **No save system.** `SaveGame` tags are latent (session-only).
- **No Pool layer.** Leveling/learning writes the **run-scoped** inventory; persistence across runs
  awaits the Pool arc (`Resources_Design.md`; see the EconomySystem discovery survey). The repointing
  pass (run → pool writes) is the bulk of that arc.
- **Account-vs-character currency routing** (Prisms/Diamond) not wired — all per-character today.
- **⚠️ Stale doc-comments in `EconomyService.h`:** the `DismantleWeapon`/`Ring`/`Spell`/`Ability`
  header comments still say *"Yield is from the item's ASSET tier (leveled-tier deferred)"* — but the
  code was repointed to the **instance** tier (weapon/ring in the dismantle-repoint commit, spell/ability
  in cluster iv). The behaviour is correct (instance tier); only the comments are stale. **TODO: refresh
  those comments.**

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-24 | Initial system doc — `UEconomyService` (dismantle/merge/purchase/level/downgrade) + `EconomyYield` curves, post-completion of the currency→tier-instance→evolution→spell-instance→level/downgrade arc. | feature/currency-component |
| 2026-07-11 | **Purchase unified into one atomic cart op.** `PurchaseWeapon`/`PurchaseSpell` deleted; `Purchase(Owner, TArray<FMerchantStockEntry>)` handles every sellable type with up-front capacity + affordability validation, rollback-thunk grant loop, and the shared `BuildCartCost` builder exposed as `PreviewCartCost → FPurchaseCost` (one cost source for UI display and charge). | feature/hub-merchants |
| 2026-07-11 | **Cost-table reprices:** Generic-element spells price like abilities (Prisms base + SkillEssence @ tier — no scaling surcharge, no typed essence); crystals/stones are **Prisms-only** (no typed-essence charge — essence stays dismantle-side); evolutions cost **2× Prisms base** (premium item class); weapons/rings gain the **attached-item surcharge** (crystal/stone = tier base, evolution = 2× its tier base, fusion = 1.5× summed half bases). | feature/hub-merchants |
| 2026-07-11 | **Bundled-skill pricing (3l):** equipment prices its bundled skills at `tier base / 3 + 10` each (floored, Prisms-only) via `BundledSkillsPrisms<TSkill>` — weapon `PresetAbilities` always; `DefaultSpells` gem-gated (`Kind == Crystal`); `DefaultAbilities` gated on `HasAbilityStoneAttachment` (stone, or fusion with an AbilityStone half); `WeaponAttack` excluded. PIE-verified: themed rings 126/152 P, 2-ability Blacksmith weapons 102 P, crystal/template rings unchanged. | feature/hub-merchants |
