# Inventory and Loadout Architecture — comparison and target (2026-10-29)

**Status:** proposal, being decided with Crown one question at a time. Outside-game references are from memory (no browsing this pass). Extends D-110 (`UnifiedLoadout_2026-10-29.md`).

---


**Status: PROPOSAL (2026-10-29).** Read-only analysis of `dev` (up to date). Nothing here is decided; §7 lists
what Crown rules on. Paths are under `Source/world_of_refraction/` unless they start with `docs/`.
Outside claims are **from memory** unless a source is named; I could not browse during this pass.

## 0. Short version

A single weapon today exists in up to **five kinds of place at once**: the pool's armoury, a copy in every
party member's inventory, a copy in every saved preset that holds it, an attachment snapshot keyed by the
*asset* (not the instance) for the run, and the save file. About **700 lines** exist only to keep those
copies in step. Items (crystals, stones) have the same shape: storage, party carried slots, a mirror per
member, and a per-preset item slot list. Found gear goes straight into the permanent pool, so D-097's
"found gear is lost at run end" has nowhere to live yet; player-edited presets are not in the save at all.

D-110 is right and stays. It fixes **what an equipped slot is**. This proposal fixes **where owned things
live**: one owned-items store per player (every instance by GUID, any kind), with views (armoury, storage,
pouch, run bag) instead of containers, loadouts and outfits as GUID references, the run as a layer that is
merged or dropped at run end, and equipment as a thin reader. That is Lyra's inventory/equipment split and
Monster Hunter's box/pouch model, which D-097 already chose for the rules.

## 1. Every container today (what, where, lifetime)

```
 GameInstance (one per machine, lives all session)          Pawn (respawned EVERY fight)
 ┌──────────────── UPoolSubsystem ───────────────┐          ┌──── UInventoryComponent ─────────────┐
 │ OwnedWeapons / OwnedRings / OwnedEvolutions   │──copy───▶│ Weapons / Rings (whole armoury copy)  │
 │ OwnedSpells / OwnedAbilities                  │          │ Spells / Abilities (per character)    │
 │ Crystals / Stones  (= home storage)           │          │ SavedLoadouts[≤5] (FCombatLoadout,    │
 │ CarriedItems (party pouch, 10 slots)          │──mirror─▶│   gear COPIED into each slot)         │
 │ Wearers  GUID → character key                 │          ├──── UCrystalInventoryComponent ──────┤
 │ wallet mirror (PoolGold, Prisms…)  (inert)    │          │ Crystals / Stones (mirror of pouch)   │
 └───────────────────────────────────────────────┘          ├──── UEvolutionInventoryComponent ────┤
 UPartySessionSubsystem → UParty::Members                   │ Entries (armoury evolutions copy)     │
   CarriedGear (attachments keyed by ASSET), RunWeapon/Ring ├──── ULoadoutComponent ───────────────┤
 AWoRPlayerState::Wallet (UCurrencyComponent, D-046)        │ BattleItemsTaken (per fight)          │
 Save: FWoRSaveData {Party, Wallet, Pool, Run}              └───────────────────────────────────────┘
 Authored: UInventoryData::SavedLoadouts (FSavedLoadout, inline in .uasset)
```

