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

- ~~**Hub → Trial door transition.**~~ **SHIPPED** (`79314c2e`, feature/trial-transition). `ATrialDoor` with element-tinted mesh, `UTrialRunSubsystem`, `UTrialData`.

- ~~**Encounter-based trial system.**~~ **SHIPPED** (T-C1, feature/trial-transition). `UEncounterComponent` trigger sphere + join window + Void arena bubble, `ABattleGameMode` battle stage, return-to-trial via `UTrialRunSubsystem::ExitEncounter`. Multi-enemy encounters and encounter composition banked below.

- **Combat camera build.** Sequencer-per-skill + distributed camera state selector. The old `CombatCameraManager` is **gone** — deleted outright on `feature/camera-removal` (2026-07-20), no rewrite, so this is a clean greenfield rather than a replacement. Design banked in `docs/Design/Resources_Design.md` §§1371-1450. Combat currently runs with no camera system at all: the view stays with the PC0-possessed pawn wherever the grid places it.

- **Save / Persistence keystone.** All persistent balances (`Prisms`, `Diamond`, `GearEssence`, `SkillEssence`, `EssenceTyped`) are `SaveGame`-tagged but no save system exists yet. Unblocks head-start persistence, account-scope routing (Prisms/Diamond → PlayerState), inventory persistence across runs. Keystone dependency for everything session-scoped today.

- **Networked multiplayer + PvP.** All economy/inventory/combat systems are replication-aware from the ground up. Prereq for Lord-vs-team PvP (the Lord/Contender challenge hierarchy). Needs dedicated server or listen-server architecture decision, matchmaking, session flow.

- **Cooperative AI.** Ally targeting, revive AI, self-ward. Currently AI is enemy-only. Adds ally enumeration, ally healing/ward decisions, revive-dead-ally logic (needs `GetDeadAllies` — flagged deferred in prior session).

- **Weapon-flag split runtime.** Beyond the design-locked split above — actual runtime consumer changes for switch-vs-hide behaviour on conjured equipment.

---

## Session-added banked arcs (2026-07-20, T-C1 encounter loop)

Banked during the T-C1 encounter-loop arc. Each needs its own survey before authoring.

- **AI/Player identity refactor.** Move `bIsAIControlled` from a data-asset flag to a runtime source — the possessing controller becomes the source of truth. Data assets retain a DEFAULT flag; a runtime component overrides it. Required for PvP and party possession swap.

- **Encounter Composition.** `UEncounterData` asset defining opposing-team roster + difficulty. Grid position moves onto `ULoadoutComponent` (per-run customisable). AI grid selection starts as a placeholder heuristic (tanks front / casters back), evolving to Utility AI. Team names deferred (PvP-driven). Multi-enemy encounters, boss fights, and encounter randomization are all downstream of this.

- ~~**Combat Component C++ promotion.**~~ **COMPLETE** (`feature/combat-components-cpp`, merged `75ed7c37`, 2026-07-20). Eight components promoted to `CreateDefaultSubobject` on `ACombatCharacter`: **WeaponMesh, Currency, Inventory, CrystalInventory, EvolutionInventory, InfusionVFX, Loadout, BrokenDarkness** — joining `CharacterDataComponent` from T-C1a. Gains the lifecycle guarantees the T-C1a deferred-spawn bug exposed, typed C++ access, and a clean Components panel. See `docs/Architecture/CombatCharacter.md`.

  Three things worth carrying forward:
  - **`UCombatMovementComponent` was a phantom** — this entry originally listed it, but it had already been dissolved when warp positioning replaced the movement system (`9563ff2d` / `9d064648`). `EvolutionInventory` took its slot. The entry was written from a stale doc snapshot; verify against source, not docs, when scoping.
  - **`InitializeBornBrokenDarkness()` hook pattern.** Promotion changes component `BeginPlay` order (natives run before SCS, natives among themselves in constructor-declaration order), and UE gives no ordering guarantee. `UBrokenDarknessManager`'s born-BD flip read a flag the `CharacterDataComponent` cascade seeds, so any reorder broke it — silently, no error, born-BD characters only. Fixed by an explicit idempotent call from the cascade rather than an ordering assumption. **Copy this pattern for any future component whose init depends on cascade state.**
  - **Every captured SCS default across all 9 BPs was already at its C++ default**, so no re-entry was needed anywhere. `ULoadoutComponent::CharacterClass` looked like the high-risk property and was not: the cascade overwrites it from `CharacterData->CharacterClass` before anything reads it.

  Still on SCS by choice: `hubCamera` / `hubSpringArm` (engine, hub-navigation) and `elementColorDebug` (debug-only).

