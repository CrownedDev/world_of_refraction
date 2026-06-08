# Duplication / Shadow-Logic Audit

**Date:** 2026-06-08
**Branch:** `feature/weapon-stones`
**Status:** SURVEY — no edits made. This is a map, not a refactor. Nothing in `Source/` was changed.

A "shadow formula" is a derived value whose math is hand-copied inline in a second
place instead of routing through one canonical getter. The original case: the
Broken-Darkness energy bake copied StatusMultiplier + Efficiency and **drifted** —
it silently dropped the equipment term. Those two are now fixed (routed through
shared getters). This audit expands the hunt to the whole `Source/` tree across 8
domains.

---

## How to use this doc

**Before adding a stone, an equipment bonus, or a new stat/derived value, check
here first:**

1. **Find the stat/value's canonical owner** in the domain tables below. Apply your
   change there, not at a read-site, unless the table says the stat is "many hooks."
2. **If it's a "many-hooks" stat** (see the read-site table), you must wire your
   bonus at *every* listed read-site or the systems will disagree. Prefer first
   adding a shared helper so it becomes "one hook."
3. **If you change a formula's *shape*** (a base term, a clamp, which pillar feeds
   it — not just a `*_PER_POINT` constant), search for its twin in the
   asset↔component family (§1) and update both. The shared constants protect against
   *value* drift; they do **not** protect against *shape* drift.
4. **If you add an enum value** (element, action type, effect type, class), check §8
   for every switch that must be updated in lock-step.

---

## PRIORITY SUMMARY

### (a) ACTIVE — live disagreement today

No second BD-style numeric drift exists right now: every duplicated formula *body*
currently agrees because the `*_PER_POINT` constants are shared. The two live
**inconsistencies** are structural, not arithmetic:

| # | Issue | Where | Why it's live |
|---|---|---|---|
| A1 | **TurnSpeed skips the crystal layer** | `TurnManager.cpp:349` reads asset `CalculateTurnSpeed()`; there is **no** `GetEvolutionModifiedTurnSpeed()` | Every *other* Spirit-derived combat stat (StatusMultiplier, Resistance, Luck, MaxEP) gets the slotted evolution-crystal pillar modifier via a `GetEvolutionModified*` getter. TurnSpeed alone does not — so a crystal that boosts Spirit changes those stats **but not turn order.** Same root cause as the BD bug (a stat bypassing the crystal layer); it just never had a getter to begin with. Blast radius: turn order / initiative. |
| A2 | **Efficiency means two numbers** | energy-cost path adds equipment `BonusEfficiency` (`ActionExecutor.cpp:3143`); crystal-wear paths read the bare getter (`CrystalManager.cpp:72,170,463,538`) | The same actor's "efficiency" is gear-inclusive when paying for a spell but gear-exclusive when computing crystal wear. Possibly intentional (wear tracks *character* power, not gear) but undocumented and fragile. Blast radius: durability wear rates. |

### (b) LATENT — agrees now, drifts on next edit (ranked by blast radius)