| Container | What it holds | Where (file:line) | Lifetime | Saved? |
|---|---|---|---|---|
| Pool armoury | weapons, rings, evolutions (full instances) | `Public/Pool/PoolSubsystem.h:291-298` | GI session | yes, `FSavedPool::Weapons/Rings/Evolutions` (`Public/Save/WoRSaveTypes.h:322-329`) |
| Pool knowledge | spells, abilities | `PoolSubsystem.h:301-305` | GI | yes (`WoRSaveTypes.h:331-335`) |
| Home storage | crystal / stone stacks by (type, tier) | `PoolSubsystem.h:311-315` | GI | yes (`:338-339`) |
| Party pouch (B-203) | ≤10 stacks, one type per slot | `PoolSubsystem.h:366-370` | GI | yes (`:342-351`) |
| Wearers (B-091) | GUID → character key | `PoolSubsystem.h:351-356` | GI | yes (`FSavedPool::Wearers :316`) |
| Pool wallet mirror | PoolGold, Prisms, Diamond, essences | `PoolSubsystem.h:326-345` | GI | **unused**: the real wallet is the PlayerState's |
| Per-character inventory | copy of whole armoury + per-char spells/abilities | `Public/Inventory/InventoryComponent.h:81-94`; drawn in `Private/Inventory/InventoryComponent.cpp:1282-1346` (`Weapons = Pool->GetOwnedWeapons()` :1313) | pawn (one fight / one hub visit) | via the pool |
| Evolution inventory | copy of armoury evolutions | `Public/Equipment/Crystals/EvolutionInventoryComponent.h:32-33`, filled at `InventoryComponent.cpp:1319` | pawn | via the pool |
| Crystal inventory | mirror of the pouch | `Public/Equipment/Crystals/CrystalInventoryComponent.h:57-63, 110` | pawn | via the pool |
| Presets | `TArray<FCombatLoadout> SavedLoadouts` (≤5) + active index | `InventoryComponent.h:103-113` | pawn | **no** (nothing in `Private/Save/` mentions loadouts); rebuilt from the authored asset each spawn |
| Authored presets | `FSavedLoadout` inline on `UInventoryData` | `Public/Inventory/InventoryData.h:112-114` | asset | n/a (`.uasset`) |
| Runtime loadout | `SavedLoadouts[ActiveLoadoutIndex]`; gear entries are by-value caches of owned gear keyed by `PersistentID` | `InventoryComponent.h:129-152` (B-085 sync) | pawn | no |
| Loadout item slots | ≤6 × ≤3 crystals debited from the crystal inventory | `Public/Loadout/FCombatLoadout.h:174-175`, `Private/Loadout/LoadoutComponent.cpp:395-430` | pawn | no |
| Battle items taken | per-fight debit tally (D-064) | `Public/Loadout/LoadoutComponent.h:813` | fight | no |
| Run carried gear | crystal wear between fights, keyed by **holder asset** | `Public/Loadout/CarriedGearState.h:16-24`; `Public/Party/Party.h:84`; resolved by `ULoadoutComponent::FindAttachedItemByHolder` (`LoadoutComponent.cpp:3491-3519`, matches `Weapon == Holder`) | run | yes (`FSavedPartyMember::CarriedGear`, `WoRSaveTypes.h:122-123`) |
| Run-only member gear | two GUIDs | `Party.h:110-115`, `Private/Party/PartySessionSubsystem.cpp:440-449` | run | yes (`WoRSaveTypes.h:110-114`) |
| Run-found gear (D-097) | **no container**: `TrialLoot::GrantTo` → `AddWeapon` → `PushGearToPool` (`Private/Trial/TrialLoot.cpp:114-127`, `InventoryComponent.cpp:628`) = permanent | — | — | permanent by accident |
| Upgrade memory per definition (D-097) | **not built** (B-238 blocked on B-203) | — | — | — |
| Run bought loadout (D-021) | **not built** (B-116 blocked on B-073) | — | — | — |
| Merchant stock | authored list; trial stock rolled from run seed | `Public/Merchant/MerchantData.h:107-108`, `Public/Merchant/MerchantStockRoll.h:56-58` | stateless | seed only (`FSavedRun::RunSeed`) — **good as is** |
| Wallet | `AWoRPlayerState::Wallet` (D-046); fallback per-pawn component | `Public/Player/WoRPlayerState.h:35-36`; `Private/Character/CombatCharacter.cpp:28`; resolver `Public/Currency/CurrencyWallet.h` | PS / pawn | yes (`FSavedWallet`); run Gold in `FSavedRun::RunGold :163` |
| Bound gear (D-033) | a lord field + bonus on the item asset | data only (D-033 notes) | asset | n/a |
| Forms + meters (B-279) | per-player meter per character, skin unlocks | design only, `docs/Design/CharacterForms.md` §1 | — | planned (D-059) |
| Outfit pieces | six slots, `FOutfitSet` of soft refs; owned pieces | design only, `docs/Design/CharacterCustomisation.md` §4 | — | planned; **no owned-pieces list is designed** |
| Co-op | pool → PS planned; wallet already PS | `docs/Design/CoopStateOwnership.md:44-45` | — | — |

## 2. Problems

