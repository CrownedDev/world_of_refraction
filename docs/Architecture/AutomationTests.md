# Automation Tests

## Overview

Headless Unreal Automation Spec tests for the combat maths and turn flow. They run without the editor UI or PIE, so a rule can be checked in seconds, not by setting up the exact fight by hand. `Scripts/ue/test.sh` runs them; the Stop hook runs them when `UE_ROOT` is set.

Every test is named `WoR.<System>.<Case>`. The default filter is `WoR.`, with the dot: a bare `WoR` also matches engine tests by substring (e.g. "Frame**wor**k").

## Architecture

All files live in `Source/world_of_refraction/Private/Tests/`, wrapped in `WITH_DEV_AUTOMATION_TESTS` (compiled out of Shipping).

### Helpers

| File | What it gives a spec |
|---|---|
| `WoRTestWorld.h` | `FWoRTestWorld`: wraps the engine's `FTestWorldWrapper`. Real `UGameInstance`, so every WoR subsystem exists. Ticked by hand. `SpawnCombatant` gives a bare actor (player path) or an unpossessed pawn (AI path) carrying only a `UCharacterDataComponent`; `SpawnCombatantWithLoadout` adds an inventory and a loadout initialised from the character data, optionally with a primary weapon (B-133); `AddComponent<T>` adds and registers any component |
| `ActionExecutedRecorder.h`, `CombatResultRecorder.h`, `InventoryChangeRecorder.h`, `PartsDroppedRecorder.h` | `UObject` listeners with a `UFUNCTION` handler, so a spec can bind a dynamic delegate and check what it broadcast |

### Specs (78 files)

