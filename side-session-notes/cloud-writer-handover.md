# Cloud writer handover (2026-11-08 night run) — IN PROGRESS

Overnight cloud writer, Crown asleep. Pre-writes only (UNBUILT); the desktop writer builds, tests, reviews and merges. All branches cut from `origin/dev` at `ef63ea66`.

| Job | Branch | State | Q- defaults chosen |
|---|---|---|---|
| B-367b | `feature/B-367b-perfect-guard-only-cloud` | REVIEW (UNBUILT): `GuardRules::StopsInterruptableHit` and `FlawlessParry::IsPerfectHit` are Perfect Guard only; GuardSpec / FlawlessParrySpec updated; Rules ED-37/37b/37c, Q-168, Q-367, CombatOrchestrator changelog, PIE B-367b | none (Crown's correction, no question) |

---

# Cloud writer handover (2026-11-04) — STOPPED at a checkpoint (user, usage 67%); the main writer continues solo

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
| B-351 | `feature/B-351-two-sockets` @ 425a1787 | round-2 docs | with D-145; both review must-fixes and should-fixes done; B-351b: authored-stone editor filters, sockets/skill-list debug helper |
| B-352 | `feature/B-352-fusions-retire` @ 6c846615 | B-351 (fixes merged) | review fixes mostly done; left: merge the stacked comment in `CrystalTypeHelpers.h`, list the remaining fusion branches for the sweep, PIE note on the Blueprint Fusion pin |
| B-353 | `feature/B-353-stat-stone` @ 253b0ef8 | B-352 (before its fixes) | job done, not reviewed; left: merge the B-351 / B-352 fixes in |
| B-354 | `feature/B-354-tier-caps-retire` @ 5ce0c408 | B-313 | review fixes done |
| B-359 | `feature/B-359-weather-baseline` | round-2 docs | done, not reviewed (review stopped) |
| B-358 | `feature/B-358-one-physical-resistance` | round-2 docs | done, not reviewed (review stopped) |
| B-356 | `feature/B-356-three-schools` @ 7bddbcd3 | B-345 | done, not reviewed; editor step: re-tag Conjuration spells (Q-356) |
| B-355 | `feature/B-355-boss-rules` @ 7f3531e8 | B-337 | code + spec done, not reviewed; left: Backlog row, PIE sheet, changelog, session entry, B-355b row (moment triggers still on the old list) |
| B-337 | `feature/B-337-conditions` @ a5f1304e | round-2 docs | review fixes done; B-337b queued |
| B-346 | `feature/B-346-party-seats` @ 96679a3d | round-2 docs | review fixes NOT started (PartyControl widget "Let AI finish" row, move to `SetMemberAISeat`, PIE console note, OBSOLETE flags on `CanHandToAI` / `CreateHandToAIButton`, nits) |
| B-232 | `feature/B-232-run-state-party` | B-229 | slice done, not reviewed; B-232b queued (menu on the commanding machine, other run readers, recruit fills a spot); `SeatRules.h` copied from B-346 |

**Not reviewed at all:** B-232, B-343, B-344, B-353, B-355, B-356, B-358, B-359.

**Save version order:** B-348 (11), then B-333 (12), then B-351 (next free). Each bumps LATEST = MIN (D-138).

## Queue still to pre-write (not started)
B-357 (evolution placement, D-147 incl. the L2 correction), B-336 (world subsystems, D-129), B-349 (console names, D-139), B-340 (docs moves, D-132; do last), B-042i / B-042j2 (GAS; skip if the Unreal writer started them), and the follow-up rows (B-228b, B-313b, B-314b, B-316b, B-331b, B-333b, B-337b, B-347b, B-348b, B-351b, B-355b to be written).

## Open items for Crown
- CLAUDE.md edits waiting on the user's go-ahead (not made on a peer's request): D-121 (GAS-only rule), D-138 (Saves note), D-139 (debug-tools / console naming), D-140 (Tests rule), plus `ConditionSystem.md` in the doc list.
- Defaults to confirm (Q- entries answered by the reference-backed default): Q-313, Q-314/b/c, Q-316, Q-325, Q-326, Q-328, Q-330, Q-331, Q-333, Q-334 (L1 infusion EP equals L0, no HP), Q-341/b/c, Q-342, Q-344, Q-345, Q-347, Q-348, Q-351 (stone socket costs no slot), Q-354, Q-356 (Conjuration → Enhancement), Q-358 (physical columns fold by average), Q-359 (a phase switches to the boss's element), Q-232 (B-232 slice; a player's own character is its pawn's).
- B-350: no nightly balance runner exists (nothing sets `WOR_BALANCE`); GASParity left the default run.
