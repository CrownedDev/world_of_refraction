# Combat Numbers Baseline (proposal)

**Status:** proposal (2026-10-27), being decided with Crown one question at a time. Nothing here is a ruling until marked. Outside sources were partly search summaries (see notes).

## Decided

- **Skill + weapon are added** (Crown, 2026-10-27; the deckbuilder's flat weapon bonus): base = skill part + weapon part (Power × scaling × requirement factor). Weapon Power defaults to 0, so today's fights stay the same until Power is set.
- **Weapon Power by tier: F 40, E 52, D 68, C 88, B 114, A 148, S 192** (Crown, 2026-10-27; the tier-power steps; tunable in data).
- **Pillar cap in a run: 7 per pillar, 21 total** (Crown, 2026-10-27), matching `MAX_WORLD_STAT_LEVEL`. `WorldStatConstants::LIVE_MAX` (20, a placeholder) drops to 7; every curve and range in this doc keyed 0..20 is re-keyed to 0..7 (early 2-3, mid 4-5, late 6-7).
- **Pillar curve: soft cap** (Crown, 2026-10-27; Elden Ring / Dark Souls): `Curve(L) = 1 - (1 - L/7)^2` over levels 0..7 (L1 0.27, L2 0.49, L3 0.67, L4 0.82, L5 0.92, L6 0.98, L7 1.0).
- **Requirement penalty: at most -50%, on the weapon's part only** (Crown, 2026-10-27; Elden Ring's flat under-requirement penalty): the requirement factor floors at 0.5 (`WEAPON_REQ_FACTOR_MIN` 0.25 -> 0.5, into data) and scales the weapon part, not the skill part. Worst pillar decides (Q-162).

---


**Status: a proposal for Crown, not a ruling.** Nothing here is built and no repo file was changed. It implements D-101 (weapons get Power and Mind/Body/Spirit grades) and follows D-082, Q-162, Q-146c, D-036 and D-086, plus the stat-maths plan (`docs/analysis/StatMathsRedesign.md`).
Code citations are `Source/world_of_refraction/...:line` on `dev` (2026-10-27). Outside claims link to a source. Where the web only gave community estimates, this doc says so.

---

## 1. Current state: what the code uses today

### 1.1 The damage pipeline (attacks and abilities)

Two stages:

1. **Base** (`Private/Combat/Actions/ActionExecutor.cpp:597-608`):
   `skill BaseDamage x charge band x tier gap (skill vs weapon) x TIER_POWER(skill tier) x weapon requirement factor`
2. **Damage maths core** (`Private/Combat/Damage/DamageMath.cpp:28-105`, B-219):
   `Base x (RawDamage stat + gear term + sum(grade coef x stat fraction)) x stone x grid x status x stat-cap correction x element x crit x (1 - Defense) / grid defense`, with a floor of 1 damage (`Public/Combat/Damage/DamageCalculator.h:33`).