| # | Hazard | Sites | Blast radius |
|---|---|---|---|
| B1 | **Stat-formula twin family** — asset `Calculate*` ↔ component `GetEvolutionModified*` (7 bodies) + MaxHP/MaxEP inline | `CharacterData.h:310-480` ↔ `CharacterDataComponent.cpp:573-662` + `:520-529` | **Largest.** This is the exact BD-bug pattern, spread across two files. A shape edit in one family silently diverges. |
| B2 | **Animation play-rate 3-hook** — asset `Calculate*Speed()` + inline equipment bonus re-stated per action type | `ActionExecutor.cpp` spell `:2412/:2423`, ability `:2858/:2869`, attack `:2900/:2911` | New action type ⇒ 3 edits; constant change ⇒ 3 sites must sync. No canonical wrapper. |
| B3 | **`ECharacterClass` active-slots** — class→which slots are active, encoded twice | `LoadoutComponent.cpp:2987` (`GetCombinedStatBonus`) vs `:3068` (`GetActiveEffects`) | Stat bonus and effects can disagree about which slots a class uses. Already flagged drift-prone in a prior session. |
| B4 | **`EActionType`→stat scattered** — no canonical owner for action→primary-stat | `ActionExecutor.cpp:254,:1496`; `AIDecisionManager.cpp:196,:1284+` | New action type touches many switches. |
| B5 | **AI infusion/buildup multipliers re-applied** instead of calling the canonical getter | AI `:721,:776,:1685,:1773` (L2 dmg), `:1707,:1796` (L1 buildup) vs `ActionExecutor.cpp:3412/:3438` (`Get{Spell,Ability}ChargeDamageMultiplier`), `:784` | AI/runtime divergence class. Shares the constant today; the level→multiplier *mapping* is re-encoded in AI. |
| B6 | **`ESkillEffectType` buff/debuff** — two complementary switches | `ActiveSkillEffect.h:637` (`IsBuff`) / `:703` (`IsDebuff`) | New effect type must be classified in both, or it falls through as neutral. |
| B7 | **StatusMultiplier 3-copy** | asset `CharacterData.h:343`, component `CharacterDataComponent.cpp:647`, runtime `StatusBuildupManager.cpp:229-231` | Three bodies agree on the base Spirit term; the SBM one is the superset (adds equip + stone). |
| B8 | **BD overload energy bake** | `CombatOrchestrator.cpp:1014-1043` computes then passes `StatusMult × Efficiency` to `ProcessOverloadTick` (`BrokenDarknessManager.cpp:583`) | Agrees today (routed through getters); rare code path, easy to desync if a future stone touches efficiency. |

### (c) INTENTIONAL / benign

| Item | Why it's fine |
|---|---|
| Asset `Calculate*` family as a whole | The data asset has no actor/`GetOwner` context, so it *cannot* apply crystal or equipment modifiers. The asset path is the deliberate no-context fallback; the component `GetEvolutionModified*` twins layer crystal/equipment on top. (Still a latent *shape*-drift hazard — see B1 — but the duplication itself is by design.) |
| DamageStone two-path | `DamageCalculator.cpp:91` (attached, at calc time) and `ItemExecutor.cpp:380` (consumable, timed buff) both read one table `CrystalEffectTable::GetDamageStoneBasePercent`. Same source, different application domains. |
| Equipment per-point adds in DamageCalculator (`:63,:67`) | Term applications reusing the shared `*_PER_POINT` constant — not re-inlined stat formulas. |
| `ESpellElement`, `EPhysicalDamageType`, `ESubStat` single-owner switches | Each switched in exactly one canonical place (see §8). |
| Resistance aggregation in `AddStatusBuildup` | All sources (base, equipment, element stack, skill-effect) summed in one site (`StatusBuildupManager.cpp:353-376`), routed through `RESISTANCE_PER_POINT`. |

---

## §1 — Stat formulas

Carried forward from the prior stat-formula pass; re-confirmed this audit.

Two parallel families by design:

| Family | Location | Pillar input | Crystal? | Equipment? |
|---|---|---|---|---|
| **A — Asset** `Calculate*` | `CharacterData.h:310-480` | `GetEffective{Mind,Body,Spirit}` | ❌ | ❌ |
| **B — Component** `GetEvolutionModified*` | `CharacterDataComponent.cpp:573-662` | `GetEvolutionModified{Mind,Body,Spirit}` | ✅ | partial |

