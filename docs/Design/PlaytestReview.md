# Playtest review: record PIE, have Claude review it

**Status:** proposal (investigation only, 2026-11-13). Nothing installed, no code. Crown decides whether to set it up.
**Checks against:** `FeelGuide.md` (the 14 moments), `docs/analysis/MaelleCombatPresentationStudy.md` (E33 shot table and timings), D-068 (60 fps at 1080p).

## 0. The idea in four lines

1. Crown records a short PIE session (video) and turns on `stat unit` + a frame-time CSV in the same run.
2. A Claude session cuts the video into pictures (ffmpeg) and reads them, plus the CSV numbers.
3. It writes a numbered findings list with timestamps (format in §5).
4. Crown triages: each finding becomes a Backlog job or a `Q-` entry.

Claude **cannot watch video**. It reads still images, so everything below is about turning a recording into good stills. Same method as the E33 YouTube study (frames stepped about every 1 s, denser around key moments), just with our own game.

## 1. Recording (Crown, Windows 11)

| Tool | Use it when | Notes |
|---|---|---|
| **Xbox Game Bar** (`Win+Alt+R` start/stop, `Win+G` panel) | Smallest first version. Already installed. | Records the focused window. Settings → Gaming → Captures: **60 fps, "High" video quality**, video only is fine. Files land in `Videos\Captures`. |
| **OBS Studio** (free) | Later, if Game Bar drops frames or you want a key overlay. | Output: **MP4 or MKV, NVENC H.264, 1920×1080, 60 fps, CBR ~15 Mbps**. Source: Window Capture (or Game Capture) on the PIE window. |

Rules for a clean recording:

- **Run PIE as "Standalone Game"** (or a separate PIE window), not the docked editor viewport, so editor panels and the editor's own frame cost stay out of the picture. In-editor frame times are worse than a packaged build: label the report "PIE".
- Set the game window to **1920×1080** (D-068 target). Fixed, so frame sizes match run to run.
- **Short clips: 1–3 minutes, one moment each** (e.g. one full fight, or fight start only). Name them `YYYY-MM-DD_<moment>.mp4` (moment = a FeelGuide row).
- Size: 1080p60 at ~15 Mbps is **about 110 MB per minute** (a 3 min clip is ~330 MB). Keep clips outside the repo (no git, no LFS). Game Bar "High" lands in the same range.
- Keep `stat unit` visible (§3) so the numbers are burnt into the video.
- Don't record other windows or anything private: the session reads every frame it extracts.

## 2. Analysis: recording → reviewable frames

**Installed here (checked 2026-11-13):** Python 3.14 only. **No ffmpeg, no ffprobe, no OpenCV, no yt-dlp, no OBS.** Nothing installed by this investigation. Needs Crown's OK to install ffmpeg (one command, `winget install Gyan.FFmpeg`, ~100 MB, no admin games). Without ffmpeg there is no way to extract frames.

Once ffmpeg exists, a session runs these (paths are examples; work in a scratch folder):

| Goal | Command (sketch) |
|---|---|
| Facts first (length, fps, size) | `ffprobe -v error -show_entries stream=width,height,r_frame_rate,duration clip.mp4` |
| Overview: contact sheet, 1 frame / 2 s, 6×5 tiles | `ffmpeg -i clip.mp4 -vf "fps=0.5,scale=320:-1,tile=6x5" sheet_%02d.png` |
| Cut points (camera cuts, shatter, scene changes) | `ffmpeg -i clip.mp4 -vf "select='gt(scene,0.25)',showinfo" -vsync vfr cut_%03d.png` (timestamps from `showinfo`) |
| Slow segment (e.g. a hit, 20 fps for 3 s from 0:42) | `ffmpeg -ss 42 -t 3 -i clip.mp4 -vf "fps=20,scale=640:-1" seg_%03d.png` |
| Frozen/duplicate frames (hitch candidates) | `ffmpeg -i clip.mp4 -vf "freezedetect=n=-60dB:d=0.05" -f null -` (lists freeze start/duration) |
| Black frames (fade/transition gaps) | `ffmpeg -i clip.mp4 -vf blackdetect=d=0.05 -f null -` |
| UI readability crop (e.g. HUD bottom strip at full res) | `ffmpeg -ss 42 -i clip.mp4 -frames:v 1 -vf "crop=1920:300:0:780" ui.png` |

Method (two passes, so cost stays low):

1. **Pass 1, overview:** probe + contact sheets + `freezedetect` + scene cuts. Produces a timeline of "interesting" timestamps.
2. **Pass 2, zoom:** slow segments only around those timestamps and around the moments the FeelGuide cares about (hit, parry, camera cut, shatter, menu open). Read at 640 px wide; a full-res crop only for UI text.

Limits: 60 fps video lets the review resolve **~17 ms** steps. Compression can hide tiny shake or flicker. Game Bar/OBS capture can itself drop a frame, so a lone single-frame freeze is not proof of a game hitch: confirm in the CSV.

## 3. What video can't measure, and the Unreal side

Video shows what happened. It can't show *why* or the true frame time.

| Can't measure from video | Use instead |
|---|---|
| Real frame time, GPU vs game-thread cost | `stat unit` (on screen: Frame / Game / Draw / GPU ms), `stat fps` |
| Exact hitch size and when | CSV profiler (below) |
| Which function is slow | Unreal Insights (markers already exist: B-266 / D-068, see `Architecture/Logging.md`) |
| Input → response delay | Only roughly: see below |
| Audio sync / sound feel | Not covered (Claude reads images only). Crown notes it by hand. |