| Piece | Today's numbers | Where |
|---|---|---|
| Skill base | Each asset's `BaseDamage` (int, default 0). `CalculateDamage` returns it unchanged. | `Private/Skills/Definitions/SkillDataBase.cpp:284-295` |
| **Weapon damage** | **None.** A weapon's tier only matters through the tier gap against the skill. It has no Power and no scaling. | `Public/Equipment/Weapons/WeaponData.h:43-125` (no damage field) |
| Tier power F..S | 1.00, 1.30, 1.70, 2.20, 2.85, 3.70, 4.80 (about +30% per tier) | `Public/Combat/Damage/TierPowerConstants.h:18` |
| Upgrading | An upgrade raises the item's **instance tier** one step (F to S, so 7 levels) | `docs/Architecture/TierOnInstance.md` |
| Tier gap (damage) | gap -4: 1.30 … 0: 1.00 … +1: 0.90, +2: 0.78, +3: 0.64, +4: 0.50 | `Public/Combat/Damage/TierGapConstants.h:19-27` |
| Requirement-gap ladder | per pillar level, ±5: 1.40 / 1.30 / 1.20 / 1.13 / 1.06 / **1.00** / 0.90 / 0.78 / 0.64 / 0.50 / 0.32 | `TierGapConstants.h:143-185` |
| Weapon requirement factor | ladder clamped to [0.25, 1]; the worst pillar decides; no bonus for being over | `Public/Skills/Definitions/WorldStatRequirements.h:88-109` (Q-162, Q-146c) |
| Scaling grades | S 1.0, A 0.8, B 0.65, C 0.5, D 0.35, E 0.2, F 0.05 (marked "PLACEHOLDER") | `Public/Skills/Definitions/EScalingTier.h:32-45` |
| Scaling fraction | Multiplier stats: `max(0, stat - 1)`, so 0..0.5 from stats alone and up to about 1.0 with gear. Percent stats: value / 0.5. Linear, with no soft cap. | `EScalingTier.h:84-123` |
| Charge bands | focus 1.15 / 1.30, off-focus 1.10 / 1.20, balanced 1.125 / 1.25 | `Public/Infusion/InfusionConstants.h:20-35` |
| Element resistance | 1:1, capped at ±50% (D-036) | `Public/Combat/Damage/DamageMath.h:76` |
| Guard / Perfect Guard | 50% / 100% off | `docs/Rules/02-elements-and-damage.md` ED-33 |

### 1.2 Stats (the same for players and enemies)

- **Pillar level** ("world level"): the asset authors 0..7; the live value is 0 + head-start (up to 4) + earned or bought in the run, capped at **20** (`Public/Character/CharacterDataComponent.h:29-32`; `docs/Design/Resources_Design.md` §World Stats). It resets to 0 every run.
- **Pillar multiplier** = 1 + 0.07 x level, so 1.49 at 7 and 2.40 at 20 (`Public/Combat/CombatConstants.h:40-42`; `Public/Character/CharacterData.h:425-438`).
- **Sub-stat points**: 30 + 3 per pillar level, spread over 14 sub-stats (`Public/Character/StatConstants.h:17-25`; `CharacterData.h:326-330`).
- **Per point** = (cap - base) / 138.57, where 138.57 is 93 points x 1.49 (`CombatConstants.h:135-146`). Everything is **linear in points x pillar multiplier until a hard cap**:

| Stat | Formula | Range | Where |
|---|---|---|---|
| Max HP | 100 + mult x pts x 6.49 | 100 → 1000 cap (gear +3/pt beyond) | `CharacterData.h:640-646`; `CombatConstants.h:224-230` |
| Max EP | 50 + mult x pts x 6.85 | 50 → 1000 | `CharacterData.h:649-655`; `CombatConstants.h:274-280` |
| Raw / Spell Damage | 1 + mult x pts x 0.0036 | x1.0 → x1.5 (stat), x2.0 composed | `CharacterData.h:658-663`; `CombatConstants.h:139-142, :76-77` |
| Defense | mult x pts x 0.0036, as % reduction | 0 → 50% (stat), 85% with gear/stone/buffs | `CombatConstants.h:203-209`; `DamageCalculator.cpp:273+` |
| Crit chance | 5% + Luck x 45% | 5% → 50% (stat), 100% with gear | `CombatConstants.h:174-175` |
| Crit damage | **x1.0 un-invested** → x1.5 (stat) → x2.5 (gear, D-034) | | `CombatConstants.h:186-193` |

### 1.3 What this means and what's missing

