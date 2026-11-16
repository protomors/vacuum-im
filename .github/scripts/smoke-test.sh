#!/usr/bin/env bash
# Start the installed client headless with a fresh profile; it must still be
# running after a few seconds (no crash or early exit during plugin loading).
set -uo pipefail
seconds=${1:-15}
export QT_QPA_PLATFORM=offscreen LANG=C.UTF-8 HOME=$(mktemp -d)
timeout "$seconds" vacuum
rc=$?
if [[ $rc -ne 124 ]]; then
    echo "vacuum exited early with status $rc" >&2
    exit 1
fi
echo "vacuum ran for ${seconds}s without exiting"
