# Changelog

All notable changes to this project are documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-11-22

### Added
- Initial public release of `godot_extended_images`.
- GDExtension for Godot Engine 4.3–4.7 adding support for AVIF, JPEG XL,
  HEIC/HEIF and QOI image formats.
- `ImageFormatLoader` and `ResourceFormatLoader` registration so `load()` and
  the editor import pipeline pick up the new extensions automatically.
- `EditorPreviewGeneratorExtended` so the FileSystem dock shows thumbnails.
- CMake build with vendored third-party libraries (static linking).
- Build scripts for Linux, Windows (LLVM-MinGW cross-compile) and macOS
  (universal x86_64 + arm64 binary).
- GitHub Actions workflow `build.yml` that compiles all three platforms and
  publishes a release on tags `v*`.
- MIT License for the project's own code, plus a `LICENSE` file with the
  full third-party attribution table required by the LGPL dependencies.
- `SECURITY.md` with a vulnerability reporting policy.

### Security
- Updated `libavif` to **1.4.2** (was 1.1.1 in the original source dump).
  The 1.1.1 → 1.4.x line fixes multiple memory-safety issues, including the
  integer overflows in `avifImageRGBToYUV` and the memory errors reported by
  Fudan University / oss-fuzz.
- Updated `libjxl` to **0.13.0** (was 0.12.0). The 0.12.0 → 0.13.0 update
  contains multiple security fixes, including a severe-vulnerability fix in
  the bundled `brotli` (now 1.2.1) and additional hardening across the
  decoder, box buffers and plugins.
- Updated `libheif` to **1.23.7** (was 1.23.4). The 1.23.5 / 1.23.6 / 1.23.7
  releases fix more than 14 issues, several rated High, including
  out-of-bounds reads in the OpenH264 plugin, AV1 size-check bypass,
  use-after-free in parallel grid decoding, heap buffer overflows and
  multiple CPU-exhaustion / memory-exhaustion DoS vectors triggered by tiny
  crafted files (e.g. a 351-byte AVIF that allocated gigabytes of memory).
- Updated `brotli` to **1.1.0**, `highway` to **1.2.0** and refreshed
  `skcms` to its current mirror tip.
- `dav1d` kept at **1.4.3** (no security advisory against it); the
  duplicate copy at the repository root was removed — only the canonical
  copy under `thirdparty/libavif/ext/dav1d/` remains.

### Repository hygiene
- Removed the duplicated `dav1d/` directory at the repository root (the copy
  that the build actually uses lives under
  `thirdparty/libavif/ext/dav1d/`). The two copies were byte-for-byte
  identical (verified by SHA-256 of `src/decode.c`); the duplicate only
  added ~14 MB of weight and confusion.
- Removed the `.git` directory that was bundled inside `dav1d/`.
- Removed the empty `docs/` directory.

### Known limitations (to be addressed in a future release)
- All images are decoded to 8 bits per channel; HDR / >8-bit content in AVIF,
  JPEG XL and HEIC is reduced to 8-bit RGB/RGBA.
- EXIF orientation and ICC color profiles are ignored (AVIF explicitly
  ignores Exif and XMP; JXL and HEIC do not request metadata).
- Animated AVIF and animated JXL return only the first frame.
- No pixel-count or memory limit is enforced before decoding; very large
  valid images can exhaust memory. A configurable cap is planned.
- Errors return generic `ERR_PARSE_ERROR`; the underlying library message
  is not surfaced.

See `docs/PLAN_LIMITACIONES_v2.md` (and the accompanying analysis document)
for the detailed plan to address these.
