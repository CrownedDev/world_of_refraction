---
name: work-backlog
description: Work through docs/Backlog.md autonomously, job by job, without stopping on pending design decisions. Use when Crown says "work the backlog", "do the next jobs", "carry on", or names backlog IDs (B-xxx). Optional argument - number of jobs or specific IDs.
---

# Work the backlog

You are in **Backlog mode** (CLAUDE.md, "Backlog mode"). Follow these steps exactly.

## 0. Sync
- `git fetch origin main` and start from an up-to-date `main`.
- Read `docs/Backlog.md` and `docs/DesignDecisions.md`.
- **Apply answers first:** for every decision marked `ANSWERED`, set the jobs it blocks back to `TODO`. Note in its "Notes for Claude" anything you'll do differently.

## 1. Pick
- Pick the **first `TODO`** whose `Needs` are all `DONE` (or `REVIEW` when the job only builds on that branch) and whose decisions aren't `OPEN`.
- **Skip** jobs marked `Who = Crown`.
- If the argument names IDs or a count, honour it.
- If nothing is pickable, go to step 6.

## 2. Branch
`git checkout -b <prefix>/B-xxx-short-name main`, using `fix/`, `refactor/`, `feature/` or `chore/`.

Mark the job `IN PROGRESS (<branch>)` in `docs/Backlog.md` and commit that line on the branch.

## 3. Build it
- Read the code before changing it; cite `file:line` in your reasoning.
- Work in clusters of **≤3 files**, committing after each, with detailed messages.
- **Hit a design question?**
  1. Don't guess.
  2. Add a `D-xxx` entry in `docs/DesignDecisions.md` (format at the top of that file), with a recommendation.
  3. Mark the job `BLOCKED (D-xxx)` and commit.
  4. Push the branch (work so far is kept).
  5. Go back to step 1 and pick another job.
- Add or extend tests (Automation Spec under `Source/world_of_refraction/Private/Tests/`) for any logic you touch. Add debug utilities per CLAUDE.md for any new system.

## 4. Prove it
1. `Scripts/ue/build.sh`. Must pass; fix and repeat.
2. `Scripts/ue/test.sh`. Must pass; fix and repeat.
3. If `UE_ROOT` isn't set (the scripts say so), you **cannot** prove it. Say so in the hand-off and mark the job `REVIEW (branch, UNBUILT)`.
4. Run the **`wor-reviewer`** agent on the branch. Fix every **[must fix]**, then re-run the reviewer once.

## 5. Hand off
- Set the job to `REVIEW (<branch>)` in `docs/Backlog.md`.
- Add a changelog line to the relevant `docs/Architecture/*.md`.
- Final commit message ends with a **"Crown: check in PIE"** list: what to look at, in plain words.
- `git push -u origin <branch>`.
- **Never push to `main` and never merge.**
- Next job: back to step 1.

## 6. Stop and report
Stop when the requested number of jobs is done, or only blocked/Crown jobs remain. Report in plain words:
- **Done (for review):** ID, branch, what to check in PIE.
- **Blocked:** ID, the D-xxx question in one line.
- **New decisions raised:** D-xxx, one line each.
- **Next pickable job.**
