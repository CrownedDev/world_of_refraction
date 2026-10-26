#!/usr/bin/env bash
# Regression test for guard.sh (B-094): feeds hook-shaped JSON for each case and checks the exit code.
# Cases file: "<expect 0|2><TAB><command exactly as it appears inside the JSON string>".
# Usage: bash Scripts/claude/guard_test.sh
DIR="$(dirname "$0")"
GUARD="$DIR/guard.sh"; CASES="$DIR/guard_test_cases.txt"
fail=0
while IFS=$'\t' read -r expect cmd; do
    cmd="${cmd%$'\r'}"   # a CRLF checkout must not add \r to the command
    [ -z "$expect" ] && continue
    printf '{"tool_name":"Bash","tool_input":{"command":"%s"}}' "$cmd" | bash "$GUARD" 2>/dev/null
    got=$?
    if [ "$got" = "$expect" ]; then echo "ok   ($got) $cmd"; else echo "FAIL (got $got, want $expect) $cmd"; fail=1; fi
done < "$CASES"
exit $fail