| Stat | Asset (A) | Component twin (B) | Status | Priority |
|---|---|---|---|---|
| SpellDamage | `:353` | `:581` | AGREES (shape match, diff input by design) | (b) B1 |
| RawDamage | `:457` | `:592` | AGREES | (b) B1 |
| CritChance | `:362` | `:604` | AGREES | (b) B1 |
| FlatDefense | `:383` | `:617` | AGREES | (b) B1 |
| Efficiency | `:315` | `:634` | AGREES | (b) B1 |
| StatusMultiplier | `:343` | `:647` | AGREES (3rd copy at SBM `:229`, superset) | (b) B1/B7 |
| Resistance | `:477` | `:659` | AGREES | (b) B1 |
| Luck | `CalculateLuck :426` | `GetEquipmentModifiedLuck :542` (superset: +equip +skill) | AGREES (B is superset) | (b) B1 |
| MaxHP / MaxEP | `:439` / `:448` | inline in `RecomputeMaxPools :520-529` (superset: +equip) | AGREES | (b) B1 |
| **TurnSpeed** | `CalculateTurnSpeed :412` | **none** — read directly at `TurnManager.cpp:349` | **NO CRYSTAL TWIN** | **(a) A1** |
| SpellSpeed | `:372` | none (asset only, `ActionExecutor.cpp:2412`) | NO CRYSTAL TWIN (visual only) | (c) |
| Action/AnimSpeed | `:391` / `:400` | none (asset only, see §5) | NO CRYSTAL TWIN (visual only) | (c)/(b) B2 |

All `*_PER_POINT` constants live once in `CombatConstants.h` and are shared, so the
duplicated bodies agree numerically today.

---

## §2 — Damage / resolution

Canonical owner: **`UDamageCalculator`** (`Private/Combat/Damage/DamageCalculator.cpp`).

| Path | Routes through canonical? | Notes |
|---|---|---|
| Attacker damage mult | ✅ `GetAttackerDamageMultiplier :257` → `GetEvolutionModifiedSpell/RawDamage` | clean |
| Crit chance | ✅ `GetCriticalChance :340` — sole owner; all callers route here (`:120,:405`, AI `:715,:770`) | one hook |
| Defender flat defense | ✅ `GetDefenderFlatDefense :282` — sole owner | one hook |
| BD overload aura/self damage | ✅ `BrokenDarknessManager.cpp:544` uses `GetEvolutionModifiedSpellDamage` as direct mult | clean |
| Inline damage-before-defense anywhere else | none found | — |

**Verdict:** core damage is clean. No inline mitigation/crit/resistance math found
outside DamageCalculator. The only damage-domain duplication is the **AI estimate**
(see §7).

---

## §3 — Status / buildup

Canonical owner: **`UStatusBuildupManager`** (`Private/Skills/Effects/StatusBuildupManager.cpp`).

| Concern | Site | Status |
|---|---|---|
| Buildup pipeline | `AddStatusBuildup :234-422` (6-step amp/reduce) — sole owner | clean |
| Attacker StatusMultiplier | `GetSourceStatusMultiplierFactor :194-232` (superset: char + equip + StatusStone). BD reuses it via `CombatOrchestrator.cpp:1027` | clean / one hook (but is the 3rd StatusMultiplier copy — B7) |
| Resistance application | `AddStatusBuildup :353-376` — all sources aggregated, routed through `RESISTANCE_PER_POINT :361` | (c) benign |
| Threshold / decay | `STATUS_EFFECT_THRESHOLD`, `STATUS_DECAY_RATE`, `STATUS_DECAY_FULL_RESET_TURNS` referenced only inside StatusBuildupManager | centralized |
| `AbilityData::CalculateStatusBuildup :75` | **calls** `Character->CalculateStatusMultiplier()` (not inlined) | clean — note it reads the *asset* StatusMultiplier (no crystal), an AI-vs-runtime nuance, not a copy |
| BD self-status | uses `bSkipBaseStatAmp=true` to avoid double-amp | clean |

**Verdict:** clean apart from the StatusMultiplier 3-copy already tracked in §1/B7.

---

## §4 — Cost / resource

Canonical owners: **`ActionExecutor::GetEffectiveEnergyCostEfficiencyMultiplier`**
(`:3111`) and the cost functions; **`CharacterDataComponent::RecomputeMaxPools`**
(`:500`).

