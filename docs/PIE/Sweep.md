# PIE sweep — every retire-after-PIE delete on one page

**Rule (D-124, Crown, 2026-11-03):** deletes are **batched into one sweep per milestone**. Old code keeps the "build new beside old, delete after PIE" rule (`CLAUDE.md`): nothing below is deleted before Crown's PIE pass. Instead of one PIE file and one delete job at a time, Crown runs the checks on this page in one sitting, and the deletes follow in one batch per system.

**Nothing is DONE without Crown's PIE (D-128, Crown, 2026-11-03).** A job stays `REVIEW` in `Backlog.md` until Crown's PIE pass ticks it in **Waiting on Crown's PIE** below; only then does its row move to `DONE`. That list is kept running: a job that adds a `docs/PIE/B-xxx.md` row to Pending in `README.md`, or sets its Backlog row to `REVIEW`, adds its row there in the same change, and a PIE pass ticks it.

**How to run a sweep:**
1. Build `dev`, open the editor.
2. For each system below, run the named PIE check (the linked `B-xxx.md` file; only the rows it names) and the **Blueprint check** where one is listed (Reference Viewer / Find in Blueprints on the named function or field: no hits).
3. Tick the row: `[x]` passed (safe to delete) · `[!]` failed (write what happened; the flag stays).
4. Claude deletes every `[x]` row of a system in one job per system, then moves this page's ticked rows to **Swept** at the bottom with the date and branch.

**Source of the list:** `git grep -n -i -E "OBSOLETE|delete after|retire after|remove after|Obsolete since"` over `Source/` on `docs/D-120-127-simplicity-round1` (2026-11-03), plus the delete jobs waiting on a PIE pass in `Backlog.md`. Line numbers are as of that commit; re-grep before deleting. Each new `OBSOLETE (B-xxx)` flag gets a row here in the job that adds it.

---

## Combat actions and defense

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 1 | `UActionExecutor::CanActorCastSpells` — `Private/Combat/Actions/ActionExecutor.cpp:357`, `Public/Combat/Actions/ActionExecutor.h:187` (B-148) | Silenced seals EP (D-038); `GetSpendableEP` | [B-148](B-148.md) (silenced character can still cast with spendable EP) + Blueprint check: no binding of `CanActorCastSpells` | sweep | [ ] |
| 2 | `UActionExecutor::EnsureResultResolved` (Stage 2 action-level defense) — `ActionExecutor.cpp:1845`, `ActionExecutor.h:775` | `ResolveImpactDefense` (per-impact defense, Stage 3) | [B-032](B-032.md) / [B-032c](B-032c.md) (per-impact guard on a multi-hit) | sweep | [ ] |
| 3 | Per-impact parry reflect, unreachable since B-168 — `ActionExecutor.cpp:2002` | No defense reflects (D-087); reflect effects deal pure damage | [B-168](B-168.md) (Guard / Perfect Guard on a multi-hit: no reflect damage) | sweep | [ ] |
| 4 | Lumped parry reflect, unreachable since B-168 — `Private/Combat/Defense/DefenseSystem.cpp:162` | As row 3 | [B-168](B-168.md) (Perfect Guard on a spell: no reflect) | sweep | [ ] |
| 5 | `UActionExecutor::ProcessMultiHit` — `ActionExecutor.h:365`, `ActionExecutor.h:812` (B-167) | The per-impact path (`ResolveImpactDefense` → `ApplyOneImpact`) | [B-167](B-167.md) (multi-hit abilities still land every hit) | sweep | [ ] |
| 6 | `UActionExecutor::DoWeaponStatsApply` — `ActionExecutor.cpp:4339` (B-307) | `ResolveInfusionSource` (one resolver) | [B-307](B-307.md) | B-309 | [ ] |
| 7 | `FCombatHitResult::bWasBlocked / bWasParried / bWasDodged` — `Public/Combat/Actions/ActionStructs.h:537` (B-277) | `EDefenseType` on the hit result | [B-277](B-277.md) + Blueprint check: no reads of the three flags | sweep | [ ] |
| 8 | `GuardRules::ImpactAnimationOverride` — `Public/Combat/Defense/GuardRules.h:30` (B-202) | Every Guard plays the block montage; `PlaysPerfectGuardFeedback` | [B-202](B-202.md) (Guard and Perfect Guard animations) | sweep | [ ] |
| 9 | `ULoadoutComponent::GetParryMontage` — `Public/Loadout/LoadoutComponent.h:490` (B-202) | `GetBlockMontage` | [B-202](B-202.md) + Blueprint check: no caller | sweep | [ ] |
| 10 | `UCosmeticsData::ParryMontage` — `Public/Character/CosmeticsData.h:55` (B-202) | `BlockMontage` | [B-202](B-202.md) + check no cosmetics asset relies on it | sweep | [ ] |
| 11 | Lumped (unconverted) spell defense path (`ApplyDamageAfterDefense` non-melee slice, `bDamageIsPerImpact=false`) | Per-impact windows on every spell (Q-031 B) | Editor check: every spell asset converted; [B-032c](B-032c.md) | B-032b | [ ] |

## Combat orchestrator and debug

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 12 | `UCombatOrchestrator::SubmitAction` (sync router) — `Public/Combat/CombatOrchestrator.h:170` (B-031) | `SubmitActionAsync` | [B-031](B-031.md) (rituals fire; debug actions work) + a spec covering `CombatOrchestratorTestActor` (D-067) | B-165 | [ ] |
| 13 | `CombatOrchestratorTestActor` (legacy test actor, the last `SubmitAction` caller) | Headless specs (D-066 / D-067) | As row 12 | B-165 | [ ] |
| 14 | `OnWorldStatDraftReady` — `CombatOrchestrator.h:291` (B-223) | `OnLevelDropsReady` (level drops, D-102) | [B-223](B-223.md) / [B-223b](B-223b.md) (win, place the level drop) | sweep | [ ] |
| 15 | `PendingWorldStatPool` — `CombatOrchestrator.h:537` (B-223) | Level drops rolled on a win (`TrialGate::RollLevelDrops`) | [B-223](B-223.md) | sweep | [ ] |
| 16 | Test-asset fallback in the debug commands (`wor.Debug.AllowTestAssetFallback`) — `Private/Debug/Combat/CombatOrchestratorDebug.cpp:55`, `:484`, `:557`, `:865`, `:1025` (B-118) | A logged error when the actor has no skill | [B-118](B-118.md) (debug commands with the CVar off) | sweep | [ ] |

## Loadout and equipment

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 17 | `ULoadoutComponent::GetValidationErrorsLegacy` / `CollectInvalidSlotFindingsLegacy` — `Private/Loadout/LoadoutComponent.cpp:623`, `:1004`, `Public/Loadout/LoadoutComponent.h:240` (B-028) | The one validator (B-028), then `FLoadoutRules` (B-303) | [B-028](B-028.md) (`wor.Loadout.CompareRules` shows only the D-073 changes) | B-172 → B-309 | [ ] |
| 18 | `FSavedLoadout::GetValidationErrorsLegacy` — `Private/Loadout/FSavedLoadout.cpp:43`, `Public/Loadout/FSavedLoadout.h:302` (B-028) | As row 17 | [B-028](B-028.md) | B-172 → B-309 | [ ] |
| 19 | `PrimarySlotCost` — `Private/Loadout/LoadoutRules.cpp:228` (B-303) | `FLoadoutRules::SlotCost` over the unified slots | [B-303](B-303.md) | B-309 | [ ] |
| 20 | Old loadout fields, entries and wrappers (pre-unified slots) | The unified slots (B-301..B-308) | PIE of B-301..B-308 | B-309 | [ ] |
| 21 | `ULoadoutComponent::CharacterClass` — `Public/Loadout/LoadoutComponent.h:116` (B-022); the spec line `Private/Tests/LoadoutGetterSpec.cpp:77` | `GetCharacterClass()` over `CharacterData` | [B-022](B-022.md) + Blueprint check: no read/write of the field | B-153 | [ ] |
| 22 | `URingManager` — `Public/Equipment/Rings/RingManager.h:20`, `Private/Equipment/Rings/RingManager.cpp:2`; `UActionExecutor::GetRingManager` — `ActionExecutor.h:381` (B-024) | `ULoadoutComponent` ring queries and `HandleCrystalBroken` | [B-024](B-024.md) + Blueprint check: no caller | B-154 | [ ] |
| 23 | `UWeaponManager` — `Public/Equipment/Weapons/WeaponManager.h:19`; `UActionExecutor::GetWeaponManager` — `ActionExecutor.h:386` (B-024) | `ULoadoutComponent::GetActiveWeapon / GetCurrentAttack` | [B-024](B-024.md) + Blueprint check | B-154 | [ ] |
| 24 | `CrystalBreakDismantleEvolution`, the Evolution case and `CrystalBreakDestroyPrimaryEvolution` — `Private/Equipment/Durability/CrystalBreak.cpp:55`, `:127`, `:174` (Q-159) | Evolutions are never lost to wear | [B-159](B-159.md) (break a crystal next to an evolution: the evolution stays) | sweep | [ ] |
| 25 | `UInventoryComponent::MaxSavedLoadouts` — `Public/Inventory/InventoryComponent.h:126` (B-311) | `PoolConstants::MAX_CUSTOM_LOADOUTS` (D-111) | [B-311](B-311.md) | sweep | [ ] |