1. **No weapon Power.** D-101 is not built. All attack and ability damage comes from the skill's `BaseDamage` x the skill's tier.
2. **Stats move damage very little.** A realistic damage build (10-20 points in Raw Damage) gives x1.04-1.14. The cap is x1.5 even with all 93 points. A pillar level only multiplies the points already in that sub-stat. Tier (x4.8) dwarfs it, so stats give roughly **5-10%** of damage and gear/skill tier gives **90%+**.
3. **The requirement ladder is the biggest stat lever.** One level under a weapon's requirement costs 10%; five levels cost 68%. Because pillar levels reset to 0 each run, D+ gear will often sit under its requirement early in a run. That pulls against pillar 6.
4. **No enemy HP lever.** Enemies use the same HP formula (100 → 1000), and nothing in the code multiplies boss or elite HP. A grep for `HealthMultiplier`, `HPMultiplier` and `BossHealth` finds nothing. `Resources_Design.md:371` says "enemy-threat rides TIER_POWER", but the code has nothing that does it.
5. **Authored skill damage can't be read.** `BaseDamage` lives in LFS `.uasset` files, and the specs use local test constants, so this proposal can't see today's real hit sizes. Defaulting Power to 0 (§2.2) keeps today's numbers unchanged until Crown authors Power.
6. Smaller items:
   - The per-point maths (`STAT_DERIVE_DENOM`) still assumes the old level-7 maximum, while live levels go to 20 (`CombatConstants.h:135-137`). Caps clamp it, but caps are reached about 1.6x sooner than designed.
   - Un-invested crit does nothing extra.
   - Defense with gear reaches 85%, which is 6.7x effective HP.

---

## 2. Proposed baseline

### 2.1 What other games do, and what players liked

