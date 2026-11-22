# Roadmap

This file tracks the high-level roadmap. For the detailed analysis and plan
of the v1.0.0 known limitations, see
[`PLAN_LIMITACIONES_v2.md`](PLAN_LIMITACIONES_v2.md).

## v1.0.0 (2026-11-22) — released

- Initial public release. AVIF / JXL / HEIC / QOI loading, editor previews,
  CMake build for Linux / Windows / macOS, CI with releases.

## v1.1.0 (planned) — security & robustness

Focus: harden the decoding path so untrusted images can be processed
safely.

- [ ] Configurable pixel-count cap (default ~268 MP, matching Godot's
      WebP limit) enforced **before** the decoder allocates the output
      buffer.
- [ ] Configurable file-size cap enforced before reading the file into
      memory.
- [ ] Surface the underlying library error string via `ERR_PRINT` when a
      decode fails (avifResultToString, heif_error_get_string,
      `JxlDecoderStatus`).
- [ ] Fuzz harness feeding crafted files to each decoder (libFuzzer or
      cargo-fuzz via a small C wrapper).

## v1.2.0 (planned) — fidelity

Focus: stop throwing away information the formats can carry.

- [ ] 16-bit (and float HDR) path for AVIF and JXL when the host Godot
      Image format supports it; fall back to 8-bit otherwise.
- [ ] Apply EXIF orientation during decode (AVIF, JXL, HEIC).
- [ ] Carry the ICC profile onto the Godot Image so color management is
      not lost.
- [ ] Animated AVIF and animated JXL: expose frames as an array or
      `AnimatedTexture`.

## v1.3.0 (planned) — packaging

- [ ] Reduce binary size (strip unused libjxl encoder paths, build
      libsharpyuv statically instead of dlopen-ing the system one).
- [ ] Provide a single universal Linux build that works across distros.
- [ ] Submit to the Godot Asset Library.
