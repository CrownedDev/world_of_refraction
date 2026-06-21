# Integration Gaps — 2026-06-21 Sweep Update

**Status:** Catalog update. Re-verifies the ~2026-05-29 gap docs against current `main` (post
dynamic-effect/defender-trigger merge) and adds the acquisition/progression/shop + attach/evolution
findings. Companion to `IntegrationGaps.md` / `Triage_2026-05-29.md` — merge these entries into
the catalog. File:line evidence is current as of 2026-06-21.

> **Forward design** for the acquisition/progression/shop gaps lives in
> `InstanceBasedRuntimeLayer_Design.md` (the build plan). This doc is the *status catalog*; that
> doc is the *what-to-build*.

---

## What the dynamic-effect / defender-trigger arc changed

- **2.3 (partial → changed):** `UDefenseSystem::OnDefenseResolved` now has a real production
  subscriber — `USkillEffectManager::OnDefenseResolvedHandler` (SkillEffectManager.cpp:54 →
  :1424). Defense outcomes now drive the effect system. The three *other* legacy defense
  delegates (`OnDefenseInputReceived` / `OnParryReflect` / `OnDefenseCueTriggered`) remain
  unsubscribed.
- **No other cataloged gap** (2.1, 2.2, 3.x, 5.x, 8.2, 9.1, 10.3, 10.7) was resolved by the arc —
  all re-verified STILL-OPEN.

## Session-doc correction (2026-06-20)

The 2026-06-20 session doc lists `RandomSkill`, `GuaranteedCrit`, `IgnoreDefense`, `DoubleHit`,
`Revive` as "log-only stubs." **Only `RandomSkill` is a genuine stub** (SkillEffectManager.cpp:1138,
no consumer). The other four are intentional passive markers, working, consumed at:
`GuaranteedCrit` (DamageCalculator.cpp:228), `IgnoreDefense` (DamageCalculator.cpp:252),
`DoubleHit` (ActionExecutor.cpp:2886), `Revive` (CharacterDataComponent.cpp:281). Fix that line.

## Evolution reversibility — design conflict RESOLVED (was a false conflict)