**P-A. One gear piece, many copies.** Pool (1) + each member's inventory (up to 3, `InventoryComponent.cpp:1313-1319`)
+ each preset holding it (up to 5 per member, cache by value, `InventoryComponent.h:129-152`) + the run's
carried attachment (`CarriedGearState.h:20`) + save. The sync layer (`LinkLoadoutsToOwned`,
`RefreshLoadoutsFromOwned`, `RefreshEvolutionInLoadouts`, `DetachEvolutionFromOwnedGear`, `WriteLoadoutToOwned`,
`AdoptPresetOnlyGear`, `StripUnownedGear`, `PushGearToPool`, `HandlePoolGearChanged`, `ClaimActiveSharedGear`:
`InventoryComponent.cpp:850-1530`, ~700 lines) exists only because of the copies. B-091's own row says "no
per-pawn copy" (`docs/Backlog.md:390`) and B-085's says "no copies" (`:382`); the built code is copies + sync.

**P-B. By-value loadout reads.** Perf audit P1: 22 sites copy `FCombatLoadout` per stat read
(`docs/analysis/PerfAudit_2026-10-28.md:27-39`). P1's fix (`const &`) is cheap and independent; but the deeper
cause is that the loadout *is* a container of gear rather than a list of ids.

**P-C. Run attachment state keyed by asset.** `FCarriedAttachment::Holder` is a `UObject*` to the weapon/ring
asset, and `FindAttachedItemByHolder` returns the first slot whose asset matches (`LoadoutComponent.cpp:3508-3518`).
Two pieces of the same asset (D-110's Ares example: **two Trainer adapters**, each with a Lightning crystal)
share one wear record. With D-110 this becomes a real bug.

**P-D. Run vs persistent boundary missing.** Found gear is permanent (`TrialLoot.cpp:123` → pool). D-097 says it
is lost at run end with upgrade progress remembered per definition. Nothing marks an instance as run-only.

**P-E. Presets are not saved.** Player edits to `SavedLoadouts` live on the pawn and are rebuilt from the
authored asset at each spawn (`InventoryComponent.h:106-109`, pool debug text `PoolSubsystem.cpp:795`
"SavedLoadouts stay authored"). The `Wearers` map survives, the preset that justified it does not.
(Worth a PIE check: edit a preset in the hub, travel, look.)

**P-F. Four item layers.** Storage → party pouch → per-member crystal mirror → per-preset `ItemSlots` (6×3,
pre-B-203 rule, `FCombatLoadout.h:174`) + `BattleItemsTaken`. D-097 says "in a fight you use only what's in the
inventory": the preset item slots are a second pouch with a different cap.

**P-G. Linear GUID scans.** ~55 `== PersistentID / InstanceID / GearID` comparisons in `Private/` (15 in
`InventoryComponent.cpp`, 13 in `PoolSubsystem.cpp`, 9 in `EconomyService.cpp`), each over a per-kind array.
Fine at today's sizes (tens), but every new kind adds another set of finders (A13/A12 in the unified-loadout analysis).

**P-H. Save shape ≠ runtime shape, the other way round.** The save already has one `FSavedOwnedItem` for every
kind (`WoRSaveTypes.h:255-276`), split into five lists by kind; runtime has five struct types. Good news: the
save is already closer to the target than the runtime.

**P-I. Per-character vs shared, mixed per kind.** Gear is shared (pool), spells/abilities and crystals are
per character on the pawn but also listed in the pool (`PoolSubsystem.h:301-315`), the pouch is per party,
wallet per player, wear per member. The rule "who owns this kind" is spread over comments (B-091 hand-off:
"skills and crystals stay per character").

**P-J. Co-op gaps.** The pool is a GI subsystem: on a listen server the host's GI only knows the host's pool;
clients' gear never reaches the server. `Wearers` keys by character asset path, not by player
(`PoolSubsystem.h:187`). Pool code looks up player 0 (`CoopStateOwnership.md:58`). No inventory state
replicates (`:59`). Personal loot (D-103) needs a per-player destination on the server. The co-op doc's
order (§"Suggested order") has no step for the owned store.

**P-K. Dead / duplicate wallet.** The pool's currency block (`PoolSubsystem.h:326-345`) mirrors the PlayerState
wallet and is unused outside the pool; three wallet places (PS, pawn fallback, pool mirror) plus two save spots.

**P-L. Not designed yet, will add containers if built the current way:** run bag (D-097), upgrade memory
(B-238), bought run loadout (B-116), form meters (B-279), owned outfit pieces. Each would be one more list.

## 3. Comparison: our implementation vs references

Sources: Lyra = Epic's Lyra Starter Game, `Source/LyraGame/Inventory/` and `Equipment/` (from memory of the
code: `ULyraInventoryManagerComponent` holds an `FLyraInventoryList` fast array of `ULyraInventoryItemInstance`;
`ULyraInventoryItemDefinition` + `ULyraInventoryItemFragment` subclasses such as `InventoryFragment_EquippableItem`;
`ULyraEquipmentManagerComponent` holds `ULyraEquipmentInstance`s spawned from an item; `ULyraQuickBarComponent`
holds slot → item-instance pointers). ArcInventory (community, GAS-based, from memory, low confidence on details):
items as stacks with fragments, slots defined by gameplay tags, abilities granted on equip. I don't know the
"Inventory System" / "Generic Item System" plugins well enough to describe; omitted. Game references are from memory.

