#include "resource_format_loader_extended.h"
#include "image_decoders.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
using namespace godot;
void ResourceFormatLoaderExtended::_bind_methods() {}
PackedStringArray ResourceFormatLoaderExtended::_get_recognized_extensions() const {
    PackedStringArray e;
    e.append("avif"); e.append("jxl");
    e.append("heic"); e.append("heif");
    e.append("qoi");
    return e;
}
bool ResourceFormatLoaderExtended::_recognize_path(const String &p_path, const StringName &p_type) const {
    String ext = p_path.get_extension().to_lower();
    return ext == "avif" || ext == "jxl" || ext == "heic" || ext == "heif" || ext == "qoi";
}
bool ResourceFormatLoaderExtended::_handles_type(const StringName &p_type) const {
    return p_type == StringName("ImageTexture") || p_type == StringName("Texture2D") || p_type == StringName("Texture");
}
String ResourceFormatLoaderExtended::_get_resource_type(const String &p_path) const {
    String ext = p_path.get_extension().to_lower();
    if (ext == "avif" || ext == "jxl" || ext == "heic" || ext == "heif" || ext == "qoi") return "ImageTexture";
    return String();
}
bool ResourceFormatLoaderExtended::_exists(const String &p_path) const {
    String ext = p_path.get_extension().to_lower();
    if (ext != "avif" && ext != "jxl" && ext != "heic" && ext != "heif" && ext != "qoi") return false;
    return FileAccess::file_exists(p_path);
}
Variant ResourceFormatLoaderExtended::_load(const String &p_path, const String &p_original_path,
                                             bool p_use_sub_threads, int32_t p_cache_mode) const {
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
    if (f.is_null()) return ERR_FILE_CANT_OPEN;
    uint64_t fl = f->get_length();
    if (!fl) return ERR_FILE_CORRUPT;
    PackedByteArray buf = f->get_buffer(fl);
    if (buf.size() != (int64_t)fl) return ERR_FILE_CANT_READ;
    const uint8_t *data = (const uint8_t*)buf.ptr();
    String ext = p_path.get_extension().to_lower();
    Ref<Image> img;
    if (ext == "avif") img = decode_avif(data, buf.size());
    else if (ext == "jxl") img = decode_jxl(data, buf.size());
    else if (ext == "heic" || ext == "heif") img = decode_heic(data, buf.size());
    else if (ext == "qoi") img = decode_qoi(data, buf.size());
    else return ERR_FILE_UNRECOGNIZED;
    if (img.is_null()) return ERR_PARSE_ERROR;
    Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
    if (tex.is_null()) return ERR_BUG;
    return Variant(tex);
}