| File | Test names | Covers |
|---|---|---|
| `AIBuildupEstimateSpec.cpp` | `WoR.AI.BuildupEstimate` | the AI's status-buildup estimate reads the same authored number the live hit uses (B-114). |
| `AIDamageEstimateSpec.cpp` | `WoR.AI.DamageEstimate` | The AI's damage estimate equals what the executor's hit takes off the target (B-029): the hit is built from UActionExecutor::PreviewActionDamage (what execution sends into the defense window) and... |
| `AIStatusQuerySpec.cpp` | `WoR.AI.StatusQueries` | The AI's status queries read the effect types the game actually applies (B-023): a Lightning bar-cap Stun counts as a dangerous debuff and a valuable status (Wind's SkipTurn is a turn-order delay,... |
| `AuthoritySpec.cpp` | `WoR.Authority` | WoRAuthority::HasAuthority and the components that gate on it (B-027). |
| `BattleRandomSpec.cpp` | `WoR.Combat.BattleRandom` | the per-battle random stream (B-010). |
| `BossPhaseSpec.cpp` | `WoR.Boss.Phases` | a boss reaches phases from an HP point or a hidden condition, keeps a phase when it heals, can pass several phases in one hit, and grows harder (weather, AI, status resistance) as it goes (B-193... |
| `BoundGearSpec.cpp` | `WoR.Equipment.BoundGear` | an item can belong to a lord; anyone can wear it, only the lord gets its bound bonus, and the bonus line is hidden from everyone else (B-192 (D-033, Q-192)). |
| `BrokenSourceSpec.cpp` | `WoR.Combat.BrokenSources` | a spell whose source's crystal breaks mid-fight stays in the menu, greyed out with the reason, and can't be cast; a broken ring keeps its F-tier spells (D-083 (B-163)). |
| `CarriedStateSpec.cpp` | `WoR.Party.CarriedState` | what fight 1 of a run leaves on a member's crystals and Broken Darkness pool is what fight 2 starts with: wear, a destroyed gem, the absorption stacks and overload EP (B-115 (D-013, D-093, Q-115)). |
| `CleanseGearSpec.cpp` | `WoR.Effects.CleanseGear` | cleanse (CleanseSelf / CleanseAllies → RemoveAllDebuffs) and Iolite only remove effects someone else applied (B-065 / D-015). |
| `CombatCameraSpec.cpp` | `WoR.Combat.Camera`, `WoR.Combat.Camera.Director` | the mode the viewer sees, the template a skill falls back to, and where each mode puts the camera (Combat camera rules (B-156, Design/CombatCamera.md)). |
| `CombatGridSpec.cpp` | `WoR.Combat.Grid` | the combat grid today: one actor per cell, teams kept apart, row damage and defense modifiers (off the grid reads Middle), push and pull one row stopping at the edge or an occupied cell, swaps only between allies (B-135). |
| `CombatHangFixesSpec.cpp` | `WoR.Combat.HangFixes` | Regression tests for the Arc A combat-hang fixes (B-001), driven through a real ACombatOrchestrator on bare 1v1 combatants: a stunned actor's turn (D-001 rule), full release on a normal win, and... |
| `CombatResetSpec.cpp` | `WoR.Combat.ResetBetweenFights` | leave a defense window open, an infusion charging, a status bar part-filled and a turn-order preview built, force the fight to end, and every per-fight subsystem is clean before the second fight... |
| `CombatRitualFireSpec.cpp` | `WoR.Combat.RitualFire` | a ritual fires on its Execution turn through the async path (the only action path left), a montage-less ritual ability is no longer dropped, and a refused fire never strands the turn (B-031). |
| `CommandMenuSubmitSpec.cpp` | `WoR.UI.CommandMenu.Submit` | confirming a target must not shut a menu the player still needs (B-110). |
| `ConfigAssetPathsSpec.cpp` | `WoR.Config.AssetPaths` | asset paths that used to be /Game/ literals in code now come from Config/DefaultGame.ini (B-205). |
| `CrystalBreakSpec.cpp` | `WoR.Durability.CrystalBreak` | between-combat crystal destruction, split out of ACombatOrchestrator (B-035). |
| `CrystalMergeSpec.cpp` | `WoR.Economy.CrystalMerge` | a crystal merge gives back its overshoot as the fewest lower-tier crystals of the same type; an exact merge is unchanged (ER-28 (Crown, 2026-10-26; B-177)). |
| `CurrencyClampSpec.cpp` | `WoR.Economy.CurrencyClamp` | a balance stops at the int32 limit instead of wrapping negative (B-075). |
| `EconomyRoundTripSpec.cpp` | `WoR.Economy.RoundTrip` | today's economy numbers as literals: a cart's Prisms price, all-or-nothing purchase, weapon level-up (Gear plus half in Reality) and revert (half the Gear back, never below the authored tier), dismantle yields by instance tier and gem element (B-137). |
| `DamageCalculatorSpec.cpp` | `WoR.Damage` | UDamageCalculator::CalculateDamage on bare combatants (no loadout, no grid slot, no effects): Pattern-P stat cap, defense reduction + clamp, crit damage. |
| `DataAssetValidationSpec.cpp` | `WoR.Data.Validation` | editor validation and debug dumps for the data assets that had none (B-120). |
| `DefenseClockSpec.cpp` | `WoR.Defense.Clock` | every defense timestamp runs on world time, so wall-clock time passing without a world tick (a pause) leaves the press window untouched (B-009). |
| `DefenseMatchSpec.cpp` | `WoR.Defense.Match` | how a press is matched to an impact today: the two-sided window (lead-in and after-grace), the perfect band, each press used once, one press per impact on a multi-hit attack (latest in range first), a Hard tier shrinking the window, Parry stored as Block, a closed window ignoring presses (B-135). |
| `DismantleEquippedSpec.cpp` | `WoR.Economy.DismantleEquipped` | dismantling an equipped weapon or ring warns first, then takes it out of every saved loadout; a Generic left with no gear is kept out of the fight (B-084 (D-016)). |
| `EconomyPreambleSpec.cpp` | `WoR.Economy.Preamble` | Every UEconomyService entry point refuses a null owner and an owner missing its components, and a well-formed owner gets through to the action (B-025). |
| `EffectIdentitySpec.cpp` | `WoR.Effects.Identity` | an effect is (holder, EffectID, source) (Effect identity (B-062)). |
| `EffectsBaselineSpec.cpp` | `WoR.Effects.Baseline` | today's effect lifecycle, pinned for B-042: apply, stack, refresh, tick-down, expiry, the timing windows (fire on application, delayed first run, Immediate, Persistent), clear vs reset, and both halves of the 2405fa82 re-entrancy case (B-134; the add-mid-pass half B-135, 14 cases). |
| `EncounterDataSpec.cpp` | `WoR.Combat.EncounterData` | an enemy team authored as UEncounterData takes the field, up to three strong; a field encounter without one fights its own pawn alone (B-074). |
| `EnemyZoneSpec.cpp` | `WoR.Trial.EnemyZone` | a zone on the map sets the strength of the fights started inside it (the highest tier where zones overlap), so the same team fights at two strengths in two zones (B-186 (Enemies §4, Q-186)). |
| `EvolutionSecondarySpec.cpp` | `WoR.Loadout.EvolutionSecondary` | a Generic whose primary slot holds an evolution keeps its secondary weapon, attacks with it and uses its abilities; every loadout query agrees (B-099 (D-027)). |
| `FallenMembersSpec.cpp` | `WoR.Party.Fallen` | a member dead at a fight's end stays in the party, marked fallen, and sits out later fights; a rest spot (ReviveFallenMembers) brings the fallen back at full HP (B-150 (D-043)). |
| `FieldEnemyTeamSpec.cpp` | `WoR.Trial.FieldEnemyTeam` | enemy teams on the map (B-184 (Enemies §2-3)). |
| `FirstStrikeSpec.cpp` | `WoR.Combat.FirstStrike` | touching a team from outside its notice cone gives the party first strike; a team reaching the player from behind gives it to the enemy (B-185 (Enemies §2, Q-185)). |
| `FleeSpec.cpp` | `WoR.Combat.Flee` | the party can flee a normal fight, not a mini-boss or boss; fleeing costs a share of each member's Gold and ends the fight as Fled (D-091 (Crown, 2026-10-26; B-176)). |
| `FormationStartSpec.cpp` | `WoR.Grid.FormationStart` | a fight starts each member in its chosen formation cell; an unset or taken cell falls back to the centre-out auto-placement; enemies start in their placeholder row (Q-149: a Refractor in the Back... |
| `GameNamesSpec.cpp` | `WoR.Text.GameNames` | every element and status effect has a cooked-safe player-facing name (B-126). |
| `GearAttachSpec.cpp` | `WoR.Inventory.GearAttach` | UInventoryComponent's weapon and ring socket operations: attach a crystal, stone, fusion or evolution, then remove it (B-026). |
| `GearSwapEffectsSpec.cpp` | `WoR.Effects.GearSwap` | gear effects follow a mid-fight swap (B-081 (D-008)). |
| `GuardSpec.cpp` | `WoR.Defense.Guard` | Block and Parry are one defense, Guard (D-084 / D-087 (Crown, 2026-10-26; B-168)). |
| `HubServicesSpec.cpp` | `WoR.Hub.Services` | every hub service has a merchant NPC (V in an outfit), including the Stone Smith and the loadout vendor; the coverage check names the gaps (B-195 (HubAndStory §3, D-047, D-048, Q-195)). |
| `InfusedBuildupSpec.cpp` | `WoR.Status.InfusedBuildup` | an infused hit's status buildup is authored × charge band × (1 + StatFraction(M)) in the executor, then × M again in AddStatusBuildup (B-113 / Q-113). |
| `InfusionCostSpec.cpp` | `WoR.Infusion.Cost` | what an infused action costs today, as literals: ability x2.25 / x3.0 (D-035), spell x1.5 / x2.0, the upside-only stat surcharge, crystal and deferred casts free, and the HP cost (share of max EP applied to max HP, less Resistance) (B-136). |
| `ItemUseSpec.cpp` | `WoR.Items.Use` | item use reports whether it landed (B-060). |
| `LoadoutGetterSpec.cpp` | `WoR.Loadout.Getters` | the active loadout is the inventory's saved loadout at ActiveLoadoutIndex (or an empty default), and the loadout's class follows the owner's CharacterData (ULoadoutComponent's read getters (B-022)). |
| `LoadoutPicksSpec.cpp` | `WoR.Loadout.Picks` | a pick that is itself invalid is refused with a reason; a loadout's invalid parts (too many abilities, too many utility crystals of a tier band) are dropped and named (B-159 (D-081 = A)). |
| `LoadoutRulesSpec.cpp` | `WoR.Loadout.Rules` | the editor check now gives the fight's answers, BD rules follow the character, the cleaner-only checks are reported, and unset context checks pass (One loadout rule set (B-028, D-073 = A)). |
| `LuckDodgeSpec.cpp` | `WoR.Defense.LuckDodge` | a hit that would land can be dodged by luck, scaled by Luck up to 15%, rolled on the battle random stream so a fight replays from its seed (D-085 (Crown, 2026-10-26; B-169)). |
| `MemoryFragmentSpec.cpp` | `WoR.Memory.Fragment` | a memory runs deep for one character and minor for another; a fragment pickup adds its memory to the run and the journal once, and is gone afterwards (B-190 (RunStructure §5, Q-190)). |
| `MerchantStockRollSpec.cpp` | `WoR.Merchant.StockRoll` | a trial vendor rolls a random part of its stock per run from the run seed; a home shop keeps its full, fixed list (B-179 (Crown 2026-10-26, rules page 05)). |
| `PartyIntoBattleSpec.cpp` | `WoR.Party.IntoBattle` | party members in battle (B-071). |
| `PlayerWalletSpec.cpp` | `WoR.Economy.PlayerWallet` | one wallet per player (B-151 (D-046)). |
| `ReactionTriggerSpec.cpp` | `WoR.Reaction.Trigger` | a reaction fires when one party member wears every listed item while facing the listed character, not when one item is missing; its effects land on the wearer or the faced character; a run memory... |
| `RecruitDataSpec.cpp` | `WoR.Party.RecruitData` | a recruit joins as the CharacterData its placed pawn carries, not its class default, so the same lookup that dismisses it finds it; a second invite of the same character is refused (B-075). |
| `RingFusionStoneSpec.cpp` | `WoR.Loadout.RingFusionStone` | on a ring, the stat-stone half of an elemental fusion counts like on a weapon, while the crystal half still gives the ring its spells (B-082 (D-009)). |
| `RingWeaponQuerySpec.cpp` | `WoR.Loadout.RingWeaponQueries` | The ring and weapon queries URingManager / UWeaponManager answer, and the ring auto-switch when a ring's crystal breaks (B-024). |
| `RowMoveSpec.cpp` | `WoR.Combat.RowMove` | a character can step one row forward or back on their turn, free and once per turn, swapping with an ally in that cell; skills can push or pull a target one row (D-096 (Crown, 2026-10-26; B-180)). |
| `SilenceSealSpec.cpp` | `WoR.Effects.SilenceSeal` | Silenced no longer blocks spells; it seals SILENCE_SEALED_EP_FRACTION of max EP while it lasts, and every EP spend validates and pays against the rest (B-148 / D-038). |
| `SmallFixesSpec.cpp` | `WoR.Combat.SmallFixes` | a DoubleHit's second hit counts toward the hit's DamageDealt, and the turn scheduler keeps running when a combatant's actor is destroyed mid-fight (B-067 regression tests). |
| `StatModifierSpec.cpp` | `WoR.Stats.Modifiers` | a buff and its debuff move each stat getter by exactly (buff - debuff) percent, so the stat-modifier table can replace the hand-written pairs with the same numbers (Transient buff/debuff reads of... |
| `StatusBreakSpec.cpp` | `WoR.Effects.StatusBreaks` | a full bar's effect lasts STATUS_CAP_TURNS of the holder's own turns, counted by the one rule (FTurnSpan, D-051): the landing turn counts if it is the holder's, and the break ends as the holder's... |
| `StatusBuildupSpec.cpp` | `WoR.Status.Buildup` | the status bar today: fill less total status resistance, an element-matched resistance buff, drain by fraction or amount, decay and the full reset, the cap firing the element status, all-status immunity, the reset between fights (B-135). |
| `StoneRowSpec.cpp` | `WoR.Items.Stones` | Every stat stone, at every tier, used on an ally and on an enemy (B-021): which item effect it maps to, which sub-stat it amplifies when attached, its base percent, and the exact effect its... |
| `StoryJournalSpec.cpp` | `WoR.Story.Journal` | collected memories unlock V's lines and conversations, the hub journal plays them, and lore read on a trial map is kept for rereading (B-194 (HubAndStory §4, Q-194)). |
| `StunRuleSpec.cpp` | `WoR.Combat.StunRule` | UActionExecutor::IsActionAllowedWhileStunned — only Defend and the loadout's current basic attack get through (D-001 stun rule (B-061)). |
| `SupportNoBuildupSpec.cpp` | `WoR.Skills.SupportNoBuildup` | heals, buffs and support skills never build status, on anyone (Rules page 03 (Crown, 2026-10-26; B-174)). |
| `TierGapScalingSpec.cpp` | `WoR.Scaling` | Pure ladder functions in TierGapConstants.h: tier-gap damage + cost multipliers, per-pillar requirement-gap multiplier. |
| `TrialGateSpec.cpp` | `WoR.Trial.Gate` | the trial door's difficulty scales enemy strength and the payout through one multiplier per level, the run holds the gate choice, and an enemy given the level's strength starts the fight with the... |
| `TrialMapFeatureSpec.cpp` | `WoR.Trial.MapFeature` | a chest rolls one item from its zone's loot pool (tag = filter, seeded roll = selector) and opens once per run; lore is recorded once; a boss arena without a boss starts nothing (B-188... |
| `TrialRunRefSpec.cpp` | `WoR.Trial.ActiveTrialRef` | UTrialRunSubsystem holds the active trial as a reflected (UPROPERTY) strong reference, so garbage collection can't drop a trial asset mid-run in a packaged build (a weak pointer let it go once... |
| `TurnManagerSpec.cpp` | `WoR.TurnManager` | UTurnManager turn order on bare combatants: deterministic tie-break, alternation at equal speed, and a faster combatant acting first and more often. |
| `TurnSwitchSpec.cpp` | `WoR.Loadout.TurnSwitch` | one free weapon or ring switch per turn (ET-19 (Crown, 2026-10-26; B-175)). |
| `VoidRandomActionSpec.cpp` | `WoR.Effects.VoidRandomAction` | a Void break takes the holder's next turn away (B-199 (Q-063b, Q-199)). |
| `WeaponMeshFollowSpec.cpp` | `WoR.Weapons.MeshFollowsLoadout` | the weapon mesh never ticks and follows `OnActiveWeaponChanged`: a loadout switch, an announced direct edit and any inventory change update the shown weapon at once, with no world tick (B-138). |
| `WoRAttributeSpec.cpp` | `WoR.GAS.Attributes` | a combatant's GAS attributes equal the stat getters they mirror, and stay equal when HP changes or a stat buff lands (UWoRAttributeDebug::GetAttributeMismatchString) (GAS stage 2 (B-041)). |
| `WoRTagsSpec.cpp` | `WoR.Tags` | the enum <-> tag maps round-trip, and an actor's effect tags give the same answers as the enum-based effect queries (UWoRTagDebug::GetTagMismatchString) (GAS stage 1 (B-040)). |
| `WoRUIStackSpec.cpp` | `WoR.UI.Stack` | the CommonUI base layer (B-103). |

Combatants are bare unless a spec needs more: `SpawnCombatant` gives no loadout, grid slot or effects; `SpawnCombatantWithLoadout` adds the loadout. Expected values come from `CombatConstants` rather than literals, so a tuning change doesn't break them, but a formula change does.

## Integration Points

- `Scripts/ue/test.sh [filter]` launches `UnrealEditor-Cmd -nullrhi`. It reads the report's `index.json` and prints each test's state with its error messages. It counts `succeeded` + `succeededWithWarnings` as passes. A test that logs a `UE_LOG` warning (the orchestrator does, on purpose) still passes.
- The log goes to `Saved/ClaudeLogs/test.log` via `-abslog`, and the report to `Saved/ClaudeLogs/tests/`.
- The editor must be closed for `build.sh`: the open editor locks the module DLL, and the link step fails with `LNK1104`.

## Known Limitations / TODOs

- The gear-beyond-the-cap half of Pattern P (gear-ceiling multipliers in `WoR.Damage`) is still untested; loadouts, stones and gear sockets have their own specs.
- Crit *chance* is forced with `OverrideCritChance` in `WoR.Damage`; rolled paths use the per-battle stream (B-010, `WoR.Combat.BattleRandom`).
- `CombatHangFixesSpec` drives turns by calling `OnActionCompleted` and `SubmitActionAsync` directly; `CombatRitualFireSpec` drives the async path. Montages never play headless.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created: test world helper + Damage, Scaling, TurnManager, Combat.HangFixes specs (26 tests). test.sh: per-test output, warnings count as passes, `-abslog`, default filter `WoR.` | `feature/B-007-automation-tests` |
| 2026-10-26 | Dev-only tooling compiled out of Shipping. All `wor.*`/`WoR.*` console commands sit in `#if !UE_BUILD_SHIPPING`. The 5 legacy `*TestActor` classes keep their constructor + BeginPlay, but their logic is guarded, with empty UFUNCTION stubs in Shipping (UHT thunks need them). New console commands and test actors follow the same pattern | `chore/B-008-shipping-guards` |
| 2026-10-27 | B-119: `test.sh` runs the `DataValidation` commandlet after the tests pass, on `PrimaryDataAsset` subclasses only (`-AssetType`, ~12 s; all of `/Game` took 200 s+ and 8 GB on vendor anims). Errors are normalised to `<path under Content/>: <message>` and compared with `Scripts/ue/validation_baseline.txt` (known content debt, 13 lines today, fixed in B-206): a new error fails, a baseline line that no longer appears prints as FIXED. `WOR_SKIP_VALIDATION=1` skips it. Not covered: assets that don't load (an LFS pointer file is skipped by the asset registry). | chore/B-119-data-validation |
| 2026-10-27 | B-133: `FWoRTestWorld::SpawnCombatantWithLoadout` and `AddComponent<T>` (first used by `WoR.Equipment.BoundGear`, replacing its own copy); the doc now lists every spec file (71) and the recorder helpers. | chore/B-133-test-harness |
| 2026-10-27 | B-134: `WoR.Effects.Baseline` (13 cases) records how effects behave today, including two surprises kept as they are: a DOT fires on application (then skips that turn's end), and a same-source re-apply with refresh off replaces the value but keeps the duration while reporting DurationRefreshed. `ClearAllEffects` removes effects and armed gear; only `ResetForNewCombat` resets the once-per-match record (it also clears armed gear). | chore/B-134-effects-baseline-spec |
| 2026-10-27 | B-135: three specs record today's behaviour: `WoR.Status.Buildup` (7 cases), `WoR.Defense.Match` (7) and `WoR.Combat.Grid` (6); `WoR.Effects.Baseline` gains the add-mid-pass case (a DOT tick caps the bar and adds a stun while the pass walks the list; landing on the holder's own turn, the stun counts that turn, per TurnSpan.h). | chore/B-135-buildup-defense-grid-specs |
| 2026-10-27 | B-136: `WoR.Infusion.Cost` (6 cases) pins infusion EP and HP costs with literal numbers; it records that a chosen ability source at L0 still pays x1.5. Not covered: `ApplyCommitCosts`' HP routing by source (needs a running action). | chore/B-136-infusion-cost-spec |
| 2026-10-27 | B-137: `WoR.Economy.RoundTrip` (5 cases) pins cart cost, the level-up/revert round trip and dismantle yields with literal numbers. | chore/B-137-economy-spec |
| 2026-10-27 | B-138: `WoR.Weapons.MeshFollowsLoadout` (9 cases); `SpawnCombatantWithLoadout` now announces its direct weapon write. | refactor/B-138-weapon-mesh-delegate |
| 2026-10-27 | B-042: `WoR.GAS.TurnTracker` (7 cases): the native Stun GE's tag, the turn-counting rule on the tracker, external removal, refusals, the effect manager's hooks, a real `ACombatCharacter`. | feature/B-042-gas-turn-tracker |
| 2026-10-27 | B-042b: `WoR.GAS.EffectBuilder` (10 cases): context class, status / immunity tags, the stat sum equal to the effect manager's, element sums, replace vs another source, DOT / HOT ticks, HealBlocked HOT, a killing DOT, gear origin + permanent, refusals. | feature/B-042b-gas-effect-builder |
