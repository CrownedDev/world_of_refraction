#!/usr/bin/env bash
# Read-only bone sampler (read part of B-373): per-frame bone positions/rotations/speed of anim sequences to JSON.
# Runs Scripts/editor/sample_bones.py headless; the open editor can stay open. Nothing is modified or saved.
# Usage: Scripts/ue/sample_bones.sh <out_dir> <anim_path> [<anim_path> ...] [--bones root,pelvis,hand_r]
# WOR_SAMPLE_PROJECT=<path to .uproject> samples another checkout (e.g. the main one, where the vendor packs live).
set -uo pipefail
source "$(dirname "$0")/common.sh"
wor_require_ue || exit $?

if [ $# -lt 2 ]; then
    sed -n 4p "$0" >&2
    exit 2
fi

PROJECT="${WOR_SAMPLE_PROJECT:-$WOR_PROJECT}"
SCRIPT="$WOR_ROOT/Scripts/editor/sample_bones.py"
LOG="$WOR_LOG_DIR/sample_bones.log"
echo "Sampling $(( $# - 1 )) anim(s) from $PROJECT ... (log: $LOG)"
"$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PROJECT" -run=pythonscript -script="$SCRIPT $*" \
    -unattended -nullrhi -nosplash -nosound -NoP4 -abslog="$LOG" > /dev/null 2>&1

grep -E "\[BoneSampler\]" "$LOG" | sed -E 's/^.*\[BoneSampler\]/[BoneSampler]/'
grep -qE "LogPython: Error|\[BoneSampler\].*(not an AnimSequence|usage)" "$LOG" && exit 1
exit 0
