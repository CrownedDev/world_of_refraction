---
name: work-backlog
description: Work through docs/Backlog.md autonomously, job by job, without stopping on pending design decisions. Use when Crown says "work the backlog", "do the next jobs", "carry on", or names backlog IDs (B-xxx). Optional argument - number of jobs or specific IDs.
---

# Work the backlog

You are in **Backlog mode** (CLAUDE.md, "Backlog mode"). Follow these steps exactly.

## 0. Sync
- `git fetch origin` and `git checkout dev && git pull --ff-only origin dev`. Jobs start from an up-to-date **`dev`**, never `main`.
- Read `docs/Backlog.md` and `docs/DesignDecisions.md`.
- **Apply answers first:** for every decision marked `ANSWERED`, set the jobs it blocks back to `TODO`. Note in its "Notes for Claude" anything you'll do differently.

## 1. Pick
- **On a machine with Unreal (`UE_ROOT` set): first finish every `REVIEW (branch, UNBUILT)` job**, oldest first (Crown, 2026-10-27, cloud + desktop split): check out its branch, merge the latest `dev` into it, then do steps 4 and 5 (build, test, reviewer, hand off, merge). Only then pick new jobs.
- **In a cloud session (no Unreal):** work as normal but stop at step 4.3: push the branch, mark the job `REVIEW (branch, UNBUILT)`, and **don't merge into `dev`**. Skip a job whose `Needs` include a job that is still `UNBUILT`. Docs-only jobs (no C++, no config) are merged as usual.
- Pick the **first `TODO`** whose `Needs` are all `DONE` or `REVIEW (in dev)` and whose decisions aren't `OPEN`.
- **Skip** jobs marked `Who = Crown`.
- **Skip** jobs marked `Who = CC+UE` unless the Unreal editor is open and unreal-mcp answers (try one read-only call). Never edit `.uasset`/`.umap` any other way.
- If the argument names IDs or a count, honour it.
- If nothing is pickable, go to step 6.

## 2. Branch
`git checkout -b <prefix>/B-xxx-short-name dev`, using `fix/`, `refactor/`, `feature/` or `chore/`.

Mark the job `IN PROGRESS (<branch>)` in `docs/Backlog.md` and commit that line on the branch.

## 3. Build it
- Read the code before changing it; cite `file:line` in your reasoning.
- Work in clusters of **≤3 files**, committing after each, with detailed messages.
- **Hit a design question?** If it's **not lore** and the reference-backed answer fits `docs/Design/DesignPillars.md`: record a `Q-<job>` entry answered with that default (cite the games), build with it, note it in the hand-off, and carry on. Otherwise (lore/story, or it would break a ruling or pillar):
  1. Don't guess.
  2. Add a **`Q-<job>`** entry in `docs/DesignDecisions.md` named after **this job's number** (B-155 → Q-155; a second question Q-155b), format at the top of that file, with a recommendation. Never take a `D-` number: those are Crown's chat decisions.
  3. Mark the job `BLOCKED (Q-<job>)` and commit.
  4. Push the branch (work so far is kept).
  5. Go back to step 1 and pick another job.
- Add or extend tests (Automation Spec under `Source/world_of_refraction/Private/Tests/`) for any logic you touch. Add debug utilities per CLAUDE.md for any new system.
- **Needs UI to try it?** If the player does it through a screen that doesn't exist yet (party, interact, choose, buy…), add a plain placeholder UI in C++ (buttons + text, no `.uasset`) so Crown can do it in PIE; say in the PIE file how to open it. The real CommonUI screen replaces it later (D-024).

## 4. Prove it
1. `Scripts/ue/build.sh`. Must pass; fix and repeat.
2. `Scripts/ue/test.sh`. Must pass; fix and repeat.
3. If `UE_ROOT` isn't set (the scripts say so), you **cannot** prove it. Say so in the hand-off and mark the job `REVIEW (branch, UNBUILT)`.
4. Run the **`wor-reviewer`** agent on the branch. Fix every **[must fix]**, then re-run the reviewer once.

## 5. Hand off
Only once step 4 is fully green (build, tests, reviewer).
1. Set the job to `REVIEW (in dev)` in `docs/Backlog.md`.
2. Add a changelog line to the relevant `docs/Architecture/*.md`.
3. Write `docs/PIE/B-xxx.md` in the format of `docs/PIE/B-001.md`: branch, what tests already prove, setup, then a table of numbered checks (**Do this** / **You should see** / **Result** `[ ]`). Add a row for it under **Pending** in `docs/PIE/README.md`.
4. Append a plain-words entry to `docs/sessions/<today>.md` (create it from `_TEMPLATE.md` if missing): what changed, why, how it was verified, what Crown should check.
5. Final commit message ends with a **"Crown: check in PIE"** list, pointing at the PIE file.
6. `git push -u origin <branch>`.
7. Merge into dev with a real message, never `--no-edit`:
   `git checkout dev && git pull --ff-only origin dev && git merge --no-ff -m "Merge B-xxx: <what it does> — tests green, PIE pending" <branch> && git push origin dev`.
   If the merge conflicts, resolve it, rebuild and retest before pushing.
- **Never commit to, merge into, or push `main`.** Crown merges `dev → main` after play-testing.
- Next job: back to step 1 (branch from the updated `dev`).

## 6. Stop and report
Stop when the requested number of jobs is done, or only blocked/Crown jobs remain. Report in plain words:
- **Done (in dev, for review):** ID, branch, its `docs/PIE/B-xxx.md`.
- **Blocked:** ID, the Q-xxx question in one line.
- **New questions raised:** Q-xxx, one line each.
- **Next pickable job.**
