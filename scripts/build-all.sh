#!/usr/bin/env bash
set -euo pipefail
DIR="$(dirname "${BASH_SOURCE[0]}")"
ARGS=("$@")
echo "==> Linux"; "$DIR/build-linux.sh" "${ARGS[@]}"
echo "==> Windows"; "$DIR/build-windows.sh" "${ARGS[@]}"
if [[ "$(uname -s)" == "Darwin" ]]; then echo "==> macOS"; "$DIR/build-macos.sh" "${ARGS[@]}" x86_64 arm64; fi
echo "==> All done"; find bin -type f -name 'libgodot*' 2>/dev/null | sort
