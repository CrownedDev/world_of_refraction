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
