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
| Each school gets **one spell**. Weapon abilities **carry no school** (Crown, 2026-11-09, Q1): schools stay with spells and their slot limits. The abilities still cover the same six **roles** (damage, buff, heal, conjured thing, control, debuff) so the camera sees each shape. | Crown's ask; also shows the camera on each skill shape. |
| New assets follow `Conventions/ContentNaming.md`; new heavy references are `TSoftObjectPtr` (D-057); text is `FText` (D-061). | Standing rules. |

**Gaps in the code this exposed** (each becomes a job in §8):

1. **No per-element VFX lookup for a Generic skill.** A skill's VFX is authored on the skill (`SkillVFXEntry`); there is nothing that says "Prism Bolt cast by a Fire user uses the Fire projectile". Needed so one Generic asset looks right in every element. → C1.
2. ~~Abilities have no school.~~ Answered by Crown (Q1, 2026-11-09): **that's correct and stays**. `ESpellSchool` lives on `USpellData` only (`SpellData.h:72`).
3. **No taunt effect.** Manipulation today has `RandomSkill` (loss of control) and `Stun` (attack/defend only). Taunt and "force a target" (D-163) don't exist yet, so the showcase uses those two. → Decision Q2.

---

## 2. The six spells

Cast by the **Refractor** (innate pool, inherits its innate element) and the **Resonator** (from its active adapter, inherits the adapter's element), so each spell is seen in two elements in one fight.

Asset column key: ✅ owned and fits · 🔧 owned, needs a tweak · ❌ missing. Stand-in = cheapest thing that makes it playable now.

| # | Name | School | What it does (effects) | Target / delivery | Cost | Animation | VFX | Sound | Icon | Missing → stand-in |
|---|---|---|---|---|---|---|---|---|---|---|
| S1 | **Prism Bolt** | Destruction | `BaseDamage` hit; nothing else. The plain read of "my element hits yours". | Enemy · Single · Projectile | 1 | ✅ Mage `Fireball_R_IP` (H) | ✅ `<Element>_Magic` Muzzle → Projectile → Hit (7 elements, H/Eye) | ❌ | 🔧 RPGIcons | Sound: PONETI `magic-missiles` (licence check). Void/Reality VFX: Dark set recoloured violet |
| S2 | **Kindle** | Enhancement | `SpellDamageBuff` + `DefenseBuff` on one ally, 2 turns | Ally · Single · Instant | 1 | ✅ Mage `Enchants` or `CollectMana` (M) | ✅ `<Element>_Magic_Buff` on the target, short (V5) | ❌ | 🔧 RPGIcons | Sound: none (silent is fine for a buff in the showcase) |
| S3 | **Renewal** | Restoration | `Heal` (instant) + `Cleanse` on one ally | Ally · Single · Instant | 1 | 🔧 `Worshiped` / `ReadsTheSpell` (M) | 🔧 `Light_Magic_Heal` / `Heal_Hit` only: recolour per element; cleanse ❌ | ❌ | 🔧 RPGIcons | Cleanse: the heal effect plays twice (stand-in). Colour: a tint parameter (C1) |
| S4 | **Shard Field** | Conjuration | A field of conjured shards bursts up under the enemy group: `BaseDamage` to all, `DefenseDebuff` 1 turn | Enemy · All · AOE | **2** | ✅ Mage `GroundSlam` (H) | 🔧 `<Element>_Magic_Spike` / `Wall` (bursts; fine for a one-shot conjuration) | ❌ | 🔧 RPGIcons | Elements with no spike: use the element's `AOE` |
| S5 | **Bewilder** | Manipulation | `RandomSkill` on one enemy for 1 turn (it uses a random skill on a random target) | Enemy · Single · Instant | 1 | 🔧 `Hypnotizing` (L/Eye) | ❌ "mind" effect | ❌ | 🔧 RPGIcons | VFX: one `NS_Magic_Circle1_*` over the head, element-tinted |
| S6 | **Sap** | Impairment | `ActionSpeedDebuff` + `SpellDamageDebuff` on one enemy, 2 turns (its swings get **easier** to read: D-151) | Enemy · Single · Projectile | 1 | 🔧 `Bewitchment` (L/Eye) | 🔧 `Dark/Blood/Poison _Debuff` exist; other elements ❌ | ❌ | 🔧 RPGIcons | VFX: the element's Projectile + a tinted magic circle on hit |

Coverage: Destruction single, Conjuration all-targets (R8 "pull back and hold"), the four support/control schools on one target each.

---

## 3. The two weapons, 6 abilities each

**Choice:** **Sword + Shield** and **Dual Axe**. Both are ✅ in the audit with a full weapon guard (block + block-hit). Sword + Shield has in-place attacks (the CLAUDE.md default); Dual Axe is root motion, which is fine as a per-montage opt-in and tests that path. Alternative: Gauntlets (✅, guard = the unarmed guard, so it shows less).

Abilities take their look from the weapon (D-170: weapon VFX fixed per weapon) and are tagged Generic so a crystal or the Resonator's adapter colours them.

### Sword + Shield (the Generic, "Warden")

| # | Name | Role | What it does | Target | Cost | Animation | VFX | Sound | Icon | Missing → stand-in |
|---|---|---|---|---|---|---|---|---|---|---|
| W1 | **Rising Cut** | Damage | Melee hit, `BaseDamage`, 2 hits (`HitCount` 2) | Enemy · Single | 1 | ✅ DSAS V1 `Combo1_IP` (H) | 🔧 weapon trail + `<Element>_Magic_Slash` on hit (V1) | ❌ | 🔧 | Sound: PONETI `melee-attack-with-voice`; trail: a plain ribbon |
| W2 | **Brace** | Buff | `DefenseBuff` + `ReduceDamageTaken` on self, 2 turns | Self | 1 | ✅ SAS `Block` start (H) or CM `Buff` | ✅ `<Element>_Magic_Shield` short | ❌ | 🔧 | — |
| W3 | **Second Wind** | Heal | `RestoreHPPercent` on self + `CleanseSelf` | Self | 1 | ✅ HSAS `Buff` (H) | 🔧 recoloured heal (as S3) | ❌ | 🔧 | As S3 |
| W4 | **Bulwark** | Conjured thing | A conjured wall in front of the party: `AbsorbDamage` on all allies, 2 turns | Ally · All | **2** | ✅ DSAS shield slam combo (M) | ✅ `<Element>_Magic_Wall` (Fire, Ice, Water, Earth, Dark, Blood) (H/Eye) | ❌ | 🔧 | Other elements: Earth wall tinted. Burst only: a held loop is not needed for one showcase (AssetAudit "Conjuration") |
| W5 | **Rattle** | Control | Shield bash: small `BaseDamage` + `RandomSkill` 1 turn | Enemy · Single | 1 | ✅ DSAS bash (M) | 🔧 hit burst + tinted circle over the head (as S5) | ❌ | 🔧 | — |
| W6 | **Hamstring** | Debuff | Melee hit + `ActionSpeedDebuff` 2 turns | Enemy · Single | 1 | ✅ DSAS V2 low cut `_IP` (M) | 🔧 slash + `Blood_Magic_Debuff` | ❌ | 🔧 | — |

### Dual Axe (the Resonator, "Reaver")

| # | Name | Role | What it does | Target | Cost | Animation | VFX | Sound | Icon | Missing → stand-in |
|---|---|---|---|---|---|---|---|---|---|---|
| A1 | **Whirl** | Damage | Spinning melee, 3 hits on two enemies | Enemy · **Double** | **2** | ✅ DynamicTwinAxe spin (RM, opt-in) (H) | 🔧 trails + `<Element>_Magic_Slash` | ❌ | 🔧 | — |
| A2 | **Bloodrush** | Buff | `RawDamageBuff` + `CritChanceBuff` on self, 2 turns | Self | 1 | ✅ DynamicAxe `Buff` (H) | ✅ `<Element>_Magic_Aura` short (V5) | ❌ | 🔧 | — |
| A3 | **Reap** | Heal | Melee hit with `DrainHP` (heals by part of the damage) | Enemy · Single | 1 | ✅ DynamicTwinAxe cross-cut (M) | 🔧 slash + small recoloured heal on the user | ❌ | 🔧 | — |
| A4 | **Spectral Axes** | Conjured thing | Two conjured axes fly out and hit: `BaseDamage` | Enemy · Double | 1 | ✅ DynamicTwinAxe throw-like swing (L/Eye) | 🔧 `Light_Sword_*` (7) recoloured as axes, or the element Projectile | ❌ | 🔧 | VFX: element Projectile ×2 |
| A5 | **Skull Splitter** | Control | Heavy overhead: `BaseDamage` + `Stun` 1 turn (attack or defend only) | Enemy · Single | 1 | ✅ DynamicTwinAxe overhead (M) | 🔧 big hit + Paragon `Stun_Loop` on the target (anim) | ❌ | 🔧 | Stun marker VFX ❌: small tinted circle over the head |
| A6 | **Sunder** | Debuff | Melee hit + `DefenseDebuff` + `ResistanceDebuff`, 2 turns | Enemy · Single | 1 | ✅ DynamicTwinAxe (M) | 🔧 slash + `Dark_Magic_Debuff` | ❌ | 🔧 | — |

**Each weapon covers all six roles** (not schools: Q1). The three cost-2 skills (S4, W4, A1) are the turn-filling "big" moves.

**Sound across all 18:** the audit found **no combat SFX at all**. For the showcase: PONETI Freesound stand-ins (licence per file) for swing, hit, metal clang and one cast sound shared by all spells. Buying one combat SFX pack is the real fix (AssetAudit §3) and is Crown's call (Q3).

**Icons across all 18:** RPGIcons (148, generic, Eye) as stand-ins; the plain C++ HUD can also show the name alone.

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
| ~~C2~~ | ~~Ability school~~: **dropped** (Crown, Q1: abilities carry no school) | — | — |
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

Q1 is answered. **Q2 to Q5 are parked** (Crown, 2026-11-09: another day); until then their defaults are working assumptions only.

| # | Question | Default | Reference |
|---|---|---|---|
| Q1 | Do weapon abilities carry a school like spells? | **Answered (Crown, 2026-11-09): no.** Schools stay with spells and their slot limits. | — |
| Q2 | Manipulation has no taunt yet; use `RandomSkill` and `Stun` for the showcase? | Parked. Working assumption: yes; taunt is its own later job. | — (uses what's built) |
| Q3 | Sound: buy one combat SFX pack now, or Freesound stand-ins until the look is set? | Parked. Working assumption: stand-ins. | Common indie practice: placeholder audio until art direction is locked. |
| Q4 | Weapons: Sword + Shield and Dual Axe (both have a full guard set)? | Parked. Working assumption: yes; asked again at item 7 (§10). | AssetAudit ✅ rows. |
| Q5 | Skill and character names (Prism Bolt, Warden, Prism Warden…) | Parked. Placeholders; naming is lore, Crown's call. | — |

---

## 10. Picks, one item at a time (Crown chooses)

Crown picks the assets for each item in turn; the choice is recorded here before moving to the next. Each item shows: animation, VFX, sound, icon (and the weapon mesh for gear), with Claude's pick and one or two alternatives. Picks are from **names** (AssetAudit confidence); Crown judges the look in the editor build (§11).

| Order | Item | Status |
|---|---|---|
| 1 | S1 Prism Bolt | **Asking Crown** |
| 2–6 | S2 Kindle, S3 Renewal, S4 Shard Field, S5 Bewilder, S6 Sap | Waiting |
| 7 | Sword + Shield (mesh, guard) | Waiting |
| 8–13 | W1–W6 | Waiting |
| 14 | Dual Axe (mesh, guard) | Waiting |
| 15–20 | A1–A6 | Waiting |

### S1 Prism Bolt (Destruction, enemy single, projectile)

| Part | Claude's pick | Alternatives | Crown's choice |
|---|---|---|---|
| Animation | `MageAnimationSet/.../InPlace/Mage_Fireball_R_IP`: a one-hand throw, in place, matches the Mage set used for the other casts | (a) `MagicAnimations/AS_Cast_PrepareAndFire` (a two-step charge then fire: a longer, clearer wind-up for FeelGuide 14); (b) `AS_Cast_Turn-basedAttackRT` (named for turn-based casting) | — |
| VFX | The element set from Big Pack: `NS_<Element>_Magic_Muzzle` → `Projectile` → `Hit` (Fire: `NS_Fire_Magic_Projectile`) | (a) the second projectile style per element (`Projectile2`), smaller; (b) Mage pack `FireballTrail_P` (old Cascade, Fire only: not recommended) | — |
| Sound | PONETI `249817__spookymodem__magic-missiles` (stand-in, licence to check) | (a) `446015__slavicmagic__wpn-2-generic`; (b) silent until a sound pack is bought | — |
| Icon | PONETI `SkillsIcons/Bonus/Skill1_Nobg/Magic_bolt_nobg` (no colour of its own, fits a Generic spell) | (a) RPGIcons `T_MagicArrow`; (b) RPGIcons `T_Fireball` (reads as Fire only) | — |

---

## 11. Unreal build order (after the picks)

The Unreal editor session builds each picked item **one at a time**, in this order, and Crown reviews it in the editor before the next. Each step is the matching E-job slice from §8.

| Step | Build | Needs first | Crown reviews |
|---|---|---|---|
| 0 | Arena `L_Showcase_Arena` (E2) and the showcase party data with empty loadouts (E5 part) | C3 for `wor.Showcase.Start` | Walk the arena |
| 1 | S1 Prism Bolt | C1 (element VFX lookup) for the per-element look; Fire only without it | Cast as Fire (Refractor) and Water (Reaver) |
| 2–6 | S2 → S6, one per step | Step 1 | Each spell on its target |
| 7 | Sword + Shield on the Warden: mesh, idle, weapon guard | B-392 for the guard choice | Idle, guard, block-hit |
| 8–13 | W1 → W6 | Step 7 | Each ability |
| 14 | Dual Axe on the Reaver: meshes, idle, guard | B-392 | Idle, guard |
| 15–20 | A1 → A6 (root motion opted in per montage) | Step 14 | Each ability |
| 21 | Enemies: Hollow group, then the Prism Warden boss data | Steps 1–20 | Both fights to victory |
| 22 | Marks, trails, stand-in sounds, notify validator, `docs/PIE/Showcase.md` (E6) | Step 21 | The PIE sheet |

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-09 | Created: content, party, enemies, arena, PIE entry, build list | docs/showcase-battle-plan |
| 2026-11-09 | Q1 answered (abilities carry no school; they keep six roles); Q2–Q5 parked; §10 picks one item at a time (S1 asked); §11 Unreal build order | docs/showcase-picks |
