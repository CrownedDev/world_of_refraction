# Simplicity audit (2026-11-03)

**Status:** proposal only. Read-only audit of `dev` at `7c3a3774`. No code, `.uasset` or ruling was changed.
**Crown's rule:** "production-grade, not over-engineered" (`CLAUDE.md`). Each finding is judged against `docs/Design/DesignPillars.md`.
**Scope:** the whole game. The earlier audits are not repeated here: `SimplifyAudit_2026-10-28.md` covered element and damage-type tables, and `DryAudit_2026-10-28.md` covered copy-paste. Both have mostly shipped (B-267 onward).

**How to read it:**
- Code paths are relative to `Source/world_of_refraction/`.
- Size: S = under half a day, M = about a day, L = several days.
- **Ruling** marks an option that would change a decided `D-xxx`. Those stay options; Crown decides.
- Lore and story (Broken Darkness, Reality, the Blades) are Crown's alone. This audit notes only the engineering cost there.

**Scale:** about 154,000 lines of C++ in 409 `.cpp` files, 41 architecture docs, 276 rules, 125+ backlog jobs. The code has 49 `OBSOLETE` markers and 46 "retired / dead" notes, and 114 `UseGAS` branches.

---

## Top 5 (biggest simplification first)

| # | Finding | Recommendation in one line | Cost |
|---|---|---|---|
| 1 | About a dozen power multipliers stack on every hit and on enemy HP | One power axis per side: pillars feed grades only; enemy HP = authored × rank × difficulty | L (3 jobs), then a retune |
| 2 | Two effect systems run side by side (enum + GAS mirror) | Finish GAS stage 3 to B-042g on a deadline, then delete the enum path; no new feature on both | L (4 open jobs) |
| 3 | Five overlapping item stores, mid-migration, and D-117 changed the target | Each character's inventory is its own store; the pool keeps only the home chest | M (re-scope B-312b–B-317 to 3 jobs) |
| 4 | Two stat layers: 3 pillars plus 14 spendable sub-stats, each through caps, ceilings and three factor groups | Pillars only for the player; sub-stats come from gear and class, not a second point pool | L (Ruling-free; touches D-102 numbers) |
| 5 | Wear runs on crystals, fusions and evolutions, on top of "used up at run end" | One limit: crystals are used up at run end; drop in-run wear and repair | M (**Ruling:** D-093, Q-159) |

Also worth doing, in order: §6 retire-after-PIE pile, §7 currencies, §8 infusion source picker, §9 per-instance rolls, §10 stacked damage reduction (already Q-293d).
Kept on purpose, because they are already simple: §11.

---

## 1. The power multiplier stack

### What exists

**Enemy HP.** A late boss's max HP is the product of five layers, each applied as a `MaxHPBuff` effect:
- **Base.** The enemy's own pillar levels, Max Health points and gear (`CharacterDataComponent::RecomputeMaxPools`).
- **Zone tier.** ×1.0 / 1.15 / 1.3 / 1.5 / 1.75 (`Private/Combat/Encounter/EnemyZone.cpp:13`).
- **Rank.** Normal ×1, Elite ×1.5, Mini-boss ×3, Boss ×6 (`Public/Combat/Tuning/CombatBaselineConstants.h:51-56`).
- **Tier power.** F–S ×1.0 to ×4.8 (`Public/Combat/Damage/TierPowerConstants.h:18`), from `EnemyTier` today and from Combat Tier once B-325 lands (D-118).
- **Trial difficulty.** Now ×1.0 on HP and damage-only (B-293c), so it no longer multiplies HP.

The rank × tier part is then multiplied by the zone strength a second time:
- `Percent = Strength × (rank × tier − 1)` (`Private/Trial/TrialGate.cpp:169`, called with `EnemyHealth` from `BattleGameMode.cpp:277`).
- An A-tier boss in a tier-4 zone comes out at 1 + 0.75 + 1.75 × (6 × 3.7 − 1) ≈ **×38.9**.
- At S tier it is **×50**.