**Simplest way to get numbers during the same play (two steps, Crown):**

1. In the PIE console: `stat unit` (add `stat fps`). Both are burnt into the recording, so every frame carries its own ms reading. Budget at 60 fps is **16.7 ms**.
2. Before the fight: `csvprofile start`. After: `csvprofile stop`. (Or launch with `-csvprofile`.) The CSV lands under `Saved/Profiling/CSV/`. A session then lists every frame over 16.7 ms (and over 33 ms = "dropped one") with its row number and the Game/Render/GPU split. Python can do this, no extra tools.

Timestamp sync: say "start" out loud or press a key with a visible effect right after `csvprofile start`, so video time 0 and CSV frame 0 line up within a second. Hitch rows are matched by the freeze list and `stat unit` spikes in the video.

**Deeper dive only when the CSV shows a real hitch:** `-trace=cpu,frame,bookmark` then open the `.utrace` in Unreal Insights, or `-tracefile` at launch. Crown opens Insights; a session can't read it without a screenshot of the timing view.

**Input-to-response:** video can only show it if the key press is visible. Cheapest: Crown says "press" in an OBS key-overlay clip, or hold one clip where the input is a clearly visible UI action (menu button → highlight moves). Result is good to **~2 frames (≈33 ms)**, enough to catch "feels laggy" (> ~100 ms), not to tune. Real latency numbers would need a game-side input timestamp (a later, separate job: not proposed now).

## 4. Review checklist (what the session looks at)

Per clip, tied to FeelGuide rows:

- **Smoothness:** freezes/duplicate-frame runs; `stat unit` spikes; frames over 16.7 / 33 ms in the CSV; hitches that line up with fight start (row 1), camera moves (row 2), hits (row 4), menu opens (row 13).
- **Animation:** pops (a pose jumps between two frames), foot-slide in place, wind-up / contact / recovery visible and in order (row 14), flinch on hit (row 4).
- **Camera:** follows the acting character (row 2); cuts land on an action, not mid-motion; no frame where the subject leaves the shot; cast-only cinematic. Compare shot types against the E33 shot table.
- **Hit weight:** freeze, shake, flinch, damage number present and scaling for weakness/crit/break (row 4); **no** freeze on perfect defense (row 3).
- **UI timing and readability:** HP/EP/turn order always visible (row 5); damage number colour readable on the background; text legible at 1080p crop; menu two-press flow (row 13); popups appear and fade on their own (row 6).
- **Flow:** fight start under the crystal-break rule (row 1); zone/load under 1 s (row 10); win return (row 6).
- **Input delay:** visible lag between a press and its response, ±2 frames.

## 5. Report format

File: `docs/PIE/Review_<date>_<moment>.md` (or the notes folder if Crown wants it out of the repo).

```
# Playtest review: <moment>, <clip name>, <date>
Build/branch: <branch @ sha>   Clip: <len>, <fps>, <res>   Run: PIE standalone
Numbers: avg <ms> / worst <ms>; frames > 16.7 ms: N; > 33 ms: N  (CSV: <file>)
Not checked: <audio, input delay beyond 2 frames, ...>

## Findings
1. [Hitch] 0:42.3, 120 ms freeze at first hit. CSV frame 2531: Game 118 ms. FeelGuide row 4. Suggest: Insights on hit path. (frames: seg_042.png)
2. [Camera] 1:05.0 cut mid-swing, subject leaves frame. Row 2. E33 table: cut after contact. (frames: ...)
3. [UI] 1:20 damage number white on pale floor, hard to read. Row 5.
...
## What looked good
- short list, so regressions are visible next time.
```

Each finding: **number, type tag, timestamp, one-line what, which FeelGuide/E33 row, evidence (frame files or CSV frame), severity (blocker / should fix / polish)**. Timestamps are `m:ss.s` in the video. Findings are observations; fixes are Crown's call (design decisions stay out, they go to `DesignDecisions.md`).

## 6. Cost and smallest first version

**Per review (rough):**

| Step | Who | Cost |
|---|---|---|
| Record + `csvprofile` | Crown | 5 min per clip |
| Pass 1 (probe, 2–3 contact sheets, freeze list, cuts) | session | ~5–10 images, ~15–25k tokens |
| Pass 2 (slow segments around 5–10 timestamps, ~3 images each) | session | ~15–30 images, ~30–60k tokens |
| CSV hitch list + report | session | ~10k tokens |
| **Total** | | **~60–100k tokens**, one Sonnet-level session, 3 min clip. Keep to 1–2 reviews per budget window. |

Cheaper levers: shorter clips, one moment per clip, 640 px frames, crops for UI, skip pass 2 when pass 1 is clean.

**Smallest first version (suggested):**

1. Crown OKs `winget install Gyan.FFmpeg` (the only missing tool).
2. Crown records **one** 2-min fight with Game Bar at 60 fps, `stat unit` on, and `csvprofile start/stop` around it.
3. A session runs §2 pass 1 + 2, writes the §5 report. Crown checks whether the findings match what they felt.
4. If useful: save the commands as a `Scripts/review/` helper and a `playtest-review` skill; add OBS and a key overlay only if Game Bar proves lossy.

**Not in v1:** audio review, packaged-build runs, automated regression comparison between clips, game-side input timestamps.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-13 | Proposal written (investigation only). | claude/docs-playtest-review-dd25b5 |
