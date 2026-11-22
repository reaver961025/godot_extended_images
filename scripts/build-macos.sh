#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$(dirname "${BASH_SOURCE[0]}")")"
BUILD_TYPE="Release"; [[ "${1:-}" == "debug" ]] && BUILD_TYPE="Debug"
ARCHS=("${@:2}"); [[ ${#ARCHS[@]} -eq 0 ]] && ARCHS=("$(uname -m)")
for t in cmake ninja meson nasm; do command -v "$t" >/dev/null 2>&1 || { echo "ERROR: $t not found — brew install $t" >&2; exit 1; }; done
BUILT=()
for ARCH in "${ARCHS[@]}"; do
    BD="build/macos-${ARCH}-${BUILD_TYPE,,}"
    cmake -S . -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DGODOTCPP_PLATFORM=macos -DAVIF_GDEXTENSION_ARCH="$ARCH" -DCMAKE_OSX_ARCHITECTURES="$ARCH" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DAVIF_DAV1D_DISABLE_ASM=OFF -DGODOTCPP_API_VERSION=4.7
    cmake --build "$BD" --parallel
    BIN="bin/macos/libgodot_extended_images.macos.template_${BUILD_TYPE,,}.${ARCH}.dylib"
    [[ "$BUILD_TYPE" == "Release" && -f "$BIN" ]] && strip -x "$BIN" 2>/dev/null || true
    BUILT+=("$BIN")
done
if [[ ${#BUILT[@]} -gt 1 ]]; then
    UNI="bin/macos/libgodot_extended_images.macos.template_${BUILD_TYPE,,}.universal.dylib"
    mkdir -p bin/macos; lipo -create -output "$UNI" "${BUILT[@]}"
    for f in "${BUILT[@]}"; do rm -f "$f"; done
fi
echo "==> Done:"; ls -lh bin/macos/ 2>/dev/null
