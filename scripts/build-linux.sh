#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$(dirname "${BASH_SOURCE[0]}")")"
[[ -d "$HOME/.local/bin" ]] && export PATH="$HOME/.local/bin:$PATH"
BUILD_TYPE="Release"; [[ "${1:-}" == "debug" ]] && BUILD_TYPE="Debug"
for t in cmake ninja meson; do command -v "$t" >/dev/null 2>&1 || { echo "ERROR: $t not found" >&2; exit 1; }; done
ASM_FLAG="ON"; command -v nasm >/dev/null 2>&1 || ASM_FLAG="OFF"
BD="build/linux-${BUILD_TYPE,,}"
cmake -S . -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DGODOTCPP_PLATFORM=linux -DAVIF_GDEXTENSION_ARCH=x86_64 -DAVIF_DAV1D_DISABLE_ASM="$ASM_FLAG" -DGODOTCPP_API_VERSION=4.7
cmake --build "$BD" --parallel
[[ "$BUILD_TYPE" == "Release" ]] && strip --strip-debug bin/linux/*.so 2>/dev/null || true
echo "==> Done:"; ls -lh bin/linux/ 2>/dev/null