Crown wants an evolution removal method. The audit found **no hard "irreversible" rule exists**
anywhere in source or docs (grep zero hits for "irreversible"/"cannot be removed"/etc.). The
"permanent" language refers only to **durability immunity** (evolution crystals don't break), not
slot-locking:
- `Conceptual_Overview_2026-05-14.md:114` — "permanently modify … immune to breakage" → durability.
- `ItemSystem.md:85-87` — "permanent and its displayed durability is cosmetic" → durability.
- Only soft framing to reconcile: `Conceptual_Overview_2026-05-14.md:331` — "committed character
  archetypes" (narrative, not a mechanical lock).

**Verdict:** removal fills a genuine gap, no contradiction. Removal MUST reset `PrimarySlotType`
away from `Evolution` (`LoadoutSystem.md:333`) or weapon-switching stays locked after removal.

---

## Consolidated gap table (2026-06-21)

### Pitch-blockers (player-facing combat feedback — logic works, it's invisible)

| Item | Status | Evidence | Scope | Blocked-on |
| ---- | ------ | -------- | ----- | ---------- |
| 2.1 Defense prompt UI | Gap | DefensePromptWidget.cpp:15-62 (7 TODO Phase 1); never added to viewport | S | Design (defense UX: options/timer/input/fallback) |
| 2.2 OnDefenseWindowOpened no sub | Gap | AddDynamic → 0; consumer is 2.1's widget | =2.1 | Collapses to 2.1 |
| Defense feedback delegates | Gap | OnDefenseCueTriggered (DefenseSystem.h:198) fully dead; OnDefensePerfect (h:207) + OnParryReflect (cpp:154,391) no sub | S–M | Design + VFX/SFX |
| 3.2 Combat result / victory screen | Gap | OnCombatResultReady bound only by test actor; OnActorTurnStarted AddDynamic→0 | M | Verify BP binding, then C++ overlay |
| 3.1 Action/impact camera | Partial | Bindings commented CombatCameraManager.cpp:78-79; handlers empty :464-470 | M | Design (camera feel) |
| Heal/kill/gamble feedback | Gap | OnHealingDone, OnTargetKilled, OnGambleResult all broadcast, 0 subs | S each | Combat-log UI + VFX |
| A. BD player toggle | Partial | Mechanism built both ways (TriggerTransformation / RevertTransformation → ServerSetBrokenDarkness). OFF has no production caller (only WoR.TestBDRevert console cmd). ON only procedural (RollForBreak). | M | Design (deliberate-toggle entry + cost/gate) |
| 9.2 BD InnateSpells pool empty | Gap (asset) | DA_Inventory_BD.uasset (LFS); BD cast menu shows only Breakthrough | S | Content (≥1 Darkness spell) |

### Progression / equipment / acquisition

| Item | Status | Evidence | Scope | Blocked-on |
| ---- | ------ | -------- | ----- | ---------- |
| B. Crystal runtime attach | Partial | Detach BUILT (RemoveCrystalFromWeapon/Ring). Attach GAP — AttachedItem written build-time only; no runtime AttachCrystal* | M | Nothing (build attach) |
| C. Evolution runtime attach | Gap | Same slot, build-time-only path; no AttachEvolution* | M | Design (evolution acquire flow) |
| D. Evolution primary-slot switch-lock | **Built (implicit)** | FCombatCapabilities.cpp:202-206 — bCanSwitchWeapon requires PrimarySlotType==Weapon; evolution makes it Evolution → locked. "Gap 2" stale. | — | Nothing |
| E. Evolution removal | Gap | No RemoveEvolution (0 hits); only narrow clears (broken-only, invalid-only), neither resets PrimarySlotType | M | Design RESOLVED above — build it |
| #6 Stat-point runtime layer | Gap | No SpendStatPoint/AllocateStat (0 hits); inert StatPool scaffolding (FWeaponInventoryEntry.h:90-99, unread). Stats design-time on CharacterData.h:380 | L | Design |
| #7 World-stat level grant | Gap | No LevelUp/AddXP (0 hits); WorldStat only a gating read (WorldStatRequirements.h:18) | L | Design |

### Shop / economy

| Item | Status | Evidence | Scope | Blocked-on |
| ---- | ------ | -------- | ----- | ---------- |
| #8 AppliedBuffs on weapon instance | Gap | No AppliedBuffs/FAppliedBuff (0 hits). Price hook ready (EffectDefinition.h:37); InstanceID reserved for effect wiring | M | Runtime inventory layer |
| #9 Currency wallet | Gap | No Currency/Wallet/Prism/RollPoint (0 hits) | M | Design |
| #10 OnInventoryChanged delegate | Gap | UInventoryComponent declares no delegates; mutators return bool silently | S | Nothing |

### Hygiene / observability (re-verified STILL-OPEN)

| Item | Status | Evidence | Scope |
| ---- | ------ | -------- | ----- |
| 3.3 OnResurrected no sub | Still-open | AddDynamic→0 | S |
| 5.1 Loadout delegates no sub | Still-open | OnLoadoutChanged/ItemUsed/ValidationFailed (LoadoutComponent.h:43-48) 0 subs | S |
| 5.2 Item delegates no sub | Still-open | OnItemUsed/OnGambleResult 0 subs | S |
| 8.2 Panel hover/click no sub | Still-open | OnPanelHovered/OnPanelClicked 0 subs | S |
| 9.1 Cycle-source Breakthrough-only | Still-open | CombatCommandMenuSubsystem.cpp:1307-1312 | S |
| 10.3 ESpellSource::Item stub | Still-open (unreachable) | ActionExecutor.cpp:2713-2716; 0 producers | S |
| 10.7 AutoPopulateLoadout dumb | Still-open | LoadoutComponent.cpp:1849 TODO; assigns Weapons[0], skips items | M |
| RandomSkill effect stub | Still-open (only true stub of the 5) | SkillEffectManager.cpp:1138 log-only | S |
| Missing *Debug.h/.cpp pairs | Still-open (~18, not ~10) | Runtime managers without a pair: SkillEffectManager, TurnManager, DamageCalculator, ActionExecutor, DefenseSystem, StatusBuildupManager, AIDecisionManager, WeatherStateManager, CrystalManager, WeaponManager, RingManager, InfusionChargeManager, CombatGridSubsystem, CombatCommandMenuSubsystem, ItemExecutor, BrokenDarknessManager, CombatOrchestrator, CombatCameraManager | S each |

---

## Headline takeaways

1. **The pitch's biggest hole is player-facing combat feedback, not gameplay logic.** Combat
   resolves correctly; it's largely invisible — defense prompt (2.1), result screen (3.2),
   heal/kill/gamble/parry feedback, action camera. Most blocked on design or assets, not
   engineering.
2. **BD is demo-fragile:** on/off mechanism fully built and correct, but no player trigger for
   OFF, only procedural for ON, and the Darkness spell pool is empty (9.2).
3. **Acquisition/progression/shop is a coherent unbuilt layer** (B, C, E, #6–10) on top of an
   otherwise-built runtime inventory. Forward plan: `InstanceBasedRuntimeLayer_Design.md`.
4. **Evolution removal is a clean gap** — the "irreversible" conflict was false.

## Changelog
- 2026-06-21 — Production-readiness sweep. Re-verified the 2026-05-29 catalog against current
  main (post effect-arc merge); 2.3 → partial-changed (OnDefenseResolved bound). Added
  acquisition/progression/shop gaps (#6–10), attach/evolution items (A–E), refreshed Debug-pair
  list (~18). Resolved the evolution reversibility false-conflict. Corrected the 2026-06-20
  session-doc stub line. Forward design: InstanceBasedRuntimeLayer_Design.md.
