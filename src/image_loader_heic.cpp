#include "image_loader_heic.h"
#include "image_decoders.h"
#include <godot_cpp/core/class_db.hpp>
using namespace godot;
void ImageFormatLoaderHEIC::_bind_methods() {}
PackedStringArray ImageFormatLoaderHEIC::_get_recognized_extensions() const {
    PackedStringArray e; e.append("heic"); e.append("heif"); return e;
}
Error ImageFormatLoaderHEIC::_load_image(const Ref<Image> &p_image, const Ref<FileAccess> &p_fileaccess,
        BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
    if (p_image.is_null() || p_fileaccess.is_null()) return ERR_INVALID_PARAMETER;
    uint64_t fl = p_fileaccess->get_length();
    if (!fl) return ERR_FILE_CORRUPT;
    PackedByteArray buf = p_fileaccess->get_buffer(fl);
    if (buf.size() != (int64_t)fl) return ERR_FILE_CANT_READ;
    Ref<Image> img = decode_heic((const uint8_t*)buf.ptr(), buf.size());
    if (img.is_null()) return ERR_PARSE_ERROR;
    if (p_scale > 0.0f && p_scale < 0.999f) {
        int dw = (int)(img->get_width() * p_scale + 0.5f);
        int dh = (int)(img->get_height() * p_scale + 0.5f);
        if (dw < 1) dw = 1; if (dh < 1) dh = 1;
        img->resize(dw, dh, Image::INTERPOLATE_TRILINEAR);
    }
    p_image->copy_from(img);
    return OK;
}
