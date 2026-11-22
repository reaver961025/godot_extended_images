# godot_extended_images

[![Build](https://github.com/reaver961025/godot_extended_images/actions/workflows/build.yml/badge.svg)](../../actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Godot 4.3–4.7 **GDExtension** that adds native loading of modern image
formats not supported by the engine out of the box:

| Format | Extension(s)        | Decoder            |
|--------|---------------------|--------------------|
| AVIF   | `.avif`             | libavif + dav1d    |
| JPEG XL| `.jxl`              | libjxl             |
| HEIC   | `.heic` / `.heif`   | libheif + libde265 |
| QOI    | `.qoi`              | qoi (header)       |

Once installed, `load("res://photo.heic")` works in GDScript and the
FileSystem dock shows thumbnails, just like PNG.

## Status

v1.1.0 — decode limits (268 MP pixel cap, 256 MiB file cap, both
configurable via Project Settings) and real library error messages. The
extension compiles and links cleanly on Linux x86_64 for both
template_debug and template_release (verified by full CMake + Ninja
builds). Windows builds are produced by CI. See
[CHANGELOG.md](CHANGELOG.md) for the full release notes and
[docs/PLAN_LIMITACIONES_v2.md](docs/PLAN_LIMITACIONES_v2.md) for the
roadmap.

## Build (Linux)

```bash
pip install --user cmake ninja meson
sudo apt install build-essential nasm
./scripts/build-linux.sh release
# → bin/linux/libgodot_extended_images.linux.template_release.x86_64.so
```

If `nasm` is missing, pass `AVIF_DAV1D_DISABLE_ASM=ON` (slower AVIF
decoding, no assembly). The build script auto-detects this.

## Cross-compile for Windows

```bash
./scripts/build-windows.sh release
# Set LLVM_MINGW_ROOT to your LLVM-MinGW install if not at the default path.
```

## macOS (on a Mac)

```bash
brew install cmake ninja meson nasm
./scripts/build-macos.sh release x86_64 arm64    # universal binary
```

## Install the addon in a Godot project

1. Download `godot_extended_images_addon.zip` from the latest
   [Release](../../releases).
2. Unzip it into your project's `addons/` folder.
3. Enable the plugin in **Project → Project Settings → Plugins**.
4. Restart the editor.

## GitHub Actions

Push to `main` builds all three platforms. Tag `v*` (e.g. `v1.0.0`) to
create a Release with the packaged addon attached.

## Bundled third-party libraries

All decoders are vendored under `thirdparty/` and statically linked, so the
produced binary has minimal external runtime dependencies. Versions shipped
in v1.0.0:

| Library   | Version | License       |
|-----------|---------|---------------|
| godot-cpp | master  | MIT           |
| libavif   | 1.4.2   | BSD-2-Clause  |
| dav1d     | 1.4.3   | BSD-2-Clause  |
| libjxl    | 0.13.0  | BSD-3-Clause  |
| highway   | 1.2.0   | Apache-2.0    |
| brotli    | 1.1.0   | MIT           |
| skcms     | master  | BSD-3-Clause  |
| libheif   | 1.23.7  | LGPL-3.0-only |
| libde265  | 1.1.2   | LGPL-3.0-only |
| qoi       | 2021    | MIT           |

See [LICENSE](LICENSE) for the full attribution table and the LGPL notice
for `libheif` and `libde265`. Source for each library is shipped in the
repository so the binary can be rebuilt and relinked, as required by the
LGPL.

## Decode limits (v1.1.0)

Two Project Settings guard against crafted or oversized files. Both are
read once and cached; set them to `0` to disable.

| Setting | Default | Meaning |
|---|---|---|
| `extended_images/max_pixels` | `268435456` (268 MP) | Maximum `width * height` accepted, checked **before** the decoder allocates its output buffer |
| `extended_images/max_file_size` | `268435456` (256 MiB) | Maximum input file size handed to a decoder |

When a file exceeds a limit, the load fails and the console shows a message
naming the actual dimensions/size, the active limit and the setting to
raise. Files larger than 50 MP are unusual for game assets; if you load
them intentionally, raise `max_pixels` accordingly.

## Known limitations

These are documented in detail in
[docs/PLAN_LIMITACIONES_v2.md](docs/PLAN_LIMITACIONES_v2.md) and are
slated for the next release:

- **8-bit only.** AVIF, JPEG XL and HEIC can carry 10/12/16-bit and HDR
  content; this extension always decodes to 8-bit RGB/RGBA. HDR is
  tone-mapped down.
- **No EXIF / ICC handling.** Orientation tags and color profiles are
  ignored, so portrait photos from a phone may appear sideways.
- **First frame only** for animated AVIF and animated JXL.

Solved in v1.1.0: memory caps before decoding (see above) and error
reporting (failed decodes now print the underlying library message —
`avifResultToString`, the JXL decoder status, or `heif_error.message`).

If you only ship assets you control, none of these is blocking. If you
load user-supplied images (mods, uploads, etc.), see the security notes
below.

## Security

The extension processes untrusted byte streams (image files) and statically
links libraries with a non-trivial CVE history. Keep the extension updated;
each release pulls in the latest upstream security fixes. See
[SECURITY.md](SECURITY.md) for the reporting policy and hardening guidance.

## License

Project code is MIT licensed. See [LICENSE](LICENSE) for the full text and
the third-party attribution table (LGPL-3.0 notice for libheif/libde265).
