# Cloud clean-up status — 2026-11-15

Cloud writer, no Unreal: every branch below is **unbuilt**. The desktop merger builds, tests and merges each one into `dev` (never `main`). Each branch was made from `origin/dev` on 2026-11-15 unless it says it stacks on another. Old 2026-11-09 / 11-14 cloud branches of the same jobs are stale: merge the `-2` branch, not the old one.

| Job | Branch | Stacks on | What | State |
|---|---|---|---|---|
| B-170h2 | `refactor/B-170h2-speed-timeline-delete-cloud-2` | dev | Speed timeline code, `wor.Turns.ActionPips` and the old-timeline specs deleted (the 11-14 branch replayed; 4 small doc / include conflicts) | REVIEW (cloud, unbuilt) |
| B-170h2b | `refactor/B-170h2b-one-break-table-cloud-2` | B-170h2 `-2` | One break table; Wind's place delay deleted (replayed, code applied cleanly) | REVIEW (cloud, unbuilt) |

## Skipped, and why

(none yet)
