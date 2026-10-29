# Rules-to-Tests Traceability (2026-10-29)

**Status:** proposal; findings turned into Phase 1e jobs B-281..B-290. 274 rules: 150 pinned by a spec (55%), 58 touched but not asserted, 4 PIE-only, 46 nothing, 19 decided-not-built.

---


Repo `world_of_refraction`, branch `dev` at `a51c818` (2026-10-29). Sources read: `docs/Rules/00..06`, every `Private/Tests/*Spec.cpp` (124 specs, 743 `It` blocks), `docs/PIE/B-*.md` (164 files).

**Classes.** OK = a spec pins the rule. OK\* = the spec asserts against the *named constant* (e.g. `STAT_MULT_CAP`), so it catches a broken code path but not an edited number. C = a spec touches the path but doesn't assert the rule's number, order or threshold. B = PIE check only. A = nothing. NB = "Decided, not built": nothing to pin yet (excluded from gap counts).

Spec files live in `Source/world_of_refraction/Private/Tests/`. PIE files are cited by job (`B-xxx` = `docs/PIE/B-xxx.md`); a rule's PIE coverage is the checklist of the job named in its status/source.

## 1. Table: rule → status → spec → PIE → class

Rule text is shortened to its subject; the page holds the full wording. Runs of rules that share a verdict are collapsed.

### 01 Classes and loadouts (41 rules)

