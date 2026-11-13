# Showcase battle plan

**2026-11-09**, side session for the brain (desktop operator), from Crown's request: one small battle that shows how the game **looks and plays** (camera, VFX, UI, animations) before lots of content is built. Plan only; nothing here is built yet.

Inputs: `AssetAudit.md` (what we own, confidence marks H / M / L / Eye), `analysis/MaelleCombatPresentationStudy.md` (rules R1–R18, V1–V10, U1–U12, F1–F7), `FeelGuide.md`, `Rules/00-index.md`, `VisualGameplayPlan.md` (D-170), `SpellSchool.h` (D-163), `ESkillEffectType.h`.

**How it relates to B-395 (vertical slice):** B-395 is one fight built to the FeelGuide end to end. This showcase is the **content** for that slice and one step wider: 18 skills, a party of three, a standard fight and a boss fight in one arena. Build B-395's spine first (entry, turns, hits, defence, victory); the showcase fills it.

---

## 1. Ground rules for the content

| Rule | Why |
|---|---|
| Every spell and ability is **Generic element** (`ESpellElement::Generic` = inherit, `ESpellElement.h`). | One base each; element variants come later from the same asset. The party shows the inherit rule twice (§4). |
| **Action cost 1** unless marked 2. Cost-2 skills are the "big" ones and show the pip rules (B-170c: Max Actions starts at 2, so a cost-2 skill is a whole turn). | B-170 default. Until B-170c lands, cost is written in the job and the Description only. |
| Effects use **existing effect types only** (`ESkillEffectType`). Durations and numbers are `// TODO: tune` starting values. | No new mechanics for a showcase. |
| Target shapes use `ETargetType` (Self / Ally / Enemy / Anyone) × `ETargetCount` (Single / Double / All), and spell delivery `ESpellDeliveryType` (Projectile / AOE / Instant). | Existing fields. |
| Each school gets **one spell and one ability per weapon**, so every school appears three times. | Crown's ask; also shows the camera on each skill shape. |
| Every melee skill is **approach → strike → return**: a dash in, with the warp only over the airborne frames (B-424), the strike, then back to the start mark. | One readable shape for every melee skill; the warp never hides a grounded step. |
| New assets follow `Conventions/ContentNaming.md`; new heavy references are `TSoftObjectPtr` (D-057); text is `FText` (D-061). | Standing rules. |

**Gaps in the code this exposed** (each becomes a job in §8):

1. **No per-element VFX lookup for a Generic skill.** A skill's VFX is authored on the skill (`SkillVFXEntry`); there is nothing that says "Prism Bolt cast by a Fire user uses the Fire projectile". Needed so one Generic asset looks right in every element. → C1.
2. **Abilities have no school.** `ESpellSchool` lives on `USpellData` only (`SpellData.h:72`); `UAbilityData` has none. Q1 answered: it stays that way (school is a theme only).
3. **No taunt effect.** Manipulation today has `RandomSkill` (loss of control) and `Stun` (attack/defend only). Taunt and "force a target" (D-163) don't exist yet. Q2 is superseded: taunt is in scope (see `ShowcaseInventory.md`).

---

## 2. The spells — superseded

The current spell picks are in `ShowcaseInventory.md` and `ShowcaseAnimAnalysis.md` (branches `docs/showcase-inventory` and `docs/showcase-anim-analysis`, merged into `dev` later). The six-spell table that was here is retired.

## 3. The weapons and abilities — superseded

The current weapon and ability picks are in the same two documents. The weapon tables that were here are retired.

---

## 4. The party

| Slot | Who | Class | Gear | Element | Shows |
|---|---|---|---|---|---|
| 1 (leader, Crown plays) | **Warden** | Generic | Sword + Shield (W1–W6); **weapon guard** | Weapon crystal: none (abilities show neutral) | Melee shot patterns (B-403), weapon guard + sparks (B-392/393), cost-2 Bulwark on the whole party |
| 2 | **Reaver** | Resonator | Dual Axe (A1–A6) in the primary slot + one adapter (rings) holding S1–S6, **Water** adapter | Water (from the adapter, CL-05) | Spells inherit the adapter's element; switching between axe and spells; root-motion abilities |
| 3 | **Refractor** | Refractor (`Caster`) | Innate pool S1–S6; no weapon; **unarmed guard** | Fire innate (CL-04) | The same six spells in Fire next to Reaver's Water; unarmed guard with the parry-effect cosmetic (B-393) |

All three on Manny / Quinn bodies (the only bodies we own), each with a plain colour MI so they're told apart. All three player-controlled by default; `wor.Party.SetControl` can hand one to the AI to watch the AI use the skills.

---

## 5. The enemies and the arena

### Arena

**`L_Showcase_Arena`**: a copy of `TestLevel_Encounter` (grey box) with a SoStylized `BP_StylizedSky` preset (AssetAudit "Arena + sky", 🔧). One flat open ring for the standard fight; a **raised platform with a backdrop piece** at the far end for the boss (R13: boss framed with its backdrop; any large prop from `MSPresets` or a scaled grey block). Two enemy groups placed in the world, walked up to in turn.