| Game | How a weapon hit is built | What players liked | Source |
|---|---|---|---|
| Elden Ring / Dark Souls | AR = weapon base + base x scaling% x stat curve. Grades are letters. Stat curves have **soft caps** (Str/Dex about 20 / 55 / 80). Normal weapons upgrade to +25, special ones to +10, and upgrades raise both base and scaling. Missing a requirement **drops that stat's scaling and applies about -40%**. | Readable letters. Big early gains, diminishing later, so hybrid builds are viable. One flat penalty for missing requirements. | [Scaling (wiki.gg)](https://eldenring.wiki.gg/wiki/Weapon_Scaling), [soft caps (Prima)](https://primagames.com/news/soft-caps-for-each-stat-and-attribute-in-elden-ring), [upgrades (GameSpot)](https://www.gamespot.com/articles/elden-ring-how-to-upgrade-weapons/1100-6501026/) |
| Expedition 33 | Each weapon has a base Power that grows with weapon level (1 to 33, through 5 catalyst tiers). Attributes add Power by letter grade. The final catalyst roughly doubles base power. Community advice: early on stats lead, later the scaling matters. No official formula is published (community estimate: C ≈ +1 damage per 6 points). | Clear "Power" number. Upgrades feel big. Letters steer builds. | [catalyst tiers (Beebom)](https://beebom.com/how-to-upgrade-weapons-in-clair-obscur-expedition-33/amp/), [scaling explainer (Deltia's)](https://deltiasgaming.com/?p=216376), [Steam thread](https://steamcommunity.com/app/1903340/discussions/0/597394861184787013) |
| Monster Hunter | Damage = weapon raw x motion value x hitzone … Affinity is crit chance; a crit is **x1.25 base**, x1.40 with Critical Boost. | Weapon is king; skills multiply it. Crits always mean something. | [Fextralife (MHW)](https://monsterhunterworld.wiki.fextralife.com:443/Attack+Power), [Steam (crit 25%/40%)](https://steamcommunity.com/app/582010/discussions/0/1735462352505278308) |
| Persona | √(power x attack) / √(defense); doubling Strength gives about x1.4 (community-reconstructed) | Stats matter but never explode | [Damage formula notes](https://lively-kernel.org/lively4/gs/damage-formula.md) |
| Dragon Quest / Octopath | Subtraction (ATK/2 - DEF/4 style; Octopath similar, crit x1.25) | Simple, but weak hits on tough foes round to 1 | [DQ calc page](https://west-games.com/dragon-quest-damage-calculator/), [Octopath stats](https://progameguides.com/octopath-traveler-2/all-stats-in-octopath-traveler-2-and-what-they-mean-explained/) |
| League of Legends (defense reference) | Ratio: dmg x 100/(100 + armor); each point adds the same 1% effective HP | No stacking to immunity; every point is equally worth it | [LoL wiki: Armor](https://wiki.leagueoflegends.com/en-us/Armor) |
| **Deckbuilder (this game's simplest level)** | Weapon adds a flat **Amp** to the ability's printed damage. Iron Sword I..IV gives +1 / +2 / +2 / +3 on abilities of "Deal 3-4 damage". Characters have 100 HP. | — | `wor-deckbuilder/docs/design/Foundation_Card_Faces.md` §Weapons |

Takeaways:
- Grades as letters with soft-capped curves (ER, E33).
- Upgrades lift the weapon's base (E33, ER).
- One flat-ish penalty for missing a requirement (ER, already Q-162).
- No subtraction defense across a 1x..5x power spread.
- Crit should mean something un-invested (MH and Octopath both use x1.25).
- On battle length the web gives only anecdote: one designer forum suggests 20-30 turns for bosses ([Penny Arcade](https://forums.penny-arcade.com/discussion/145826/combat-in-rpgs)), and others say common fights must stay short ([Siliconera](https://www.siliconera.com/makes-good-common-encounters-rpgs/)).

### 2.2 The proposed formula (weapon-delivered attacks and abilities)

```
WeaponPart = Power(weapon instance tier) x (1 + Σ grade_coef(pillar) x Curve(live pillar level)) x WeaponReqFactor
SkillPart  = SkillBase x TIER_POWER(skill tier) x TierGap(skill vs weapon) x charge band
Base       = SkillPart + WeaponPart
Damage     = WoRDamageMath::Compute(Base, ...)   // RawDamage stat, crit, element, Defense: unchanged
```

- **Additive, the deckbuilder's rule.** The weapon's Power adds to the ability's base, like the deckbuilder's Amp. The deckbuilder comes first where it has a rule (CLAUDE.md).
  - It also means **Power defaults to 0, so today's fights are byte-identical** until Crown authors Power, which is D-101's "test fights stay close".
  - A basic weapon attack authors `SkillBase = 0`, so it hits for exactly WeaponPart ("starts from Power").
  - Additive keeps tiers linear: weapon tier and skill tier don't square each other.
- **Alternative (decision 1):** Monster Hunter / E33 style, where `Base = Power x skill motion value %`. The weapon matters more, but every existing `BaseDamage` would need re-authoring as a percent.
- **Requirement factor on WeaponPart only.** That is the reading of D-082's note ("the weapon's own contribution"). Today it scales the whole hit because the weapon has no term of its own (Q-162 note). Decision 4.
- Spells are unchanged: their own `StatScaling` and no weapon Power (D-101).

### 2.3 Stat ranges (pillar levels)

| | Start of run | Early run | Mid run | Late run | Hard cap |
|---|---|---|---|---|---|
| Main pillar level | 0-4 (head-start) | 2-4 | 6-10 | 12-16 | 20 (`LIVE_MAX`) |
| Pillar multiplier | 1.00-1.28 | 1.14-1.28 | 1.42-1.70 | 1.84-2.12 | 2.40 |

Keep these as they are. Only the curve below is new.

### 2.4 Pillar curve (new, with a soft cap)

`Curve(L) = 1 - (1 - L/20)^2`. It is ER-like: fast early, diminishing past about 10, and full at the live cap of 20. Bake it as constant keys 0..20 in a curve table.

| Level | 0 | 2 | 4 | 5 | 6 | 8 | 10 | 12 | 14 | 16 | 20 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Curve | 0 | 0.19 | 0.36 | 0.44 | 0.51 | 0.64 | 0.75 | 0.84 | 0.91 | 0.96 | 1.00 |

Half the value comes by about level 6 and 90% by level 14. Spreading points over two pillars therefore stays worthwhile, which supports pillar 2 (build variety).

### 2.5 Scaling grades: keep the ladder

Keep **S 1.0 / A 0.8 / B 0.65 / C 0.5 / D 0.35 / E 0.2 / F 0.05** (`EScalingTier.h:36-42`); it now multiplies Power.
- At the cap, an S weapon doubles its Power (1 + 1.0), C gives x1.5 and F x1.05.
- That hits `Resources_Design.md:813`'s lean of "~60-70% self-tier / 30-40% world-stat" for B-D grades, and lets S reach about 50/50 late.
- ER's S is stronger (about 175%+ of base, per community datamines), but ER also has 99 stat levels. Ours is a 30-45 minute run.

### 2.6 Weapon Power by tier (= upgrade level)

Power = authored Power x TIER_POWER(instance tier) / TIER_POWER(asset tier), so a levelled copy grows on the existing spine. Anchor: **a plain F weapon has Power 40**.

| Tier (upgrade step) | F | E | D | C | B | A | S |
|---|---|---|---|---|---|---|---|
| TIER_POWER | 1.00 | 1.30 | 1.70 | 2.20 | 2.85 | 3.70 | 4.80 |
| Reference Power | 40 | 52 | 68 | 88 | 114 | 148 | 192 |
| Proposed main-pillar requirement | 0 | 1 | 2 | 4 | 6 | 9 | 12 |

- Per weapon, ±15% around the reference row, e.g. Hammer 46 / Dagger 34 at F. The deckbuilder's Hammer is likewise +2 where most weapons are +1.
- The requirements are lower than the asset's 0..7 reach implies on purpose. Levels reset every run, so each step should be reachable mid-run.

### 2.7 Requirement-penalty ladder sanity

- Keep the shared ladder: −10% at 1 under, −22% at 2, −36% at 3, −50% at 4, −68% at 5+.
- **Raise the weapon floor from 0.25 to 0.5** (decision 4). That is close to ER's flat −40%.
  - Today the floor never bites: the ladder bottoms out at 0.32.
  - At 0.32 an S weapon 5+ levels under is barely better than a matched C weapon (worked out in §3).
  - At 0.5 it is a real step up, still well below a levelled user's.

### 2.8 Defense

- **Keep percent reduction**: no subtraction, which would round weak hits to 1 across a x1..x5 power spread. Keep the 50% stat cap.
- **Flag the 85% gear ceiling.** Effective HP is 1/(1-d), so 85% is 6.7x and every extra point near the top is worth more. With Guard on top, 7.5% gets through.
- Recommendation: lower the ceiling to 75% (4x).
- Option for B-042, when Defense becomes an MMC: rebuild Defense as a rating with `reduction = DEF/(DEF+K)` (LoL's constant-EHP-per-point ratio).
- Decision 7.

### 2.9 Enemy HP targets (normal 3-5 party turns, boss 12-20)

**Proposal:** enemy Max HP = stat formula (unchanged) x TIER_POWER(enemy tier) x rank multiplier. The tier factor is `Resources_Design.md:371`'s "enemy threat rides TIER_POWER".

| Rank | Multiplier | Target |
|---|---|---|
| Normal | x1 | about 3-5 same-tier hits each. A group of 2-3 dies in about 3-5 party rounds (3 actors, about 70% of actions dealing damage). |
| Elite | x1.5 | about 5-7 hits |
| Mini-boss | x3 | about 10-14 hits (6-8 rounds) |
| Boss | x6 | about 25-35 damaging hits, i.e. 12-16 rounds. **Aim for the low end:** at about 40 s per round (3 party actions plus the boss's, with defense windows) 16 rounds is already about 11 minutes, a third of a 30-45 minute run. |

Reference enemies (examples 1-3 below):

| | Early (F) | Mid (C) | Late (S) |
|---|---|---|---|
| Normal enemy HP | 149 | 606 | 2,106 |
| Boss HP | about 900 (no tier) to 1,400 | about 3,600 | about 12,600 |

Run budget at these numbers:
- Normal fight: about 4 rounds, about 2-3 minutes.
- About 8 normal fights + 2 mini-bosses + 1 boss ≈ 20 + 10 + 9 ≈ 39 minutes, which fits pillar 5.

---

## 3. Worked examples (all arithmetic shown; skill requirement met exactly, no crit)

Constants used:
- per point = 0.5 / 138.57 = 0.003608
- HP per point = 6.4949
- mult(L) = 1 + 0.07L

**Example 1: early Generic, F sword**

Setup:
- Character: Body 2, 6 points in Raw Damage.
- Weapon: F sword, Power 40, Body C, requirement Body 0, so the factor is 1.
- Enemy: early F normal, Body 1, 4 Defense points, 7 HP points.

Arithmetic:
- WeaponPart = 40 x (1 + 0.5 x 0.19) = 40 x 1.095 = 43.8 → **44**
- Raw Damage stat = 1 + 1.14 x 6 x 0.003608 = **1.0247**
- Enemy Defense = 1.07 x 4 x 0.003608 = **0.0154**
- Basic attack: 44 x 1.0247 x 0.9846 = **44**
- F ability with SkillBase 20: (20 + 44) x 1.0247 x 0.9846 = **65**
- Enemy HP = 100 + 1.07 x 7 x 6.4949 = **149**, so 2.3-3.4 hits ✔

Shares: Power 40 of 44 (91%), stats 4 (9%). Early on, gear carries you (as `Resources_Design.md:813` intends).

**Example 2: mid Resonator, C weapon**

Setup:
- Character: Mind 8, Spirit 5, Body 3, 8 points in Raw Damage.
- Weapon: C staff, Power 88, Mind B and Spirit D, requirement Mind 4 (met).
- Skill: C ability with SkillBase 30, matched tier.
- Enemy: C-tier, Body 5, 15 Defense points, 20 HP points.

Arithmetic:
- WeaponPart = 88 x (1 + 0.65 x 0.64 + 0.35 x 0.4375) = 88 x 1.5691 = **138**
- SkillPart = 30 x 2.2 x 1.0 = **66**
- Raw Damage stat = 1 + 1.21 x 8 x 0.003608 = **1.0349**
- Enemy Defense = 1.35 x 15 x 0.003608 = **0.0731**
- Ability: (66 + 138) x 1.0349 x 0.9269 = **196**
- Basic attack: 138 x 1.0349 x 0.9269 = **132**
- Enemy HP = (100 + 1.35 x 20 x 6.4949) x 2.2 = 275.4 x 2.2 = **606**, so 3.1-4.6 hits ✔

Shares in WeaponPart: Power 88 (64%), scaling 50 (36%), the 60/40 target.

**Example 3: late build, S weapon**

Setup:
- Character: Body 14, Spirit 6, 20 points in Raw Damage.
- Weapon: S greatsword, Power 192, Body S and Spirit E, requirement Body 12 (met).
- Skill: S ability with SkillBase 40.
- Enemy: S-tier, Body 7 (asset maximum), 25 Defense points, 35 HP points.

Arithmetic:
- WeaponPart = 192 x (1 + 1.0 x 0.91 + 0.2 x 0.51) = 192 x 2.012 = **386**
- SkillPart = 40 x 4.8 = **192**
- Raw Damage stat = 1 + 1.98 x 20 x 0.003608 = **1.1429**
- Enemy Defense = 1.49 x 25 x 0.003608 = **0.1344**
- Ability: (192 + 386) x 1.1429 x 0.8656 = **572**
- Basic attack: 386 x 1.1429 x 0.8656 = **382**
- Enemy HP = (100 + 1.49 x 35 x 6.4949) x 4.8 = 438.7 x 4.8 = **2,106**, so 3.7-5.5 hits ✔
- Crit expected value with 30% chance at x1.5 is +15%.

Shares in WeaponPart: Power 192, scaling 194, about 50/50. That is the late-game build payoff.

---

## 4. Pillar check (pillar 6: progression never locks you out)

Basic-attack WeaponPart, before Raw Damage and Defense.

| Character | Weapon | WeaponPart |
|---|---|---|
| Low level (Body 4, Spirit 2) | S, requirement 12, gap 8 → ladder 0.32 | 192 x (1 + 0.36 + 0.2 x 0.19) = 268 x 0.32 = **86** |
| same | same, with **proposed floor 0.5** | 268 x 0.5 = **134** |
| same | C, requirement 4 (met), Body C | 88 x (1 + 0.5 x 0.36) = **104** |
| same | D (met) | 68 x 1.18 = **80** |
| High level (Body 14) | F, Body C | 40 x (1 + 0.5 x 0.91) = **58** |
| High level (Body 14) | S, matched | **386** |

What the numbers show:
- **Good gear gives a low level a real chance.** With the 0.5 floor, a low-level character with an S weapon (134) beats a high-level character with poor F gear (58) by 2.3x, and beats their own matched C weapon by 29%.
- The reverse also holds: high level with poor gear can be caught out.
- At today's 0.32 the S weapon (86) still beats the high-level F user, but it is **worse than a matched C (104)**. That is a soft lock-out on the best gear early in every run, because levels reset.
- **Verdict:** pillar 6 holds with the proposal if the floor rises to about 0.5 and requirements stay low (§2.6). Stones, crystals, evolutions and +pillar gear (which raise the live level) close the rest of the gap.

---

## 5. What goes into `UCombatTuningData` / curve tables

Keep the existing pattern: constant-key curves, C++ fallback, `PrintTuning` / `CompareWithCode` (`Public/Combat/Tuning/CombatTuningData.h:27-118`).

| Add | Kind | Keys / default | Note |
|---|---|---|---|
| `PillarScalingCurve` | ladder (`FScalableFloat`) | levels 0..20 → §2.4 | new `ECombatLadder` member, appended |
| `ScalingGrade` | ladder (exists) | unchanged S..F | now also read for weapon grades |
| `TierPower` | ladder (exists) | unchanged | also drives Power upgrades and enemy tier HP |
| `RequirementGap` | ladder (exists) | unchanged | |
| `WeaponReqFactorMin` | single value | **0.5** proposed (code 0.25) | moves `WorldStatRequirements.h:108` into data |
| `EnemyRankHealthMultiplier` | 4 values | 1 / 1.5 / 3 / 6 | Normal / Elite / Mini-boss / Boss |
| `bEnemyHealthRidesTierPower` | bool | true | turns the §2.9 tier factor on or off |
| `ReferenceWeaponPowerF` | single value | 40 | debug and validation only (flags weapons outside ±15%) |

On the weapon asset (per D-101):
- `Power` (int, at the asset's tier, default 0 = today's behaviour)
- `TArray<FPillarScaling>` with Mind/Body/Spirit grades. These are pillars, not `ESubStat`, so it is a new small struct or a pillar-keyed variant.

Debug: a `GetWeaponDamageString()` showing Power → x grades x curve → x requirement, plus `WoRDamageMath::GetInputsString` gaining the WeaponPart lines.

Later, at B-042 when the caps move into data (Q-218):
- Defense gear ceiling
- crit base
- HP / EP per point

## 6. Decisions for Crown (each a proposal; none is lore)

1. **How the weapon joins a hit.** Recommended: **additive**, `SkillBase + WeaponPart` (the deckbuilder's Amp; zero migration). Alternative: skill as a % of Power (MH / E33 motion values).
2. **Power anchor.** F = 40, growing x TIER_POWER to S = 192.
3. **Pillar curve.** Quadratic soft cap reaching 1.0 at level 20 (§2.4). Or another "full value" level?
4. **Requirement factor.** On the weapon's part only (recommended) or on the whole hit (today)? Raise the floor 0.25 → **0.5**?
5. **Requirements per tier.** F 0 / E 1 / D 2 / C 4 / B 6 / A 9 / S 12, main pillar.
6. **Enemy HP.** x TIER_POWER(enemy tier) x rank (1 / 1.5 / 3 / 6). Should boss fights aim at the low end (12-14 rounds) to fit 30-45 minutes?
7. **Defense gear ceiling.** 85% → 75%? Ratio formula at B-042?
8. **Un-invested crit.** x1.0 → x1.25, like MH and Octopath. D-034 only set the ceiling.
9. **Body counts twice for weapon users.** Through the weapon's Body grade and through the Raw Damage sub-stat. That is acceptable (ER: Strength scaling plus physical-damage buffs), but it should be a conscious choice.

*Every number above is a starting point marked `// TODO: tune` until Crown's PIE pass.*
