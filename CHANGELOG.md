# Changelog

All notable changes to this project are documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.2.1] - 2026-11-26

### Fixed
- **The extension did not load in a real Godot editor** (found while testing
  inside the official 2D Platformer demo with Godot 4.7.2): the shared
  library failed with `undefined symbol: BrotliDefaultAllocFunc`. GNU ld
  processes archives left-to-right, and `libbrotlicommon` was listed before
  `libbrotlidec`/`libbrotlienc` in the link line, so the linker discarded it
  before dec/enc pulled in its symbols. The symbol-smoke-tests never caught
  this because it only manifests at `dlopen` time. Reordered the link line;
  verified loading and importing in Godot 4.7.2 stable.

## [1.2.0] - 2026-11-25

### Added
- **16-bit / HDR output.** AVIF and JXL sources with more than 8 bits per
  channel now decode to Godot's 16-bit `Image` formats instead of being
  quantized to 8 bits: float sources become `FORMAT_RGBH`/`FORMAT_RGBAH`
  (IEEE 754 half float, true HDR), integer 10/12/16-bit sources become
  `FORMAT_RGB16`/`FORMAT_RGBA16`. Controlled by the new
  `extended_images/hdr_output` ProjectSetting (default `true`; set `false`
  for the previous always-8-bit behavior). HEIC/QOI stay 8-bit (their
  pipelines output 8-bit interleaved RGB anyway).
- **Orientation applied.** Portrait photos no longer come out sideways:
  AVIF `irot`/`imir` boxes, the JXL `orientation` field (EXIF values 1-8)
  and the HEIC `irot`/`imir` item properties are applied to the decoded
  image before it is returned. Still images AND every animation frame.
- **Animations exposed: new `ExtendedImageInfo` class.** `load_file()` /
  `load_bytes()` decode every frame of an AVIF sequence or JXL animation
  (HEIC/QOI load their single still image), then expose
  `is_animated()`, `get_frame_count()`, `get_frame(i)`,
  `get_frame_duration(i)` (seconds, from the container timing),
  `get_frame_durations()`, `get_loop_count()` (0 = infinite),
  `get_total_duration()`, `get_bit_depth()`, `is_hdr()`,
  `get_has_alpha()` and `get_error()`. Plain `load("res://x.avif")`
  keeps returning the first frame as before.

### Fixed
- `ClockDirection` used by the new rotation code is a global godot enum
  (`ClockDirection::CLOCKWISE`), not `Image::CLOCKWISE` as first written.

## [1.1.0] - 2026-11-22

### Added
- **Pixel-count cap enforced before decoding.** `extended_images/max_pixels`
  (ProjectSettings, default 268 MP — same as Godot's WebP cap) is checked
  against the dimensions declared in the file header *before* the decoder
  allocates the output buffer, for all four formats (AVIF, JXL, HEIC, QOI).
  HEIC now also checks the handle dimensions before `heif_decode_image`,
  and JXL before the out-buffer event. Files exceeding the cap are rejected
  with a descriptive error instead of exhausting memory.
- **File-size cap enforced before decoding.**
  `extended_images/max_file_size` (default 256 MiB) rejects oversized
  inputs before they are handed to the decoders. Both caps are set to `0`
  to disable.
- **Real error messages.** Failed decodes now print the underlying library
  message: `avifResultToString()` for AVIF, a readable name for the
  `JxlDecoderStatus` (including "truncated file" vs "corrupt") for JXL,
  and `heif_error.message` for HEIC. QOI reports corrupt/truncated input.
  Errors that used to be silent `nullptr` returns (e.g. basic-info
  failures) are also reported.
- CI now builds **both** `template_debug` and `template_release` for Linux
  and Windows, and the release job attaches the four binaries plus the
  packaged addon zip.

### Security
- Mitigates memory-exhaustion vectors through crafted headers (huge
  declared dimensions) and oversized files, complementing the upstream
  library updates from v1.0.0.

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