| System | Ours (file:line) | References | Gap | Keep / change |
|---|---|---|---|---|
| **Pool / armoury** | GI subsystem, 3 typed lists of full instances (`PoolSubsystem.h:291-298`) | Lyra: one inventory component, kind-agnostic instances + definition fragments. MH World: Equipment Box separate from Item Box. Diablo IV: account stash (shared across characters), tabs. | typed lists; GI not PS; copied into pawns | **Change**: one kind-agnostic store, on the PlayerState (co-op). Keep the MH "box" as a *view*. |
| **Per-character inventory** | full armoury copy per pawn + sync (`InventoryComponent.cpp:1282-1346, 850-1530`) | Lyra: the pawn has an equipment manager only; the inventory lives on the controller and the pawn reads it. D4: per-character bags exist because characters do not share gear; we share (D-020). | copies + ~700 lines sync | **Change**: no per-character owned list; a character has equipment (references) only. |
| **Storage + party slots** | storage + pouch in pool (`PoolSubsystem.h:311-370`) + per-member mirror (`CrystalInventoryComponent.h:110`) | MH: Item Box (big), Item Pouch (fixed slots), auto-refill from box at camp; Elden Ring: carry caps per item, overflow auto-sent to the chest (D-097 copies both rules). E33 / FF: consumables party-shared. | the per-member mirror and per-preset `ItemSlots` | **Keep** the D-097 rules; **change** to one stack store with a "carried" flag/count, no mirrors. |
| **Loadouts / presets** | `FCombatLoadout` by value with gear caches (`InventoryComponent.h:108`); not saved | MH Rise/World: equipment loadouts store references to box pieces, an item loadout stores counts to refill; Lyra quick bar stores item-instance pointers. D4: none (wardrobe only). | copies; unsaved; per pawn | **Change**: preset = GUID list + picks; saved per character in the save (D-059). |
| **Runtime equipped loadout** | `SavedLoadouts[Active]` read through 10 getters (D-110 analysis §1) | Lyra: `ULyraEquipmentInstance` spawned per equipped item, owns actors/abilities while equipped; inventory item stays the truth. | equipped state and owned state are the same struct copied | **Change** (D-110 + this): equipment component resolves slots → store instances each read (or caches pointers per fight). |
| **Run bag / run-only** | none; found gear permanent (`TrialLoot.cpp:123`) | Hades / roguelites: run boons discarded, meta currency kept; MH: quest rewards to box; Nightreign: run gear discarded, relics kept. | missing | **Change**: run layer = a run flag on instances + upgrade memory per definition, merged/dropped at run end. |
| **Attachments** (crystal, stone, fusion, evolution) | on the gear instance (`FRuntimeAttachedItem`), wear copied per run by asset (`CarriedGearState.h:20`); evolutions both in `Entries` and on gear (`EvolutionInventoryComponent.h:57-60`) | PoE: gems sit in an item's sockets and travel with it; the gem is its own item (level, quality). FF7R: materia in weapon slots, materia its own item. | run wear keyed by asset; evolution state duplicated on attachment | **Keep** "on the item" (D-110 A). **Change**: wear lives on the instance (run layer), evolution referenced by GUID. |
| **Outfits** | design only (`CharacterCustomisation.md` §4) | MH layered armour: unlocked pieces list per hunter + per-slot choice; Lyra cosmetics: a separate pawn component reading a controller-level set. | no owned-pieces store designed | **Change (design)**: owned pieces are store entries (kind Outfit, stackless); a set = six GUIDs/defs. |
| **Forms / meters** | design only (B-279) | MH weapon mastery / Hades affinity: per-profile counters keyed by definition. | would be another list | **Keep** as counters keyed by character definition in the progression part of the save, not in the store. |
| **Saves** | one `FSavedOwnedItem`, 5 kind lists (`WoRSaveTypes.h:255-351`); v3 | Lyra has no save; typical UE practice: explicit structs, versioned (we do this, D-059). | lists by kind; no presets; no run bag | **Change**: one `Items` list with a kind field, presets per character, run layer in `FSavedRun`; v4 (D-110 already bumps). |
| **Co-op ownership** | pool on GI, wallet on PS, nothing replicates (`CoopStateOwnership.md:44-59`) | Lyra: inventory list is a `FFastArraySerializer` on a replicated component, server-authoritative, owner-only. D4 / Nightreign: personal loot instanced per player. | no server copy of a client's gear | **Change**: store component on `AWoRPlayerState`, fast array, `COND_OwnerOnly` for the full list; equipped GUIDs replicate to all. |
| **Shop / merchant** | stateless stock from data + seed (`MerchantStockRoll.h:56-58`) | standard | none | **Keep**. Purchase = server adds an instance to the buyer's store. |
| **Wallet** | PS component (D-046) + pawn fallback + unused pool mirror | Lyra: currency usually as a stat/tag stack on the PS | dead mirror | **Keep** PS wallet; **cut** the pool mirror. |