| Concern | Site | Status |
|---|---|---|
| Efficiency multiplier | `GetEffectiveEnergyCostEfficiencyMultiplier :3111`; all 3 cost callers use it (`:292,:305,:849`) | one hook |
| Equipment efficiency term `1 − BonusEff×EPP` | single site `:3143-3146` | one site |
| Crystal-wear efficiency reads | `CrystalManager.cpp:72,170,463,538` read **bare** getter, **without** the equipment term applied at `:3143` | **(a) A2 — split read-sites** |
| Max pools | `RecomputeMaxPools :520-529` (base + equip bonus), single call site | centralized — but duplicates asset `CalculateMaxHealth/Energy` (B1) |
| BD energy release | `CombatOrchestrator.cpp:1014-1043` reads getters, passes product to `ProcessOverloadTick`; bake at `BrokenDarknessManager.cpp:583` | (b) B8 — agrees today |

---

## §5 — Turn / speed order

Canonical owner: **`UTurnManager`** (`Private/Combat/TurnManager.cpp`).

| Concern | Site | Status |
|---|---|---|
| Turn-speed base | `CacheActorStats :349` = asset `CalculateTurnSpeed()` + flat `BonusTurnSpeed :357` | **(a) A1 — no crystal twin** |
| Effective speed + buff/debuff | `CalculateSpeedRatios` lambda `:151-182` | one read system |
| TurnSpeedStone | `:177` `GetAttachedStonePercent(…TurnSpeed)` — one hook | clean |
| **Animation play-rate** | spell `:2412/:2423`, ability `:2858/:2869`, attack `:2900/:2911` — asset `Calculate*Speed()` + inline equip bonus, **re-stated 3×** | **(b) B2 — 3 hooks, no wrapper** |
| SpellSpeed skill-effect (buff/debuff) | wired **only** on spell path (`:2428-2432`); ability/attack have no equivalent | (b) inconsistent integration |

---

## §6 — Item / crystal / stone

Canonical owners: **`CrystalEffectTable`** (stone % + tier tables),
**`BreakCalculator`** (wear), **`InventoryConstants`/`ItemConstants`** (caps).

| Concern | Site | Status |
|---|---|---|
| Attached-stone % lookup | `CrystalEffectTable::GetAttachedStonePercent` (stat-match guarded) | one table |
| DamageStone base % | `GetDamageStoneBasePercent` — read by `DamageCalculator.cpp:91` (attached) + `ItemExecutor.cpp:380` (consumable) | (c) intentional two-path |
| Tier→slot count | `GetAttachmentSlotsForTier` via `ResolveSpellSlotCap` dispatcher | one owner |
| Durability wear | `BreakCalculator::CalculateDurabilityWear`; all callers route through it | one owner |
| Per-tier caps | `InventoryConstants::CRYSTAL_PER_TIER_CAP`, `MAX_QUANTITY_PER_ITEM_SLOT` | single constants |

**No central "apply attached stone for `ESubStat` X" dispatcher exists** — each stone
is wired at its own consumer (see read-site table). Today each is a *single* hook for
its stat, so this is acceptable; but a stone affecting a *new* stat means adding a
fresh read-site by hand.

---

## §7 — AI scoring (AI-vs-runtime divergence)

Canonical owners as in §2/§4. AI lives in `Private/AI/AIDecisionManager.cpp`.

| AI computation | Canonical it should call | AI site | Status |
|---|---|---|---|
| Crit expected-value | `GetCriticalChance` (it **does** call it `:715,:770`) then EV `×(1+chance×(mult−1))` | `:716` | AGREES — EV math correct, uses `CRIT_MULTIPLIER` |
| L2 infusion damage mult | `Get{Spell,Ability}ChargeDamageMultiplier` (`ActionExecutor.cpp:3412/:3438`) | re-applied inline `:721,:776,:1685,:1773` | (b) B5 — shares constant, does **not** call the getter |
| L1 infusion buildup mult | `ActionExecutor.cpp:784` (`SPELL_L1_BUILDUP_MULT`) | inline `:1707,:1796` | (b) B5 — ability path reuses the *spell* constant (TODO in code) |
| Threat level | `GetEvolutionModifiedRaw/SpellDamage`, `GetTotalStatusMultiplier` | `:901-922` — calls the getters | AGREES — clean |

