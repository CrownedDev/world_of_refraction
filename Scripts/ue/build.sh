#!/usr/bin/env bash
# Compile the editor target. Exit 0 = success, 1 = compile failed, 3 = UE_ROOT missing.
# Note: fails with "Live Coding is active" if the editor is open with Live Coding on -
# close the editor or press Ctrl+Alt+F11 there instead.
set -uo pipefail
source "$(dirname "$0")/common.sh"
wor_require_ue || exit $?

LOG="$WOR_LOG_DIR/build.log"
echo "Building $WOR_EDITOR_TARGET $WOR_PLATFORM $WOR_CONFIG ... (log: $LOG)"
cmd.exe //c "$UE_ROOT/Engine/Build/BatchFiles/Build.bat" "$WOR_EDITOR_TARGET" "$WOR_PLATFORM" "$WOR_CONFIG" \
    "-Project=$WOR_PROJECT" -WaitMutex -NoHotReloadFromIDE > "$LOG" 2>&1
STATUS=$?

if [ $STATUS -ne 0 ]; then
    echo "BUILD FAILED (exit $STATUS). Errors:" >&2
    grep -E "error [A-Z]+[0-9]+|error:|Error:" "$LOG" | head -40 >&2
    exit 1
fi
echo "BUILD OK"
