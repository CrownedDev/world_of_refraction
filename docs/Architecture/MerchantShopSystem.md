# Merchant & Shop System

## Overview

The hub merchant pipeline: authored merchant data assets → a placeholder in-level
interactable → a modal shop window that prices and buys through the economy. The
player walks up to a merchant cube in the hub, presses E (`IA_Interact`), and a
three-column shop window opens — stock list, hover detail panel, cart with
per-currency totals. Confirm hands the cart to
`UEconomyService::Purchase(Owner, Items)` as one atomic transaction (see
[`EconomySystem.md`](./EconomySystem.md)).

Design reference: `docs/Design/Resources_Design.md` ("Hub/Trial Merchants —
LOCKED DESIGN"). v1 = **fixed authored stock lists**; the §12 rolled/reroll stock
and the registry tag-QUERY are v2 layers. Currency is **context-driven** (hub =
Prisms, trial = Gold), so the same merchant asset serves both contexts — currency
is deliberately NOT a field on the merchant.

## Architecture

### `EMerchantType` (5 archetypes)

Blacksmith (weapons, augment stones) · Jeweler (rings) · CombatMaster
(abilities) · SpellShop (spells) · Spiritualist (evolutions, crystals, repairs).
Item-type → merchant is automatic from the item's class (the merchant map);
vendor tags only override. Selling a type outside the map is a **validation
warning**, not an error — deliberate overrides are allowed.

### `FMerchantStockEntry` (`MerchantData.h`)

One shop-shelf line, and also the **cart element** handed to
`UEconomyService::Purchase`. EXACTLY ONE of:
- `Asset` (`UPrimaryDataAsset*`) — asset-backed goods: `UWeaponData`,
  `URingData`, `USpellData`, `UAbilityData`, `UEvolutionItemData`.
- `Crystal` (`FCrystalId`) — count-based goods with no data asset: gem crystals
  and augment stones (Type + Tier).

Plus `Count` (ClampMin 1). `IsAssetEntry()` / `IsCrystalEntry()` discriminate.
Prices are NOT authored — pricing is tier-keyed via `EconomyYield` at purchase.

### `UMerchantData` (`UPrimaryDataAsset`)

- `MerchantType` (`EMerchantType`).
- `AvailabilityTags` (`FGameplayTagContainer`) — locked availability rules
  (option a): **empty = hub-only** (the default); `Trial.X` = trial-only,
  hierarchical (`Trial.Garnet` matches any Garnet floor, `Trial.Garnet.Floor1`
  that floor); `Hub` + `Trial.X` = both contexts. Queried via `AppearsInHub()` /
  `AppearsInTrial(ContextTag)`. The tags (`Hub`, `Trial`, `Trial.Garnet`,
  `Trial.Garnet.Floor1`) live in `Config/DefaultGameplayTags.ini` — the
  project's first gameplay tags.
- `Stock` (`TArray<FMerchantStockEntry>`) — v1 fixed authored list.
- Debug: `GetMerchantString()` (formatted snapshot), `PrintMerchant()` and
  `ValidateStock()` CallInEditor buttons, `IsDataValid` — all share
  `CollectStockIssues` (structural problems → errors; wrong-type-for-merchant →
  warnings).

### The five authored merchants (`Content/Data/Pool/Merchant/`)

All stock is Count=1 per line; every merchant also stocks all 12 crystals
(9 gems + DamageStone + AbilityStone + Quartz) at E-tier:

| Asset | Enum type | Stock |
|---|---|---|
| `DA_Merchant_Blacksmith` | Blacksmith | 30 = 18 weapons + 12 crystals |
| `DA_Merchant_Jewler` *(sic)* | Jeweler | 22 = 10 rings + 12 crystals |
| `DA_Merchant_CombatMaster` | CombatMaster | 156 = 144 abilities + 12 crystals |
| `DA_Merchant_Refractor` | SpellShop | 27 = 15 spells + 12 crystals |
| `DA_Merchant_Evolutionist` | Spiritualist | 39 = 27 evolutions + 12 crystals |

(`DA_Merchant_Test` under `Content/Data/Merchant/` is the wiring-test asset.)

### The pool asset library (`Content/Data/Pool/`) — the stock source

Authored on this branch as the persistent item library the merchants sell from
(and the future Pool-arc persistence layer draws on). ~274 assets, organized
`Pool/<Generic|Element>/<Weapons|Rings|Evolutions|Spells>/…`:

- **27 weapon templates** — 18 single (Sword ×6, Greatsword ×4, Axe ×4,
  SwordAndShield ×4) + 9 dual (DualDagger ×3, DualSword ×3, DualAxe ×2,
  Gauntlets ×1).
- **~204 attacks + abilities** — 12 clusters of 17 (5 attacks + 12 abilities)
  per weapon-family × physical type: {Sword, Greatsword, Axe, SwordAndShield} ×
  {Slash, Pierce, Impact}. Each asset carries its authored
  `PhysicalDamageType` (see [`WeaponSystem.md`](./WeaponSystem.md) — the
  weapon no longer has one). The 18 single weapon templates are wired to these
  attacks/abilities.
- **10 rings** — Generic + 9 element rings (one per element gem).
- **27 evolution crystals** — 3 per element × 9 elements.
- **15 Generic spells + 4 effect defs** — Conjuration (Bolt/Lance/Missile/
  Storm/Wave), Enhancement (Might/Grace/Warding/Aura/EnhancementAura),
  Restoration (Mend/Restore/Renewal/Cleanse/Rally) + Might/Grace/Warding/
  Renewal effect definitions.

### `AMerchantInteractable` (`AActor`)

Placeholder hub merchant: an engine-cube mesh (root) + a pawn-only
`USphereComponent` trigger whose radius IS the interaction range. Overlap
begin/end tracks `PawnInRange` (weak; single-player hub — latest wins) and shows
an on-screen "Press E" hint. No registration, no traces:
`FindNearestInRange(Pawn)` (static) scans trigger overlap state — the hub
controller's `IA_Interact` event calls it and `Interact()`s the result
(wired in `BP_HubPlayerController`).

`Interact()`: null-`Merchant` → on-screen warning; otherwise resolves
`UMerchantShopSubsystem` and calls `OpenForMerchant(Merchant, PawnInRange)`.

### `UMerchantShopSubsystem` (`UGameInstanceSubsystem`, `Config=Game`)

Owns the shop-window **lifecycle**, no purchase logic:

- `OpenForMerchant(Merchant, Instigator)` — closes any open shop, stamps
  `ActiveMerchant`/`ActivePawn` **before** creating/adding the widget (the
  widget self-resolves them in `NativeConstruct` — the subsystem never knows
  the concrete widget type), then brackets the modal state: **UIOnly input +
  cursor + pause** (the hub is real-time).
- `Close()` — tears down the widget and restores input/cursor/unpause; safe
  no-op when nothing is open. The widget's Close button routes here so the
  bracket always unwinds.
- `ShopWindowClass` (`Config`) — authored in `DefaultGame.ini`
  (`[/Script/world_of_refraction.MerchantShopSubsystem]` →
  `/Game/UI/Shop/WBP_ShopWindow`). Null class = **NO_UI_YET fallback**: Open
  logs and skips widget + modal bracket, so PIE can never soft-lock without a
  Close path.
- All state refs are weak — the viewport owns the widget lifetime.

### `UShopWindowWidget` (C++ base of `WBP_ShopWindow`)

ALL logic in C++; the WBP supplies layout only, wired by `BindWidgetOptional`
name. Buttons bind in C++ (`AddUniqueDynamic` in `NativeConstruct`) — no WBP
OnClicked graphs. Three columns:

1. **Stock** — `StockList` (`UListView` of `UShopEntryObject`, the UObject
   carrier for `FMerchantStockEntry` + a weak `ParentWindow` back-ref stamped
   before `SetListItems`).
2. **Detail panel** (3e) — `DetailName/Tier/Description/Stats/Cost` + a single
   `DetailAddButton` (replaces per-row Add). Populated by `SetHoveredEntry`
   (rows call it on hover; defaults to the first stock entry). `DetailStats`
   shows per-type stat lines — attack/spell lists, damage numbers, scaling
   grades, and for crystals the `CrystalDescription::GetItemEffectText`
   mechanical sentence (3j; see [`ItemSystem.md`](./ItemSystem.md)).
   `DetailCost` shows the line cost via a one-entry `PreviewCartCost`.
3. **Cart** — `CartList` + `CartTotalsPanel` + Confirm/Close. `AddToCart`
   coalesces same-item lines (3c); `RemoveFromCart` decrements the first
   matching line, 0 removes. `Cart` (`TArray<FMerchantStockEntry>`) is the
   exact array handed to `Purchase`.

Totals (3h): `RefreshTotals` prices the cart with ONE
`UEconomyService::PreviewCartCost` — the same builder `Purchase` charges with,
so display and charge can never drift — and fills a pooled
`| Currency | Cost | Wallet |` table (header + one row per currency: Prisms,
Skill, each `EEssenceType`). Rows are built **once** into `CartTotalsPanel`
(`EnsureTotalsPool` — the TransBuffer gotcha: never recreate widgets inside a
designer-placed widget); refresh only updates texts/colors/visibility, red on
shortfall, and gates the Confirm enable.

Other mechanics: purchase toast auto-hides via `FTSTicker` (NOT a world timer —
the hub is paused under the shop, `FTimerManager` never fires);
`UCurrencyComponent::OnCurrencyChanged` re-runs totals/enable on any wallet
movement; `GetShopString()` is the debug snapshot (merchant, purchaser, cart,
totals, affordability).

### `UShopRowWidget` (abstract C++ base of `WBP_StockRow` / `WBP_CartRow`)

`IUserObjectListEntry` row for both lists. Texts refresh in
`NativeOnListItemObjectSet` (fires per pooled re-bind); buttons bind once in
`NativeConstruct`. Stock rows: `RowName/RowTier/RowCost`, hover →
`NativeOnMouseEnter` → `ParentWindow->SetHoveredEntry`. Cart rows:
`RowName/RowCount/RowCost/RemoveButton` → `RemoveFromCart`. The static
`NameFor/TierFor/DescriptionFor` builders are the ONE display ladder shared
with the window's detail panel. Line cost = one-entry `PreviewCartCost`.
`OnEntrySet` is the BP hook for extra visuals per re-bind.

## Integration Points

- `UEconomyService::PreviewCartCost` (display) / `Purchase` (charge) — the
  single shared cost builder; see [`EconomySystem.md`](./EconomySystem.md).
- `UCurrencyComponent::OnCurrencyChanged` → `HandleCurrencyChanged` → totals
  refresh.
- `BP_HubPlayerController` `IA_Interact` → `FindNearestInRange` → `Interact()`
  → `UMerchantShopSubsystem::OpenForMerchant`.
- Display helpers: `UShopRowWidget::NameFor/TierFor/DescriptionFor`,
  `ItemIdentity::GetTypeName` / `GetDisplayName`,
  `CrystalDescription::GetItemEffectText` / `GetCrystalText`,
  `EconomyYield` scaling-grade letters.
- Config: `DefaultGame.ini` (ShopWindowClass), `DefaultGameplayTags.ini`
  (availability tags).

## Known Limitations / TODOs

- **v1 fixed stock.** The §12 rolled/reroll stock generator and the C0 registry
  tag-QUERY replace/augment the authored `Stock` lists later; tags are authored
  from day one so that migration is "swap list for query".
- **Hub context only.** Prisms is the only wired currency; the trial context
  (Gold) exists in design + availability tags but no trial flow consumes it.
- **Quality is a C placeholder** in equipment pricing until the shop-roll
  generator lands (see EconomySystem cost table).
- **Spiritualist "repairs"** are named in the archetype but unimplemented.
- **Placeholder visuals** — engine cube + on-screen hint text; 3D models /
  shop-door interaction come later per the locked design.
- **Asset-name mismatches** — `DA_Merchant_Jewler` (typo), and
  `Refractor`/`Evolutionist` asset names vs `SpellShop`/`Spiritualist` enum
  display names.
- Stock rows keep an unused `AddButton` bind (3e moved Add to the detail
  panel) — harmless if a WBP omits it (`BindWidgetOptional`).

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-07-11 | Initial system doc — covers the full arc: `UMerchantData` + tags (Cluster 1), `AMerchantInteractable` + `IA_Interact` loop (Cluster 2), `UMerchantShopSubsystem` (3a), `UShopWindowWidget` (3b), rows + coalescing cart (3c), row polish (3d), three-column + hover detail (3e), UI polish (3f–3i), stone effect text via `GetItemEffectText` (3j); 5 merchants stocked from the ~274-asset pool library. | feature/hub-merchants |