- **SubmitAction hybrid router retirement.** The D8 deferred-ritual path (`CombatOrchestrator.cpp:741`) still routes through the sync/async hybrid `SubmitAction`. Convert it to `SubmitActionAsync`, then remove `SubmitAction` and its `bRequiresAsync` test entirely. Note: the conversion is not a one-liner — that callsite needs the `bool` return for its turn-stranding recovery.

- **Multi-level-per-door.** Pinned. `UTrialData` grows named `EncounterLevel` entries (currently a single level ref). Player chooses at the door.

- **Persistence keystone.** State must survive `OpenLevel` transitions. Encounter entry + exit currently wipes character HP/EP/inventory/currency, and the trial level reloads whole from the map asset (defeated enemies respawn). Prerequisite for real gameplay loops. Solve via `UGameInstance`-scoped persistent player state. Overlaps the Save/Persistence keystone above — treat as one arc.

- **Multi-player BattleGameMode.** Bootstrap possesses each `LocalParty` pawn with a matching PlayerController (PC0, PC1, PC2…). Requires networked play mode. T-C2+.

- ~~**Camera system full replacement.**~~ **DELETION SHIPPED** (`feature/camera-removal`, 2026-07-20). `ACombatCameraManager`, `ECombatCameraState`, `EActionCameraPhase`, `BP_CombatCameraManager`, and all placed instances + home-camera actors removed. The from-scratch build remains open — tracked as **Combat camera build** above; Sequencer-per-skill + distributed state selector retained as the design bank in `docs/Design/Resources_Design.md`.

---

## Session-added banked arcs (2026-07-20, party rename)

- ~~**Team0/Team1 → LocalParty/OpposingParty rename.**~~ **COMPLETE** (`feature/party-system`, merged `ef0e71f7`, 2026-07-20). 660+ occurrences across 17 files, pure mechanical rename. Perspective-based naming: under PvP both sides are player parties, each client seeing itself as `LocalParty` — which 0/1 indexing could not express.

  Two things worth carrying forward:
  - **`UTurnManager::InitializeCombat` was 1-based** (`Team1`, `Team2`) while every other file was 0-based, so `Team1` meant the LOCAL party there and the OPPOSING party everywhere else. A blanket find/replace would have compiled, run correctly, and left a parameter named `OpposingParty` assigned `TeamIndex 0` — a lie that only bites when someone later trusts the name. **Check for mixed indexing conventions before any large rename.**
  - **BP nodes orphan silently on C++ signature renames.** `OnCombatStartedUI` is a `BlueprintImplementableEvent` override; renaming its params detaches the override with no compile error and the combat HUD simply stops building. Always re-verify BP overrides after a rename.

- **Team-index vocabulary migration.** Deferred. `TeamIndex`, `GetActorTeam`, `GetTeamMembers`, `EnemyTeam`, `SourceTeam`, `TargetTeam`, `SelfTeam`, `UserTeam`, `PositionInTeam`, `ComputeTeamHPPercent` (~200 sites) all still use `Team*`. This is deliberate — the `int32` index is the low-level mechanism, distinct from the party domain concept. Revisit if the mixed vocabulary becomes a readability cost.

- **LevelMechanics legacy retirement.** Placed actors in `LevelMechanics` still carry the old `"Team0"` / `"Team1"` string tags, so `ACombatOrchestrator::DebugStartCombatWithLevelActors` finds nothing there (characters "disappear" from that debug flow). Debug-only — `bAutoStartCombat` defaults false and the T-C1 production path spawns from a stashed roster, never from tags. Either re-tag the actors to `LocalParty` / `OpposingParty`, or retire the whole `BP_Combat_GameMode` debug flow now that the encounter loop supersedes it. Leaning retirement.

---

## Immediate next

1. **Encounter Composition** — `UEncounterData` roster + difficulty, grid position onto `ULoadoutComponent`. The remaining unblocked T-C1 arc, and the gateway to multi-enemy encounters, boss fights, and encounter randomization.
2. **AI/Player identity refactor** or **SubmitAction hybrid router retirement** — both small, both unblocked.

---

*Last updated: 2026-07-20 (post-party-rename arc).*
