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
| 2026-11-03 | Round 2: §12–§25 (process, subsystems and co-op, conditions, screens, docs, turns, grid, input, AI, party, run buy-in, saves, debug, tests) | chore/simplicity-audit |