## Inventory, pool and party gear

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 26 | `UInventoryComponent::ClaimActiveSharedGear` — `Private/Inventory/InventoryComponent.cpp:1813`, `Public/Inventory/InventoryComponent.h:209` (B-221b, B-321) | Per-character gear: a member draws only its own (D-117) | [B-321](B-321.md) | B-317 | [ ] |
| 27 | `IsWornByOtherMember`, `IsWornByOtherMemberOf`, `GetSelectableWeapons/Rings` — `InventoryComponent.h:302` (B-321) | Gear owners (`GetGearOwner`) | [B-321](B-321.md) | B-317 | [ ] |
| 28 | `FPoolGearClash` — `Public/Pool/PoolSubsystem.h:99`; wearers and `NoteClashes` / `GetClashes` — `PoolSubsystem.h:267`, `:275` (B-321) | Gear owners | [B-321](B-321.md) | B-317 | [ ] |
| 29 | The old gear copies and sync; the pool's per-member stores (D-122) | The gear store (B-312..B-316) | [B-312](B-312.md) and the store PIE files | B-317 | [ ] |
| 30 | Stat-stone consumable wrappers (`ExecuteRawDamage...MaxEPBuffEffect`, the DefenseStone and CritStone branches) — `Public/Inventory/ItemExecutor.h:153` (B-021) | The stone row table (`ApplyStoneBuffEffect`) | [B-021](B-021.md) (use each stone kind) | B-152 | [ ] |
| 31 | Fusions (B-352, D-141): `FusionRules::RETIRED` and the discarded `if constexpr` bodies — `Private/Inventory/InventoryComponent.cpp` (`AttachFusionToGear`), `Private/Equipment/EquipmentDataBase.cpp` (`IsDataValid` fusion checks), `Private/Equipment/FRuntimeAttachedItem.cpp` (`FromAttachedItem` fusion mapping, fusion bonus); `FFusionId.h`, `FFusionAttachment.h`, `CrystalEffectTable.h` fusion rows, `CrystalTypeHelpers.h` (`IsElementalFusion`, `IsAugmentedFusion`, `IsValidFusionPair`), `EAttachedItemKind::Fusion` (enum value stays, append-only), `FAttachedItem.h` flat fusion fields, `FBrokenCrystalPayload.h` fusion payload; fusion wear / repair in `CrystalBreak.cpp` and `GearRepair.cpp`; fusion pricing in `EconomyService.cpp`; `CrystalManager.cpp`, `ActionExecutor.cpp`, `LoadoutComponent.cpp`, `RingData.cpp` fusion branches; D-077's fusion counting line in `LoadoutRules.cpp`; specs `RingFusionStoneSpec.cpp`, the fusion cases in `GearAttachSpec.cpp`, `ResistanceSeamSpec.cpp`, `CrystalBreakSpec.cpp`, `LoadoutRulesSpec.cpp` | Nothing: fusions are gone (crystals and evolutions in the element socket, stones in the stone socket, B-351) | [B-352](B-352.md) | sweep | [ ] |

## Effects, status and resistance

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 31 | `USkillEffectManager::TickDurations` — `Private/Skills/Effects/SkillEffectManager.cpp:1347` (B-225) | `FTurnSpan` holder-turn timing | [B-225](B-225.md) | sweep | [ ] |
| 32 | The four unreachable status-break cases (`SpeedDebuff`, `CritDebuff`, `EnergyDebuff`, `RandomDebuff`) — `SkillEffectManager.cpp:3106` (B-268) | `BarCapTriggerResolver`'s real outcomes | [B-268](B-268.md) (each element's status break) | sweep | [ ] |
| 33 | `SkillEffectDisplayNames::GetDisplayInfo` — `Public/Skills/Effects/SkillEffectDisplayNames.h:212` (B-248) | The one status-per-element table, `FText` names (D-061) | [B-248](B-248.md) + Blueprint check: no caller | sweep | [ ] |
| 34 | `UStatusBuildupManager` legacy resistance query — `Public/Skills/Effects/StatusBuildupManager.h:177` (B-216) | `WoRResistance::GetTypeResistance` | [B-216](B-216.md) (`UResistanceQueryDebug` comparison matches) | sweep | [ ] |
| 46 | The old skill-effect conditions: `FSkillEffect::Conditions` / `FActiveSkillEffect::Conditions` (`FSkillCondition`), the legacy trigger fields (`TriggerCondition`, `SecondaryTriggerCondition`, thresholds, `bRequireBothTriggers`), `EvaluateConditionGroup`, `ECondSubject`, `IsOwnerSide` / `IsTargetSide`, the read-across (`SkillConditions::ToSets` / `FromLegacy`) and `USkillEffectManager::IsSingleTriggerMet` (B-337d, Crown 2026-11-04) | The authored `FWoRConditionSet` field (B-337e: filled on load, every effect asset resaved in the editor) | [B-337d](B-337d.md) + [B-337e](B-337e.md) (after the editor resave; the list command shows no asset on old conditions; Q-337e: `wor.Conditions.ListAnyOf` lists every skill using Any of: if each can be rewritten without "or", delete Any of too) + Blueprint check: no reads of `Conditions` / `TriggerCondition` | sweep (after B-337e) | [ ] |

## Infusion

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 35 | `UInfusionChargeManager` — `Public/Infusion/InfusionChargeManager.h:88` (B-034) | The menu's pending infusion level (`UCombatCommandMenuSubsystem::GetPendingInfusionLevel`) | [B-034](B-034.md) + Blueprint check: no caller | B-034b | [ ] |
| 36 | `UInfusionVFXComponent::OnChargeLevelChanged` (and the other `OnCharge*` handlers) — `Public/Infusion/InfusionVFXComponent.h:136` (B-034) | `SetInfusionLevel` from the menu | [B-034](B-034.md) (the infusion glow follows the menu level) | B-034b | [ ] |

## Memory, bosses and reactions

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 37 | `UMemoryData::BossModifiers` — `Public/Memory/MemoryData.h:203`; its debug text `Private/Memory/MemoryData.cpp:48` (B-211) | `UBossData::Triggers` (D-100) | [B-211](B-211.md) | sweep | [ ] |
| 38 | `ReactionTrigger::ApplyMemoryBossModifiers` — `Public/Reaction/ReactionTrigger.h:104` (B-211) | Boss triggers | [B-211](B-211.md) | sweep | [ ] |
| 39 | `MemoryUnlock::UnlockBossDrop` — `Public/Memory/MemoryUnlock.h:32` (B-209) | `MemoryConditions` fight-won event (`ABattleGameMode::CheckFightWonMemories`) | [B-209](B-209.md) | sweep | [ ] |

## Wallet, UI and asset migration

| # | Flag (`file:line`) | Replaced by | PIE check that proves it | Delete job | Result |
|---|---|---|---|---|---|
| 40 | `ACombatCharacter`'s own wallet for the party — `Public/Character/CombatCharacter.h:58` (B-151) | The player's wallet on `AWoRPlayerState` (D-046) | [B-151](B-151.md). Partial: the field stays for enemies; only the party path retires | sweep (party path only) | [ ] |
| 41 | `ACombatHUD::FallbackCharacterPanelClass` — `Public/UI/Combat/CombatHUD.h:56` (B-205) | `CharacterPanelClass` | [B-205](B-205.md) (the character panels show) | sweep | [ ] |
| 42 | Old combat widgets | The new HUD (B-105) | PIE of B-105 | B-106 | [ ] |
| 43 | `ETargetType_Legacy` — `Public/Combat/TargetType.h:10` | `ETargetType` + `ETargetCount` (`USkillDataBase::PostLoad`) | Editor: re-save every skill asset (no PIE check) | sweep | [ ] |

