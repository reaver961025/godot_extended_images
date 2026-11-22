#include "editor_preview_generator_extended.h"
#include "image_decoders.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <algorithm>
using namespace godot;
void EditorPreviewGeneratorExtended::_bind_methods() {}
bool EditorPreviewGeneratorExtended::_handles(const String &p_type) const {
    return p_type == "ImageTexture" || p_type == "Texture2D" || p_type == "Texture";
}
bool EditorPreviewGeneratorExtended::_generate_small_preview_automatically() const { return true; }
bool EditorPreviewGeneratorExtended::_can_generate_small_preview() const { return true; }
Ref<Texture2D> EditorPreviewGeneratorExtended::_generate_from_path(const String &p_path, const Vector2i &p_size, const Dictionary &p_metadata) const {
    String ext = p_path.get_extension().to_lower();
    if (ext != "avif" && ext != "jxl" && ext != "heic" && ext != "heif" && ext != "qoi") return nullptr;
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
    if (f.is_null()) return nullptr;
    uint64_t fl = f->get_length();
    if (!fl) return nullptr;
    PackedByteArray buf = f->get_buffer(fl);
    if (buf.size() != (int64_t)fl) return nullptr;
    const uint8_t *data = (const uint8_t*)buf.ptr();
    Ref<Image> img;
    if (ext == "avif") img = decode_avif(data, buf.size());
    else if (ext == "jxl") img = decode_jxl(data, buf.size());
    else if (ext == "heic" || ext == "heif") img = decode_heic(data, buf.size());
    else if (ext == "qoi") img = decode_qoi(data, buf.size());
    if (img.is_null()) return nullptr;
    int tm = std::max(p_size.x, p_size.y);
    if (tm > 0) {
        int sm = std::max(img->get_width(), img->get_height());
        if (sm > tm) {
            float r = (float)tm / (float)sm;
            int dw = std::max(1, (int)(img->get_width() * r + 0.5f));
            int dh = std::max(1, (int)(img->get_height() * r + 0.5f));
            img->resize(dw, dh, Image::INTERPOLATE_TRILINEAR);
        }
    }
    return ImageTexture::create_from_image(img);
}
Ref<Texture2D> EditorPreviewGeneratorExtended::_generate(const Ref<Resource> &p_resource, const Vector2i &p_size, const Dictionary &p_metadata) const {
    if (p_resource.is_null()) return nullptr;
    Ref<Texture2D> tex = p_resource;
    if (tex.is_valid()) {
        int tm = std::max(p_size.x, p_size.y);
        if (tm > 0) {
            int sm = std::max(tex->get_width(), tex->get_height());
            if (sm > tm * 2) {
                Ref<Image> img = tex->get_image();
                if (img.is_valid()) {
                    Ref<Image> small = img->duplicate();
                    small->resize(std::max(1, (int)(img->get_width() * (float)tm / sm + 0.5f)),
                        std::max(1, (int)(img->get_height() * (float)tm / sm + 0.5f)), Image::INTERPOLATE_TRILINEAR);
                    return ImageTexture::create_from_image(small);
                }
            }
        }
        return tex;
    }
    return nullptr;
}