### Fight 1: standard group (Normal rank)

| Enemy | Body | Kit | Why it's here |
|---|---|---|---|
| **Hollow Duelist** ×2 | Manny + dark MI | Gauntlets (DynamicHandtoHand V1/V2, guard V2 ✅); uses W1-like jabs | Several targets → **target cut** (B-400); defendable swings with trails (V2) |
| **Hollow Seer** ×1 | Quinn + dark MI | Innate S1 **Prism Bolt** and S6 **Sap** (Darkness) | FeelGuide 14: the player sees **the same Prism Bolt wind-up** they use, on an enemy; a ranged defend |

Shows: Centred Wide / Over-the-Shoulder turn view (B-398), calm menus (B-399), target cut (B-400), no reset between actions (B-401), defence two-shot and party-wide wide shot (B-402, when the Seer's Sap or a Duelist hits), wide beats and handover (B-404), **normal kill shot** (B-405).

### Fight 2: boss

| Enemy | Body | Kit | Phases |
|---|---|---|---|
| **The Prism Warden** (main boss, `EEncounterRank` Boss) | Manny scaled ~1.3 with a bright MI (Eye) | Greatsword (DynamicGreatSword, **root motion only**, block set ✅); own skills W1, A1 Whirl (named attack), A5 Skull Splitter, S4 Shard Field | **Phase 1** (100–50 % HP): Generic, greatsword only. **Phase 2** at `HPBelowPercent 50` (`UBossData` phase): `Element` → Lightning, `AddedSkills` Whirl + Shard Field, `WeatherIntensity` up, a weapon-glow change (V10). Damage carries past 50 % (B-193). |

Shows: **low camera looking up** (B-405 R13), named-attack front shot (B-406, Whirl), the **opening sequence and phase-change sequence on trial** (B-406, both opt-in on this boss only), boss bar + refill on phase (U3, with the HUD), **boss kill shot** (B-405), short signature-skill cameras on the party's cost-2 skills (B-407).

No adds in the boss fight: B-224b scales HP per team, and a lone boss keeps the phase change readable. Crown can add a Duelist pair later as a second team.

---

## 6. How Crown opens it in PIE

1. Open `L_Showcase_Arena`, Play (single player).
2. Console: **`wor.Showcase.Start`** (new, job C3): fills the party with Warden, Reaver and Refractor from `DA_Showcase_Party` at full HP/EP and puts you at the arena start. `wor.Showcase.Start Boss` puts you at the boss platform instead.
3. Walk into the Hollow group: the fight starts (FeelGuide 1; the crystal-break transition is B-395's, a fade until then). Win, walk on to the platform, touch the Warden: the boss fight.
4. Useful while watching: `wor.Combat.PrintState`, `wor.Party.SetControl` (hand a member to the AI), `wor.Boss.Print`, the camera console switches from B-398.
5. A PIE sheet `docs/PIE/Showcase.md` (job E6) lists one check per camera job and per skill: "use Bulwark → see the wall on all three, the camera holds wide".

---

## 7. Assets to make or buy (summary)

| Missing | Cheap stand-in for the showcase | Real fix |
|---|---|---|
| Per-element VFX for a Generic skill | C1's lookup with the 7 owned element sets; Void = Dark recoloured; Reality = Light + circle | Same lookup, better art later |
| Combat sounds (all) | PONETI Freesound files (licence per file) | Buy one combat SFX pack (Q3) |
| Cleanse, stun, silence, "mind" markers | Tinted `NS_Magic_Circle1_*` | A buff/debuff VFX pack (VisualDirection §2d) if recolours look weak |
| Parry sparks | (B-393) a small Niagara burst | Same |
| Skill icons | RPGIcons / name only | Style-matched icon set later |
| Weapon trails | A plain ribbon in the weapon's element colour | Per-weapon trails |
| Enemy bodies | Manny / Quinn + MIs | Paragon meshes (free on Fab) or a bought pack |

---

## 8. Build list

Small jobs, ≤ 3 files each. **C** = Code Writer (C++, headless specs). **E** = Unreal editor session (assets and data through MCP; Crown's eye on each). B-numbers are given by the brain when they go into `Backlog.md`.

### Code Writer (C++)

| Job | What | Depends on | Done when |
|---|---|---|---|
| **C1** | **Element VFX set**: a `UElementVFXSet` data asset (per element: muzzle, projectile, hit, AOE, buff, shield, wall, slash, debuff, heal; `TSoftObjectPtr<UNiagaraSystem>`) and a resolver: a Generic skill's VFX slot left empty takes the caster's resolved element's entry; a `Tint` colour parameter for recolours. `PrintElementVFXSet` debug. | B-394 (cast VFX source rule) | Spec: a Generic skill cast as Fire resolves Fire's projectile; an authored VFX on the skill still wins |
| **C3** | **`wor.Showcase.Start [Boss]`**: fills the party from a `UShowcaseSetupData` asset (members, loadouts, start points) through `UPartySessionSubsystem` (as `wor.Party.RecruitForRun` does), full HP/EP, teleports | — | Spec: the command fills three members with the authored loadouts |
| **C4** | **Action cost on the 18 skills**: nothing new if B-170c has landed (set `ActionCost` in data, E-job); if not, the cost lives in Description only | B-170c | — |
| **C5** | **Boss phase weapon glow** (V10): a boss phase can set a weapon VFX (`TSoftObjectPtr`, empty = none) played on the boss's weapon from the phase event | B-193 / B-355 | Spec: the phase event swaps the weapon VFX; empty does nothing |
| Camera | **B-398 → B-407** as already written. Order for the showcase: B-398, B-399, B-401, B-402 first (every turn); then B-400, B-403, B-404; then B-405, B-406, B-407 | B-370, B-371, B-156 (as in Backlog) | Each job's own spec + the showcase PIE sheet |
| GAS | The 18 skills run through the **GAS abilities** path once B-043f is merged; until B-043h they run either way. No new GAS work for the showcase | B-043a–g (REVIEW), B-043h after Crown's PIE | Showcase PIE passes with the GAS path on |

### Unreal editor session (assets and data, via MCP)

| Job | What | Depends on | Done when |
|---|---|---|---|
| **E1** | **Contact sheet** (AssetAudit §4): thumbnails / short captures of every animation and VFX named in §2–§5, for Crown to pick before any data is made | — | Crown has picked or swapped each candidate |
| **E2** | **Arena**: `L_Showcase_Arena` from `TestLevel_Encounter`, sky preset, boss platform + backdrop block, two enemy placements with `EncounterComponent` (Normal group; Boss rank) | — | Both fights start by walking in |
| **E3** | **The 6 spells**: `DA_Spell_Showcase_*` (`USpellData`, Generic, school, effects, target, delivery), montages from the Mage pack, effects as `UEffectDefinition`s | E1; C1 for the VFX to show per element | Each spell casts in PIE in Fire and Water |
| **E4** | **The 2 weapons and 12 abilities**: `DA_Weapon_Showcase_SwordShield` / `_DualAxe`, `DA_Ability_Showcase_*`, montages (Dual Axe with root motion opted in), guard montages for B-392 | E1; B-392 for the guard pick | Each ability plays in PIE |
| **E5** | **Party and enemies**: `DA_Showcase_Party` (C3's asset), three `UCharacterData` with loadouts; Hollow Duelist / Seer data; `DA_Boss_PrismWarden` (`UBossData`, two phases, named-attack flag on Whirl, opening + phase sequence fields for B-406's trial); MIs for each body | C3, E3, E4; B-406 for the sequence fields | `wor.Showcase.Start` gives the party; both fights run to victory |
| **E6** | **Marks and polish per skill** (VisualGameplayPlan step 3): cast and impact notifies, trails, stand-in sounds, the notify validator (B-368); write `docs/PIE/Showcase.md` | E3–E5 | Validator clean; PIE sheet written |

**Suggested order:** E1 → (C1, C3, E2 in parallel) → E3, E4 → E5 → E6, with the camera jobs landing alongside in the order above. B-391 (new HUD) and B-392/393 (guards, parry VFX) are prerequisites for the showcase to *look* right but not for it to *run*.

---

## 9. Decisions for Crown

Reference-backed defaults (approve by default; Crown vetoes):

| # | Question | Default | Reference |
|---|---|---|---|
| Q1 | Do weapon abilities carry a school like spells? | **Answered: No.** Abilities get no school field; school is a theme only (Crown, 2026-11-13). Job C2 is dropped. | — |
| Q2 | Manipulation has no taunt yet; use `RandomSkill` and `Stun` for the showcase? | **Superseded:** taunt is in scope (`ShowcaseInventory.md`). | — |
| Q3 | Sound: buy one combat SFX pack now, or Freesound stand-ins until the look is set? | **Stand-ins for the showcase**, buy before the first public build. | Common indie practice: placeholder audio until art direction is locked. |
| Q4 | Weapons: Sword + Shield and Dual Axe (both have a full guard set)? | **Yes.** Gauntlets is the swap if Crown prefers. | AssetAudit ✅ rows. |
| Q5 | Skill and character names (Prism Bolt, Warden, Prism Warden…) | **Placeholders**; naming is lore, Crown's call. | — |

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-09 | Created: content, party, enemies, arena, PIE entry, build list | docs/showcase-battle-plan |
| 2026-11-13 | Q1 answered (no ability school; C2 dropped); §2–§3 superseded by ShowcaseInventory.md / ShowcaseAnimAnalysis.md; Q2 superseded (taunt in scope); §1 rule: melee skills are approach → strike → return (B-424) | docs/showcase-plan-q1 |
