# Cloud writer handover (2026-11-04)

The cloud writer pre-writes backlog jobs as **UNBUILT** branches (no Unreal in the cloud). The Unreal writer builds, tests and merges them. Decisions and backlog rows for the simplicity audit live on **`docs/D-128-136-simplicity-round2`** (rounds 1 to 3: D-120 to D-149, Q-21-audit). That branch carries round 1's `docs/D-120-127-simplicity-round1` too.

## Branches pushed (all UNBUILT unless the Backlog says otherwise), with their base

| Job | Branch | Base | State |
|---|---|---|---|
| B-293d + Q-293e | `chore/B-293d-decided-balance` | dev | merged to dev by the desktop |
| B-223b | `feature/B-223b-personal-level-drops` | dev | reviewed, fixed |
| B-325 | `feature/B-325-combat-tier` | dev | reviewed, fixed (sim weapon instance tier fix) |
| B-326 | `chore/B-326-flat-requirement-penalty` | B-293d | reviewed, fixed |
| B-327 | `fix/B-327-zone-health-once` | dev | spec only (no bug) |
| B-328 | `feature/B-328-enemy-hp-no-zone` | B-325 + B-327 + round-1 docs | D-120 widened applied |
| B-228 | `feature/B-228-turn-state-gamestate` | dev | reviewed, fixed; B-228b (client HUD) queued |
| B-229 | `feature/B-229-input-rpcs` | B-228 | reviewed, fixed |
| B-343 | `chore/B-343-guard-dodge-only` | B-229 + round-2 docs | not reviewed |
| B-313 | `refactor/B-313-pouch-per-member` | dev | reviewed; B-313b queued |
| B-314 | `refactor/B-314-run-layer` | B-313 | reviewed, fixed; B-314b queued |
| B-347 | `feature/B-347-free-run-loadout` | B-314 | reviewed, fixed; B-347b queued |
| B-316 | `feature/B-316-give-request-buyfor` | B-347 | buy-for slice, reviewed, fixed; B-316b queued |
| B-339 | `feature/B-339-inventory-screen` | B-316 | reviewed, fixed |
| B-354 | `feature/B-354-tier-caps-retire` | B-313 | reviewed; fixes queued with its writer |
| B-338 | `chore/B-338-delete-item-storage` | round-2 docs | done |
| B-330 | `feature/B-330-pillars-only` | round-1 docs | reviewed, fixed |
| B-331 | `feature/B-331-one-speed` | B-330 | reviewed, fixed; B-331b queued |
| B-332 | `docs/B-332-pie-sweep` | round-1 docs | docs only |
| B-333 | `feature/B-333-seven-balances` | round-1 docs | reviewed, fixed; B-333b queued |
| B-334 | `feature/B-334-infusion-costs` | round-1 docs | reviewed, fixed |
| B-335 | `chore/B-335-per-instance-rolls-obsolete` | round-1 docs | reviewed, fixed |
| B-344 | `feature/B-344-difficulty-sets-ai` | round-2 docs | not reviewed |
| B-345 | `feature/B-345-one-ai-path` | B-344 | reviewed, fixed |
| B-341 | `feature/B-341-turn-rules-trim` | round-2 docs | reviewed, fixed |
| B-342 | `feature/B-342-three-rows` | round-2 docs | reviewed, fixed |
| B-350 | `chore/B-350-test-actors-out` | round-2 docs | reviewed, fixed (actors flagged, editor step) |
| B-348 | `chore/B-348-one-save-version` | round-2 docs | reviewed, fixed; B-348b queued |
| B-346 | `feature/B-346-party-seats` | round-2 docs | reviewed; fixes queued with its writer |
| B-337 | `feature/B-337-conditions` | round-2 docs | reviewed; fixes queued; B-337b queued |
| B-351 | `feature/B-351-two-sockets` | round-2 docs | with D-145; under review; B-351b queued |
| B-352 | `feature/B-352-fusions-retire` | B-351 | under review |

**In progress (writers):** B-359 → B-358 → B-354 fixes; B-356 (on B-345) → B-355 (on B-337) → B-346 / B-337 fixes; B-353 (on B-352) → B-357.

**Save version order:** B-348 (11), then B-333 (12), then B-351 (next free). Each bumps LATEST = MIN (D-138).

## Queue still to pre-write
B-336 (world subsystems, D-129), B-340 (docs moves, D-132; do last), B-349 (console names, D-139), B-232 (run state / party, D-129 slice), B-042i / B-042j2 (GAS; skip if the Unreal writer started them), and the follow-up rows (B-228b, B-313b, B-314b, B-316b, B-331b, B-333b, B-337b, B-347b, B-348b, B-351b).

## Open items for Crown
- CLAUDE.md edits waiting on the user's go-ahead (not made on a peer's request): D-121 (GAS-only rule), D-138 (Saves note), D-139 (debug-tools / console naming), D-140 (Tests rule), plus `ConditionSystem.md` in the doc list.
- Defaults to confirm (Q- entries answered by the reference-backed default): Q-313, Q-314/b/c, Q-316, Q-325, Q-326, Q-328, Q-330, Q-331, Q-333, Q-334 (L1 infusion EP equals L0, no HP), Q-341/b/c, Q-342, Q-344, Q-345, Q-347, Q-348, Q-351 (stone socket costs no slot), Q-354.
- B-350: no nightly balance runner exists (nothing sets `WOR_BALANCE`); GASParity left the default run.