## 4. Target model (plain terms)

```
 AWoRPlayerState ─ UOwnedItemsComponent  (one per player; replicated fast array, server-authoritative)
   Items: TArray<FOwnedItem>                       FOwnedItem
     ├ gear       (weapon / adapter)                 Guid, Def (asset), Kind, Count (1 for unique, N for stacks)
     ├ evolutions                                    Tier, Quality, Rolls, Attached (crystal/stone/fusion or evo Guid)
     ├ crystals / stones (stacks by type+tier)       Durability / wear (run-time value lives here, not on the slot)
     ├ spells / abilities (knowledge, per character  Location: Storage | Carried | Equipped(by)   ← views, not lists
     │    via an Owner field, as today)              bRunOnly (found this run)
     └ outfit pieces                                 OwnerCharacter (for per-character kinds; None = shared)
   Index: TMap<FGuid,int32>  (rebuilt on change; replaces the ~55 scans)
   RunLayer: upgrade memory TMap<Def, Tier/rolls>, run Gold (already in FSavedRun)

 Views (functions over Items, no storage):  Armoury = gear+evos · Storage = stacks not carried ·
   Pouch = stacks carried (≤ slot count) · Run bag = bRunOnly · Knowledge(char) · Wardrobe

 Per character (save + PS): Presets[≤5] = { Slots: [Guid…] (D-110 FLoadoutSlot holding a Guid + picks),
                                            ActiveWeaponSlot, ActiveAdapterSlot, EvolutionSeat Guid,
                                            spell/ability picks }, ActivePreset, Outfit {6 Guid/defs}, FormMeter
 Pawn: ULoadoutComponent = thin reader: resolves the active preset's Guids against the store
       (pointer cache rebuilt on OnItemsChanged; never a by-value copy)
```

How each ruling fits:
- **D-110 slots / points / one active per kind**: unchanged; `FLoadoutSlot` keeps `GearID` and drops its `Gear` cache
  (D-110 analysis §2.1 already has both — the cache becomes a resolved pointer). Q-7 "attachments on the instance" is this model.
- **D-097** storage / inventory / pool: one store with `Location`; the 10-slot cap is a rule on the Carried view;
  auto-refill = move Storage → Carried; "found items kept" = clear `bRunOnly` at run end for stacks; "found gear lost"
  = remove `bRunOnly` gear, write its tier/rolls into upgrade memory by `Def`; next drop reads it.
- **D-020 / B-091 one wearer**: `Location = Equipped(by character)` on the instance replaces `Wearers`; B-221's "worn
  piece leaves the list" is the Armoury view filtering `Equipped(other)`; B-221b's "preset keeps it, slot reads empty"
  falls out (the preset holds the Guid; resolution sees it is equipped by someone else).
- **D-021 / B-116 run purchase**: buying = mark chosen armoury pieces as taken for the run (or copy into the run bag
  if D-021 means a rental — §7 D-4); slot limits read D-110's table.
- **D-033 bound gear**: a field on the definition, checked against the wearer; and B-279 "wearing the skin makes bound gear
  count as owned" = a *virtual* store entry (no instance minted) shown while the skin is worn.
