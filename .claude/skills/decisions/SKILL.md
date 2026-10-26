---
name: decisions
description: Walk Crown through the open questions in docs/DesignDecisions.md one at a time, in plain words, and record the answers. Use when Crown says "let's do the decisions", "what's waiting on me", "answer D-xxx", typically from a Claude cloud session on phone or web.
---

# Answer open design decisions

Goal: Crown answers questions quickly; the answers land in the doc so the local backlog run can pick them up.

1. `git fetch origin main`, then read `docs/DesignDecisions.md` on `main`.
2. List the `OPEN` items in one short line each, with what each blocks, and ask which to start with. Default: the ones that block the most jobs.
3. **One decision at a time:**
   - Explain it in plain words: what happens today, the options, your recommendation and why.
   - Back the recommendation with **proven, well-received examples from other games** (what worked), unless it's a lore question (then no recommendation).
   - No jargon. If a term is unavoidable, explain it in a few words.
   - If Crown wants to think, mark it skipped and move on.
4. **Recording each answer:**
   - set **Status** to `ANSWERED`
   - put the answer in Crown's words under **Answer (Crown)**
   - add any **Notes for Claude**
   - leave **Blocks** as it is (the backlog run unblocks the jobs)
5. Commit to a `docs/decisions-YYYY-MM-DD` branch, one commit per batch, message listing the D-IDs answered.
6. **Merge to `main` only if Crown says so** (decisions are docs-only, so Crown may say "merge" directly).
7. Finish with: answered, skipped, still open.

Never change code here, never decide on Crown's behalf, and never move an item to `DECIDED`. That happens when the backlog run applies it.