**Pattern:** the AI correctly defers to canonical getters for *stats*, but **re-encodes
the infusion level→multiplier mapping** inline rather than calling the charge-multiplier
getters the live path uses. Agrees today via shared constants; the mapping itself is
duplicated.

---

## §8 — Enum / type switches

| Enum | Canonical owner | Duplicate switch sites | Status | Priority |
|---|---|---|---|---|
| **ESpellElement** | concern-split: `ElementColors` (color), `BarCapTriggerResolver` (→trigger), `StatusBuildupManager` (→immunity `:57`), `SkillEffectDisplayNames` (→name) | none — each mapping owned once | AGREES | (c) |
| **EPhysicalDamageType** | `BarCapTriggerResolver.h:54` (→trigger) | none | AGREES | (c) |
| **ESubStat** | `ActionStatModifiers::GetModifier :128` | none (other access unrolled by name, by design) | AGREES | (c) |
| **EActionType** | **none** | cost `ActionExecutor.cpp:254`, effects `:1496`; AI `:196,:1284+` | AGREES on concerns | (b) B4 |
| **ESkillEffectType** | apply: `SkillEffectManager.cpp:968`; name: `SkillEffectDisplayNames` | `ActiveSkillEffect.h:637` `IsBuff` / `:703` `IsDebuff` (complementary) | AGREES | (b) B6 |
| **ECharacterClass** | **none** | `LoadoutComponent.cpp:2987` `GetCombinedStatBonus` vs `:3068` `GetActiveEffects` (same class→active-slots map) | AGREES | (b) B3 |

---

## One-hook vs many-hooks (read-site map)

When you add a stone or equipment bonus, this tells you how many places you must wire
it. "One hook" = a single getter owns the application; "many hooks" = every read-site
must be edited or the systems disagree.

| Stat / bonus | Read-site(s) | Hooks | Adding a stone here means… |
|---|---|---|---|
| CritChance | `DamageCalculator::GetCriticalChance :340` (all callers route here) | **one** | edit one function |
| Defense | `DamageCalculator::GetDefenderFlatDefense :282` | **one** | edit one function |
| StatusMultiplier | `StatusBuildupManager::GetSourceStatusMultiplierFactor :194` (BD reuses) | **one** | edit one function |
| TurnSpeed | `TurnManager` speed lambda `:177` (only read system) | **one** | edit one lambda — but base lacks crystal (A1) |
| RawDamage / SpellDamage (equip) | `DamageCalculator :63/:67` | **one** | edit one block |
| **Efficiency** | energy-cost `ActionExecutor:3131/:3143` **+** wear `CrystalManager:72,170,463,538` | **many** | equip term only on the cost side today (A2); a new efficiency source must be added to **both** contexts |
| **ActionSpeed (anim play-rate)** | `ActionExecutor` `:2869` (ability), `:2911` (attack), `:2423` (spell, via `BonusSpellSpeed`) | **many (3)** | wire at all 3 animation sites |
| MaxHP / MaxEP (equip) | `RecomputeMaxPools :523/:529` | **one** | edit one function |
| Luck (equip) | `GetEquipmentModifiedLuck :552` | **one** | edit one function |

**Future stones that are "one hook":** Crit, Defense, StatusMultiplier, TurnSpeed,
Raw/Spell damage, MaxHP/MaxEP, Luck.
**Future stones that are "many hooks":** anything touching **Efficiency** (2 contexts)
or **ActionSpeed/animation** (3 sites). Add a shared helper before wiring those.

---

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-06-08 | Initial whole-tree duplication/shadow-logic audit (survey only, no edits). | `feature/weapon-stones` |