## Flags on branches not yet in `dev`

These rows join the sweep once their branch merges.

| # | Flag | Replaced by | PIE check | Branch | Result |
|---|---|---|---|---|---|
| 44 | The 3-points-per-level sub-stat pool: `StatConstants::POINTS_PER_WORLD_STAT_LEVEL`, `MAX_SUBSTAT_POINTS_PER_PILLAR`, `INITIAL_STAT_BUDGET`; `UCharacterData::GetTotalPool` (as a pool), `GetPointsRemaining`, `IsValidDistribution` (B-330, D-123). Before deleting, the enemy caliber and the Broken Darkness EP aura range need their own constants | `GetSubStatBudget` / `IsWithinSubStatBudget` | [B-330](B-330.md) | `feature/B-330-pillars-only` | [ ] |
| 45 | Per-instance random rolls (B-335, D-127): `RerollSubstats` / `RerollPillars` / `RerollResistance`, their call sites and `UEquipmentRollDebug`; the `StatBonus` / `ResistanceBonus` instance fields and the `MaxPool` save fields stay (authored bonuses use them) | Tier + quality (`TierOnInstance.md`) | B-335's PIE file | `chore/B-335-per-instance-rolls-obsolete` | [ ] |
| 47 | `UCharacterData::CanUseSpells` / `CanUseAbilities` (BlueprintPure, `DeprecatedFunction`) — `Public/Character/CharacterData.h` (B-097, D-027). The C++ `CharacterClassHelpers` copies are already deleted (no caller) | Slots, not class gates: `FCombatCapabilities::BuildFrom` | [B-097](B-097.md) + Blueprint check: Find in Blueprints "Can Use Spells" / "Can Use Abilities" (no hits; or no deprecation warning on compile) | `feature/B-097-class-gates-cleanup-cloud` | [ ] |
| 48 | The weapon spell path (B-098, D-027): `wor.Loadout.WeaponSpells` and `WeaponSpellRules` (incl. `FScopedWeaponSpells`); the weapon branches of `FWeaponLoadoutEntry::GetAllSpells` / `GetAllSpellsUncapped` / `GetSpellSlots` / `ReplaceSpell` / `RestoreSpell`, `FCombatCapabilities::WeaponCrystalSpells` / `bHasWeaponCrystal`, `ULoadoutComponent::GetWeaponResonateSpells`; the data fields `FWeaponInventoryEntry::AssignedSpells` / `SpellPicks` / `bSpellSlotsActive`, `FWeaponLoadoutEntry::AssignedSpells` (no longer saved since save v37, Q-098; and the weapon-spell findings in `LoadoutRules`); the `CreateAndConfigureLoadout` weapon branch; the old-path cases in BrokenSources / SpellSlotList / GearChangeDrops / LoadoutRules (the weapon spell-count case) | Weapons give no spells; the crystal keys the abilities' element (B-080) | [B-098](B-098.md) + Blueprint check: no reads of `WeaponCrystalSpells` / `GetWeaponResonateSpells`; Backlog B-411 | `feature/B-098-weapons-no-spells-cloud` | [ ] |

---

## Waiting on Crown's PIE (D-128)

Every job waiting on Crown's PIE: the union of the **Pending** table in [`README.md`](README.md) and the `REVIEW` rows in `docs/Backlog.md` (as of `docs/D-120-127-simplicity-round1`, 2026-11-03). "no PIE file" means the job is `REVIEW` in the Backlog but has no `B-xxx.md`; its PIE check is its Backlog row's done-when. Tick `[x]` when Crown's PIE passes; Claude then sets the Backlog row to `DONE` and moves the README row to Done.

