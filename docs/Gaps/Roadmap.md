# World of Refraction — Roadmap

Backlog banked at the end of the merchant/shop arc (feature/hub-merchants → main, 2026-07-11). This is the working queue of everything not-yet-built, grouped by scope. Items move out of here when a sprint is opened against them.

---

## Design Gaps — small, own arc each

Small greenfields with clear scope. Each unblocks something already authored or fixes a known issue.

- **Aura source-element greenfield.** Two Enhancement spells (`DA_Spell_Aura`, `DA_Spell_EnhancementAura`) currently ship with empty `ReferencedEffects` — inert on cast. Needs runtime element-inheritance for buff types (`StatusMultiplierBuff` + `ResistanceBuff`): expand `ActionExecutor::ApplySkillEffects` element-stamp branch (~line 6206) to cover buff types, and upgrade the support-spell resolver (~line 2219) to call `ResolveSpellCastElement`. ~5–8 lines C++. Unlocks two already-authored spells.

- **Cleanse payload wiring.** `DA_Spell_Cleanse` authored with empty `ReferencedEffects`. Cleanse effect type exists per `USkillEffectManager`; needs payload shape confirmed and wired into the spell asset.

- **Placeholder stone text pass.** Ten of sixteen stones return vague placeholder text from `CrystalDescription::GetItemEffectText`: `DefenseStone`, `CritStone`, `TurnSpeedStone`, `StatusStone`, `EfficiencyStone`, `MaxHPStone`, `MaxEPStone`, `LuckStone`, `ReflexStone`, `AbilityStone`. Collapse into one generic branch — `"+X% {stat} when attached"` — driven by `CrystalEffectTable::StoneTargetStat` + `GetStoneBasePercent`, with pool stones (`MaxHP`, `MaxEP`) using name overrides. Bespoke text kept for `AbilityStone` (slot grant — depends on Cluster 4), `DurabilityStone` (flat fusion durability), `HealingStone` (consumable), and `DamageStone`'s Resonate rider sentence.

- **Rings `DefaultSpells` — 9 crystal rings.** The nine element crystal rings (`DA_Ring_Garnet` … `DA_Ring_Iolite`) ship with empty `DefaultSpells`; the player fills them. Open design call: should they carry starter spells (e.g. 1 low-tier element spell each), or stay as blank template-with-gem?

---

## Polish — deferred

Content-layer work that improves feel or completeness without unblocking new mechanics.

- **Mannequin merchants.** Merchants currently render as placeholder cube actors (`AMerchantInteractable`). Attempted mid-session — swap to `SKM_Quinn` failed (root component type change broke overlap trigger). Deferred to a proper `AMerchantCharacter` actor class with correct character mesh + AnimBP + interaction trigger.

- **Icons pass.** ~301 pool assets have no `Icon` field authored (weapons, rings, spells, abilities, evolutions, crystals). Icon pack available (Fab listing referenced in the merchant arc). Needs bulk-assign patterns — icon per asset class, or per element, or per school. Sequencing: probably after Cluster 4 (any UI reorganisation that references icons should land first).

- **Weapon-flag split.** `bAbilitiesLocked` and `bSpellsLocked` currently share the "conjured weapon / conjured ring" meaning. Design banked to split into two flags: one for switch-behaviour (can equipment be switched), one for show/hide behaviour (are extra slots visible). No runtime consumer changes yet.

---

## Bigger arcs — banked, own sprints

Multi-cluster arcs. Each needs its own survey + design pass before authoring.

- **Hub → Trial door transition.** Player walks through a door in the hub → level load into a trial level. Interactable door actor, level streaming or hard load, hub state preserved through the transition. **NEXT ARC after roadmap doc.**

- **Encounter-based trial system.** Walkable trial space, touch enemy → combat starts. Enemy actors with detection radius, radius-joining for multi-enemy encounters, combat orchestrator hook to start combat from world-space collision. **ARC AFTER TRIAL DOOR.**

- **Combat camera build.** Sequencer-per-skill + distributed camera state selector, replaces the older `CombatCameraManager`. Design banked in prior session's design bank (`docs/Design/Resources_Design.md` or equivalent).

- **Save / Persistence keystone.** All persistent balances (`Prisms`, `Diamond`, `GearEssence`, `SkillEssence`, `EssenceTyped`) are `SaveGame`-tagged but no save system exists yet. Unblocks head-start persistence, account-scope routing (Prisms/Diamond → PlayerState), inventory persistence across runs. Keystone dependency for everything session-scoped today.

- **Networked multiplayer + PvP.** All economy/inventory/combat systems are replication-aware from the ground up. Prereq for Lord-vs-team PvP (the Lord/Contender challenge hierarchy). Needs dedicated server or listen-server architecture decision, matchmaking, session flow.

- **Cooperative AI.** Ally targeting, revive AI, self-ward. Currently AI is enemy-only. Adds ally enumeration, ally healing/ward decisions, revive-dead-ally logic (needs `GetDeadAllies` — flagged deferred in prior session).

- **Weapon-flag split runtime.** Beyond the design-locked split above — actual runtime consumer changes for switch-vs-hide behaviour on conjured equipment.

---

## Immediate next

1. This roadmap doc → PK
2. **Hub → Trial door arc** — new branch, survey level architecture + door interaction pattern.

---

*Last updated: 2026-07-11 (post-merchant/shop arc merge to main).*
