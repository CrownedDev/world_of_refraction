#!/usr/bin/env bash
# Stop hook: Claude may not finish with C++ changes that don't build or pass tests.
# Exit 2 = block the stop (stderr goes back to Claude). Exit 0 = allow.
# Skips quietly when UE_ROOT is unset (nothing to prove with) or nothing under Source/ changed.
INPUT="$(cat)"
ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || exit 0
cd "$ROOT" || exit 0
STATE_DIR="$(git rev-parse --git-dir)/claude"
mkdir -p "$STATE_DIR"
ATTEMPTS_FILE="$STATE_DIR/stop-attempts"
GREEN_FILE="$STATE_DIR/last-green"
MAX_ATTEMPTS=3   # after this many blocked stops in a row, let Claude stop and report instead of looping

[ -z "${UE_ROOT:-}" ] && exit 0

# Anything C++ changed on this branch or in the working tree?
BASE="$(git merge-base HEAD origin/main 2>/dev/null || echo HEAD)"
if git diff --quiet "$BASE" -- Source && git diff --quiet -- Source && [ -z "$(git ls-files --others --exclude-standard Source)" ]; then
    exit 0
fi

# Already proven for exactly this state?
STATE_HASH="$( (git rev-parse HEAD; git diff -- Source; git ls-files --others --exclude-standard Source) | sha1sum | cut -c1-40)"
[ -f "$GREEN_FILE" ] && [ "$(cat "$GREEN_FILE")" = "$STATE_HASH" ] && exit 0

ATTEMPTS=$(cat "$ATTEMPTS_FILE" 2>/dev/null || echo 0)
if echo "$INPUT" | grep -Eq '"stop_hook_active"[[:space:]]*:[[:space:]]*true'; then
    ATTEMPTS=$((ATTEMPTS + 1))
else
    ATTEMPTS=1
fi
echo "$ATTEMPTS" > "$ATTEMPTS_FILE"
if [ "$ATTEMPTS" -gt "$MAX_ATTEMPTS" ]; then
    echo 0 > "$ATTEMPTS_FILE"
    exit 0   # give up blocking; the work-backlog skill reports the job as not proven
fi

if ! OUT="$(bash Scripts/ue/build.sh 2>&1)"; then
    printf 'Build failed - fix it before finishing (attempt %s/%s):\n%s\n' "$ATTEMPTS" "$MAX_ATTEMPTS" "$(echo "$OUT" | tail -40)" >&2
    exit 2
fi
if ! OUT="$(bash Scripts/ue/test.sh 2>&1)"; then
    printf 'Tests or data validation failed - fix them before finishing (attempt %s/%s):\n%s\n' "$ATTEMPTS" "$MAX_ATTEMPTS" "$(echo "$OUT" | tail -40)" >&2
    exit 2
fi

echo "$STATE_HASH" > "$GREEN_FILE"
echo 0 > "$ATTEMPTS_FILE"
exit 0
