#!/usr/bin/env bash
# Shared setup for the UE scripts. Runs under Git Bash on Windows (what Claude Code uses there).
# Requires UE_ROOT, e.g.  export UE_ROOT="C:/Program Files/Epic Games/UE_5.8"
# (set it once as a Windows user environment variable).

WOR_ROOT="$(git rev-parse --show-toplevel)"
WOR_PROJECT="$WOR_ROOT/world_of_refraction.uproject"
WOR_EDITOR_TARGET="world_of_refractionEditor"
WOR_PLATFORM="Win64"
WOR_CONFIG="Development"
WOR_TEST_FILTER="${WOR_TEST_FILTER:-WoR.}"  # Automation test prefix: every WoR test is named "WoR.<System>.<Case>" (the dot keeps engine tests like "Framework" out of a substring match)
WOR_LOG_DIR="$WOR_ROOT/Saved/ClaudeLogs"

wor_require_ue() {
    if [ -z "${UE_ROOT:-}" ]; then
        echo "UE_ROOT is not set - cannot build or test. Set it to the engine folder (e.g. C:/Program Files/Epic Games/UE_5.8)." >&2
        return 3
    fi
    if [ ! -d "$UE_ROOT/Engine" ]; then
        echo "UE_ROOT='$UE_ROOT' has no Engine/ folder." >&2
        return 3
    fi
    mkdir -p "$WOR_LOG_DIR"
}
