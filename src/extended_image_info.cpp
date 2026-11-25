#include "extended_image_info.h"
#include "image_decoders.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void ExtendedImageInfo::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_file", "path"), &ExtendedImageInfo::load_file);
    ClassDB::bind_method(D_METHOD("load_bytes", "bytes", "format"), &ExtendedImageInfo::load_bytes);

    ClassDB::bind_method(D_METHOD("is_animated"), &ExtendedImageInfo::is_animated);
    ClassDB::bind_method(D_METHOD("get_frame_count"), &ExtendedImageInfo::get_frame_count);
    ClassDB::bind_method(D_METHOD("get_frame", "index"), &ExtendedImageInfo::get_frame);
    ClassDB::bind_method(D_METHOD("get_frame_duration", "index"), &ExtendedImageInfo::get_frame_duration);
    ClassDB::bind_method(D_METHOD("get_frame_durations"), &ExtendedImageInfo::get_frame_durations);
    ClassDB::bind_method(D_METHOD("get_loop_count"), &ExtendedImageInfo::get_loop_count);
    ClassDB::bind_method(D_METHOD("get_total_duration"), &ExtendedImageInfo::get_total_duration);

    ClassDB::bind_method(D_METHOD("get_bit_depth"), &ExtendedImageInfo::get_bit_depth);
    ClassDB::bind_method(D_METHOD("is_hdr"), &ExtendedImageInfo::is_hdr);
    ClassDB::bind_method(D_METHOD("get_has_alpha"), &ExtendedImageInfo::get_has_alpha);

    ClassDB::bind_method(D_METHOD("get_error"), &ExtendedImageInfo::get_error);
}

ExtendedImageInfo::ExtendedImageInfo() {}

ExtendedImageInfo::~ExtendedImageInfo() {}

bool ExtendedImageInfo::load_file(const String &p_path) {
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
    if (f.is_null()) {
        error = String("cannot open file: ") + p_path;
        extimg::print_decode_error(String("extended_images: ExtendedImageInfo: ") + error);
        return false;
    }
    uint64_t fl = f->get_length();
    if (!fl) {
        error = "empty file";
        extimg::print_decode_error("extended_images: ExtendedImageInfo: empty file.");
        return false;
    }
    PackedByteArray buf = f->get_buffer(fl);
    if (buf.size() != (int64_t)fl) {
        error = "file read failed";
        extimg::print_decode_error("extended_images: ExtendedImageInfo: file read failed.");
        return false;
    }
    String format = p_path.get_extension().to_lower();
    return load_bytes(buf, format);
}

bool ExtendedImageInfo::load_bytes(const PackedByteArray &p_bytes, const String &p_format) {
    // Reset state so a failed reload does not keep stale frames.
    frames = Array();
    durations = PackedFloat32Array();
    loop_count = 0;
    total_duration = 0.0f;
    bit_depth = 8;
    is_float = false;
    has_alpha = false;
    animated = false;
    error = String();

    if (p_bytes.is_empty()) {
        error = "empty buffer";
        extimg::print_decode_error("extended_images: ExtendedImageInfo: empty buffer.");
        return false;
    }
    const uint8_t *data = (const uint8_t *)p_bytes.ptr();
    size_t size = (size_t)p_bytes.size();

    String format = p_format.to_lower();
    if (format == "avif") {
        AnimInfo info;
        if (!decode_avif_info(data, size, info)) {
            error = "AVIF decode failed (see console for the library message)";
            return false;
        }
        animated = info.animated;
        loop_count = info.loop_count;
        total_duration = info.total_duration;
        bit_depth = info.bit_depth;
        is_float = info.is_float;
        has_alpha = info.has_alpha;
        for (int i = 0; i < info.frames.size(); i++) {
            frames.push_back(info.frames[i]);
        }
        for (int i = 0; i < info.frame_durations.size(); i++) {
            durations.push_back(info.frame_durations[i]);
        }
        return true;
    }
    if (format == "jxl") {
        AnimInfo info;
        if (!decode_jxl_info(data, size, info)) {
            error = "JXL decode failed (see console for the library message)";
            return false;
        }
        animated = info.animated;
        loop_count = info.loop_count;
        total_duration = info.total_duration;
        bit_depth = info.bit_depth;
        is_float = info.is_float;
        has_alpha = info.has_alpha;
        for (int i = 0; i < info.frames.size(); i++) {
            frames.push_back(info.frames[i]);
        }
        for (int i = 0; i < info.frame_durations.size(); i++) {
            durations.push_back(info.frame_durations[i]);
        }
        return true;
    }
    // HEIC/HEIF and QOI: single still image.
    Ref<Image> img;
    if (format == "heic" || format == "heif") {
        img = decode_heic(data, size);
    } else if (format == "qoi") {
        img = decode_qoi(data, size);
    } else {
        error = String("unsupported format: ") + p_format;
        extimg::print_decode_error(String("extended_images: ExtendedImageInfo: unsupported format: ") + p_format);
        return false;
    }
    if (img.is_null()) {
        error = String("decode failed (see console for the library message)");
        return false;
    }
    has_alpha = (img->get_format() == Image::FORMAT_RGBA8 || img->get_format() == Image::FORMAT_RGBAH
        || img->get_format() == Image::FORMAT_RGBA16);
    frames.push_back(img);
    durations.push_back(0.0f);
    return true;
}

bool ExtendedImageInfo::is_animated() const { return animated; }

int ExtendedImageInfo::get_frame_count() const { return frames.size(); }

Ref<Image> ExtendedImageInfo::get_frame(int p_index) const {
    if (p_index < 0 || p_index >= frames.size()) return Ref<Image>();
    return frames[p_index];
}

float ExtendedImageInfo::get_frame_duration(int p_index) const {
    if (p_index < 0 || p_index >= durations.size()) return 0.0f;
    return durations[p_index];
}

PackedFloat32Array ExtendedImageInfo::get_frame_durations() const { return durations; }

int ExtendedImageInfo::get_loop_count() const { return loop_count; }

float ExtendedImageInfo::get_total_duration() const { return total_duration; }

int ExtendedImageInfo::get_bit_depth() const { return bit_depth; }

bool ExtendedImageInfo::is_hdr() const { return is_float; }

bool ExtendedImageInfo::get_has_alpha() const { return has_alpha; }

String ExtendedImageInfo::get_error() const { return error; }

} // namespace godot