| Rule | Subject | Status | Spec that pins it | PIE | Class |
|---|---|---|---|---|---|
| CL-01 | three classes | In game | LoadoutGetterSpec "matches the owner's CharacterData for every class"; GameNamesSpec | — | OK |
| CL-02 | slots make the class, no gate helper | In game | EvolutionSecondarySpec (weapon abilities through an evolution primary) | B-099 | B |
| CL-03 | Generic: primary + secondary weapon | In game | EvolutionSecondarySpec "keeps the secondary weapon…" | B-099 | C (saved-loadout slot shape not asserted) |
| CL-04 | Refractor: primary + innate pool, innate element | In game | TrialRecruitSpec "keeps a Caster recruit's spells…" | B-076 | C |
| CL-05 | Resonator: never a ring primary | In game | TrialRecruitSpec "puts a Resonator recruit's ring in its ring loadout" | B-076 | C (refusal of a ring primary not asserted) |
| CL-06 | Resonator EP fills only with a spend target | Kept | — | — | **A** |
| CL-07 | gearless Generic/Resonator sits out | In game | PartyIntoBattleSpec "keeps a gearless Generic out… lets a Refractor in"; DismantleEquippedSpec | B-071, B-084 | OK |
| CL-08 | Reality casts any element; BD only its pools | In game | — (BrokenSourceSpec covers broken sources only) | — | **A** |
| CL-09 | one shared pool | In game (B-091) | SharedGearSpec (10 tests) | B-091 | OK |
| CL-10 | Resonator budget ≤5, slot costs 1/2/3/2/1/2 | In game | — (`LOADOUT_TOTAL_BUDGET` unread by any spec) | — | **A** |
| CL-11 | no ring-count cap | In game | — | — | **A** |
| CL-12 | budget is Resonator-only | In game | — | — | **A** |
| CL-13 | 6 item slots × 3 | In game | ItemSlotsSpec; LoadoutPicksSpec (`MAX_QUANTITY_PER_ITEM_SLOT`) | B-203 | OK |
| CL-14 | buy loadout at run start | NB (B-116) | — | — | NB |
| CL-15 | max 5 evolutions owned | In game | — | — | **A** |
| CL-16 | Attack = active weapon's; none without | In game | RingWeaponQuerySpec "weapon queries match the loadout's active weapon and attack" | — | C ("no weapon → no Attack" not asserted) |
| CL-17 | which weapon is active, per class | In game | RingWeaponQuerySpec; WeaponMeshFollowSpec "follows a Generic's weapon toggle" | B-138 | C (Refractor/Resonator with ring primary = none, not asserted) |
| CL-18 | Switch Weapon: free, Generic, not stunned | In game | TurnSwitchSpec; StunRuleSpec "refuses… weapon switch" | B-175 | OK |
| CL-19 | swap moves stats and effects | In game (B-081) | GearSwapEffectsSpec | B-081 | OK |
| CL-20 | ability slots by tier F1…S6 | In game | LoadoutPicksSpec "trims abilities over the weapon's cap" | B-159 | C (`ATTACHMENT_SLOTS` ladder not asserted) |
| CL-21 | weapons never give spells | NB (B-098) | — | — | NB |
| CL-22 | evolution primary keeps secondary weapon | partly built | EvolutionSecondarySpec | B-099 | OK |
| CL-23 | wield mode is visual only | Kept | — | — | A (low risk) |
| CL-24 | ring element = crystal's; broken = none | In game | RingWeaponQuerySpec "…the active ring's element"; BrokenSourceSpec | B-163 | C (crystal-less ring = None not asserted) |
| CL-25 | ring spell count by holder/crystal/evolution tier | In game | LoadoutRulesSpec "spell cap (B-196)", "(Q-147 = B)" | B-196 | OK |
| CL-26 | Switch Ring, cycles, needs 2+ | In game | RingWeaponQuerySpec "switching rings"; TurnSwitchSpec | B-175 | OK |
| CL-27 | ring spells cost no EP, wear the crystal | In game | InfusionCostSpec "charges no EP when a crystal powers the cast" | — | C (wear per cast not asserted) |
| CL-28 | ring accepts crystal/evolution/elemental fusion only | In game | GearAttachSpec "refuses a stone on a ring", "augmented pair… refuses it on a ring" | — | OK |
| CL-29 | ring fusion's stone half counts | In game (B-082) | RingFusionStoneSpec | B-082 | OK |
| CL-30 | one attachment slot | In game | GearAttachSpec "refuses an occupied slot…" | — | OK |
| CL-31 | stones on weapons only; Ability Stone slots 1…6 | In game | GearAttachSpec | — | C (slot count by tier not asserted) |
| CL-32 | which pairs fuse | In game | GearAttachSpec "refuses an invalid pair" | — | C (the pair table isn't enumerated) |
| CL-33 | evolution primary: spells, pillars, destroy-for-essence, 10% wear on removal | In game | GearAttachSpec "returns a removed evolution… with 10% wear", "(Q-159)"; OwnedGearSyncSpec; BrokenSourceSpec | B-171 | OK |
| CL-34 | innate budget 24, cost = tier, 6/school, discount at Mind 4/Body 7/Spirit 5 | In game | LoadoutRulesSpec "weighs spells with the owning character's authored discount (D-080)" | B-161 | C (24, 6, thresholds not asserted) |
| CL-35 | BD budget 48 | In game | LoadoutRulesSpec "uses BD rules when the character is BD" | — | C (48 not asserted) |
| CL-36 | Refractor spell element check | In game | — (BrokenSourceSpec Q-163b touches it for F-tier only) | — | **A** |
| CL-37 | pillars 0–7, 3 pts/level, live cap 20 | In game | LiveWorldLevelsSpec; PillarSourcesSpec | B-217 | C (cap 20 not asserted) |
| CL-38 | requirements never block | In game | LiveWorldLevelsSpec | B-217, B-147 | C ("usable when under" not asserted) |
| CL-39 | requirement-gap ladder ±5, pillar→substat map | In game | TierGapScalingSpec "Requirement gap" | — | C (ladder asserted against constants only; the pillar→substat map not at all) |
| CL-40 | weapon requirement factor, floor 0.25 | In game (B-162) | WeaponRequirementSpec | B-162 | OK |
| CL-41 | bound gear | In game | BoundGearSpec | B-192 | OK |

### 02 Elements and damage (43 rows; **ED-42 is used twice**, see §4)

| Rule | Subject | Status | Spec | PIE | Class |
|---|---|---|---|---|---|
| ED-01, ED-02 | nine elements; Generic/None | In game | WoRTagsSpec; InfusedAbilityElementSpec "never Generic" | — | OK |
| ED-42 (names) | Reality shows as Unknown | In game | GameNamesSpec "shows Reality as Unknown (D-095)" | B-126, B-207 | OK |
| ED-03 | no weakness wheel; resistance is the element step | In game | ElementDamageSpec | B-064 | OK |
| ED-04 | Ice | NB (B-064a) | BuildupExecutionSpec (B-226 half only) | B-226 | NB |
| ED-05, ED-06 | physical vs infused element | In game (B-080) | InfusedAbilityElementSpec | B-080 | OK |
| ED-07 | Generic spell takes its slot's element | In game | BrokenSourceSpec (Q-163b, F-tier only) | — | C (ring/weapon/evolution/BD pool resolution not asserted) |
| ED-08 | Slash/Pierce/Impact type | In game | ResistanceSeamSpec | B-216 | C |
| ED-09 | base damage, split across impacts | In game | SmallFixesSpec (DoubleHit) | — | C (split table not asserted) |
| ED-10 | charge multiplier incl. stat surcharge | In game | AIDamageEstimateSpec "infused ability at what the executor's hit deals" | B-029 | C (equality to the AI, not the band numbers) |
| ED-11 | tier-gap ladder | In game | TierGapScalingSpec "Tier-gap damage" | — | C (constants, not numbers; CombatTuningSpec pins the asset = C++) |
| ED-12 | tier power F1.0…S4.8 | In game | CombatTuningSpec "starts equal to the C++ constants" | B-218 | C |
| ED-13 | attacker stat 1.0→1.5, gear to 2.0 | In game | DamageCalculatorSpec "caps… at STAT_MULT_CAP"; StatSourcesSpec | — | OK\* |
| ED-14 | requirement gap per pillar | In game | TierGapScalingSpec | — | OK\* (pillar→stat map: see CL-39) |
| ED-15 | skill stat scaling grades S1.0…F0.05 | In game | — | — | **A** |
| ED-16, ED-21 | attacker/defender row ×1.05/0.95 | In game | CombatGridSpec "sets damage and defense by row"; RowMoveSpec | B-180 | OK\* |
| ED-17 | buff/debuff multipliers; Damage Taken Down cap 90% | In game | StatModifierSpec (stat pairs) | B-020 | C (the 90% cap not asserted) |
| ED-18 | crit chance 5% + 45% Luck | In game | DerivedStatsSpec | B-042s | OK\* |
| ED-19 | crit dmg ×1.0 base, 1.5 stat, 2.5 gear | In game (D-034) | DamageCalculatorSpec "Critical hits"; StatSourcesSpec "floors crit damage" | B-147 | C (`CRIT_DAMAGE_GEAR_CEILING` 2.5 unread by any spec) |
| ED-20 | Defense 50%/85%; Ignore Defense amount | In game (B-042m) | DamageCalculatorSpec "Defense"; IgnoreDefenseSpec | B-042m | OK |
| ED-22 | min damage 1 | In game | DamageMathSpec | B-219 | OK |
| ED-23 | Reality boosts +10/+5/+2.5/+5% | In game | — | — | **A** |
| ED-24 | resistance row choice order; data asset | In game | ResistanceSeamSpec "starts a new profile data asset from the C++ table" | B-216 | C (row pick order not asserted) |
| ED-25 | row values (±15/10/5) | In game | ResistanceSeamSpec "for a Caster of every innate element" (before/after equality) | B-216 | C |
| ED-26 | buildup resistance composition | In game | StatusBuildupSpec; BuildupExecutionSpec; ResistanceSeamSpec | B-216 | OK |
| ED-27 | element → damage 1:1, ±50%, DoT ticks too | In game | ElementDamageSpec "caps resistance at +-50%", "(B-064b, Q-064)" | B-064, B-064b | OK |
| ED-28 | charge bands 1.15/1.30, 1.10/1.20, 1.125/1.25 | In game | InfusedBuildupSpec (buildup side) | B-113 | C (band numbers not asserted) |
| ED-29 | spell size ×1.5/×2.0 | In game | — | — | **A** |
| ED-30 | EP cost at L1/L2 and the ability ×1.5 | In game (D-035) | InfusionCostSpec "pins D-035's numbers: x2.25… x3.0" | B-136, B-147 | OK |
| ED-31 | free sources | In game | InfusionCostSpec "charges no EP when a crystal powers the cast" | B-136 | OK |
| ED-32 | HP backlash formula | In game | InfusionCostSpec "turns infused EP into HP by share of max EP, less Resistance" | B-136 | OK ("can kill" not asserted) |
| ED-33 | Guard 50% / Perfect 0 / Dodge 0; no on-hit on Perfect | In game | GuardSpec "damage", "treats a Parry as a Guard and never reflects" | B-168 | OK |
| ED-34 | buildup through Guard 50% / Perfect 0% | In game | GuardSpec "lets less status through a Perfect Guard" | B-168 | OK\* |
| ED-35, ED-36, ED-37 | window 0.5 s, grace 0.15 s, difficulty bands, Perfect 0.1 s | In game | DefenseMatchSpec (7 tests); DefenseClockSpec | B-009, B-168 | OK\* |
| ED-38 | legacy per-spell defense list | In game | — | — | A (low risk, dead code) |
| ED-42b (luck dodge) | Luck dodge max 15% | In game | LuckDodgeSpec "is capped at 15%" | B-169, B-198 | OK |
| ED-39 | BD resistance row | In game | — | — | **A** |
| ED-40 | BD absorbs EP on Guard 5%/10%, ×2 Perfect, overload 30% | In game | — | — | **A** |
| ED-41 | BD forbidden Light/Void cost 25% HP + buildup | In game | — | — | **A** |

### 03 Status bars (36 rules)

| Rule | Subject | Status | Spec | PIE | Class |
|---|---|---|---|---|---|
| SB-01 | one bar, fills at 100 | In game | StatusBuildupSpec "fires the element's status at the threshold" | B-135 | OK\* |
| SB-02, SB-03 | what adds buildup; multi-hit split | In game | InfusedBuildupSpec; AIBuildupEstimateSpec "multi-hit… equals the buildup the live hit adds" | B-113, B-114 | OK |
| SB-04 | Guard 50% / Perfect 0 / Dodge 0 | In game | GuardSpec | B-168 | OK\* |
| SB-05 | Status Multiplier, element-keyed buffs | In game | StatModifierSpec; BuildupExecutionSpec "element-keyed multiplier" | B-042l | OK |
| SB-06 | BD stacks ×1/1/2/4; forbidden self-buildup | In game | — | — | **A** |
| SB-07 | last hit decides | In game | StatusBuildupSpec | — | C (two-element sequence not asserted) |
| SB-08, SB-09 | element over physical; no type = bar held | In game | — | — | **A** |
| SB-10..SB-13 | resistance, layers, clamp −1…+1, immunities | In game | StatusBuildupSpec; ResistanceSeamSpec; BuildupExecutionSpec (B-226); ResistanceSourcesSpec | B-216, B-226 | OK (SB-12 OK\*) |
| SB-14, SB-15 | 25% decay / full reset at 3 turns | In game | StatusBuildupSpec "decays each quiet turn and resets after the full-reset count" | — | OK\* |
| SB-16, SB-17, SB-18 | empties on fire; Quartz/flat drain; reset per fight | In game | StatusBuildupSpec; CombatResetSpec | B-030 | OK |
| SB-19 | Burn/Bleed 8% max HP, once | In game | StatusBreakSpec "Burns exactly once, at that turn's start" | B-063 | C (8% not asserted; it is a literal at `SkillEffectManager.cpp:2914`) |
| SB-20 | Earth/Pierce defence −30% | In game | StatusBreakSpec (Earth DefenseDebuff held one turn) | B-063 | C (30% not asserted) |
| SB-21 | Lightning/Impact Stun | In game | StatusBreakSpec "of Lightning holds its Stun…" | B-063 | OK |
| SB-22 | Water slows speed 30% | In game | StatusBreakSpec "of Water slows…" | B-063 | C (size not asserted) |
| SB-23 | Wind: one place later, once per attacker | In game | StatusBreakSpec "of Wind pushes… once per attacker" | B-063 | OK |
| SB-24 | Light: crit down | In game | StatusBreakSpec "of Light… (B-112)" | B-112 | OK |
| SB-25 | Silenced seals 50% EP | In game | SilenceSealSpec (5 tests) | B-148 | OK |
| SB-26 | BD Darkness drains 25% EP | In game | StatusBreakSpec "of Broken Darkness drains EP…" | B-063 | OK\* |
| SB-27 | Void random action | In game | VoidRandomActionSpec | B-199 | OK |
| SB-28 | Reality: 25% max HP instant, can kill | In game | — | — | **A** |
| SB-29 | Ice | NB | — | — | NB |
| SB-30 | same attacker refreshes | In game | EffectIdentitySpec "Full bars (D-012)" | B-062 | OK |
| SB-31, SB-32 | 1 holder turn by `FTurnSpan` | In game | StatusBreakSpec "The holder-turn counter"; TurnEffectTrackerSpec | B-063, B-225 | OK |
| SB-33, SB-34 | stun: Attack or Defend; no infusion | In game | StunRuleSpec | B-061, B-092 | OK |
| SB-35 | stunned menu; AI picks attack/defend | In game | CombatHangFixesSpec "asks a stunned actor… only lets it Attack or Defend" | B-092 | OK |
| SB-36 | an armed ritual still fires while stunned | In game | CombatRitualFireSpec | B-031 | C (stun not applied in the test) |

### 04 Effects and turns (55 rules)

| Rule | Subject | Status | Spec | PIE | Class |
|---|---|---|---|---|---|
| ET-01 | one per side; team 0/1 | In game | EncounterDataSpec | B-074 | C |
| ET-02 | fight start: once-record wiped, gear effects applied | In game | EffectsBaselineSpec "only ResetForNewCombat resets the once-per-match record"; GearSwapEffectsSpec | B-081 | OK |
| ET-03 | first strike | In game | FirstStrikeSpec | B-185 | OK |
| ET-04 | turn speed base 10, cap 15, gear 20 | In game | TurnManagerSpec; TurnSpeedSourcesSpec | B-042v | C (10/15/20 not asserted) |
| ET-05 | effective speed formula, floor 1 | In game | TurnSpeedSourcesSpec "multiplies an attached stone and a buff in", "floors at 1" | B-042v | OK |
| ET-06 | debt scheduler | In game | TurnManagerSpec "lets a faster combatant act first and more often" | — | OK |
| ET-07 | ratio uncapped (Conflict, D-049 OPEN) | Conflict | — | — | **A** |
| ET-08 | tie-break chain (8 steps) | In game | TurnManagerSpec "first turn to the local party on a full tie" | — | C (only the last step) |
| ET-09 | speed change recalculates at once | In game | — | — | **A** |
| ET-10 | belt 16 / UI 10 | In game | FirstStrikeSpec "the preview shows it" | — | C |
| ET-11 | Wind push (duplicate of SB-23, stale) | says NB | StatusBreakSpec (Wind) | B-063 | OK (row stale, §4) |
| ET-12, ET-13 | Emerald pinned turn, delays 6…0, S = two | In game | — | — | **A** |
| ET-14 | Emerald turn is borrowed (D-052 OPEN) | Conflict | — | — | **A** |
| ET-15 | Extra Action adds a turn | Code only | — | — | **A** |
| ET-16 | rituals: arm, Execution turn, ordering by spell speed | In game | CombatRitualFireSpec (5 tests) | B-031 | C (ordering of two ready rituals not asserted) |
| ET-17 | turn-start order of operations | In game | CombatRitualFireSpec (Q-044d); EffectsBaselineSpec "Re-entrancy" | B-225 | C (the order itself is not asserted) |
| ET-18 | one action per turn | Kept | CombatHangFixesSpec | B-001 | C |
| ET-19 | one free switch per turn | In game (B-175) | TurnSwitchSpec | B-175 | OK |
| ET-20 | Defend +50% for 1 turn (D-050 OPEN) | Conflict | CommandMenuKeysSpec "defends… and ends the turn"; StunRuleSpec | B-092, B-093 | C (50%, duration not asserted) |
| ET-21 | Flee: normal fights, 10% Gold | In game | FleeSpec "costs a share of the Gold, rounded down" | B-176 | OK |
| ET-22, ET-23 | stunned / silenced | In game | StunRuleSpec; SilenceSealSpec | B-061, B-148 | OK |
| ET-24 | refused action: player re-asked, AI passes | In game | CombatHangFixesSpec (2 tests) | B-001 | OK |
| ET-25 | EP cost formula, Efficiency 50%/90% | In game | InfusionCostSpec; DerivedStatsSpec (`EFFICIENCY_GEAR_CEILING`) | B-136 | OK\* |
| ET-26 | what costs no EP | In game | InfusionCostSpec (crystal, deferred fire) | B-136 | C (Defend, items, BD evolution casts not asserted) |
| ET-27 | HP cost of Raw/Innate/Evolution | In game | InfusionCostSpec | B-136 | OK |
| ET-28 | costs paid on commit | In game | ItemUseSpec (items only) | B-201 | C (EP not asserted) |
| ET-29 | EP never refills by itself | Code only | — | — | **A** |
| ET-30 | BD overload 30%, aura tick | In game | CarriedStateSpec "resumes… overload" | B-115 | C (aura/self-damage/leak not asserted) |
| ET-31, ET-32 | instant / N turns / permanent; timings | In game | EffectsBaselineSpec; EffectSpecBuilderSpec | B-134, B-225 | OK |
| ET-33, ET-34 | first execution; duration counting (D-051) | marked Conflict | EffectsBaselineSpec "Tick-down and expiry (D-051)", "Timing windows (Q-044c)" | B-225 | OK (rows stale, §4) |
| ET-35 | DoTs can kill | In game | GASPayoutsSpec "kills through the death path" | B-042n | OK |
| ET-36 | charges | In game | — | — | **A** |
| ET-37 | Last Stand 50%/30%, Sapphire window 2…5 | In game | — | — | **A** |
| ET-38 | Absorb; Reflect pure, capped at HP; DoubleHit skip | In game | HitPathSpec (Q-032 ×3); GuardSpec "reflect effects" | B-032c, B-168 | C (Absorb → EP not asserted) |
| ET-39..ET-44 | identity, stacks, summing, per-item, once-per-fight, full-bar identity | In game | EffectIdentitySpec (15); EffectsBaselineSpec "Stack"; EffectSpecBuilderSpec | B-062 | OK |
| ET-45 | cleanse scopes; Iolite N | In game | AIStatusQuerySpec; CleanseGearSpec | B-065 | C (N count / IOLITE_REMOVE_ALL not asserted) |
| ET-46 | gear effects immune to cleanse | In game | CleanseGearSpec; TagReadsSpec (D-015) | B-065 | OK |
| ET-47, ET-48 | plain Cleanse / auras do nothing | Code only | — | — | NB (B-066) |
| ET-49 | mid-fight swap | In game | GearSwapEffectsSpec | B-081 | OK |
| ET-50 | effect text tree | says NB | EffectTextSpec (19) | B-045 | OK (row stale) |
| ET-51 | win/lose/draw check at turn end | In game | CombatHangFixesSpec "normal win" | B-001 | C (Defeat, Draw not asserted) |
| ET-52 | win continues / lose ends run | NB (B-072) | — | — | NB |
| ET-53 | fight end clears effects, destroys broken, repairs | In game | CrystalBreakSpec; CombatResetSpec | B-035, B-030 | OK |
| ET-54, ET-55 | failed item spends charge; menu keys | In game | ItemUseSpec; CommandMenuKeysSpec | B-201, B-093 | OK |

### 05 Economy and runs (60 rules)

| Rule | Subject | Status | Spec | PIE | Class |
|---|---|---|---|---|---|
| ER-01, ER-02 | six wallets; all-or-nothing | In game | PlayerWalletSpec; EconomyRoundTripSpec "spends exactly the cart's price, or nothing when short" | B-151 | OK |
| ER-03, ER-04 | Gold run-only; Prisms/Diamond kept, one wallet | partly stale | SaveSpec "banks every persistent balance but never Gold", "carries the run's Gold… clears…"; PlayerWalletSpec | B-090, B-151 | OK |
| ER-05 | Gear/Skill essence, Reality half co-cost | In game | EconomyRoundTripSpec "Gear plus half in Reality" | B-137 | OK |
| ER-06..ER-09 | payouts | NB (B-073) | — | — | NB |
| ER-10 | trial = run | In game | TrialRunRefSpec; FieldEnemyTeamSpec | B-070 | C |
| ER-11, ER-12 | win/lose routing | Conflict (B-072) | FieldEnemyTeamSpec "marks every team of a won fight beaten" (ER-11 half) | B-184 | NB (row stale) |
| ER-13 | full HP/EP on entry; carry; hub clears | In game | CarriedStateSpec; FallenMembersSpec; SaveRunSpec | B-115 | C (full on entry, hub clear not asserted) |
| ER-14 | carry wear/absorption | In game | CarriedStateSpec | B-115 | OK |
| ER-15, ER-16, ER-21 | run-end loss list; buy loadout; Gold in run | NB | — | — | NB |
| ER-17 | level change rebuilds everything | Code only | SharedGearSpec "draws the same sword next level without banking it again" contradicts it | B-085 | NB (row stale) |
| ER-18 | merchant types; rolled stock of 8 | In game | MerchantStockRollSpec "shows TRIAL_VENDOR_STOCK_SIZE different lines" | B-179 | OK\* |
| ER-19 | merchant placement tags | Code only | — | — | A (low) |
| ER-20 | cart refusal reasons | In game | EconomyRoundTripSpec | B-137 | C (no-room, already-known refusals not asserted) |
| ER-22 | Prism prices by tier + attachment/skill add-ons | In game | EconomyRoundTripSpec "prices a cart as the sum of its Prisms prices" | B-137 | C (add-on pricing not asserted) |
| ER-23 | spell essence costs | In game | — | — | **A** |
| ER-24 | world-stat buy 50/+25, cap 20 | Kept | — | — | **A** |
| ER-25, ER-27 | upgrade costs; downgrade refund half | In game | EconomyRoundTripSpec "levels a weapon… refunds half the Gear on the way back", "refuses a level-up it can't fully pay" | B-137 | OK\* |
| ER-26 | upgrade anywhere, kept | Conflict | OwnedGearSyncSpec "shows a level-up in every loadout" | B-085 | C |
| ER-28 | merge with change | In game | CrystalMergeSpec | B-177 | OK |
| ER-29 | merge Prisms cost, max-count refusal | Kept | — | — | **A** |
| ER-30, ER-31 | dismantle yields | In game | EconomyRoundTripSpec "pays a weapon's dismantle by its levelled tier, and a crystal's by type and count"; EconomyPreambleSpec | B-137 | OK\* |
| ER-32, ER-33 | dismantle equipped | In game | DismantleEquippedSpec | B-084 | OK |
| ER-34 | max durability by tier; HUD 25% | In game | CrystalBreakSpec | B-035 | OK\* (HUD warning not asserted) |
| ER-35, ER-36 | break-destroy, +10 repair | In game | CrystalBreakSpec (13) | B-035 | OK |
| ER-37 | paid repair | In game | GearRepairSpec | B-178 | OK |
| ER-38 | drop quality weights, Luck tilt | In game | — | — | **A** |
| ER-39 | trial loot pools | Code only | — | — | NB |
| ER-40, ER-41, ER-42 | shared pool, presets, run recruits | say NB | SharedGearSpec; TrialRecruitSpec | B-091, B-076 | OK (rows stale) |
| ER-43 | join window 2 s; living members; dead party | In game | EncounterDataSpec | B-074 | C |
| ER-44..ER-46 | AI difficulty; teams of three; trial door | In game | PartyIntoBattleSpec; EncounterDataSpec; TrialGateSpec | B-071, B-074, B-183 | OK |
| ER-47..ER-50 | field teams, zones, map features, fragments | In game | FieldEnemyTeamSpec; EnemyZoneSpec; TrialMapFeatureSpec; MemoryFragmentSpec | B-184, B-186, B-188, B-190/B-210 | OK |
| ER-51..ER-60 | reactions, boss phases/triggers, memories, journal, hub NPCs | In game | ReactionTriggerSpec; BossPhaseSpec; BossTriggersSpec; FightTriggersSpec; MemoryConditionsSpec; MemorySourcesSpec; StoryJournalSpec; HubServicesSpec | B-191..B-195, B-209, B-211, B-212 | OK |

### 06 Party (39 rules)

| Rule | Subject | Status | Spec | PIE | Class |
|---|---|---|---|---|---|
| PA-01 | max 3 members, invite refused | In game | — | B-075 | B |
| PA-02 | party of one, leader slot 0 | In game | RecruitDataSpec | B-071 | C |
| PA-03 | recruit within 200 units | In game | — | B-075 | B |
| PA-04 | leader can't be dismissed | In game | TrialRecruitSpec "never lets one lead" | B-076 | C |
| PA-05 | no duplicates | In game | RecruitDataSpec "joins… once" | B-075 | OK |
| PA-06 | hub team building | partly built | TrialRecruitSpec | B-076 | OK |
| PA-07 | formation cell; swap on occupied | In game | FormationStartSpec | B-149 | C (swap not asserted) |
| PA-08 | side from the party list | In game | PartyIntoBattleSpec; EncounterDataSpec | B-071 | OK ("one enemy" wording stale) |
| PA-09 | own pawn / default pawn | In game | — | B-071 | B |
| PA-10..PA-13 | gearless out; leader player; take-over; mid-fight switch | In game | PartyIntoBattleSpec (6) | B-071 | OK |
| PA-14 | AI allies defend like AI enemies | In game | — | — | **A** |
| PA-15..PA-17 | trial recruits | say NB | TrialRecruitSpec (8) | B-076 | OK (rows stale) |
| PA-18 | full HP/EP on trial entry | In game | — | — | **A** |
| PA-19 | HP/EP carry | In game | SaveRunSpec; CarriedStateSpec | B-115, B-208 | OK |
| PA-20, PA-21 | clamp to new max; hub wipes | In game | — | — | **A** |
| PA-22 | bars don't carry | In game | StatusBuildupSpec | B-030 | OK |
| PA-23, PA-24 | Last Stand 50/30%; Sapphire revive 30% | In game | — | — | **A** |
| PA-25 | dead take no turns | In game | FirstStrikeSpec "ends the opening when the rest… have fallen" | — | C |
| PA-26 | Victory / Defeat / Draw | In game | CombatHangFixesSpec | B-001 | C |
| PA-27 | fallen members, rest spot | In game | FallenMembersSpec | B-150 | OK |
| PA-28 | dead party: encounter re-arms | stopgap | — | — | **A** |
| PA-29, PA-30 | 3×3 grid; row modifiers | In game | CombatGridSpec | B-135 | OK (PA-30 OK\*) |
| PA-31, PA-32 | start cells; row moves | In game | FormationStartSpec; RowMoveSpec | B-149, B-180 | OK |
| PA-33 | allies at Hard | In game | PartyIntoBattleSpec "plays AI allies at the fixed ally level" | B-071 | OK\* |
| PA-34 | think times | In game | — | — | **A** |
| PA-35 | AI choice thresholds (25%/40%/45%) | In game | AIStatusQuerySpec (partial) | B-023 | C |
| PA-36 | AI defense attempt rates 40/65/85/95% | In game | — | — | **A** |
| PA-37, PA-38 | co-op sphere | decided / in game | — | — | **A** |
| PA-39 | co-op revive | Open | — | — | NB |

## 2. Coverage per page

| Page | Rows | OK (incl. OK\*) | C | B | A | NB |
|---|---|---|---|---|---|---|
| 01 Classes and loadouts | 41 | 15 | 15 | 1 | 8 | 2 |
| 02 Elements and damage | 43 | 24 | 11 | 0 | 7 | 1 |
| 03 Status bars | 36 | 26 | 5 | 0 | 4 | 1 |
| 04 Effects and turns | 55 | 29 | 14 | 0 | 9 | 3 |
| 05 Economy and runs | 60 | 38 | 6 | 0 | 5 | 11 |
| 06 Party | 39 | 18 | 7 | 3 | 13 | 1 |
| **Total** | **274** | **150 (55%)** | **58 (21%)** | **4** | **46 (17%)** | **19** |

Of the 150 OK, about 25 are OK\* (pinned to the named constant, not the number). The pages say 254 rules; 20 rows were added since (ER-45..ER-60, CL-40/41, ED-42 ×2).

## 3. Class A and C rules ranked by risk, with the spec that should pin each

Risk order: numbers and order-of-operations first, then recent D-/Q- changes, then one-off rules. "Extend" names an existing spec file; "new" a file to add.

| # | Rule | Class | Spec to pin it | Assertion (one line) |
|---|---|---|---|---|
| 1 | ET-17 turn-start order | C | extend `EffectsBaselineSpec` (new Describe "Turn start order") | with a recorder on each step, the order is: flags reset → start-of-turn effects → always-on → OnTurnStart triggers → bar decay → BD overload; a lethal HoT/DoT at step 2 ends the turn with no action |
| 2 | ET-37 / PA-23 / PA-24 Last Stand & Sapphire | A | new `LastStandSpec` | a lethal hit leaves HP = 50% max (`LAST_STAND_HP_PERCENT`), 0 falls back to 30%, highest of two fires, one charge used; Sapphire on a dead ally revives at 30%; window F…S = 2,2,3,3,4,4,5 |
| 3 | ET-12..ET-15 Emerald / Extra Action | A | new `BonusTurnSpec` | F Emerald fires after 6 turns, S after 0 with two turns back to back; the pinned turn beats the speed pick; a dead holder's pinned turn is dropped; Extra Action adds a real owed turn (and records the D-052 question: Emerald costs a later turn) |
| 4 | CL-10..CL-12 Resonator budget | A | new `LoadoutBudgetSpec` | 1 + 1×4 passes, weapon-with-crystal 2 + 4 rings fails, evolution primary 2, evolved ring 2; Generic/Refractor never checked; no ring-count cap |
| 5 | CL-34 / CL-35 innate budget | C | extend `LoadoutRulesSpec` | 24 total, 48 for BD, 6 per school, cost = tier (S 7), −1 per pillar at Mind 4 / Body 7 / Spirit 5, floor 1 |
| 6 | ED-40 / ED-41 / ED-39 / SB-06 Broken Darkness numbers | A | new `BrokenDarknessSpec` | Guard on a Fire hit gains 5% × Efficiency, Perfect 20%; None/Generic/Reality absorb nothing; EP may reach MaxEP + 30%; a Light cast costs 25% base × SpellDamage HP and 25% self-buildup, waived at exact requirements; stacks ×1/1/2/4; BD row −5 all, −15 Reality |
| 7 | SB-19 / SB-20 / SB-22 / SB-28 break sizes | C/A | extend `StatusBreakSpec` | Burn tick = 8% max HP; Earth −30% Defense; Water −30% both speeds; Reality bar = 25% max HP at once and can kill (first turn the literals into named constants per the 2026-10-26 ruling) |
| 8 | ED-19 crit ceiling 2.5 (D-034) | C | extend `DamageCalculatorSpec` | with a Crit Stone and a +200% buff the multiplier is exactly `CRIT_DAMAGE_GEAR_CEILING` = 2.5, and 1.0 with no investment |
| 9 | ED-11 / ED-12 / ED-14 / CL-39 ladders | C | extend `TierGapScalingSpec` | the literal ladders: damage 1.30/1.20/1.13/1.06/1/0.90/0.78/0.64/0.50; cost 0.77…2.00; requirement 1.40…0.32; tier power 1/1.3/1.7/2.2/2.85/3.7/4.8 (one table, so a tuning edit shows up in review) |
| 10 | ET-04 / ET-08 turn speed numbers and ties | C | extend `TurnManagerSpec` | base 10, cap 15 by stats, 20 with gear; a full tie resolves by: base speed, action speed, lower pillar total, Body, Mind, Spirit, party, slot (one test per step) |
| 11 | ED-23 Reality boosts | A | new `RealityBoostSpec` | +10% Refractor, +5% evolution primary, +2.5%/+5% per infusion level feed the attacker stat and crit |
| 12 | ED-28 charge bands | C | extend `InfusionCostSpec` or `InfusedBuildupSpec` | Physical L1 1.15 / L2 1.30, Status 1.10/1.20, Balanced 1.125/1.25; the buildup side swaps them |
| 13 | ED-17 Damage Taken Down cap 90% | C | extend `DamageCalculatorSpec` | two Damage Taken Down effects at 60% each cut 90%, not 100%; Damage Taken Up has no cap |
| 14 | ED-15 skill stat scaling grades | A | extend `DamageCalculatorSpec` | an S-grade pair adds 1.0 × investment, F adds 0.05 |
| 15 | ER-24 world-stat vendor | A | extend `EconomyRoundTripSpec` | first buy 50 Gold, second 75; refused at 20 |
| 16 | ER-38 drop quality | A | new `DropQualitySpec` (seeded) | over 10 000 rolls at Luck 0 the shares are 26/22/18/14/10/6/4 ± tolerance; at Luck 1.0 S ≈ 7% |
| 17 | ER-29 / ER-23 merge Prisms cost, spell essence | A | extend `CrystalMergeSpec`, `EconomyRoundTripSpec` | a merge to C costs 100 Prisms; a Fire spell at tier D costs 17 Fire essence + 50 × grade Prisms |
| 18 | PA-34 / PA-36 / PA-35 AI numbers | A/C | new `AIDifficultySpec` | defense attempt 40/65/85/95%; lethal hit always Dodge; heal below 25% (Hard 40%); ward at 45% |
| 19 | CL-06 / ET-29 EP rules | A | new `EnergyPoolSpec` | a Resonator with a ring loadout only gains 0 EP; with a weapon it gains; EP never exceeds max except BD |
| 20 | CL-08 / CL-36 element check | A | extend `BrokenSourceSpec` | a Fire Refractor's Water spell is refused, a Generic's is not; Reality casts anything; BD casts only its pools |
| 21 | ET-36 charges | A | extend `EffectsBaselineSpec` | a 2-charge trigger fires twice then is removed whatever its duration; DoT ticks don't spend charges |
| 22 | ET-20 Defend | C | extend `StunRuleSpec` | Defend applies +50% Defense for 1 turn (and documents D-050's open conflict: it expires before the enemy acts) |
| 23 | ET-38 Absorb | C | extend `HitPathSpec` | two Absorbs of 20% turn 40% of final damage into EP |
| 24 | ED-07 Generic spell element | C | extend `InfusedAbilityElementSpec` | a Generic spell on a Fire ring deals Fire; on a BD pool, that pool's element; unbound → None |
| 25 | CL-15 / CL-20 / CL-31 / CL-32 slot and pair tables | A/C | extend `GearAttachSpec`, `LoadoutPicksSpec` | 6th evolution refused; F weapon 1 ability … S 6; each fusion pair in `CrystalTypeHelpers` accepted/refused as the table says |
| 26 | PA-01 / PA-03 / PA-18 / PA-20 / PA-21 party plumbing | A/B | extend `FallenMembersSpec` / `RecruitDataSpec` | 4th invite refused; trial entry = full HP/EP (BD 0 EP); carried HP clamped to a lowered max; hub wipes it |
| 27 | SB-07 / SB-08 / SB-09 which effect fires | C/A | extend `StatusBuildupSpec` | Fire then Earth hit → Earth break; uninfused Slash → Bleed; typeless cap holds the bar until a typed hit |
| 28 | ET-16 ritual ordering | C | extend `CombatRitualFireSpec` | two rituals ready together fire faster spell speed first, ties in arming order |

**New rulings not yet on the pages (highest future risk):** D-101 (weapon Power and scaling) rewrites ED-09/ED-13 and D-102 (how pillar levels are gained in a run) touches CL-37/ER-24; neither has a rule row or spec yet.

## 4. Stale citations and stale rows

**Line-number citations are broadly stale.** The pages were written against pre-B-033 files (the god-file splits) and have drifted since. Examples checked:
- `ActionExecutor.cpp` is 5 661 lines; CL-39, ED-14, ED-23, ED-32, ET-27, CED-6 cite `:6374-6437`, `:6667-6672`, `:6776-6826`, `:6846-6860` (past EOF). `:345-352` (CL-21, CL-27, ED-31) is now a tier-gap header; `:2769-2804` (ET-20 Defend) is item consumption, `ExecuteDefend` is at `:2796`; `:4292-4358` (ED-07) is spell-cost modifiers; `:4668-4694` (ED-10, ED-28) is execution facing.
- `DamageCalculator.cpp` is 521 lines; ED-17 `:644-714` and ED-19 `:580-642` are past EOF; `:211-230` (ED-18) is `GetEffectiveStatForScaling`; `:134-140` (ED-16) is the retired `CalculateAttackDamage`; `:266-273` (ED-21) is Ignore Defense.
- `SkillEffectManager.cpp`: SB-19 `:2595-2615` is the once-per-match counter (the 8% Burn literal is at `:2914`); SB-25/ET-23 `:2201-2220` is payload building; ET-20 `:974-1008` is `RemoveAllEffects`.
- `StatusBuildupManager.cpp`: SB-14 `:559-596` is `ReduceStatusBuildup`; SB-07 `:439-447` is the element-keyed multiplier; SB-16 `:467-474` is the firing block now at `:380-391`.
- `TurnManager.cpp`: ET-04 `:402-414` is Execution-turn scheduling; ET-08 `:352-385` is `RequestExtraTurn`/`DelayNextTurn`; ET-05 `:218-249` is belt rebuild.
- `CharacterDataComponent.cpp`: CL-06 `:356-366` is `ServerHeal`/`ServerSetHP` (the Resonator gate is at `:425-431`); ET-37/PA-23 `:410-440` is EP code (`CheckDeath` starts at `:442`).
- `CombatOrchestrator.cpp`: ET-01 `:168-177` is HUD build; ET-51/PA-26 `:1065-1084` is the Execution-turn fire path.
- `EconomyService.cpp`: ER-05/ER-25 `:1089-1138` is the end of `Purchase` and the start of `BuyWorldStat`; ER-29 `:169-185` is `DismantleCrystal`.
- `PartySessionSubsystem.cpp` `:153-158`, `:205-213` (PA-01, PA-04) and `InventoryComponent.cpp:664-668` (CL-28) no longer hold the cited logic.
Citations that name a function (`GuardRules.h`, `CrystalBreak::RepairOn`, `FleeRules::GOLD_LOSS_SHARE`, `ResolveImpactDefense`…) all still resolve. `WoRGameNames.cpp` (ED-42) lives at `Private/WoRGameNames.cpp` (no `Text/` folder). All 150-odd cited files exist.

**Stale rows (status or text behind the code):**
- **ED-42 is two rules** (Unknown naming and the Luck dodge). One needs a new id: the Luck dodge became **ED-42b** (B-281).
- ET-11 says Wind is "Decided, not built (B-063)" and "next two turns"; SB-23 and `StatusBreakSpec` say built, one place.
- ET-33, ET-34 are marked Conflict; B-225 built D-051 / Q-044c (`EffectsBaselineSpec` pins it). CL-C3/C4-3/C4-4 should close.
- ET-50 says "Decided, not built (B-045)"; `EffectTextSpec` and `docs/PIE/B-045.md` exist.
- ER-04 "not yet carried across level changes or saved (Q-151)"; `SaveSpec` banks every persistent balance (B-090).
- ER-17 "nothing survives a level change"; `SharedGearSpec` proves the opposite (B-085/B-091). ER-40, ER-41, ER-42, PA-15..PA-17 say not built; B-091 and B-076 are shipped with specs.
- ER-11 "that enemy stays beaten" is half-built (B-184 marks field teams beaten); the row still says Conflict.
- PA-08 "your party against one enemy" predates ER-45 (teams of three).
- CL-C1 / CL-C3 / CED-1 / CED-2 are marked done but still read as open conflicts in their first sentence.

## 5. Proposed backlog-job clusters (≤3 files each, in order) — PROPOSAL

| # | Job (proposed id) | Files | Closes |
|---|---|---|---|
| 1 | **B-281 Rules page hygiene**: renumber the second ED-42 → ED-43; flip ET-11/33/34/50, ER-04/17/40/41/42, PA-08/15..17 to their real status; replace every `file:line` with `file (Function)` on 02 and 04 | `docs/Rules/02-elements-and-damage.md`, `04-effects-and-turns.md`, `00-index.md` | §4 stale rows and the worst citations (01, 03, 05, 06 in a follow-up B-281b) |
| 2 | **B-282 Last Stand and revive spec** | new `Tests/LastStandSpec.cpp`; `docs/Architecture/SkillEffectSystem.md` changelog | ET-37, PA-23, PA-24 (#2) |
| 3 | **B-283 Bonus and Extra turns spec** (records D-052 as the open conflict it is) | new `Tests/BonusTurnSpec.cpp`; `docs/Architecture/TurnManager.md` | ET-12..ET-15 (#3) |
| 4 | **B-284 Turn-start order and ritual ordering** | `Tests/EffectsBaselineSpec.cpp`, `Tests/CombatRitualFireSpec.cpp`, `docs/Architecture/CombatOrchestrator.md` | ET-17, ET-16 (#1, #28) |
| 5 | **B-285 Loadout budgets spec**: Resonator ≤5 and innate 24/48 | new `Tests/LoadoutBudgetSpec.cpp`, `Tests/LoadoutRulesSpec.cpp`, `docs/Architecture/LoadoutSystem.md` | CL-10..12, CL-34, CL-35 (#4, #5) |
| 6 | **B-286 Break sizes as named constants + spec**: `BURN_SHARE_OF_MAX_HP` 8%, `EARTH_BREAK_DEFENSE_DOWN` 30%, `REALITY_BREAK_HP_SHARE` 25% (the 2026-10-26 ruling) | `Public/Combat/CombatConstants.h`, `Private/Skills/Effects/SkillEffectManager.cpp`, `Tests/StatusBreakSpec.cpp` | SB-19, SB-20, SB-22, SB-28 (#7) |
| 7 | **B-287 Broken Darkness numbers spec** | new `Tests/BrokenDarknessSpec.cpp`; `docs/Architecture/BrokenDarkness.md` | ED-39..ED-41, SB-06 (#6) |
| 8 | **B-288 Literal ladders spec**: tier gap, cost gap, requirement gap, tier power, crit ceiling 2.5, Damage Taken Down 90% | `Tests/TierGapScalingSpec.cpp`, `Tests/DamageCalculatorSpec.cpp` | ED-11/12/14/17/19, CL-39 (#8, #9, #13) |
| 9 | **B-289 Turn speed and tie-break spec** | `Tests/TurnManagerSpec.cpp`, `docs/Architecture/TurnManager.md` | ET-04, ET-08 (#10) |
| 10 | **B-290 Economy numbers**: world-stat vendor, merge Prisms cost, spell essence, drop quality (seeded) | `Tests/EconomyRoundTripSpec.cpp`, `Tests/CrystalMergeSpec.cpp`, new `Tests/DropQualitySpec.cpp` | ER-23, ER-24, ER-29, ER-38 (#15–#17) |

Left for later clusters: Reality boosts (#11), charge bands (#12), AI numbers (#18), EP rules (#19), element check (#20), charges (#21), Defend (#22), Absorb (#23), slot/pair tables (#25), party plumbing (#26), bar type choice (#27).
