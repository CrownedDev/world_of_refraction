#!/usr/bin/env bash
# Run WoR automation tests headless, then validate data assets (B-119). Exit 0 = all passed, 1 = failures, 3 = UE_ROOT missing.
# Usage: Scripts/ue/test.sh [filter]   (default filter "WoR."; e.g. "WoR.Damage")
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
    -ReportExportPath="$REPORT" -unattended -nullrhi -nosplash -nosound -NoP4 -abslog="$LOG" > /dev/null 2>&1

INDEX="$REPORT/index.json"
if [ ! -f "$INDEX" ]; then
    echo "TESTS DID NOT RUN (no report). Last log lines:" >&2
    tail -30 "$LOG" >&2
    exit 1
fi

# The report's summary counts (field names per UE automation report format).
# A test that logged warnings lands in "succeededWithWarnings", not "succeeded" — both are passes.
report_count() { grep -oE "\"$1\"[[:space:]]*:[[:space:]]*[0-9]+" "$INDEX" | head -1 | grep -oE '[0-9]+$'; }
FAILED=$(report_count failed)
SUCCEEDED=$(( $(report_count succeeded || echo 0) + $(report_count succeededWithWarnings || echo 0) ))
echo "Tests: ${SUCCEEDED} passed, ${FAILED:-?} failed"

# Per-test state, and every error message under the test that raised it.
awk -F'"' '
    /"fullTestPath"/ { path = $4 }
    /"state"/        { printf "  %-8s %s\n", $4, path }
    /"type"/         { is_error = ($4 == "Error") }
    /"message"/      { if (is_error) { printf "             ERROR: %s\n", $4 }; is_error = 0 }
' "$INDEX"

if [ "${SUCCEEDED}" = "0" ] && [ "${FAILED:-0}" = "0" ]; then
    echo "No tests matched '$FILTER'." >&2
    exit 0
fi
if [ "${FAILED:-1}" != "0" ]; then
    echo "TESTS FAILED (full log: $LOG)" >&2
    exit 1
fi
echo "TESTS OK"

# Data validation (B-119): every data asset's IsDataValid. Errors already in the baseline are known
# content debt and only listed; a new error fails the run. Set WOR_SKIP_VALIDATION=1 to skip.
[ "${WOR_SKIP_VALIDATION:-0}" = "1" ] && { echo "Data validation skipped (WOR_SKIP_VALIDATION=1)"; exit 0; }
VALIDATION_LOG="$WOR_LOG_DIR/validation.log"
VALIDATION_BASELINE="$WOR_ROOT/Scripts/ue/validation_baseline.txt"
echo "Validating data assets ... (log: $VALIDATION_LOG)"
# MSYS_NO_PATHCONV keeps Git Bash from turning /Script/... into a Windows path.
MSYS_NO_PATHCONV=1 "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$WOR_PROJECT" \
    -run=DataValidation -AssetType=/Script/Engine.PrimaryDataAsset \
    -unattended -nullrhi -nosplash -nosound -NoP4 -abslog="$VALIDATION_LOG" > /dev/null 2>&1

if ! grep -q "Commandlet DataValidationCommandlet.* finished execution" "$VALIDATION_LOG" 2>/dev/null; then
    echo "DATA VALIDATION DID NOT RUN. Last log lines:" >&2
    tail -30 "$VALIDATION_LOG" >&2
    exit 1
fi

# One line per error: "<path under Content/>: <message>", sorted, without duplicates.
CURRENT_ERRORS="$(grep -F 'AssetCheck: Error: [AssetLog]' "$VALIDATION_LOG" \
    | tr '\\' '/' | sed -e 's#.*/Content/##' -e 's#\.uasset: #: #' | tr -d '\r' | sort -u)"
KNOWN_ERRORS="$(grep -v '^#' "$VALIDATION_BASELINE" 2>/dev/null | tr -d '\r' | sed '/^$/d' | sort -u)"
NEW_ERRORS="$(comm -23 <(echo "$CURRENT_ERRORS" | sed '/^$/d') <(echo "$KNOWN_ERRORS"))"
FIXED_ERRORS="$(comm -13 <(echo "$CURRENT_ERRORS" | sed '/^$/d') <(echo "$KNOWN_ERRORS"))"
count_lines() { echo -n "$1" | grep -c '^' ; }

echo "Data validation: $(count_lines "$CURRENT_ERRORS") error(s), $(count_lines "$NEW_ERRORS") new"
[ -n "$FIXED_ERRORS" ] && echo "$FIXED_ERRORS" | sed 's/^/  FIXED    /' \
    && echo "  (remove the FIXED lines from Scripts/ue/validation_baseline.txt)"
if [ -n "$NEW_ERRORS" ]; then
    echo "$NEW_ERRORS" | sed 's/^/  NEW      /'
    echo "VALIDATION FAILED: fix the asset, or add the line to Scripts/ue/validation_baseline.txt if it is known debt" >&2
    exit 1
fi
echo "VALIDATION OK"
