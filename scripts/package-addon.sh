#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$(dirname "${BASH_SOURCE[0]}")")"
STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT
ADDON="$STAGING/godot_extended_images"
mkdir -p "$ADDON/bin"
cp godot_extended_images.gdextension "$ADDON/"
for plat in linux windows macos; do
    [[ -d "bin/$plat" ]] && { mkdir -p "$ADDON/bin/$plat"; cp bin/$plat/* "$ADDON/bin/$plat/" 2>/dev/null || true; }
done
cat > "$ADDON/INSTALL.md" <<'IEOF'
# Installation
1. Copy `godot_extended_images/` into your Godot project's `addons/` folder.
2. Restart the editor.
3. `load("res://photo.avif")`, `.jxl`, `.heic`, `.heif`, `.qoi` all work.

## Supported formats
| Ext | Format | Library |
|-----|--------|---------|
| .avif | AV1 Image | libavif + dav1d |
| .jxl | JPEG XL | libjxl |
| .heic/.heif | HEVC Image | libheif + libde265 |
| .qoi | Quite OK Image | qoi (phoboslab) |

Built for Godot 4.3-4.7.
IEOF
mkdir -p download
ZIP="download/godot_extended_images_addon.zip"
rm -f "$ZIP"
cd "$STAGING" && zip -r "$OLDPWD/$ZIP" godot_extended_images
echo "==> $ZIP"; ls -lh "$OLDPWD/$ZIP"