**Player damage.** A weapon ability passes through about 14 factors:
- the skill base × its own tier power × the tier-gap ladder (±6)
- the requirement-gap ladder per pillar (×0.32 to ×1.40, through `ActionMods`; CL-39)
- the pillar-derived sub-stat (capped alone at ×1.5)
- gear points × stone % × buffs (three groups multiplied; `StatMathsRedesign.md` §2.1)
- the per-skill `StatScaling` grades
- plus a weapon part: Power × tier power × pillar-curve grades × the weapon requirement factor (D-101, D-119's ×0.6 cliff)
- then DamageStone % × grid × status buffs × infusion charge × crit × defense × grid defense (`DamageCalculator.md` steps 1–7, plus the assembly layer in `ActionExecutor`)

**The same input arrives several times:**
- **Pillars reach damage three ways:** sub-stats (`RawDamage` from Body), skill grades (`DamageCalculator.cpp:91-100`) and weapon grades (`Public/Equipment/Weapons/WeaponPower.h`).
- **Tier reaches it four ways:** skill tier power, the tier gap, tier-weighted gear points (`LoadoutSystem.md`) and Power scaled by tier power.
- **Requirements reach it twice:** the skill's ±5 ladder, which also gives a bonus above the requirement, and the weapon's cliff (D-119).

### Why it's more than needed
- **It can't be balanced by hand.** The first nightly failed 19 of the bands and the second still fails 17 (`docs/sessions/2026-11-03.md`, B-293c). Three targets need a decided number changed (Q-293d).
- **It works against pillar 6** (never locked out). Tier enters four times, so a tier gap compounds. Under-levelled S gear wins 92% against a band of 35–65% (Q-293d).
- **Players can't read it.** The design pillars prefer readable feedback over hidden numbers (pillars 1 and 3).

### Simpler options
- **A. Expedition 33's shape.**
  - A weapon has base power and a letter grade on up to two attributes. Levelling attributes raises only the weapon's base damage, and skills multiply that ([Maxroll](https://maxroll.gg/clair-obscur-expedition-33/guides/weapon-attributes-upgrades-guide), [Deltia's](https://deltiasgaming.com/?p=216376)).
  - Here that means pillars feed damage **only** through grades (weapon and skill), and the pillar → `RawDamage` / `SpellDamage` sub-stat route is dropped.
  - D-101 already moves this way, and E33's balance is the game's reference (`analysis/Expedition33Balance.md`).
- **B. One tier axis, one gap ladder.** Keep `TIER_POWER` on the item and skill, and drop the separate tier-gap ladder and the tier weighting of gear points.
  - Destiny 2 uses one Power number per side, and one difference between them scales damage both ways ([Destinypedia](https://www.destinypedia.com/Power), already the model for Q-325).
- **C. Authored enemy HP, few multipliers.**
  - Enemy HP is a number on the enemy asset, times rank, times one difficulty setting. Drop zone HP and tier-power HP.
  - Hades does it this way: authored enemies, and difficulty comes from named opt-in conditions (Pact of Punishment / Heat; [Hades wiki](https://hades.fandom.com/wiki/Heat)), not hidden layers.

### Recommendation
Do all three in that order:
- **C first.** It is pure data and ends the ×38 surprises. Zone tier then picks *which* enemies appear, not their HP.
- **Then B.**
- **Then A** for the sub-stat route.
- **Requirements:** keep one requirement rule. D-119's cliff applies to skills too, and the skill ladder's bonus above the requirement goes.

**Rulings:**
- C drops D-118's "tier-power HP for enemies" (Combat Tier stays as a shown letter and the level-drop tier).
- The single requirement rule changes CL-39 (code-only) and D-082's ladder wording.
- A keeps D-101.

**Cost:**
- C: one job, M (`TrialGate`, `BattleGameMode`, `EnemyZone`; B-224b / B-325 HP part retired). Ruling.
- B: one job, M (`TierGapConstants`, the `ActionExecutor` assembly, gear weighting).
- A: one job, L (sub-stat derivation, `StatLayers`, `DamageCalculator` step 1). Do it after #4 is decided.
- Then a B-293c retune.

---

## 2. Two effect systems run side by side

### What exists
- **The enum path.** `USkillEffectManager` stores `FActiveSkillEffect`s with 112 `ESkillEffectType` members and owns their lifetime. It is still the source of truth.
- **The GAS mirror.** `UWoREffectMirrorComponent` copies each stored effect onto the ASC, and `UWoRAttributeSyncComponent` copies every getter onto attributes on HP / EP / effect / loadout / turn events (`GASSystem.md`).
- **The switch.** `wor.Effects.UseGAS 0/1` decides which side is read. It appears in **114** branches across the code.
- **Upkeep.** Parity is guarded by `wor.CompareEffects`, `wor.CompareAttributes`, a fingerprint cache (B-261) and the balance sim's GAS parity (B-293b).
- **Size.** 27 B-042 jobs are in review; B-042f, g, i and j2 plus B-043 and B-318 are still open.

### Why it's more than needed
- **Every effect change is paid twice:** the enum write, the mirror and a compare.
- **It is the root of a class of bugs.** Examples: the stun span stored with 0 turns (Q-042c), the double count if a GE lands beside the mirror (Q-042d), and stale reads between a change and its event (B-261).
- **Nothing ends it.** The parallel run was chosen as a short, safe swap (Q-042c, GitHub's Scientist pattern), but its default is still 0 and no date ends it.

### Simpler options
- **A. Finish and cut.** Land B-042f → B-042g, flip the default to 1 after one PIE pass, then delete the enum storage, the mirror and the sync. The parallel run is meant to be short-lived and removed once the paths agree ([Martin Fowler, Strangler Fig](https://martinfowler.com/bliki/StranglerFigApplication.html)). GAS is also what replicates for co-op (D-103), so Lyra's shape (one ASC as the truth) wins long-term.
- **B. Stop at attributes.** Keep the enum effect path as the truth, drop the effect mirror, and keep only the attribute sets for replication and UI. This needs less code but leaves two ways to express a buff.
- **C. Freeze.** Keep both as they are and add nothing new to GAS. This is the cheapest today and the most expensive later.

### Recommendation
**A, with a rule:** from now on, no feature job touches both paths. Each new effect goes in as a GE only, behind the switch.

**Cost:**
- B-042f (M), B-042g (L), B-042i (M), B-042j2 (S).
- Then one deletion job (M): remove `FActiveSkillEffect` storage, the mirror, the sync and the 114 branches.
- B-318 folds in.

No ruling is affected.

---

## 3. Item stores and copies

### What exists
Five overlapping places hold "what you own", and the plan changed during the move:
- **Per-member copies.** Each member has `Weapons` / `Rings` copies, which 39 readers in 12 files use (B-312b).
- **The shared pool.** `UPoolSubsystem` holds the shared gear and its wearers (`PoolSubsystem.h:184-215`), with the clash rules from B-221 and B-221b.
- **Presets.** These are GUID lists (D-111) beside the old per-loadout item slots (6 × 3, retiring).
- **The pouch.** It was one party pouch (D-097) and is now one per character (D-117, B-313).
- **The planned central store.** D-111's one store per player, where the armoury, storage, pouch and run bag are "views". B-312 to B-317 build it.

D-117 (2026-11-03) made gear per character again, which reverses D-020 and D-111's sharing rule. Its notes list a dozen shared-pool paths to retire.

### Why it's more than needed
- **The central "store with views" design was for gear that every member shares.** With gear per character, most of its job (worn-by, clash rules, a duplicate count across the party) goes away.
- **Three generations exist at once:** copies, pool and store. Each one is "kept until PIE".

### Simpler options
- **A. Baldur's Gate 3's shape:** each character's own inventory plus one camp chest. Crown named it as the model (D-117, Q-321). Here the character's `UInventoryComponent` (GUID entries: gear, pouch, learned skills) is that character's store. `UPoolSubsystem` shrinks to the home chest and the run flags. Give moves an entry between components.
- **B. Keep D-111's central store** with an owner per entry (B-315 as re-scoped). This is one more indirection, but co-op can find everything in one place on the PlayerState.

### Recommendation
**A.**
- B-312b becomes "readers onto the member's component", B-313 stays, and B-314 (run flags) stays.
- B-315 is dropped, because the owner is the component itself.
- B-316 (give) is simpler.
- B-317 deletes the pool wearers and the copies.

**Cost:** 7 jobs become about 4 (M each).

**Ruling:** D-111's "one store per player" wording would change, while its behaviour (GUIDs, references, the run layer) stays.

---

## 4. Two stat layers

### What exists
- **Pillars.** A character has Mind / Body / Spirit levels, 0–7 per pillar and 21 in total (CL-37).
- **Sub-stats.** A separate **sub-stat point pool** is spent on 14 sub-stats: Efficiency, SpellDamage, CritDamage, SpellSpeed, Defense, ActionSpeed, RawDamage, MaxHealth, Reflex, MaxEnergy, Resistance, TurnSpeed, Luck and StatusMultiplier. The pool is `INITIAL_STAT_BUDGET` + 3 per pillar level (`Public/Character/CharacterData.h:338-349`).
- **How each sub-stat is composed:**
  - the per-point value, from `STAT_DERIVE_DENOM`
  - then the stat part capped alone (×1.5 or 0.5)
  - then gear points × stones × buffs, three groups multiplied, up to a gear ceiling (`StatComposition.md`)
  - Pools have their own formula.
- **Gear also grants pillar points** (pillar 6).
- **Three speeds plus Reflex:**
  - `TurnSpeed` (turn order)
  - `ActionSpeed` / `SpellSpeed` (they narrow the defender's timing window, ED-35)
  - `Reflex` (it widens the window, and becomes action pips under D-088)

### Why it's more than needed
- **Two currencies for one build choice.** A level-up picks a pillar, and then points are spent again inside it.
- **Overlap.** Most sub-stats exist only to be multiplied by the same pillar later (§1).
- **Pillar 2 still holds without the second layer.** Build variety comes from skills, gear and pillar choice.
- **D-088 already retires `TurnSpeed`.**

### Simpler options
- **A. Expedition 33.** Five attributes, and each one feeds a fixed set of derived stats with no second allocation ([Steam discussion](https://steamcommunity.com/app/1903340/discussions/0/592896965922002093), [Gfinity](https://www.gfinityesports.com/article/clair-obscur-expedition-33-best-attributes-for-each-character)). Here each pillar level raises its sub-stats by a fixed class profile, and only gear and stones add sub-stat points.
- **B. Keep sub-stat points for authored characters only.** The character asset's spread shapes each character, but the player never spends them. Pillar levels are the player's only stat choice.
- **C. Merge the timing stats.** `ActionSpeed` + `SpellSpeed` become one "Speed", against the defender's Reflex.

### Recommendation
**B + C now.**
- The data barely changes. Assets keep their spreads, and the run's levels move only pillars, which D-102 already does.
- Retire the 3-points-per-level pool.
- A is the later step, if playtests say the authored spreads aren't readable.

**Cost:**
- B: S (the pool stops growing with level; UI text).
- C: M (one sub-stat, the window maths, `StatModifierTable`, a stone kind).

**Ruling:** none for B. C touches D-088's Ice → Action Speed wording.

---

## 5. Wear on top of "used up"

### What exists
**Crystal durability:**
- Max durability by tier is F 30 to S 100 (ER-34), and each cast wears it.
- A broken crystal is destroyed at fight end and pays out like a dismantle (ER-35).
- Each fight end gives +10 repair (ER-36), and a run also has paid repairs in Gold (ER-37).

**Wear elsewhere:**
- Evolutions and fusions wear, break and are repaired, and reset outside the run (Q-159, Q-159c, D-093).
- `DurabilityStone` and an `Unbreakable` flag exist.
- Run wear is keyed by GUID (B-310), which is the bug D-111 found.

**And on top:** D-111's roguelike layer says a crystal used in a run is **used up at run end** anyway.

**Size:** `CrystalWear.md` is 205 lines.

### Why it's more than needed
- **Two limits do one job:** wear and being used up at run end.
- **It sits uneasily with the pillars.** In a 30–45 minute run (pillar 5), per-cast wear plus repair stops adds bookkeeping that the run already ends. It also leans against pillar 3's "second chances". Breath of the Wild's breakable weapons are its most debated system ([Game Informer](https://www.gameinformer.com/opinion/2019/08/01/we-deserve-better-from-games-with-breakable-weapons), [Unwinnable](https://unwinnable.com/2017/04/27/weapon-durability-in-breath-of-the-wild/)).

### Simpler options
- **A. Hades' boons.** What you gain lasts the run and is gone after it, with no in-run wear. The crystal is consumed at run end only.
- **B. Wear on crystals only.** Keep crystal wear (a resource for casting), and drop evolution and fusion wear and their break and repair rules.
- **C. Make wear a visible charge count** (N casts, then empty), with no repair economy, as Tears of the Kingdom's Fuse made materials matter without repair ([Den of Geek](https://www.denofgeek.com/games/zelda-tears-of-the-kingdom-breakable-weapons)).

### Recommendation
**B.** The crystal is the casting fuel and keeps wear. Evolutions and fusions stop wearing, which removes break, repair, reset and Unbreakable for them. Then decide A against keeping crystal wear after a playtest.

**Ruling:** D-093, Q-159 and Q-159c (the evolution wear and repair rules).

**Cost:** one job, M (`CrystalWear`, the evolution and fusion wear paths, the repair price list).

---

## 6. The retire-after-PIE pile

### What exists
"Build new beside old, delete after PIE" (`CLAUDE.md`) is the right rule, but the deletes have piled up:
- **Deletes waiting on a PIE pass:** B-152, B-153, B-154, B-165, B-172, B-309, B-317, B-034b, B-032b and B-106.
- **Code markers:** 49 `OBSOLETE` markers and 46 "retired / dead" notes.
- **Old loadout model:** the old loadout fields, the `*Legacy` validators, `URingManager` / `UWeaponManager` forwarders and `CombatOrchestratorTestActor` all still compile.

### Why it's more than needed
- Every new job reads around the old paths, and every rename (B-299) waits on them.
- Specs often already cover the new path, so the PIE gate is usually a formality.

### Simpler options
- **A. One "PIE sweep" checklist.** List every pending delete's check on one page, run it in one sitting, then delete in one batch per system. This is the cleanup sprint teams use with the Strangler Fig pattern.
- **B. Time-box.** A delete whose spec is green and has had no PIE complaint for two weeks goes ahead.

### Recommendation
**A now, then B as the standing rule.** B changes `CLAUDE.md`'s rule, so it is Crown's call.

**Cost:** S to write `docs/PIE/Sweep.md`; the deletes are the existing jobs.

---

## 7. Nineteen currency balances

### What exists
- **Five scalar currencies:** Gold, Prisms, Diamond, Gear Essence and Skill Essence.
- **14 typed essences:** ten elements, three pillars and Ability (`Public/Currency/CurrencyTypes.h:22-43`, `CurrencySystem.md`).
- **Run progress besides these:** world-stat points and level drops.

### Why it's more than needed
- Ten element essences mean ten separate prices and drops to balance.
- Only crystals use them, and players must track which colour they hold.

### Simpler options
- **A. Expedition 33.** One currency (Chroma) plus upgrade materials by grade.
- **B. One essence per family:** Elemental, Pillar and Ability. Three keys instead of 14, and the element still decides *what* you craft, not *what you pay*.

### Recommendation
**B.** Pre-release saves may be wiped (D-060), so the migration is cheap.

**Ruling:** D-105 (Quartz essence name and use) and the essence rules on 05-economy.

**Cost:** M (enum append and retire, `EconomyYield`, shop prices).

---

## 8. Infusion: choosing a source as well as a crystal

### What exists
**Infusion charge:**
- Infusion has charge levels L0–L2, with damage ×1.15 / ×1.30 and its own status bonus.
- Its EP cost has six factors plus an HP backlash (`StatMathsRedesign.md` §2.6).

**Choosing the element:**
- A **source picker**, `EInfusionSourceOption`, chooses which gear gives the element. It has five options, each with its own resolve path (`SimplifyAudit_2026-10-28.md` S4).
- Meanwhile D-108 says the crystal on the weapon or adapter already gives that item's abilities its element.

**Extras:** Reality boost and evolution infusion add to `ActionMods`.

### Why it's more than needed
- **Two ways to choose a hit's element:** the equipped crystal and the infusion source.
- The cost formula is hard to show in a HUD.

### Simpler options
- **A. Octopath Traveler's Boost.** One meter, and spending 1–3 points makes the action stronger. Breaking and boosting are each one readable rule ([Nintendo Life](https://www.nintendolife.com/guides/octopath-traveler-beginners-faq-best-characters-breaks-and-boosts-and-path-actions-explained)). Here infusion becomes "spend EP for L1 / L2", and the element always comes from the item in use.
- **B. Keep the picker, cut the cost formula** to base × level.

### Recommendation
**A.**

**Ruling:** D-007 (damage type = the infusion source's element) and D-035.

**Cost:** M (`ActionExecutor` source resolve, `ActionExecutorCosts`, the HUD picker).

---

## 9. Per-instance random rolls

### What exists
- Gear rolls stat and resistance bonuses per copy at pickup, with pools, max pools and rerolls (`PerInstanceRollSystem.md`).
- This sits on top of instance tier and rolled quality (`TierOnInstance.md`).
- The toggle `bRandomGenerateOnPickup` **defaults off**.

### Why it's more than needed
- Three ways to make two copies differ: tier, quality and rolls.
- D-115 now converts duplicates anyway.

### Simpler options
- **A.** Drop the rolls and keep tier + quality. Hades and Expedition 33 gear has no random stats.
- **B.** Drop quality and keep the rolls (Diablo).

### Recommendation
**A.** It is already off, so nothing changes in play.

**Cost:** S (flag the roll path OBSOLETE; delete in the §6 sweep). No ruling.

---

## 10. Stacked damage reduction (already Q-293d)

- **What exists:** Defense (up to 50%) × Damage Taken Down (up to 90%) × Guard 50% lets 1.25% through. Resistance, the absorb family (D-092) and grid defense come on top.
- **Options and recommendation:** Q-293d has them: one overall cap, as Expedition 33 capped stacking after the Stendhal patch.
- **Recommendation:** answer Q-293d A. It is the cheapest fix in this audit (S, `DamageCalculator` step 5).

---

## 11. Kept: already simple

- **One status bar per combatant** (SB-01). The last hit decides the break, the same idea as Octopath's single shield count.
- **The element and break tables** (B-267 to B-274): one row per damage type.
- **One budget-plus-limit loadout model per class** (D-110).
- **Weather** (`WeatherSystem.md`) only changes the sky by the leaders' HP (pillar 1). It is low cost, so leave it.
- **Broken Darkness** (630-line doc): this is lore. The engineering note is that its strain maths reads its own stat-only Defense (`StatMathsRedesign.md` §2.8), so it won't follow #1 or #4 automatically. Whoever builds those must check it.

---

# Round 2 (2026-11-03): flow, systems, tooling and docs

Same rules as round 1. Read-only, on `dev` at `7c3a3774`. **Ruling** marks an option that changes a decided `D-xxx`.

## Top 5 of round 2

| # | Finding | Recommendation in one line | Cost |
|---|---|---|---|
| 12 | 253 of 339 backlog jobs sit in "REVIEW (in dev)", each with its own PIE checklist | Specs gate each job; Crown plays one checklist per milestone | S (process; `CLAUDE.md` + `work-backlog` skill) |
| 13 | Per-fight state lives on 10+ GameInstance subsystems; replication tags were added before co-op runs | Move the fight subsystems to world subsystems in one step; co-op as one vertical slice | M, then B-228–B-232 shrink |
| 14 | Five condition vocabularies for "when X happens, if Y" | One condition struct and one evaluator; the systems stay separate (D-100) | M |
| 15 | Five placeholder inventory screens, all to be replaced by one | One placeholder inventory screen with verbs | S–M |
| 16 | Two rule sources (`Rules/` and `Mechanics/`), stale dated overviews, a 3,100-line decision log | `Rules/` is the only rule source; archive the rest | S (docs only) |

---

## 12. One PIE checklist per job: 253 waiting

### What exists
- **The backlog:** 339 jobs in `docs/Backlog.md`. **253 are "REVIEW (in dev)"** and 7 are DONE. A job is only DONE after Crown's PIE pass.
- **253 checklist files in `docs/PIE/`** (5,867 lines), one per job.
- **Deletes waiting on PIE:** about 10 jobs wait only on a PIE pass before they delete old code (round 1 §6).

### Why it's too much
- **The review queue grows faster than one person can play it.** The specs already prove most of these jobs headless: 1,109 tests, with D-066 making specs the test layer until the new HUD.
- **Paths stay alive.** Every unplayed job keeps its old path alive (§6), and every later job reads around it.

### Simpler options
- **A. Milestone playtests.**
  - A job is DONE when its spec and the reviewer pass.
  - Crown plays one checklist per milestone covering what changed for the player (e.g. "M3: combat loop").
  - This is how Supergiant ran Hades' early access: one playable build per major update, with feedback on the whole build, not per change.
- **B. PIE only for jobs that change what the player sees or does.** Refactors, specs and docs close on green tests.

### Recommendation
**B now, and A from the next milestone.**
- Mark the refactor-, spec- and docs-only REVIEW rows DONE in one sweep (they are not counted yet; the sweep counts them).
- Merge their PIE files into `docs/PIE/Sweep.md` (round 1 §6).

**Ruling:** none (process). It changes `CLAUDE.md`'s Backlog-mode hand-off rule 6, which is Crown's call. **Cost:** S.

---

## 13. Per-fight state on GameInstance subsystems; replication ahead of co-op

### What exists
- **23 `UGameInstanceSubsystem`s.** At least 10 hold per-fight state, against D-064:
  - `TurnManager`, `DefenseSystem`, `CombatGridSubsystem`, `WeatherStateManager`
  - `SkillEffectManager`, `StatusBuildupManager`, `InfusionChargeManager`
  - `AIDecisionManager`, `ActionExecutor`, `CombatCommandMenuSubsystem`
- **Leftovers among them:** `RingManager` and `WeaponManager` are OBSOLETE forwarders (B-154). `CombatCommandMenuSubsystem` is UI logic (2,033 lines) living as a game service.
- **Replication ahead of co-op.** The code has 71 `Replicated` properties and 27 authority checks, but **one** Server RPC. The co-op moves (B-228 to B-235) are all TODO.
- **Host hand-over (D-103).** A save copy goes to every friend at each checkpoint, and a friend takes over if the host drops.

### Why it's too much
- Subsystems outlive the fight, so every one needs reset code (`ForceReset`, the orchestrator resets, `CancelPendingDecision`), and a missed reset is a carry-over bug.
- Replication tags on state that can't replicate (a GameInstance subsystem has no network presence, `CoopStateOwnership.md`) cost review time and prove nothing.
- Host hand-over is the hardest part of co-op, and few well-received co-op games do it.

### Simpler options
- **A. World subsystems for fight state.** A `UWorldSubsystem` lives and dies with the battle map, so the resets go away. That is the engine's intended home for level-scoped services (Lyra puts match state on world and GameState objects). Replicated pieces then move to GameState and PlayerState components (B-228, B-230) from there.
- **B. Co-op as one vertical slice.** Build one fight end to end with two players (input, turn, effects, damage) before tagging any more state, as Elden Ring's and Monster Hunter's session co-op does: host-authoritative, and the session ends when the host leaves.
- **C. No host migration.** When the host leaves, friends return to their own hub and keep their personal loot. This is Elden Ring's rule: guests are sent back to their own world.

### Recommendation
**A + B.** C is a ruling; offer it to Crown.

**Ruling:** C changes D-103's "passing on host".

**Cost:**
- A: M (subsystem base class swap + lifetime fixes, one system per cluster).
- B: re-scopes B-228 to B-232 into one slice job, L.
- C: removes B-234.

---

## 14. Five condition vocabularies

### What exists
"When something happens, if a condition holds, do something" is written five times:
- **Memories:** `EMemoryConditionKind` / `FMemoryConditionSet` (`Public/Memory/MemoryData.h:54-144`; 397-line evaluator, `MemoryConditions.cpp`).
- **Boss phases:** `EBossPhaseConditionKind`.
- **Boss triggers:** `EBossTriggerConditionKind` (`BossData.h:36, 109`; a boss has two vocabularies of its own).
- **Gear and skill triggers:** `ESkillTrigger` plus conditional effects in the effect manager.
- **Reactions:** `FReactionTrigger` and `FightTriggers` (worn items facing a character, B-191, B-212).

### Why it's too much
- Each new condition ("HP below 30%", "Fire hit taken") has to be added up to five times.
- Hidden conditions (pillar 10) are supposed to work everywhere.

### Simpler options
- **A. The deckbuilder's model.** One trigger → condition → effect tree (`wor-deckbuilder/docs/reference/05-triggers-conditions.md`, already the model for D-031's text). Here that means one `FWoRCondition` + `FWoRConditionSet` with one evaluator. Memories, boss phases, boss triggers, gear triggers and reactions each hold a set.
- **B. Gameplay tags as conditions.** GAS tag requirements (`FGameplayTagRequirements`) cover "has / lacks state". Use these for the state checks, and keep a small custom set for counts and HP thresholds.

### Recommendation
**A,** built beside the old ones, one system at a time (memories first: the biggest evaluator). D-100 keeps memories and boss triggers as separate *systems*, and they can still share the condition vocabulary.

**Ruling:** none. **Cost:** M for the struct and evaluator, then S per system (5).

---

## 15. Placeholder screens that overlap

### What exists
**Nine C++ placeholder screens:** `wor.UI.*` Dismantle, Duplicates, Forms, Give, ItemStorage, LevelDrop, PartyControl, Storage, TestScreen. A memory journal exists too.

**Five of them are inventory moves, each headed "replaced by the CommonUI inventory screen (D-024)":**
- **Storage** (B-324): the shared chest, put and take gear and stacks.
- **ItemStorage** (B-203): carried items to storage. This is the same move, done before D-117.
- **Give** (B-323): between characters.
- **Dismantle** (B-084).
- **Duplicates** (B-319).

They total about 1,500 lines in `Private/UI/Hub/`.

### Why it's too much
- Five screens list the same items with different buttons.
- Storage and ItemStorage now both move pouch stacks to the chest.
- Each one is extra work to delete later.

### Simpler options
- **A. One inventory screen with verbs.** Pick an item, then pick Give / Store / Take / Dismantle / Convert. This is Baldur's Gate 3's inventory (the model in D-117): the item's context menu holds every action.
- **B. Keep the screens, but delete ItemStorage now.** Storage covers it since B-322.

### Recommendation
**B now (S), then A** as the next placeholder job, so the CommonUI screen (D-024) has one placeholder to replace.

**Ruling:** none. **Cost:** S + M.

---

## 16. Docs: two rule sources and stale pages

### What exists
- **459 markdown docs in all.**
- **Rules:** `docs/Rules/` (7 pages, 276 rules, the stated rule source) and `docs/Mechanics/` (56 files, 3,283 lines) cover the same rules. For example, `Mechanics/Scaling/` vs `Rules/01` CL-37 to CL-40, and `Mechanics/Status/` vs `Rules/03`. B-117 already found stale lines there.
- **Dated May overviews** in the docs root: `Conceptual_Overview_2026-05-11.md`, `Conceptual_Overview_2026-05-14.md`, `HighLevel_Overview_2026-05-14.md`, `Architecture_2026-05-14.md`, `PastDocumentation_Audit.md`, `TODO.md`. These predate most rulings.
- **`DesignDecisions.md`:** 3,122 lines, 285 entries, answered and open together.
- **`Backlog.md`:** 339 rows, many with long "REVIEW" notes.

### Why it's too much
- Two places for one rule means one of them is wrong.
- Agents and Crown read stale overviews first because they sit at the top level.
- Finding the 3 open questions among 285 entries is slow.

### Simpler options
- **A. One source per kind.**
  - `Rules/` holds the rules. `Mechanics/` pages either fold into it or move to `docs/archive/`.
  - The dated overviews move to `docs/archive/`.
  - The deckbuilder does it this way: one `docs/reference/` (the shape Crown asked for in B-046).
- **B. Split the decision log.** Open questions go in `DesignDecisions.md`, and answered entries go in `DesignDecisions_Archive.md`.
- **C. Trim the backlog.** DONE and REVIEW rows older than a milestone move to `Backlog_Done.md`.

### Recommendation
**A + B + C.** Moves only, no rewrites, so the links stay checkable.

**Ruling:** none. **Cost:** S each (docs only, three commits).

---

## 17. Turn order rules waiting on D-088

### What exists
**Today's turn order:**
- A speed-ratio scheduler (ET-04 to ET-06) with a 2:1 cap (D-049) and an 8-step tie-break (ET-08).
- Pinned Emerald bonus turns that pass the cap (ET-12 to ET-14), the Extra Action effect (ET-15) and ritual Execution turns (ET-16).
- A one-place push from Wind (ET-11).
- Turn speed from Spirit with its own stat, stone and buff family (ET-04, ET-05).

**The pending rework:** D-088 ("discussed, not to build yet") replaces all of it with round-robin plus action pips from Reflex. B-170 is blocked on that discussion.

### Why it's too much
Every job that touches turns (B-297's Emerald rules, Q-297b) adds rules to a scheduler that D-088 may retire.

### Simpler options
- **A. Keep a speed timeline and close D-088.** Final Fantasy X's CTB, a visible turn order driven by speed, is widely praised as one of the series' clearest systems, and Expedition 33 uses a speed-ordered timeline too. Cut the extras: one bonus-turn rule, and drop the tie-break to three steps.
- **B. Build D-088 now** and stop adding scheduler rules until it lands.

### Recommendation
**Decide D-088 first.** A is the simpler game; B is Crown's stated direction.

**Ruling:** D-088 (and D-049, D-052, Q-297). **Cost:** A is S (trim); B is L (B-170).

---

## 18. Grid: 3×3 cells for a ±5% row effect

### What exists
- Each side has a 3×3 grid (9 cells) for at most 3 characters (PA-29).
- Rows give ±5% damage and defence (PA-30).
- There are formation cells and centre-out placement (PA-31), and a free once-per-turn row step into empty cells (PA-32).
- `CombatGridSubsystem.cpp` is 878 lines.

### Why it's too much
- Columns have no rule effect.
- ±5% is too small to read (pillar 3 prefers readable feedback).
- A 9-cell grid for 3 bodies needs placement and fallback rules.

### Simpler options
- **A. Two rows, a clear effect.** Classic Final Fantasy front and back row: a back-row character takes and deals half physical melee damage, one readable rule.
- **B. Keep three rows, drop columns.** Make the row effect larger (e.g. ±20%).

### Recommendation
**B.** It keeps the D-096 row step and removes the column bookkeeping. Weigh it against A in playtest.

**Ruling:** D-042 (formation cells), D-096 numbers. **Cost:** M.

---

## 19. Input leftovers

### What exists
- **Combat input.** D-084 rules **two** defence buttons, Guard and Dodge. The combat controller binds four: Block, Parry, DodgeLeft and DodgeRight (`CombatPlayerController.cpp:108-121`). Parry only logs "a Guard, D-084" and guards.
- **Infusion menu keys.** These include a source-cycle key (R / LB), which goes away if round 1 §8 lands.
- **Hub controllers.** Two exist (`ExplorationPlayerController`, `WoRHubController`), and folding them is B-158, blocked on the editor.

### Why it's too much
- Four inputs for two actions.
- Each input needs an `InputMap.md` row and a gamepad binding (pillar 9).

### Simpler options
- **A. Two actions, as ruled.** Guard and Dodge, which is Expedition 33's scheme (D-084 cites it). Dodge direction is cosmetic or comes from the stick.
- **B. Keep the actions, but map both dodge keys and Parry to the two real handlers.** This is what the code does today, with less code.

### Recommendation
**A.** Remove `IA_Parry` and the left/right split in the controller; the `.uasset` input actions are removed in the editor.

**Ruling:** none (it applies D-084). **Cost:** S, plus an editor step.

---

## 20. AI: two difficulty axes and one 2,455-line class

### What exists
**The AI class.** `UAIDecisionManager` (2,455 lines, one file) holds:
- think time, defence rolls and timing accuracy per difficulty
- target scoring, and survival / cleanse / offence branches
- infusion choice
- five `Find*Item` and two `Find*Spell` helpers
- spell and ability twins of `EstimateStatusScore`, `CanAfford*` and `Decide*Infusion*`

**Two difficulty settings:**
- AI difficulty Easy / Medium / Hard / Expert, per encounter (PA-33 to PA-36).
- Trial difficulty Easy / Medium / Hard, per door, now scaling enemy damage (B-293c).

**Allies** always play at Hard.

### Why it's too much
- The player sees one "difficulty", but two axes multiply behind it. The balance sim found Hard about 2.5× longer for this reason (B-293c).
- The spell and ability twins repeat the same maths.

### Simpler options
- **A. One difficulty.** Expedition 33's Story / Expeditioner / Expert setting changes enemy damage and the defence timing window in one place. Here, the trial door's difficulty also picks the AI tier.
- **B. Utility AI as data.** The scoring weights per difficulty go in a table (`UCombatTuningData`), and spell and ability share one `Skill` path, as most utility AIs do (one score function per action).

### Recommendation
**A + B.**

**Ruling:** D-044 (AI difficulty per encounter) and Q-183. **Cost:** A is S; B is M (twins merged, weights to data).

---

## 21. Party control modes

### What exists
- Each member has a Player/AI setting (PA-11).
- Takeover rules when your fighters fall (PA-12), a "Let AI finish" setting, and a mid-fight switch on the member's turn (PA-13).
- Ally AI is fixed at Hard (PA-33).
- Recruiting works in the hub and in trials, run-only (PA-15, PA-16), and recruited members are optional (D-026).

### Why it's too much
- Four control rules to test solo and co-op.
- Most solo turn-based games let you command the whole party.

### Simpler options
- **A. Solo controls everyone.** In Expedition 33 and Octopath, the player commands every member. AI control exists only for an empty co-op seat or an opt-in "auto" toggle.
- **B. Gambits.** Final Fantasy XII's per-character rule lists are a well-liked way to make ally AI the player's own build choice (pillar 2). This replaces the fixed Hard AI.

### Recommendation
**A** for solo; keep the AI for co-op seats.

**Ruling:** pillar 7's "your party of three, ally AI" and D-026 wording; Crown decides. **Cost:** S (remove the default Player/AI per member; keep the switch command for debug).

---

## 22. Buying your own gear at run start

### What exists
- D-021 / ER-16: a run starts by **buying** your loadout from your own pool with a Gold budget. Prices rise with tier and level.
- B-116 is blocked on B-073.
- D-110's slot budget already limits what you bring.

### Why it's too much
- Two limits on the same choice: a points budget and a Gold price.
- A shop screen for items you already own.

### Simpler options
- **A. Bring what fits the budget, free.** Hades lets you take any one keepsake and your chosen weapon. The limit is the slot, not a price.
- **B. Keep the Gold budget, drop the slot budget for runs.**

### Recommendation
**A.** The D-110 budget is the limit, and Gold stays run money for in-run vendors.

**Ruling:** D-021. **Cost:** removes B-116 (M saved).

---

## 23. Saves: nine versions, all still read

### What exists
- `WoRSaveVersion::LATEST` is 9 and `MIN_SUPPORTED` is 1 (`Public/Save/WoRSaveGame.h:14-17`).
- The loader still reads v3 and v4 gear lists and the old party pouch.
- **14 `SaveGame` property tags** remain (e.g. the currency component), beside the explicit save structs.

### Why it's too much
- D-060 allows wiping any pre-release save below the minimum, yet every old shape is still read.
- D-059 asks for explicit structs, not `SaveGame` tags.

### Simpler options
- **A. Raise `MIN_SUPPORTED` to 9** and delete the old-shape reads (D-060 as written). Then move the 14 tagged fields into `FWoRSaveData` (D-059).

### Recommendation
**A.**

**Ruling:** none (it applies D-059 and D-060). **Cost:** S + S.

---

## 24. Debug commands: 112 names, several per job

### What exists
**112 console commands.** Several overlap:
- **Effects:** `wor.PrintEffects` / `wor.PrintAllEffects` / `wor.DescribeEffects` / `wor.DumpTurnEffects`.
- **Inventory:**
  - `wor.PrintPool` / `wor.Pool.PrintStore`
  - `wor.PrintInventory` / `wor.Inventory.PrintSlots` / `wor.Inventory.CompareStore`
- **World stats:** `wor.PrintWorldStats` / `wor.WorldStat.Print`.
- **Tuning:** `wor.PrintCombatTuning` / `wor.Tuning.Print`.

The naming is mixed: `WoR.` vs `wor.`, flat (`wor.BreakCrystal`) vs grouped (`wor.Economy.Merge`).

### Why it's too much
Nobody can remember which of four effect printers is current.

### Simpler options
- **A. One scheme, `wor.<System>.<Verb>`,** one `Print` per system. Unreal's own `showdebug <system>` works this way, one entry per system.

### Recommendation
**A.** Keep the old names as aliases for one milestone, then delete them in the §6 sweep.

**Ruling:** none. **Cost:** S.

---

## 25. Tests: parity specs and legacy test actors

### What exists
- **189 test files**, about 30,000 lines (`Private/Tests/`), and 1,109 tests.
- **GAS parity:** 22 GAS spec files, mostly proving the GAS path equals the enum path (round 1 §2).
- **Legacy test actors:** `CombatOrchestratorTestActor`, `TurnManagerTestActor`, `SkillEffectManagerTestActor`, `HUDTestActor` and `SkillProjectileTestActor`. They are retired only after a matching spec and PIE (D-067).
- **Balance sim:** the quick balance sim runs in every suite.
- **No timing:** no per-spec time is recorded, so slow specs can't be found today.

### Why it's too much
- Parity specs are temporary by nature: once the enum path is gone (round 1 §2) they test nothing.
- The test actors duplicate specs.

### Simpler options
- **A.** Delete the parity specs with the enum path.
- **B.** Add timing: `test.sh` prints the ten slowest tests from the automation report, and the balance sim moves to the nightly preset only.

### Recommendation
**A** (with round 1 §2) and **B** now (S). Retire the test actors in the §12 sweep.

**Ruling:** D-067's PIE gate for test actors; covered by §12. **Cost:** S.

---

## Round 2: kept, already simple

- **Economy core.** One upgrade rule (tier step for essence), one merge rule and one dismantle table (ER-25 to ER-31) are simple enough. The cuts are in round 1 §7 (essences) and §22.
- **Memories and story unlocks.** These are lore (Crown's). The only engineering note is §14's shared conditions.
- **Encounters** (ER-45 onward): one team per encounter, up to three enemies.

---

# Round 3 (2026-11-03): content systems

**Same rules as before.** Every finding keeps the intention and gives a fix. Where the deckbuilder has a rule, it is cited first (`wor-deckbuilder/docs/reference/`).

**Already ruled (D-120 to D-140), not raised again.** Evolution and fusion wear stays (Crown).

## Ranked

| # | Finding | Fix in one line | Cost |
|---|---|---|---|
| 26 | One socket holds four families (gems, stones, fusions, evolutions); fusion exists to fit two things in it | Two sockets per weapon / adapter (element + stone); fusion stays only as a paired item | M–L (**Ruling**) |
| 27 | 17 stone kinds, 14 of them a stat each | One Stat Stone with a stat field (the deckbuilder's stat list) | M |
| 28 | Seven separate limits on what a character carries | The D-110 budget, the pouch size and one S-tier cap | S (**Ruling** D-072) |
| 29 | A boss has two condition lists that both change the boss | One rule list per boss; "next phase" is one of its results | S–M |
| 30 | Class identity is split over a budget, a resistance table and an unbuilt passive | Class = budget + one passive (the deckbuilder's) | M (**Ruling**: D-027's open part) |
| 31 | Spell school is authored on every spell but only the AI reads it | The AI reads the spell's effects; the school retires | S |
| 32 | An evolution can sit in two places at two costs | One placement | M (**Ruling** D-110) |
| 33 | Three physical damage types each carry a resistance column | One Physical resistance; the subtype only picks the break | M (**Ruling** D-037) |
| 34 | Weather has three intended drivers and no rule effect | One driver: the trial's element, stepped by boss phases | S |
| 35 | Skins, six shared outfit slots and a tier-credited meter | Unlock by one condition; outfits as whole sets | S (**Ruling** Q-279) |
| 36 | Six ways to make an item stronger | Upgrade and attach; merge stays for crystals | S (follows #26) |

---

## 26. One socket, four families

### What exists
- **One attachment slot.** Each weapon and adapter has one, `FAttachedItem` / `FRuntimeAttachedItem`, discriminated by `EAttachedItemKind`: Crystal, Evolution, AugmentStone, Fusion (`AugmentStoneSystem.md` "EAttachedItemKind").
- **What can go in it:**
  - 10 gem crystals, each an element plus its own use: Garnet damage, Sapphire healing, Citrine energy, Emerald bonus turn, Amber defence, Opal crit, Onyx silence, Amethyst gambling, Iolite cleanse, Quartz status clear (`ECrystalType`).
  - 17 stones (#27).
  - Evolutions.
- **A fusion.** It is a player-made item that joins a stat stone with a gem or an Ability Stone, with a tier-formula bonus % to a chosen sub-stat (`AugmentStoneSystem.md` "FusionStone").
- **Why fusion exists.** It is the only way to have an element *and* a stat on one weapon.
- **Its rules:** the fusion id (`FFusionId`, symmetric halves), its restrictions, its wear, its 1.5× price and its own counting rule (D-077).

### Why it's too much
- Fusion is a second socket in disguise, with its own crafting step, identity type, price and restrictions.
- **The deckbuilder** has two attachable kinds only, `Crystal` and `Evolution` (`02-enums.md` "ECardKind"). It has no stones and no fusions.

### Fix (keeps: element plus stat on one weapon)
- **A. Two sockets: an element socket and a stone socket.** A gem or an evolution goes in the first, a stone in the second.
  - Monster Hunter's decoration slots and Diablo II's sockets let you combine effects without crafting a combined item; both are long-running and well liked.
  - Fusion items, their bonus formula and `FFusionId` retire (enum append-only: kept, retired).
- **B. Keep fusion, drop the bonus formula.** A fusion is just the two halves. This removes the per-fusion sub-stat choice and the 7×7 bonus rule.

### Recommendation
**A.** The second socket gives the same builds with one rule fewer.

**Ruling:** Crown kept fusion wear (round 3 brief), and D-009 and D-077 mention fusions. If Crown wants fusions to stay, take **B**.

**Cost:** A is M–L (slot struct, loadout costs, validation, save; B-299's rename rides along). B is S.

---

## 27. Seventeen stone kinds

### What exists
**The stone enum** (`ECrystalType` after the gems) holds:
- `DamageStone` and `AbilityStone`.
- 14 single-stat stones: Defense, Crit, TurnSpeed, Status, Efficiency, MaxHP, MaxEP, SpellDamage, Resistance, SpellSpeed, ActionSpeed, Durability, Luck and Reflex.
- `HealingStone`.

**Shared machinery:**
- They share a row table (`StoneRowTable.h`, B-021) and a "dual-form" model: each attaches and is also a consumable.
- D-123 merges Action Speed and Spell Speed into one Speed, so two stone kinds now mean the same thing.

### Why it's too much
- Each new sub-stat needs a new enum member, a row, a price, an essence and a name.
- The stones duplicate gear's own sub-stat bonuses (D-123: sub-stats come from the asset and gear).

### Fix (keeps: stones that raise one stat)
- **A. One `StatStone` kind with a sub-stat field.** This is the deckbuilder's rule: stats are an ordered list of (stat, value) on the card, not fixed fields (`01-card-model.md` "`Stats` — an ordered list, not fixed fields").
  - Diablo III's gems work this way: few gem types, and what a gem raises depends on where it sits.
  - Damage, Ability and Healing stones stay as they are, because each does something other than one stat.
- **B.** Keep the kinds, but retire the two speed stones into one (D-123), plus Durability, which only feeds wear.

### Recommendation
**A,** built beside the old kinds: the old members map to a `StatStone` + sub-stat when loaded.

**Ruling:** none (D-123 already merges Speed). **Cost:** M.

---

## 28. Seven limits on what a character carries

### What exists
- **D-110 point budget per class:** Refractor 3, Generic 6, Resonator 5.
- **D-110 item limit per class:** 1 / 2 weapons / 1 weapon.
- **Skill slots by tier per weapon or crystal:** F 1 to S 6 (`ATTACHMENT_SLOTS`, `AugmentStoneConstants.h:35`; D-039).
- **D-072 crystal tier caps for carried crystals:** S 1, A/B 2, C/D 3, E/F 4.
- **The pouch:** 10 slots per character (Q-322).
- **D-115:** one copy per character.
- **D-116:** always one primary.
- **Presets:** up to 5 per character (D-111).

### Why it's too much
- Three of these (budget, tier caps, pouch size) limit the same thing: how much power a character brings.
- The deckbuilder leaves crystals **uncapped** (D-072's notes cite `WoR_TCG_Pivot_Design.md` §8b rule 4).

### Fix (keeps: no character carries everything)
- **A.** The D-110 budget limits gear and the pouch size limits items. D-072's caps keep only "1 S-tier crystal". Expedition 33 limits Pictos by slots and Lumina by one point budget, two limits that players read at a glance.
- **B.** Drop D-072 entirely, as the deckbuilder does.

### Recommendation
**A.**

**Ruling:** D-072. **Cost:** S (loadout validation, constants, Rules 01).

---

## 29. A boss's two condition lists

### What exists
- **Boss phases:** `EBossPhaseConditionKind` / `FBossPhaseCondition` (D-098). Phases are triggered by HP and also by other conditions (absorption max, a crystal breaking, being infused).
- **Boss triggers:** `EBossTriggerConditionKind` / `FBossTriggerCondition` (D-100). Their results are weaken, strengthen, **skip a phase**, **add a phase** and a dialogue line (`Public/Combat/Boss/BossData.h:36-141`).

D-130 already gives them one condition vocabulary.

### Why it's too much
- Both lists answer "if this, change the boss", and each can move the boss's phase.
- So a phase change has two authoring places and two orders to resolve.

### Fix (keeps: hidden conditions, one-way phases, D-100's separation from memories)
- **A. One boss rule list.** Each rule is a condition set (D-130) and a result. "Enter the next phase" is one result, beside weaken, strengthen and dialogue. HP thresholds are just conditions.
  - This is the deckbuilder's trigger → condition → effect row (`05-triggers-conditions.md`).
  - Elden Ring's bosses switch phase on HP; the hidden extras (pillar 10) are more rows in the same list.

### Recommendation
**A.**

**Ruling:** none (D-098 already says phase triggers aren't only HP). **Cost:** S–M (`BossData`, the phase runner, `wor.Boss.Print`), after B-337.

---

## 30. Class identity is split three ways

### What exists
**A class is built from three things:**
- the D-110 budget and limit
- a row of `ClassInnateResistanceTable` (class × element resistance, `Public/Combat/Resistance/ClassInnateResistanceTable.h:47`)
- for the Refractor, the innate element and spell pool (CL-04)

**Two more pieces sit beside it:**
- Broken Darkness picks a separate resistance row.
- Class passives are a listed gap (Rules 01 Gaps; D-027 "part unruled").

### Why it's too much
- Resistance by class is a hidden number table the player can't see, while the readable passive doesn't exist.
- Pillars (stats) and class (shape) don't overlap; that part is fine.

### Fix (keeps: three distinct classes)
- **A. Class = budget + one passive.** Use the deckbuilder's passives: Generic +1 Physical Resistance, Resonator +1 Resistance per keyed adapter element, Refractor +3 Amp and +1 Resistance to its innate (`01-card-model.md` "Class passives"). The class rows of the resistance table become those passives. Character-specific resistances stay on the character asset.
  - Octopath's job identities are similar: each job is its kit plus one or two printed support skills.

### Recommendation
**A.**

**Ruling:** D-027's unruled part (class passives); D-005's table "could be improved" (Crown). **Cost:** M.

---

## 31. Spell school, read only by the AI

### What exists
- Every spell authors `ESpellSchool`: Destruction, Enhancement, Restoration or Conjuration (`Public/Skills/Definitions/SpellSchool.h`).
- Outside debug, it is read only by the AI, which uses it to find damage and heal spells (`AIDecisionManager.cpp:1580, 1765, 2040`).
- **A leftover:** `ESpellSource::WeaponCrystal` remains after D-108 / B-098 (weapons stop giving spells).

### Why it's too much
- An authored field that can disagree with the spell's actual effects: a "Restoration" spell that deals damage.
- The deckbuilder: "schools don't license" (`02-enums.md` "ECardKind", Spell). What a skill does is its effect verbs.

### Fix (keeps: the AI knows a heal from an attack)
- **A.** The AI classifies a spell by its effects: has a damage row, has a heal row, has a buff. `ESpellSchool` is retired from authoring (append-only, kept in the enum).

### Recommendation
**A,** inside B-345 (one AI path).

**Ruling:** none. **Cost:** S.

---

## 32. Evolutions in two places

### What exists
An evolution can sit on a weapon or adapter (an "evolved weapon") or alone in the primary slot (D-110 costs: weapon + evolution 3, evolution alone 2, adapter + evolution 2). It holds spells by tier (D-039).

Code and rules split on where it is:
- `GetActiveLoadout().PrimaryEvolution` vs the attachment.
- The infusion source picker's Evolution case (round 1 §8).
- Evolutions aren't on loadout slots for the form meter yet (`CharacterForms.md`).

### Why it's too much
- Two placements mean two cost rows, two resolve paths, and an "is it attached or standalone?" check in every reader.

### Fix (keeps: evolutions as the big transformation)
- **A. Evolutions attach to gear only.** With #26 they take the element socket. A Refractor who wants one alone carries it on an adapter.
- **B. Evolutions attach to the character only.** This is the deckbuilder's rule ("Attaching it to the Character costs 1 Action + its EN", `02-enums.md` `ECardKind` Evolution).

### Recommendation
**B,** the deckbuilder's rule. It is one slot per character, it is never "evolved weapon or not", and it makes evolutions read as Expedition 33's character-level Pictos.

**Ruling:** D-110's evolution costs. **Cost:** M (loadout slot, costs, infusion source, save).

---

## 33. Three physical resistances

### What exists
- D-037 made Physical a damage type with subtypes Slash, Pierce and Impact.
- Each subtype has its own resistance column in all four resistance shapes (round 1 §7 of `SimplifyAudit_2026-10-28.md`).
- Each subtype shares its break with an element: Slash with Fire, Pierce with Earth, Impact with Lightning (SB-19 to SB-21).

### Why it's too much
- Three resistance numbers per character for one kind of damage.
- The deckbuilder has a single `Physical` (`02-enums.md` "EWoRElement" 10).

### Fix (keeps: weapon types feel different through their break)
- **A.** One Physical resistance column. The subtype only picks which break fires.
  - Expedition 33 and Elden Ring both print one physical defence.
  - Elden Ring's per-type standard / slash / strike / pierce absorption is a common example of numbers players don't read.

### Recommendation
**A.**

**Ruling:** D-037's subtypes stay; only their resistance columns merge. **Cost:** M (resistance shapes, columns; save append-only).

---

## 34. Weather with three drivers

### What exists
- **In battle:** the sky follows which leader "dominates" by HP (`UWeatherStateManager`, `WeatherSystem.md`). Its element field is captured but never read.
- **Boss fights:** D-098 makes weather show a phase change.
- **Trials:** B-189 makes a trial's element set the map's weather.
- **No rules:** weather changes no rule.

### Why it's too much
- Three sources for one cosmetic channel, with no priority between them.
- The HP tug-of-war is hard to read: a small HP swing changes the sky.

### Fix (keeps: weather as mood and as the boss-phase signal)
- **A. One driver.** The trial's element sets the weather (B-189), and a boss phase steps it (D-098). The leader-HP tug retires.
  - Hades' regions have fixed looks, and the final boss's arena changes only between phases.

### Recommendation
**A.**

**Ruling:** none (the HP tug was never ruled). **Cost:** S.

---

## 35. Cosmetics: skins, outfits and the meter

### What exists
- **Skins.** Forms are whole-character skins unlocked by a loadout-match meter (per fight won, by share of the form's pieces worn; a piece at another tier gives partial credit, `OTHER_TIER_CREDIT`) (B-279, B-279b).
- **Outfits.** On top of that, six outfit slots (Helmet, Jacket, Shirt, Waist, Trousers, Footwear) whose pieces are shared store entries any member can wear (D-111, B-279c).
- **Mini-bosses.** Forms double as mini-bosses (B-279d), which is the good part: one data set.

### Why it's too much
- Two cosmetic layers (a whole skin, and six piece slots on top of it).
- The meter has a partial-credit rule by tier that the player can't see.

### Fix (keeps: "become the character by using their gear", the Mortal Shell model)
- **A. Unlock by one condition.** "Win N fights wearing M of their pieces" is a D-130 condition set with no tier credit. It also fits pillar 10 (hidden conditions).
- **B. Outfits as whole sets per form.** Expedition 33's outfits are whole per character, with hair separate. B-279c's six slots become one set choice per form.

### Recommendation
**A + B.**

**Ruling:** Q-279 (tier credit), D-111's "outfit pieces as store entries". **Cost:** S each; B shrinks B-279c.

---

## 36. Ways to make an item stronger

### What exists
**Six routes:**
- upgrade tier (essence, ER-25)
- merge crystals to a higher tier (ER-28)
- attach a crystal, stone or evolution
- fuse two attachables (#26)
- evolve a weapon (#32)
- in a fight only, infuse (round 1 §8)

**Around them:**
- downgrade for a half refund (ER-27), dismantle (ER-30), convert duplicates (D-115) and repair.

### Why it's too much
Fuse and evolve-a-weapon are attach-with-extra-rules. Downgrade exists to undo an upgrade.

### Fix (keeps: upgrading as the main sink for essence)
- **A.** Once #26 and #32 land, the routes are **upgrade, attach, merge (crystals only)**: the same three Monster Hunter has (upgrade the weapon, slot decorations, craft decorations).
- **B.** Downgrade can retire too, because dismantle already turns an item back into essence.

### Recommendation
**A** (it falls out of #26 and #32). **B** is optional.

**Ruling:** ER-27 ("kept by Crown"). **Cost:** S.

---

## 37. Skills on gear: preset skills that the player can override

### What exists
- **Slots by tier.** A weapon holds abilities by its tier, F 1 to S 6 (`ATTACHMENT_SLOTS`, CL-20). A gem crystal or evolution sets the spell slots by its own tier (D-039, `ResolveSpellSlotCap`).
- **Authored skills.** A weapon asset lists `PresetAbilities` and a single `bAbilitiesLocked` flag (`WeaponData.h:62`). An adapter asset lists `PresetSpells` and a `bSpellsLocked` flag (`RingData.h:46`).
  - Flag true: the presets are locked ("conjured" gear) and fill slots.
  - Flag false: the presets are only defaults.
- **Overrides.** The loadout entry holds `AssignedAbilities` and `AssignedSpells`. `GetAllAbilities()` (`FWeaponLoadoutEntry.cpp:44-85`) merges them with the presets **by position**:
  - A non-null assigned entry replaces the preset at the same index.
  - Extra assigned entries are appended.
  - The result is cut to 6.
  - Nothing records that a preset was replaced. The asset keeps it, and clearing the assigned list brings it back.
- **Separate channels.** Ability Stone abilities (`AssignedAugmentStoneAbilities`) and a per-loadout `OverrideAttack` are further lists beside these.
- **Save.** Per slot, ability and spell lists as asset refs (`PrimaryAbilities`, `PrimaryStoneAbilities`, `EvolutionSpells`, `Secondary*`, `WoRSaveTypes.h:354-376`). The learned skills are separate owned entries (D-111).

### Why it's too much
- **The position merge is fragile.** A null in the assigned list does not advance the index, so a hole at index 0 keeps every remaining preset and pushes later entries to the end, not to their slot.
- **Three rules for one slot:** locked, replaceable default, and appended extra.
- **Two lists for abilities** (main and stone), and the stone list's validity depends on a stone being attached.
- **D-141 doubles the places** where slot counts come from: the weapon's tier, the element socket's gem or evolution, and now the stone socket's Ability Stone.

### Fix (keeps: skills live on gear, and some gear has its own)
- **A. Built-in skills sit outside the slots.**
  - A preset is an always-on built-in of the gear. It cannot be replaced and takes no slot.
  - The tier slots hold only what the player chooses, as one ordered list. An Ability Stone adds slots to that list.
  - Expedition 33 does this: each weapon has fixed passives, and the player's own choices go in Pictos slots.
- **B. FF7-style empty slots.** Gear has slots and no built-ins. The player fills every slot, and a "starter" is just the first item in the shop. Simplest data, but it drops "some gear has its own".
- **C. FFIX-style learning.** Gear teaches its skill after use, and the player then equips it. Closest to the owned-skill list (D-111), but it adds a progress meter per skill.

### Recommendation
**A.**
- **Removed:** `bAbilitiesLocked` and `bSpellsLocked`, the position merge and the stone ability channel.
- **Kept:** conjured gear is simply gear with built-ins and no free slots.
- **Save:** one chosen list per item and the built-ins read from the asset. This rides the D-138 wipe (`SAVE_VERSION` bump from B-351).

**Ruling:** CL-20's "locked presets fill slots" and D-039's slot wording change. D-108 and D-141 are untouched.

**Cost:** M, in the same job as B-351 (the two sockets), since both change the slot struct and the loadout validation.

---

## Round 3: kept, already simple

- **One status bar** with one break per damage type (SB-01, B-274).
- **Gems with an element and a use.** Each gem is an element source *and* a utility (Emerald bonus turn matches Wind's push). The theme is consistent, and it matches the deckbuilder's utility `Crystal`.
- **Element weakness by per-character resistance** (D-005 C) rather than a wheel.
- **Spells and abilities** as the two skill kinds, the same as the deckbuilder's `Ability` / `Spell`. The basic attack is an ability.
- **Forms doubling as mini-bosses** (one data set).
- **Lore, Crown's alone:** Reality / Unknown (D-095) and Void. The deckbuilder has retired Reality (`02-enums.md` "`Elemental`"), but here it carries lore, so it is noted, not raised.

## Sources

- Expedition 33: [Maxroll weapons and attributes](https://maxroll.gg/clair-obscur-expedition-33/guides/weapon-attributes-upgrades-guide), [Deltia's scaling](https://deltiasgaming.com/?p=216376), [Gfinity attributes](https://www.gfinityesports.com/article/clair-obscur-expedition-33-best-attributes-for-each-character), [Steam: attribute effects](https://steamcommunity.com/app/1903340/discussions/0/592896965922002093)
- Destiny 2 Power: [Destinypedia](https://www.destinypedia.com/Power)
- Hades Heat: [Hades wiki](https://hades.fandom.com/wiki/Heat)
- Octopath Traveler Break and Boost: [Nintendo Life](https://www.nintendolife.com/guides/octopath-traveler-beginners-faq-best-characters-breaks-and-boosts-and-path-actions-explained)
- Durability: [Game Informer](https://www.gameinformer.com/opinion/2019/08/01/we-deserve-better-from-games-with-breakable-weapons), [Unwinnable](https://unwinnable.com/2017/04/27/weapon-durability-in-breath-of-the-wild/), [Den of Geek on Fuse](https://www.denofgeek.com/games/zelda-tears-of-the-kingdom-breakable-weapons)
- Strangler Fig: [Martin Fowler](https://martinfowler.com/bliki/StranglerFigApplication.html)

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | Audit written | chore/simplicity-audit |
| 2026-11-04 | §37: skills on gear (preset skills, overrides) | chore/simplicity-audit |
| 2026-11-03 | Round 3: §26–§36 (attachments and stones, carry limits, boss rules, class passives, spell school, evolution placement, physical resistance, weather, cosmetics, upgrade routes) | chore/simplicity-audit |
| 2026-11-03 | Round 2: §12–§25 (process, subsystems and co-op, conditions, screens, docs, turns, grid, input, AI, party, run buy-in, saves, debug, tests) | chore/simplicity-audit |
