# Cloud clean-up status — 2026-11-15

Cloud writer, no Unreal: every branch below is **unbuilt**. The desktop merger builds, tests and merges each one into `dev` (never `main`). Each branch was made from `origin/dev` on 2026-11-15 unless it says it stacks on another. Old 2026-11-09 / 11-14 cloud branches of the same jobs are stale: merge the `-2` branch, not the old one.

| Job | Branch | Stacks on | What | State |
|---|---|---|---|---|
| B-170h2 | `refactor/B-170h2-speed-timeline-delete-cloud-2` | dev | Speed timeline code, `wor.Turns.ActionPips` and the old-timeline specs deleted (the 11-14 branch replayed; 4 small doc / include conflicts) | REVIEW (cloud, unbuilt) |
| B-170h2b | `refactor/B-170h2b-one-break-table-cloud-2` | B-170h2 `-2` | One break table; Wind's place delay deleted (replayed, code applied cleanly) | REVIEW (cloud, unbuilt) |
| B-408a | `feature/B-408a-closed-run-quits-cloud-2` | dev | A game closed mid-run is settled as a quit (11-14 branch replayed; fixes: the can't-restore path is a full quit (no upgrades kept, run recruits leave); "last run lost" not shown for a handed-over copy) | REVIEW (cloud, unbuilt) |
| B-034b | `refactor/B-034b-charge-manager-delete-cloud-2` | dev | `UInfusionChargeManager` deleted (11-09 branch replayed). **Merge only after** B-034's PIE and the editor's Find in Blueprints check (PIE sheet check 1) | REVIEW (cloud, unbuilt), gated |
| B-042g2 | `refactor/B-042g2-usegas-switch-delete-cloud-2` | dev | `wor.Effects.UseGAS` and the parity specs deleted; effects always read GAS (11-09 branch replayed, code applied cleanly). **Merge only after** Crown's play pass at `wor.Effects.UseGAS 1` on dev (PIE sheet section A) | REVIEW (cloud, unbuilt), gated |
| B-309b (part 1) | `refactor/B-309b-switch-forwarders-cloud` | B-170h2 `-2` | The switch forwarders and `WeaponStatsApply` go; the rest split into B-309c..f | REVIEW (cloud, unbuilt) |
| B-411 (part 1) | `refactor/B-411-weapon-spell-path-delete-cloud` | dev | The weapon spell path and `wor.Loadout.WeaponSpells` deleted; a spell left on a weapon is dropped by the rules. **Merge only after** Crown's B-098 PIE. Fields and Blueprint nodes split to B-411b (CC+UE, after B-351e) | REVIEW (cloud, unbuilt), gated |

## Skipped, and why

- **B-408b** (`FSavedRun::DropEncounters`): its row says fold it into the next job that bumps the save; none of this session's branches does. Left TODO.
- **B-043h**: needs Crown's PIE of B-043a–g; none of those sheets is ticked.
- **B-317c**: CC+UE, and needs B-312b, which is only at step 1 (29 copy reads left); its part 3 is an editor step.
- **B-351e**: its PIE gate (B-351d) isn't passed.
- **B-066**: needs the editor (the two spell assets).
- **B-042g3 to g6**: not started. g3 needs a new held-effect struct and the effect conditions moved onto the GE context (net-serialized), on top of B-042g2, which waits for Crown's play pass; g4 and g5 are L. Too much new design to write without a compiler: best done on the desktop once B-042g2 is merged.
- **B-317b**: the store's co-op move to `AWoRPlayerState` (replication, owner-only lists): not attempted blind; it can't be checked without a two-player run.

## Reviews

wor-reviewer (2026-11-15) on B-042g2 and B-408a: no must-fix; B-408a's two should-fixes applied; B-042g2's specs-to-watch added to its PIE sheet. On B-411 and B-309b: no must-fix; B-411's small fixes applied. **Merge note:** B-411 and B-309b both add to `LoadoutSystem.md` and `PIE/README.md` (doc-only conflicts); B-309b, B-170h2b stack on B-170h2 `-2`.

## Merge order suggested

1. B-170h2 `-2`, then B-170h2b `-2` and B-309b (both on it).
2. B-408a `-2` (independent).
3. Gated, when their gate passes: B-034b `-2` (B-034 PIE + Find in Blueprints), B-042g2 `-2` (play pass at `wor.Effects.UseGAS 1`), B-411 (B-098 PIE).