| Job | What it covers | PIE file status | Backlog status | Crown's PIE |
|---|---|---|---|---|
| [B-001](B-001.md) | Combat can no longer freeze; clean end of combat | Not tested | REVIEW (in dev) | [ ] |
| [B-002](B-002.md) | `dev` merged into the native-UI refactor; HUD + results + vitals banking | Not tested | REVIEW (branch `refactor/B-002-merge-dev`; B-003 merges it) | [ ] |
| [B-003](B-003.md) | Combat HUD, panel effects list and turn slots run on C++ (BP overrides deleted); hub unchanged (D-076) | Not tested | REVIEW (in dev; hub controller part is B-158) | [ ] |
| [B-158](B-158.md) | One hub controller: E interact + EnsureLocalParty on `AWoRHubController`; `AExplorationPlayerController` (+ its `DefaultGame.ini` section and `WoR.Config.AssetPaths` case) OBSOLETE, delete after this pass. Blueprint check: `BP_HubPlayerController` has no IA_Interact / EnsureLocalParty nodes | Not tested | REVIEW (in dev) | [ ] |
| [B-004](B-004.md) | GameplayAbilities plugin enabled (CommonUI already on) | Not tested | REVIEW (in dev) | [ ] |
| [B-006](B-006.md) | Seven `LogWoR*` log categories; no gameplay change | Not tested | REVIEW (in dev) | [ ] |
| [B-007](B-007.md) | 26 headless tests + test.sh output; no gameplay change | Not tested | REVIEW (in dev) | [ ] |
| [B-008](B-008.md) | Debug console commands + test actors compiled out of Shipping; no editor change | Not tested | REVIEW (in dev) | [ ] |
| [B-009](B-009.md) | Defense press window on world time: survives a pause, matches the close timer | Not tested | REVIEW (in dev) | [ ] |
| [B-010](B-010.md) | One random seed per battle, logged; `wor.Battle.Seed` replays a fight | Not tested | REVIEW (in dev) | [ ] |
| [B-020](B-020.md) | Stat buffs/debuffs read from one table (same numbers); `wor.Stats.Compare`, `wor.Stats.Print` | Not tested | REVIEW (in dev) | [ ] |
| [B-021](B-021.md) | Stat stones read from one row table (same effects); `wor.Stones.Print` | Not tested | REVIEW (in dev) | [ ] |
| [B-022](B-022.md) | Loadout read without copying; class from CharacterData; no gameplay change | Not tested | REVIEW (in dev) | [ ] |
| [B-023](B-023.md) | AI sees Stun (not Wind's delay) and finds Cleanse Self spells | Not tested | REVIEW (in dev) | [ ] |
| [B-024](B-024.md) | Ring/weapon managers folded into the loadout; ring auto-switch on break; no gameplay change | Not tested | REVIEW (in dev) | [ ] |
| [B-025](B-025.md) | Economy actions share one owner/authority/component check; no gameplay change | Not tested | REVIEW (in dev) | [ ] |
| B-026 | — | no PIE file | REVIEW (in dev) | [ ] |
| B-027 | — | no PIE file | REVIEW (in dev) | [ ] |
| [B-028](B-028.md) | One loadout rule set for the editor and the fight (D-073); `wor.Loadout.CompareRules` shows the difference | Not tested | REVIEW (in dev) | [ ] |
| B-029 | — | no PIE file | REVIEW (in dev) | [ ] |
| [B-030](B-030.md) | Nothing from one fight carries into the next (defense, charge, status bars, turn order, menu) | Not tested | REVIEW (in dev) | [ ] |
| [B-031](B-031.md) | Rituals fire through the async path; old sync router obsolete | Not tested | REVIEW (in dev) | [ ] |
| [B-032](B-032.md) | One hit path: same damage, defense and reflect as before | Not tested | REVIEW (in dev) | [ ] |
| [B-032c](B-032c.md) | Reflect returns a share of the HP lost; Double Hit skips a killed target | Not tested | REVIEW (in dev) | [ ] |
| [B-033](B-033.md) | God-file splits (move-only): debug buttons, montage runner, costs | Not tested | REVIEW (in dev) | [ ] |
| [B-034](B-034.md) | The menu holds the infusion level; the VFX only shows it | Not tested | REVIEW (in dev) | [ ] |
| [B-035](B-035.md) | Combat-end crystal destruction split out (no behaviour change) | Not tested | REVIEW (in dev) | [ ] |
| [B-040](B-040.md) | Gameplay tags for elements, classes and statuses (`wor.Tags.Compare`) | Not tested | REVIEW (in dev) | [ ] |
| [B-041](B-041.md) | GAS attributes mirror the stats (`wor.Attributes.Compare`) | Not tested | REVIEW (in dev) | [ ] |
| [B-042](B-042.md) | GAS stage 3, part 1: Stun GE + turn tracker (nothing uses it in play yet) | Not tested | REVIEW (in dev) | [ ] |
| [B-042b](B-042b.md) | GAS stage 3, part 2: effect context, family GEs, spec builder (nothing uses them in play yet) | Not tested | REVIEW (in dev) | [ ] |
| [B-042c](B-042c.md) | GAS stage 3, part 3: effects mirrored onto GAS; `wor.Effects.UseGAS 1` reads them | Not tested | REVIEW (in dev) | [ ] |
| [B-042d](B-042d.md) | GAS stage 3, part 4: `Event.Combat.*` GameplayEvents sent (nothing listens yet) | Not tested | REVIEW (in dev) | [ ] |
| [B-042e](B-042e.md) | GAS stage 3, part 5: damage execution on GAS (not used in play yet) | Not tested | REVIEW (in dev) | [ ] |
| [B-042h](B-042h.md) | GAS stage 3, part 3b: with UseGAS 1 the turn tracker owns how long effects last | Not tested | REVIEW (in dev) | [ ] |
| [B-042j](B-042j.md) | Barrier / Shield / Ward absorbers (D-092): spent after Guard and Defense, one charge per impact; `wor.Absorb.Add` to try them | Not tested | REVIEW (in dev) | [ ] |
| [B-042k](B-042k.md) | GAS stage 3, part 4b: every effect type has a GAS family; the mirror copies them all | Not tested | REVIEW (in dev) | [ ] |
| [B-042l](B-042l.md) | GAS stage 3, part 5b: status buildup maths as one core; buildup execution on GAS (not used in play yet) | Not tested | REVIEW (in dev) | [ ] |
| [B-042m](B-042m.md) | Ignore Defense is an amount per skill (D-086); IgnoreDefense effects ignore their value | Not tested | REVIEW (in dev) | [ ] |
| [B-042n](B-042n.md) | GAS stage 3, part 5d: DOT and HOT ticks on GAS pay what the enum path pays | Not tested | REVIEW (in dev) | [ ] |
| [B-042o](B-042o.md) | GAS stage 3, part 5e: with UseGAS 1 the status bar lives on GAS | Not tested | REVIEW (in dev) | [ ] |
| [B-042p](B-042p.md) | GAS stage 3, part 5f: spell / raw damage and action / spell speed composed on GAS from their gear, stone and buff layers | Not tested | REVIEW (in dev) | [ ] |
| [B-042q](B-042q.md) | GAS stage 3, part 5g: status multiplier, Defense and crit damage composed on GAS; `wor.Attributes.Compare` checks the composed bases | Not tested | REVIEW (in dev) | [ ] |
| [B-042r](B-042r.md) | GAS stage 3, part 5h: Luck and the Reflex window factor composed on GAS | Not tested | REVIEW (in dev) | [ ] |
| [B-042s](B-042s.md) | GAS stage 3, part 5i: crit chance and Efficiency derived on GAS from Luck and the cost reduction | Not tested | REVIEW (in dev) | [ ] |
| [B-042t](B-042t.md) | GAS stage 3, part 5j: Mind, Body and Spirit built on GAS from their world base and layers | Not tested | REVIEW (in dev) | [ ] |
| [B-042u](B-042u.md) | GAS stage 3, part 5k: max HP and max EP built on GAS from the pool layers | Not tested | REVIEW (in dev) | [ ] |
| [B-042v](B-042v.md) | GAS stage 3, part 5l: turn speed built on GAS from the number turn order uses | Not tested | REVIEW (in dev) | [ ] |
| [B-042w](B-042w.md) | GAS stage 3, part 5m: the resistances built on GAS from their sources | Not tested | REVIEW (in dev) | [ ] |
| [B-042x](B-042x.md) | GAS stage 3, part 3c: with UseGAS 1 the status and immunity checks read tags | Not tested | REVIEW (in dev) | [ ] |
| [B-042y](B-042y.md) | GAS stage 3, part 3d: with UseGAS 1 burns and heal-over-time are paid by GAS | Not tested | REVIEW (in dev) | [ ] |
| [B-042yb](B-042yb.md) | GAS stage 3, part 3d2: with UseGAS 1 damage, speed and pillar stats come from GAS | Not tested | REVIEW (in dev) | [ ] |
| [B-042yc](B-042yc.md) | GAS stage 3 part 3d3: resistances, turn speed, Defense, crit, luck, reflex, efficiency and status multiplier read GAS at UseGAS 1; no change at UseGAS 0 | Not tested | REVIEW (in dev) | [ ] |
| [B-042yd](B-042yd.md) | GAS: energy, drains, self-damage and percent restores pay through GAS at UseGAS 1 | Not tested | REVIEW (in dev) | [ ] |
| [B-042ye](B-042ye.md) | GAS: the AI's burn checks, buff / debuff counts and the panel's effect list read GAS at UseGAS 1 | Not tested | REVIEW (in dev) | [ ] |
| [B-042z](B-042z.md) | GAS stage 3 part 3e: fires-once records and armed conditionals live on each fighter's turn tracker (per-fight state on the battle's actors, D-064) | Not tested | REVIEW (in dev) | [ ] |
| B-044 | — | no PIE file | REVIEW (in dev) | [ ] |
| [B-045](B-045.md) | Item text generated from its effects | Not tested | REVIEW (in dev) | [ ] |
| [B-046](B-046.md) | Game rules reference in `docs/Rules/` (254 rules) + 23 new questions D-034..D-056. Read on the phone | Not tested | REVIEW (in dev) | [ ] |
| [B-047](B-047.md) | Best-practice audit vs Epic/Lyra; 25 jobs B-118..B-142, questions D-057..D-068. Read on the phone | Not tested | REVIEW (in dev) | [ ] |
| [B-060](B-060.md) | A failed item use keeps its charge | Not tested | REVIEW (in dev) | [ ] |
| [B-061](B-061.md) | Stunned = Attack or Defend only; no more skipped turn | Not tested | REVIEW (in dev) | [ ] |
| [B-062](B-062.md) | An effect belongs to its source: same source refreshes, another adds | Not tested | REVIEW (in dev) | [ ] |
| [B-063](B-063.md) | Every full bar lasts one holder turn; Water slows, Wind delays, BD drains | Not tested | REVIEW (in dev) | [ ] |
| [B-064](B-064.md) | Element resistance changes elemental damage (+-50% cap) | Not tested | REVIEW (in dev) | [ ] |
| [B-064b](B-064b.md) | Element resistance also cuts elemental DoT ticks | Not tested | REVIEW (in dev) | [ ] |
| [B-065](B-065.md) | Cleanse and Iolite leave gear effects alone | Not tested | REVIEW (in dev) | [ ] |
| [B-067](B-067.md) | Small fixes: no corpse walk-back, no animation hang, DoubleHit totals | Not tested | REVIEW (in dev) | [ ] |
| [B-070](B-070.md) | The trial run keeps hold of its trial (packaged builds) | Not tested | REVIEW (in dev) | [ ] |
| [B-071](B-071.md) | Recruited party in battle: own pawns, Player/AI per member, switch on its turn, take over when you fall, gearless sit out | Not tested | REVIEW (in dev) | [ ] |
| [B-074](B-074.md) | An encounter fields a team of up to three enemies | Not tested | REVIEW (in dev) | [ ] |
| [B-075](B-075.md) | Recruit as the placed character; currency limit; shop always closes; hints cleared | Not tested | REVIEW (in dev) | [ ] |
| [B-076](B-076.md) | A character found in a trial joins for that run, ungeared, uses armoury gear, and leaves at the end | Not tested | REVIEW (in dev) | [ ] |
| [B-080](B-080.md) | An infused ability deals its source's element (Generic + fire crystal = Fire) | Not tested | REVIEW (in dev) | [ ] |
| [B-081](B-081.md) | Gear effects follow a mid-fight weapon toggle or ring switch | Not tested | REVIEW (in dev) | [ ] |
| [B-082](B-082.md) | A ring's fusion stone gives its stat (e.g. Defense) | Not tested | REVIEW (in dev) | [ ] |
| [B-084](B-084.md) | Dismantling equipped gear warns, then leaves no copy in any loadout | Not tested | REVIEW (in dev) | [ ] |
| [B-085](B-085.md) | Loadouts follow the owned gear: wear, level-ups, attaches and dismantles stay in sync | Not tested | REVIEW (in dev) | [ ] |
| [B-090](B-090.md) | Party and currencies saved between sessions | Not tested | REVIEW (in dev) | [ ] |
| [B-091](B-091.md) | The party shares one armoury: each piece once, one wearer, changes reach everyone | Not tested | REVIEW (in dev) | [ ] |
| [B-092](B-092.md) | Defend button; stunned menu = Attack + Defend, plain attack only (D-023 A) | Not tested | REVIEW (in dev) | [ ] |
| [B-093](B-093.md) | Keys for Defend and infusion (keyboard + gamepad) | Not tested | REVIEW (in dev) | [ ] |
| [B-094](B-094.md) | Guard blocks real `rm -r` only, not read-only commands; tooling, no game change | Not tested | REVIEW (in dev) | [ ] |
| [B-099](B-099.md) | Generic + evolution keeps its secondary weapon (attack, abilities, stance) | Not tested | REVIEW (in dev) | [ ] |
| [B-100](B-100.md) | Mockup tool in `Tools/ui_drafts/` + one test draft to view on the phone | Not tested | REVIEW (in dev) | [ ] |
| [B-101](B-101.md) | Combat HUD drafts v1: command, target, hit, stunned. View on the phone; approval is B-102 | Not tested | REVIEW (in dev) | [ ] |
| [B-103](B-103.md) | CommonUI layers: an empty test screen opens and closes with Back (keyboard and gamepad) | Not tested | REVIEW (in dev) | [ ] |
| [B-110](B-110.md) | A refused pick keeps the command menu open | Not tested | REVIEW (in dev) | [ ] |
| [B-111](B-111.md) | Combat HUD drafts v2: header candidates A-E, crystal look, BD EP, damage colours, cracking bars. Review is B-102 | Not tested | REVIEW (in dev) | [ ] |
| [B-112](B-112.md) | A full Light bar stops crits for one turn | Not tested | REVIEW (in dev) | [ ] |
| [B-113](B-113.md) | No game change: infused status buildup confirmed as designed (Q-113) | Not tested | REVIEW (in dev) | [ ] |
| [B-114](B-114.md) | The AI judges status buildup by the real number | Not tested | REVIEW (in dev) | [ ] |
| [B-115](B-115.md) | Crystal wear and Broken Darkness absorption carry between fights | Not tested | REVIEW (in dev) | [ ] |
| [B-117](B-117.md) | Stale comments and docs fixed; unread constants removed (no behaviour change) | Not tested | REVIEW (in dev) | [ ] |
| [B-118](B-118.md) | Combat debug commands with no skill log an error instead of loading a test asset | Not tested | REVIEW (in dev) | [ ] |
| [B-119](B-119.md) | `test.sh` also validates data assets; a newly broken DA fails it (no PIE, a terminal check) | Not tested | REVIEW (in dev) | [ ] |
| [B-120](B-120.md) | Validate and Debug > Print on trial, cosmetics, stance, infusion, character, skill and inventory data assets (editor, no PIE) | Not tested | REVIEW (in dev) | [ ] |
| [B-123](B-123.md) | Read the one-page naming convention (doc only) | Not tested | REVIEW (in dev) | [ ] |
| [B-124](B-124.md) | A saved weapon or ring keeps its crystal, wear, spells and rolls | Not tested | REVIEW (in dev) | [ ] |
| [B-126](B-126.md) | Element, status and class names on the combat panels; Reality reads Unknown | Not tested | REVIEW (in dev) | [ ] |
| [B-130](B-130.md) | Docs only: read the co-op state map | Not tested | REVIEW (in dev) | [ ] |
| [B-131](B-131.md) | Co-op: the character data component replicates (HP / EP and their max, alive, Broken Darkness, the template); only the server derives the max pools | Not tested | REVIEW (in dev) | [ ] |
| [B-132](B-132.md) | Encounters, battle start and the party wallet use the party leader | Not tested | REVIEW (in dev) | [ ] |
| [B-133](B-133.md) | Tests only: nothing to play | Not tested | REVIEW (in dev) | [ ] |
| [B-134](B-134.md) | Tests only: nothing to play | Not tested | REVIEW (in dev) | [ ] |
| [B-135](B-135.md) | Tests only: nothing to play | Not tested | REVIEW (in dev) | [ ] |
| [B-136](B-136.md) | Tests only: nothing to play | Not tested | REVIEW (in dev) | [ ] |
| [B-137](B-137.md) | Tests only: nothing to play | Not tested | REVIEW (in dev) | [ ] |
| [B-138](B-138.md) | Weapon mesh swaps without a tick | Not tested | REVIEW (in dev) | [ ] |
| [B-139](B-139.md) | Combat stances and infusion glow unchanged | Not tested | REVIEW (in dev) | [ ] |
| [B-141](B-141.md) | A normal fight has a quiet Output Log | Not tested | REVIEW (in dev) | [ ] |
| B-144 | — | no PIE file | REVIEW (in dev) | [ ] |
| B-145 | — | no PIE file | REVIEW (in dev) | [ ] |
| B-146 | — | no PIE file | REVIEW (in dev) | [ ] |
| [B-147](B-147.md) | D-034 / D-035 / D-039 / D-041 in comments, docs and a test (no behaviour change) | Not tested | REVIEW (in dev) | [ ] |
| [B-148](B-148.md) | Silenced seals half your EP instead of blocking spells | Not tested | REVIEW (in dev) | [ ] |
| [B-149](B-149.md) | Members start a fight in their chosen cell; enemies Front/Back by class | Not tested | REVIEW (in dev) | [ ] |
| [B-150](B-150.md) | Fallen members stay in the party; a rest revives them | Not tested | REVIEW (in dev) | [ ] |
| [B-151](B-151.md) | One wallet per player, shared by the party (`wor.Wallet.Print`) | Not tested | REVIEW (in dev) | [ ] |
| [B-155](B-155.md) | Utility crystal tier caps reported in loadout checks | Not tested | REVIEW (in dev) | [ ] |
| [B-156](B-156.md) | Combat camera: turn view behind the acting character, pan-out on enemy turns, `wor.Camera.Print` style / movement-off commands (no cinematics until B-157) | Not tested | REVIEW (in dev) | [ ] |
| [B-159](B-159.md) | Invalid picks refused; invalid loadout parts dropped at load | Not tested | REVIEW (in dev) | [ ] |
| [B-160](B-160.md) | Camera cinematic settings: cinematics on/off, enemy cinematics, skip after hit | Not tested | REVIEW (in dev) | [ ] |
| [B-161](B-161.md) | Editor loadout check uses the character's authored spell discount | Not tested | REVIEW (in dev) | [ ] |
| [B-162](B-162.md) | Under a weapon's requirement, its attacks and abilities deal less | Not tested | REVIEW (in dev) | [ ] |
| [B-163](B-163.md) | Broken-source spells greyed with the reason; broken ring keeps F-tier | Not tested | REVIEW (in dev) | [ ] |
| [B-166](B-166.md) | A montage-less skill no longer crashes the async start; ritual spec covers one | Not tested | REVIEW (in dev) | [ ] |
| [B-167](B-167.md) | "Subsystem missing" damage fallbacks removed (no behaviour change in a normal game) | Not tested | REVIEW (in dev) | [ ] |
| [B-168](B-168.md) | Guard (Block + Parry merged), Perfect Guard, no built-in reflect | Not tested | REVIEW (in dev) | [ ] |
| [B-169](B-169.md) | Luck dodge: a hit that would land can be dodged by luck, max 15% | Not tested | REVIEW (in dev) | [ ] |
| [B-171](B-171.md) | Taking a crystal off or swapping it drops the spells it powered, in every loadout, with a message | Not tested | REVIEW (in dev) | [ ] |
| [B-174](B-174.md) | Heals, buffs and support skills build no status | Not tested | REVIEW (in dev) | [ ] |
| [B-175](B-175.md) | One free weapon or ring switch per turn | Not tested | REVIEW (in dev) | [ ] |
| [B-176](B-176.md) | Flee a normal fight (not mini-boss/boss), at a Gold cost | Not tested | REVIEW (in dev) | [ ] |
| [B-177](B-177.md) | Crystal merge gives back its overshoot (ER-28) | Not tested | REVIEW (in dev) | [ ] |
| [B-178](B-178.md) | Paid gear repair in a run; the run's Gold carries between levels | Not tested | REVIEW (in dev) | [ ] |
| [B-179](B-179.md) | Trial vendors roll their stock per run; home shops unchanged | Not tested | REVIEW (in dev) | [ ] |
| [B-180](B-180.md) | Free one-row step per turn; push / pull effects | Not tested | REVIEW (in dev) | [ ] |
| [B-181](B-181.md) | The AI skips spells greyed by a broken source | Not tested | REVIEW (in dev) | [ ] |
| [B-182](B-182.md) | A Perfect Guard plays the parry animation | Not tested | REVIEW (in dev) | [ ] |
| [B-183](B-183.md) | The trial door asks for a variation and a difficulty; Hard makes enemies stronger | Not tested | REVIEW (in dev) | [ ] |
| [B-184](B-184.md) | Enemy teams on the map; beaten teams stay gone for the run | Not tested | REVIEW (in dev) | [ ] |
| [B-185](B-185.md) | First strike: sneak up and act first; caught from behind and the enemy does | Not tested | REVIEW (in dev) | [ ] |
| [B-186](B-186.md) | Zones: a fight inside a higher-tier zone has stronger enemies | Not tested | REVIEW (in dev) | [ ] |
| [B-188](B-188.md) | Map features: a chest from the zone's loot pool, a lore object, the boss arena | Not tested | REVIEW (in dev) | [ ] |
| [B-190](B-190.md) | Memory fragments: pick one up, it goes into the run and the journal | Not tested | REVIEW (in dev) | [ ] |
| [B-191](B-191.md) | Reactions: worn items facing a character change the fight; memories weaken a boss | Not tested | REVIEW (in dev) | [ ] |
| [B-192](B-192.md) | Bound gear: only its owner gets the bound bonus | Not tested | REVIEW (in dev) | [ ] |
| [B-193](B-193.md) | Boss phases: HP and hidden conditions, phase kept after a heal, weather and AI rise | Not tested | REVIEW (in dev) | [ ] |
| [B-194](B-194.md) | Hub journal: reread memories and lore, V's next line, play a conversation (placeholder screen) | Not tested | REVIEW (in dev) | [ ] |
| [B-195](B-195.md) | Hub services: one V NPC per service (Stone Smith, loadout vendor added), each opens its shop | Not tested | REVIEW (in dev) | [ ] |
| B-196 | — | no PIE file | REVIEW (in dev) | [ ] |
| B-197 | — | no PIE file | REVIEW (in dev) | [ ] |
| [B-198](B-198.md) | A Luck dodge plays the dodge montage | Not tested | REVIEW (in dev) | [ ] |
| [B-199](B-199.md) | A Void break plays the holder's next turn at random | Not tested | REVIEW (in dev) | [ ] |
| [B-200](B-200.md) | The AI's buildup estimate counts gear and buffs | Not tested | REVIEW (in dev) | [ ] |
| [B-201](B-201.md) | A failed item use spends its charge (Q-060 changed) | Not tested | REVIEW (in dev) | [ ] |
| [B-202](B-202.md) | One Guard animation; a Perfect Guard flashes and sounds (Q-182 changed) | Not tested | REVIEW (in dev) | [ ] |
| [B-203](B-203.md) | The party carries 10 item slots; the rest goes to storage; move items between them | Not tested | REVIEW (in dev) | [ ] |
| [B-204](B-204.md) | A broken secondary weapon's spells are greyed; the AI skips them | Not tested | REVIEW (in dev) | [ ] |
| [B-205](B-205.md) | Hub interact key and combat HUD widgets still work with their asset paths moved to `DefaultGame.ini` | Not tested | REVIEW (in dev) | [ ] |
| [B-207](B-207.md) | Shop, defense prompt, party row and merchant hint names | Not tested | REVIEW (in dev) | [ ] |
| [B-208](B-208.md) | Quitting mid-trial and restarting in the hub picks the run back up: fights, chests, HP, wear and Gold | Not tested | REVIEW (in dev) | [ ] |
| [B-209](B-209.md) | Memories from bosses, reactions, map spots and bound gear | Not tested | REVIEW (in dev) | [ ] |
| [B-209b](B-209b.md) | Memories by conditions (who, gear, where, what you beat, how, chains) | Not tested | — | [ ] |
| [B-210](B-210.md) | A collected memory is gone for good: its fragment isn't in later runs | Not tested | REVIEW (in dev) | [ ] |
| [B-211](B-211.md) | Boss triggers: the right character + gear weakens / strengthens / changes a boss | Not tested | REVIEW (in dev) | [ ] |
| [B-212](B-212.md) | Reactions and boss triggers firing mid-fight (turn, hit, HP line, phase), once each | Not tested | REVIEW (in dev) | [ ] |
| B-213 | — | no PIE file | REVIEW (in dev; built as feature/B-209-requirement-fit, Q-209e) | [ ] |
| [B-216](B-216.md) | Resistance per damage type; same buildup numbers; optional resistance profile asset | Not tested | REVIEW (in dev) | [ ] |
| [B-217](B-217.md) | Requirements count a pillar raised during the run | Not tested | REVIEW (in dev) | [ ] |
| [B-218](B-218.md) | Combat tuning data asset and ladders in curve tables; no number changes without it | Not tested | REVIEW (in dev) | [ ] |
| [B-219](B-219.md) | Damage maths moved into one core function; no number should change | Not tested | REVIEW (in dev) | [ ] |
| [B-220](B-220.md) | Weapons get Power and Mind / Body / Spirit scaling grades (D-101); Power 0 by default, so no damage changes until set; `wor.Weapon.Print` | Not tested | REVIEW (in dev) | [ ] |
| [B-221](B-221.md) | Shared pool: lists of gear selectable by a member (worn-by-others filtered out) and the inventory debug tag; the pickers switch with B-221b | Not tested | REVIEW (in dev) | [ ] |
| [B-222](B-222.md) | Mind / Body / Spirit can't pass 7 in a run (was 20): head-start, earned and bought levels clamp at 7; the vendor refuses at 7 | Not tested | REVIEW (in dev) | [ ] |
| [B-223](B-223.md) | Level drops: chance from normal enemies, guaranteed from mini-bosses; place them with `wor.UI.LevelDrop` | Pending | REVIEW (in dev) | [ ] |
| [B-223b](B-223b.md) | Level drops per character: each member rolls its own, places its own | Not built | REVIEW (in dev) | [ ] |
| [B-224](B-224.md) | Combat numbers baseline: rank HP x1.5/3/6, Defense cap 75%, crit base x1.25, requirement floor 0.5, tuning ladders | Not built | REVIEW (in dev) | [ ] |
| [B-224b](B-224b.md) | Rank HP per team; enemy HP by tier (F default) | Pending | REVIEW (in dev) | [ ] |
| [B-225](B-225.md) | Effect timing per D-051 / Q-044c / Q-044d: N holder turns, payouts at turn start, Execution turns run the turn-start step | Not tested | REVIEW (in dev) | [ ] |
| [B-226](B-226.md) | Water status buildup also reads Ice resistance (Q-042o changed) | Not tested | REVIEW (in dev) | [ ] |
| [B-227](B-227.md) | Co-op travel: hub / trial / battle changes use seamless ServerTravel so clients come along; `AWoRGameState`; PlayerState carries currencies over | Not tested | REVIEW (in dev) | [ ] |
| [B-241](B-241.md) | AI picks its infusion level with one helper for spells and abilities; energy-percent reads safe at Max EP 0 | Not tested | REVIEW (in dev) | [ ] |
| [B-242](B-242.md) | One crystal / evolution cast-wear formula (`ComputeCastWear`); same wear numbers | Not tested | REVIEW (in dev) | [ ] |
| [B-245](B-245.md) | Wear and the tier gap use the same action tier (`ResolveActionTier`); ability and basic-attack wear numbers change (Q-245) | Not tested | REVIEW (in dev) | [ ] |
| [B-246](B-246.md) | Every way a fight ends runs one finish tail (crystal destruction, repair, resets); a TurnManager-ended fight now runs destruction and repair (Q-246) | Not tested | REVIEW (in dev) | [ ] |
| [B-247](B-247.md) | One heal amplify + HealBlock gate for every heal kind; every heal broadcasts `OnHealingDone` | Not tested | REVIEW (in dev) | [ ] |
| [B-248](B-248.md) | Each element's break name comes from one table (D-061 / D-062); Water's break shows Frozen (D-090); Heal Block shows Heal Blocked | Not tested | REVIEW (in dev) | [ ] |
| [B-249](B-249.md) | One element -> immunity map (no change in play) | Not tested | REVIEW (in dev) | [ ] |
| [B-250](B-250.md) | One effect-bundle flatten for equipment and evolutions (8 copies before); same effects listed | Not tested | REVIEW (in dev) | [ ] |
| [B-251](B-251.md) | Inventory add / remove and the owned-gear resolvers for weapons and rings share one template each; same behaviour | Not tested | REVIEW (in dev) | [ ] |
| [B-252](B-252.md) | Economy actions (dismantle, level-up, revert, repair) are refused during a fight; one owned-gear lookup by PersistentID | Not tested | REVIEW (in dev) | [ ] |
| [B-252b](B-252b.md) | One body for weapon / ring dismantle, level up, revert (no change in play) | Not tested | REVIEW (in dev) | [ ] |
| [B-253](B-253.md) | Crystal names and tier letters come from the cooked-safe name table (B-126); stones now named properly | Not tested | REVIEW (in dev) | [ ] |
| [B-254](B-254.md) | The command menu's infusion level / source controls are one helper for the single and group pickers; same behaviour | Not tested | REVIEW (in dev) | [ ] |
| [B-255](B-255.md) | One "is in Broken Darkness form" check (two definitions before); same behaviour | Not tested | REVIEW (in dev) | [ ] |
| [B-256](B-256.md) | Shared nearest-interactable search (`WoRInteract`) and debug-opponent helper; same hints and prints | Not tested | REVIEW (in dev) | [ ] |
| [B-256b](B-256b.md) | One interact-hint helper; shared crystal / BD debug preambles; same hints and prints | Not tested | REVIEW (in dev) | [ ] |
| [B-258](B-258.md) | Stat reads stop copying the loadout (const reference); no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-259](B-259.md) | AI damage estimates cached within one decision; same AI choices | Not tested | REVIEW (in dev) | [ ] |
| [B-260](B-260.md) | Lighter GAS attribute sync on hits; attributes still match | Not tested | REVIEW (in dev) | [ ] |
| [B-261](B-261.md) | A GAS stat read skips the mirror re-sync when nothing changed (P5) | Pending | REVIEW (in dev) | [ ] |
| [B-262](B-262.md) | Per-frame fixes: projectiles move in one call and tick only in flight; the loadout caches its sibling components | Not tested | REVIEW (in dev) | [ ] |
| [B-263](B-263.md) | Boss data found by an asset-registry tag instead of loading every BossData; resave the BossData assets once | Not tested | REVIEW (in dev) | [ ] |
| [B-264](B-264.md) | Output Log quiet during a normal fight (per-turn lines at Verbose) | Not tested | REVIEW (in dev) | [ ] |
| [B-265](B-265.md) | Header includes trimmed; `EWorldPillar` in its own header (Blueprint check) | Not tested | REVIEW (in dev) | [ ] |
| [B-266](B-266.md) | CPU profiler markers on the five suspect functions; Crown's profiling run (D-068) | Not tested | REVIEW (in dev) | [ ] |
| [B-267](B-267.md) | Energy Cost and Turn Speed modifiers follow D-054: a buff on an ally, a debuff on an enemy; an ally's cost cut makes skills cheaper and survives cleanse | Not tested | REVIEW (in dev) | [ ] |
| [B-268](B-268.md) | Dead effect types: Speed Buff / Debuff refused by validation (D-056), Crit Debuff stored as Crit Chance Debuff, Energy / Random Debuff warn and are hidden in the editor | Not tested | REVIEW (in dev) | [ ] |
| [B-269](B-269.md) | Essence names in the shop: Quartz Essence, Unknown Essence | Not tested | REVIEW (in dev) | [ ] |
| [B-271](B-271.md) | One element table behind the element switches; no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-272](B-272.md) | Resistance columns read by damage type; no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-273](B-273.md) | Hit-kind struct and Ice tag under Water; no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-274](B-274.md) | Status breaks read one row each; no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-275](B-275.md) | Element-keyed immunity and DoT on hit; old members unchanged | Not tested | REVIEW (in dev) | [ ] |
| [B-276](B-276.md) | Element helpers (`IsConcrete` / `IsInherit`, one Reality Refractor check); innate element read only for Casters | Not tested | REVIEW (in dev) | [ ] |
| [B-277](B-277.md) | Small tidy-ups: cost-clamp constants named, unused hit-result flags and `EItemQuality` flagged, tier order notes | Not tested | REVIEW (in dev) | [ ] |
| [B-279](B-279.md) | Character-form meter fills on a won fight, unlocks at 100%, survives save | Not tested | REVIEW (in dev) | [ ] |
| [B-279b](B-279b.md) | Wear an unlocked character's skin (hub / rest spot); its bound gear works; Manny or Quinn | Not tested | REVIEW (in dev) | [ ] |
| [B-279c](B-279c.md) | Outfits: six slots, piece assets, authored / current outfit (save v10), Leader Pose assembly | Not built | REVIEW (in dev) | [ ] |
| [B-279d](B-279d.md) | An encounter can field a character form as its mini-boss (its look, loadout, phases) | Not tested | REVIEW (in dev) | [ ] |
| [B-282](B-282.md) | Last Stand / Sapphire numbers pinned by a test (50%, the 30% fallback, the 2..5 turn window, the revive) | Not tested | REVIEW (in dev) | [ ] |
| [B-283](B-283.md) | Emerald bonus-turn delays 6..0 and S giving two turns, and Extra Action, pinned by a test | Not tested | REVIEW (in dev) | [ ] |
| [B-284](B-284.md) | The order of the turn-start steps and the ritual fire order pinned step by step (swapping two steps fails a test); slow, ~1100 frames per ritual step | Not tested | REVIEW (in dev) | [ ] |
| [B-285](B-285.md) | Loadout budgets (Resonator 5, innate 24 / 48) pinned by a test | Not tested | REVIEW (in dev) | [ ] |
| [B-286](B-286.md) | Break sizes (Burn 8%, Earth 30%, Water 30%, Reality 25%) as named constants, pinned by a test | Not tested | REVIEW (in dev) | [ ] |
| [B-287](B-287.md) | Broken Darkness absorption, overload and forbidden-cast numbers pinned by a test | Not tested | REVIEW (in dev) | [ ] |
| [B-288](B-288.md) | Every ladder rung pinned by a spec; Damage Taken Down cap named; no gameplay change | Not built | REVIEW (in dev) | [ ] |
| [B-289](B-289.md) | Turn speed 10 / 15 / 20 and every tie-break step pinned by a test | Not tested | REVIEW (in dev) | [ ] |
| [B-290](B-290.md) | World-stat vendor price and cap, merge, spell essence and drop-quality numbers pinned by a test | Not tested | REVIEW (in dev) | [ ] |
| [B-291](B-291.md) | Balance-sim seams: InstantImpacts / AI think-delay console variables (off by default), round and bar-break events | Not tested | REVIEW (in dev) | [ ] |
| [B-292](B-292.md) | Balance sim runner, recorder, archetypes (tests only; nothing in play changes) | Not tested | REVIEW (in dev) | [ ] |
| [B-293](B-293.md) | Balance targets, reports, GAS parity, `wor.Balance.Run`; no gameplay change | Pending | REVIEW (in dev) | [ ] |
| [B-293b](B-293b.md) | GAS parity with status breaks; test code only | Not built | REVIEW (in dev) | [ ] |
| [B-293c](B-293c.md) | Balance first pass: AI defends less in even steps; difficulty scales enemy damage, not HP; normal fights in party rounds | Not built | REVIEW (in dev) | [ ] |
| [B-293d](B-293d.md) | Crown's balance answers: rank HP x4 / x8, requirement floor 0.25, 10% damage floor | Not built | REVIEW (in dev; done-when unmet: the pillar-6 band still fails, floor 0.25 under the ladder's lowest rung 0.32, needs Crown) | [ ] |
| [B-294](B-294.md) | Colour-blind element palette: new element colours in bars, tints and glows | Not tested | REVIEW (in dev) | [ ] |
| [B-295](B-295.md) | An Emerald bonus turn is a real extra turn (no later turn lost, D-052) | Not tested | REVIEW (in dev) | [ ] |
| [B-296](B-296.md) | Generic spells cost Quartz essence like elemental ones (D-105) | Not tested | REVIEW (in dev) | [ ] |
| [B-297](B-297.md) | 2:1 turn cap: a fast combatant gets at most 2 turns per opponent turn | Not tested | REVIEW (in dev) | [ ] |
| [B-297b](B-297b.md) | Emerald bonus turns go past the 2:1 cap (Q-297 A); no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-298](B-298.md) | Player-facing Ring is now Adapter; adapter Form field (D-107) | Not tested | REVIEW (in dev) | [ ] |
| [B-300](B-300.md) | On Turn Start / End triggers fire every turn (not just once) | Not tested | REVIEW (in dev) | [ ] |
| [B-301](B-301.md) | Unified loadout 1: one gear-instance type (not used yet) | Not tested | REVIEW (in dev) | [ ] |
| [B-302](B-302.md) | Unified loadout 2: slot list mirrored beside the old fields; `wor.Loadout.CompareSlots` | Not tested | REVIEW (in dev) | [ ] |
| [B-303](B-303.md) | Unified loadout 3: one class table and one slot cost (budget unchanged except Q-303b) | Not tested | REVIEW (in dev) | [ ] |
| [B-304](B-304.md) | Unified loadout 4: one active-slot rule; empty slot falls back (Q-304); secondary weapon spell checks | Not tested | REVIEW (in dev) | [ ] |
| [B-305](B-305.md) | One weapon / adapter switch behind both menu buttons; no behaviour change | Not tested | REVIEW (in dev) | [ ] |
| [B-306](B-306.md) | One gear rule for stats, resistances and effects (Q-244); two Generic fixes | Not tested | REVIEW (in dev) | [ ] |
| [B-307](B-307.md) | One infusion-source resolver; broken gives no element everywhere (CL-24) | Not tested | REVIEW (in dev) | [ ] |
| [B-308](B-308.md) | Save v5: one gear list; older saves still load | Not tested | REVIEW (in dev) | [ ] |
| [B-310](B-310.md) | Run wear follows the item's GUID: two identical adapters keep their own wear | Not tested | REVIEW (in dev) | [ ] |
| [B-311](B-311.md) | Your own loadouts (up to 5 per character) are kept and saved; the character's own loadouts stay as authored | Not tested | REVIEW (in dev) | [ ] |
| [B-312](B-312.md) | Gear store step 1: GUID index, member views, `wor.Inventory.CompareStore`; no gameplay change | Pending | REVIEW (in dev) | [ ] |
| [B-318](B-318.md) | Co-op guests read effects from the server's GAS read (UseGAS 1) | Not built | REVIEW (in dev) | [ ] |
| [B-319](B-319.md) | A duplicate weapon / adapter: weaker copy to Gold or dismantled (`wor.UI.Duplicates`) | Not tested | REVIEW (in dev) | [ ] |
| [B-320](B-320.md) | A loadout's primary can't be dismantled; an empty active primary is filled (D-116) | Not tested | REVIEW (in dev) | [ ] |
| [B-321](B-321.md) | Each character has its own gear; an old save gives each piece to its wearer, else the leader | Not tested | REVIEW (in dev) | [ ] |
| [B-322](B-322.md) | A 10-slot pouch per character; save v9; older save's pouch to the leader | Pending | REVIEW (in dev) | [ ] |
| [B-323](B-323.md) | Give a weapon, adapter or pouch stack between your characters; `wor.UI.Give` | Pending | REVIEW (in dev) | [ ] |
| [B-324](B-324.md) | Unworn gear in the shared chest, any member takes it; `wor.UI.Storage` | Pending | REVIEW (in dev) | [ ] |
| [B-337d](B-337d.md) | Skill-effect conditions check through the shared conditions (HP / EP percent) | Pending | REVIEW (in dev) | [ ] |
| [B-366e](B-366e.md) | Gear repair costs the currency of the place | Pending | REVIEW (in dev) | [ ] |
| [B-367](B-367.md) | Only a Perfect Guard / Perfect Dodge stops an interrupt point; Flawless Parry at an interrupt | Pending | REVIEW (in dev) | [ ] |
| [B-096](B-096.md) | Evolutions' Run Payout Bonus Percent raises the run-end payout (adds up, cap x2.0) | Pending | REVIEW (in dev, `feature/B-096-payout-passives`) | [ ] |
| [B-129](B-129.md) | Mapping-context priorities named (`WoRInputPriority`); no change in play | Pending | REVIEW (in dev, `feature/B-129-input-priorities`) | [ ] |
| [B-097](B-097.md) | Class gates retired (D-027): helpers deleted, two Blueprint functions flagged; no behaviour change | Pending | REVIEW (in dev, `feature/B-097-class-gates-cleanup-cloud`) | [ ] |
| [B-098](B-098.md) | Weapons give no spells (D-027); weapon spells not saved (save v37); gem spell slots become ability slots | Pending | REVIEW (in dev, `feature/B-098-weapons-no-spells-cloud`) | [ ] |
| [B-239](B-239.md) | Buy +1 item slot per character at the hub (`wor.UI.ItemSlots`) | Pending | REVIEW (in dev, `feature/B-239-item-slot-upgrades`) | [ ] |
| [B-434](B-434.md) | A checkpoint copy claiming over 40 chunks is refused (2-player: the checkpoint still arrives) | Pending | REVIEW (in dev, `fix/B-434-checkpoint-chunk-limit`) | [ ] |
| [B-429](B-429.md) | A refused item costs no action (refused at validation); items still work | Pending | REVIEW (in dev, `fix/B-429-refused-item-validate`) | [ ] |
| [B-427](B-427.md) | A guest's shop Confirm and repair greyed out with the reason (2-player) | Pending | REVIEW (in dev, `fix/B-427-guest-buy-greyed`) | [ ] |
| [B-432](B-432.md) | Buy-for-member checks the receiver's room only | Pending | REVIEW (in dev, `fix/B-432-buy-for-capacity`) | [ ] |
| [B-426](B-426.md) | Run Gold cleared for every player, not only the host (2-player) | Pending | REVIEW (in dev, `fix/B-426-clear-run-gold-all`) | [ ] |
| [B-425](B-425.md) | A co-op guest's saved wallet reaches the host once and is never overwritten by the host's copy (2-player) | Pending | REVIEW (in dev, `fix/B-425-guest-wallet`) | [ ] |
| [B-433](B-433.md) | A guest's map-spot or fragment memory find needs its character near a spot or fragment (2-player) | Pending | REVIEW (in dev, `fix/B-433-memory-report-reach`) | [ ] |
| [B-428](B-428.md) | Host hand-over chest records follow their player; a leaver's dropped Prisms split among the rest (3-player) | Pending | REVIEW (in dev, `fix/B-428-handover-rekey`) | [ ] |
| [B-435](B-435.md) | Same-element infusion sources stack: each pays its cost, buildup +25% per extra up to +50%; spells don't (Q-435) | Pending | REVIEW (in dev, `feature/B-435-same-element-stacking`) | [ ] |
| [B-437](B-437.md) | The Ability Stone is removed (D-175): no stone adds ability slots, save v38 wipes older saves; editor step B-437b | Pending | REVIEW (in dev, `feature/B-437-remove-ability-stone`) | [ ] |
| [B-443](B-443.md) | The hub HUD (D-181): interact prompt and money chip; the old debug hints go after this passes | Pending | REVIEW (in dev, `feature/B-443-hub-hud`) | [ ] |
| [B-442](B-442.md) | The character sheet (D-181): pillars, sub-stats, resistance by damage type | Pending | REVIEW (in dev, `feature/B-442-character-sheet`) | [ ] |
| [B-440a](B-440a.md) | The loadout screen, piece 1 (D-181, D-180): the gear page, read only | Pending | REVIEW (in dev, `feature/B-440-loadout-screen`) | [ ] |
| [B-439](B-439.md) | The game menu (D-181): pages, party, side panel; a quit run loses everything (D-176); editor step: delete the Blueprint's `IA_ToggleMenu` nodes | Pending | REVIEW (in dev, `feature/B-439-pause-menu`) | [ ] |

---

## Swept

| Date | Rows | Branch |
|---|---|---|
| | | |
