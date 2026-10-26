#!/usr/bin/env bash
# Run WoR automation tests headless. Exit 0 = all passed, 1 = failures, 3 = UE_ROOT missing.
# Usage: Scripts/ue/test.sh [filter]   (default filter "WoR"; e.g. "WoR.Damage")
set -uo pipefail
source "$(dirname "$0")/common.sh"
wor_require_ue || exit $?

FILTER="${1:-$WOR_TEST_FILTER}"
REPORT="$WOR_LOG_DIR/tests"
LOG="$WOR_LOG_DIR/test.log"
rm -rf "$REPORT"

echo "Running automation tests '$FILTER' ... (log: $LOG)"
"$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$WOR_PROJECT" \
    -ExecCmds="Automation RunTests $FILTER" -TestExit="Automation Test Queue Empty" \
    -ReportExportPath="$REPORT" -unattended -nullrhi -nosplash -nosound -NoP4 -log="$LOG" > /dev/null 2>&1

INDEX="$REPORT/index.json"
if [ ! -f "$INDEX" ]; then
    echo "TESTS DID NOT RUN (no report). Last log lines:" >&2
    tail -30 "$LOG" >&2
    exit 1
fi

# The report's summary counts (field names per UE automation report format).
FAILED=$(grep -oE '"failed"[[:space:]]*:[[:space:]]*[0-9]+' "$INDEX" | head -1 | grep -oE '[0-9]+$')
SUCCEEDED=$(grep -oE '"succeeded"[[:space:]]*:[[:space:]]*[0-9]+' "$INDEX" | head -1 | grep -oE '[0-9]+$')
echo "Tests: ${SUCCEEDED:-?} passed, ${FAILED:-?} failed"

if [ "${SUCCEEDED:-0}" = "0" ] && [ "${FAILED:-0}" = "0" ]; then
    echo "No tests matched '$FILTER'." >&2
    exit 0
fi
if [ "${FAILED:-1}" != "0" ]; then
    echo "FAILED TESTS:" >&2
    grep -E "Test Completed\. Result=\{Fail(ed)?\}|Error:" "$LOG" | head -40 >&2
    exit 1
fi
echo "TESTS OK"