- **Forms (B-279)**: meter counters keyed by character definition, in the per-player progression save; "matching pieces"
  reads equipped Guids → Defs. Not in the store.
- **Outfits**: pieces are store entries of kind Outfit (Count 1, no rolls); `FOutfitSet` = six Defs (pieces are
  unique cosmetics, so a Def is enough); replicates as six ids (already the doc's plan).
- **Co-op / D-103 personal loot**: the server adds drops to the looter's PS store; each client owns its own
  list (owner-only replication); everyone sees what is equipped (the preset's active Guids + resolved Defs).
  Hand-over (D-103 "copy of the run save") copies each player's store as part of the run save.

## 5. What it simplifies and what it costs

Simplifies / cuts (estimates from `wc -l` and the sections cited):
| Cut | Today | Approx. lines |
|---|---|---|
| Per-pawn armoury copies + the sync layer (P-A) | `InventoryComponent.cpp:850-1530`, pool push/pull `PoolSubsystem.cpp:100-200` | 600-800 |
| Per-member crystal mirror + preset `ItemSlots` + return-dropped paths (P-F) | `CrystalInventoryComponent.cpp` (192), `LoadoutComponent.cpp:395-430, 1560-1630, 2310-2340` | 250-350 |
| Per-kind finders / add / remove (P-G; overlaps D-110's A13/A12 estimate) | Inventory, Pool, Economy | 150-250 beyond D-110 |
| Holder-keyed carried attachments → wear on instance (P-C) | `CarriedGearState.h`, `FSavedCarriedGear`, carry/restore in PartySession | 150-200 |
| Pool wallet mirror (P-K) | `PoolSubsystem.h:317-345` + cpp | 50-80 |
| Separate save lists → one list | `WoRSaveTypes.h:322-351`, `SaveSubsystem.cpp` | 50-100 |
| **Total, on top of D-110's ~1,700-2,100** | | **~1,250-1,800** |
Also avoided: the containers P-L would add (run bag, upgrade memory, bought loadout, outfits) — each becomes a
field or view instead of a list with its own save and sync. Perf P1's root goes away (no gear copies in presets).

Costs:
- **Save migration**: v4 (D-110 already bumps) or v5: fold five `FSavedOwnedItem` lists + stacks into one list; add
  presets per character (new data, defaults from authored assets on first load); move `CarriedGear` wear onto items.
  v3 must load (above `MIN_SUPPORTED`, `Public/Save/WoRSaveGame.h:15-17`).
- **Call sites**: every `Inv->Weapons` / `Rings` / `EvoInv->Entries` / `CrystalInv->` reader. Keep them as views
  (const array builders or forwarding getters) so callers move late, like D-110's forwards.
- **Pawn lifetime**: the store must exist before the pawn (PS or, until the co-op travel job lands, the GI pool
  re-shaped). Start it in `UPoolSubsystem` (already GI, already saved) and move the component to the PS with the
  co-op step; same struct.
- **Co-op replication**: a `FFastArraySerializer` of `FOwnedItem` (owner-only), plus server RPCs for every mutation
  (equip, socket, move to pouch, buy, dismantle). This is work co-op needs anyway; with five containers it is five times.
- **Risk**: authored `UInventoryData` presets (inline `.uasset`, unreadable here) stay the authoring shape; they seed
  a character's presets once (first load), as D-110 §4 already plans for `FSavedLoadout`.

## 6. Relation to B-301..B-309 and an incremental path

D-110 **stands as step one**. Its slot model is needed in either world, and its B-308 ("`OwnedGear` with weapon /
adapter views, save v4") is the natural seam: aim B-308 at the store instead of a per-character list.
**Minimum change to D-110's plan:**
1. B-302: `FLoadoutSlot` keeps `GearID` as the truth from day one and treats `Gear` as a derived cache (already the B-085 rule; just say it).
2. B-308: put `OwnedGear` (kind-agnostic, by GUID, with an index map) in **`UPoolSubsystem`**, not `UInventoryComponent`; the
   inventory's `Weapons` / `Rings` become views over the pool. Same cluster count.
3. Fix P-C (wear keyed by asset) **before B-304** lands two same-asset adapters: key `FCarriedAttachment` by `GearID`.

Proposed order (≤3 files per cluster; "S" = after D-110):
| # | Cluster | Files (≤3) | Notes |
|---|---|---|---|
| 0 | Perf P1 (`const &`) | already planned | independent, do anytime |
| 1 | `FCarriedAttachment` keyed by `GearID` (holder kept as fallback for old saves) + spec with two same-asset rings | `CarriedGearState.h`, `LoadoutComponent.cpp`, spec | before B-304 |
| 2-9 | B-301..B-307 as planned (with change 1 above) | — | — |
| 10 | B-308a: `FOwnedItem` + `UPoolSubsystem::Items` + index; `OwnedWeapons/Rings/Evolutions` become views | `PoolSubsystem.h/.cpp`, spec | replaces "OwnedGear on inventory" |
| 11 | B-308b: save v4 one list + loader for v3 | `WoRSaveTypes.h`, `SaveSubsystem.cpp`, `WoRSaveGame.cpp` | D-110's bump |
| 12 | S1: inventory reads views; `DrawSharedGear` stops copying; sync layer marked obsolete | `InventoryComponent.h/.cpp`, spec | PIE: three members, one sword |
| 13 | S2: presets as GUID lists saved per character (`FSavedPreset`), seeded from authored assets | `WoRSaveTypes.h`, `SaveSubsystem.cpp`, `InventoryComponent.cpp` | fixes P-E |
| 14 | S3: `Location` on stacks (Storage / Carried); crystal mirror and preset `ItemSlots` forward to the pouch | `PoolSubsystem.*`, `CrystalInventoryComponent.cpp` | fixes P-F; Q on the 6×3 rule |
| 15 | S4: run layer: `bRunOnly` + upgrade memory; `TrialLoot::GrantTo` marks; run end merges/drops | `PoolSubsystem.*`, `TrialLoot.cpp` | = B-238 rebuilt on this; unblocks it |
| 16 | S5: `Equipped(by)` replaces `Wearers`; B-221/B-221b become view filters | `PoolSubsystem.*`, `InventoryComponent.cpp` | supersedes B-221 branch logic |
| 17 | S6: delete obsolete sync + mirrors after PIE (B-309 style) | ≤3 per cluster, 2-3 clusters | — |
| 18 | Co-op: move the store to `AWoRPlayerState` component, fast array + server RPCs | after `CoopStateOwnership` step 1 | add as step 7b in that doc |
Outfits (B-279) and B-116 build directly on the store once S3-S4 exist; no new containers.

## 7. Decisions for Crown (non-lore; each ships with the default unless Crown changes it)

- **D-1 One owned-items store per player, with views, instead of separate containers?** Default **yes** (Lyra inventory
  component + fragments; MH box/pouch as views of one ownership; D-097's rules unchanged).
- **D-2 Where does it live?** Default **the pool subsystem now, the PlayerState with co-op** (Lyra: server-owned
  replicated component; CoopStateOwnership already says pool → PS).
- **D-3 Presets saved, as GUID references?** Default **yes, per character, ≤5** (MH equipment loadouts reference box pieces;
  a preset that loses its picks on travel is a bug to players).
- **D-4 Run purchase (D-021): take your own pieces, or a run copy?** Default **take your own** (the piece is marked "on the run";
  wear and upgrades during the run stay on it, since D-093 resets wear anyway). Note: it changes nothing about "found gear is lost".
- **D-5 Retire the per-preset item slots (6×3) for D-097's pouch?** Default **yes** (MH: one pouch; D-097 "in a fight you use
  only what's in the inventory"); one rule, one cap.
- **D-6 Who owns knowledge (spells, abilities)?** Default **keep per character** (as B-091 left it) but stored in the player's store with an
  owner field, so co-op and save have one path.
- **D-7 Outfit pieces as store entries, sets as six definitions?** Default **yes** (MH layered armour unlock list).
- **D-8 Equipped gear in co-op is visible to all, the rest owner-only?** Default **yes** (Lyra owner-only inventory; D-103 personal loot).
- **D-9 Order: D-110 first, store at B-308?** Default **yes**, with the three small changes in §6.

## Appendix: numbers used
`InventoryComponent.cpp` 2,423 lines; `PoolSubsystem.cpp` 847; `CrystalInventoryComponent.cpp` 192;
`EvolutionInventoryComponent.cpp` 142; `SaveSubsystem.cpp` 684; `PartySessionSubsystem.cpp` 854;
GUID compare sites in `Private/`: 55 (tests included).
