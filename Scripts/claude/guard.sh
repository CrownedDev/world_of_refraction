#!/usr/bin/env bash
# PreToolUse guard (Bash, Edit, Write). Reads the hook JSON on stdin.
# Exit 2 = block the tool call; the message on stderr goes back to Claude.
# settings.json runs `bash -n` on this file first and skips it (fail open) if it doesn't parse.
INPUT="$(cat)"

block() { echo "Blocked by project guard: $1" >&2; exit 2; }

# --- Shell commands -------------------------------------------------------
if echo "$INPUT" | grep -q '"command"'; then
    CMD="$(echo "$INPUT" | tr '\n' ' ')"
    echo "$CMD" | grep -Eq 'git[^"]*push[^"]*(--force|--force-with-lease|[[:space:]]-f([[:space:]]|"))' \
        && block "force-push is never allowed (CLAUDE.md: no history rewriting)."
    # Escape hatch for an explicit Crown request ("merge it to main"): prefix the push with
    # WOR_CROWN_APPROVED_MAIN=1. Never use it on your own initiative (CLAUDE.md, Backlog mode).
    if ! echo "$CMD" | grep -q 'WOR_CROWN_APPROVED_MAIN=1'; then
        echo "$CMD" | grep -Eq 'git[^"]*push[^"]*[[:space:]](origin[[:space:]]+)?(HEAD:)?(refs/heads/)?main([[:space:]]|"|$)' \
            && block "pushing to main is Crown's job. Push your branch and hand off for review (if Crown explicitly asked you to merge, prefix the push with WOR_CROWN_APPROVED_MAIN=1)."
    fi
    echo "$CMD" | grep -Eq 'git[^"]*reset[^"]*--hard' \
        && block "git reset --hard can destroy work. Use a new commit or ask Crown."
    echo "$CMD" | grep -Eq 'rm[^"]*-[a-zA-Z]*r[a-zA-Z]*[^"]*(Content|Source|Config)(/|[[:space:]]|")' \
        && block "recursive delete under Content/Source/Config is not allowed."
fi

# --- File edits -----------------------------------------------------------
if echo "$INPUT" | grep -q '"file_path"'; then
    echo "$INPUT" | grep -Eq '"file_path"[[:space:]]*:[[:space:]]*"[^"]*\.(uasset|umap)"' \
        && block ".uasset/.umap are binary (LFS). Change assets through the editor connection (unreal-mcp), never as files."
fi
exit 0
