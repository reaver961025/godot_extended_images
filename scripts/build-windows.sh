#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$(dirname "${BASH_SOURCE[0]}")")"
[[ -d "$HOME/.local/bin" ]] && export PATH="$HOME/.local/bin:$PATH"
BUILD_TYPE="Release"; [[ "${1:-}" == "debug" ]] && BUILD_TYPE="Debug"
for t in cmake ninja meson; do command -v "$t" >/dev/null 2>&1 || { echo "ERROR: $t not found" >&2; exit 1; }; done
LLVM_MINGW_ROOT="${LLVM_MINGW_ROOT:-/home/z/my-project/tools/llvm-mingw-20260826-ucrt-ubuntu-22.04-x86_64}"
if [[ ! -x "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-gcc" ]]; then
    echo "ERROR: LLVM-MinGW not found at $LLVM_MINGW_ROOT" >&2; exit 1
fi
export PATH="$LLVM_MINGW_ROOT/bin:$PATH"

# Fix missing include symlinks if needed
for arch in x86_64-w64-mingw32 aarch64-w64-mingw32; do
    if [[ -d "$LLVM_MINGW_ROOT/$arch" && ! -e "$LLVM_MINGW_ROOT/$arch/include" ]]; then
        cp -r "$LLVM_MINGW_ROOT/generic-w64-mingw32/include" "$LLVM_MINGW_ROOT/$arch/include" 2>/dev/null || true
    fi
done

# Generate meson cross-file
MESON_CROSS="${PWD}/build/meson-cross-x86_64-mingw.ini"; mkdir -p build
WINE_BIN=""
command -v wine64 >/dev/null 2>&1 && WINE_BIN="wine64"
[[ -x /home/z/my-project/tools/lutris-GE-Proton8-26-x86_64/bin/wine64 ]] && WINE_BIN="/home/z/my-project/tools/lutris-GE-Proton8-26-x86_64/bin/wine64"
cat > "$MESON_CROSS" <<MEOF
[binaries]
c = '${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-gcc'
cpp = '${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-g++'
ld = '${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-ld'
ar = '${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-ar'
strip = '${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-strip'
windres = '${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-windres'
MEOF
[[ -n "$WINE_BIN" ]] && echo "exe_wrapper = '$WINE_BIN'" >> "$MESON_CROSS"
cat >> "$MESON_CROSS" <<MEOF2

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[build_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
MEOF2
export DAV1D_MESON_CROSS_FILE="$MESON_CROSS"
BD="build/windows-${BUILD_TYPE,,}"
cmake -S . -B "$BD" -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-x86_64.cmake -DLLVM_MINGW_ROOT="$LLVM_MINGW_ROOT" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DGODOTCPP_PLATFORM=windows -DAVIF_DAV1D_DISABLE_ASM=ON -DGODOTCPP_API_VERSION=4.7
cmake --build "$BD" --parallel
[[ "$BUILD_TYPE" == "Release" ]] && "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-strip" --strip-debug bin/windows/*.dll 2>/dev/null || true
rm -f bin/windows/*.dll.a 2>/dev/null || true
cp "$LLVM_MINGW_ROOT/x86_64-w64-mingw32/bin/libc++.dll" bin/windows/ 2>/dev/null || true
cp "$LLVM_MINGW_ROOT/x86_64-w64-mingw32/bin/libunwind.dll" bin/windows/ 2>/dev/null || true
echo "==> Done:"; ls -lh bin/windows/ 2>/dev/null
