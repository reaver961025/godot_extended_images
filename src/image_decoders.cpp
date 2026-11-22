#include "image_decoders.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <avif/avif.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <libheif/heif.h>

// QOI: define implementation in this translation unit
#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

// ============================================================
// Limits & error reporting (v1.1.0)
// ============================================================
namespace extimg {

static uint64_t g_max_pixels = 0;
static bool g_max_pixels_read = false;
static uint64_t g_max_file_size = 0;
static bool g_max_file_size_read = false;
static bool g_setting_warned = false;

uint64_t get_max_pixels() {
    if (!g_max_pixels_read) {
        g_max_pixels_read = true;
        ProjectSettings *ps = ProjectSettings::get_singleton();
        if (ps) {
            Variant v = ps->get_setting("extended_images/max_pixels", Variant());
            if (v.get_type() != Variant::NIL) {
                g_max_pixels = (uint64_t)(int64_t)v;
            } else {
                g_max_pixels = 268435456ULL; // 268 MP, same cap as Godot's WebP
            }
        } else {
            g_max_pixels = 268435456ULL;
        }
    }
    return g_max_pixels;
}

uint64_t get_max_file_size() {
    if (!g_max_file_size_read) {
        g_max_file_size_read = true;
        ProjectSettings *ps = ProjectSettings::get_singleton();
        if (ps) {
            Variant v = ps->get_setting("extended_images/max_file_size", Variant());
            if (v.get_type() != Variant::NIL) {
                g_max_file_size = (uint64_t)(int64_t)v;
            } else {
                g_max_file_size = 268435456ULL; // 256 MiB
            }
        } else {
            g_max_file_size = 268435456ULL;
        }
    }
    return g_max_file_size;
}

void warn_setting_once(const char *p_setting) {
    if (g_setting_warned) return;
    g_setting_warned = true;
    UtilityFunctions::printerr(String("extended_images: could not read ProjectSettings; using default for ") + p_setting);
}

void print_decode_error(const String &p_msg) {
    UtilityFunctions::printerr(p_msg);
}

bool check_dimensions(const char *p_format, uint32_t p_w, uint32_t p_h) {
    if (p_w == 0 || p_h == 0 || p_w > 65535 || p_h > 65535) {
        print_decode_error(String("extended_images: ") + p_format + String(" image has invalid dimensions ") +
            String::num_int64(p_w) + "x" + String::num_int64(p_h) + String("."));
        return false;
    }
    uint64_t cap = get_max_pixels();
    if (cap == 0) return true; // unlimited
    uint64_t pixels = (uint64_t)p_w * (uint64_t)p_h;
    if (pixels > cap) {
        print_decode_error(String("extended_images: ") + p_format + String(" image is ") +
            String::num_int64(p_w) + "x" + String::num_int64(p_h) + String(" (") +
            String::num_int64((int64_t)(pixels / 1000000ULL)) + String(" MP), exceeding the limit of ") +
            String::num_int64((int64_t)(cap / 1000000ULL)) +
            String(" MP. Raise 'extended_images/max_pixels' in Project Settings if this file is legitimate."));
        return false;
    }
    return true;
}

bool check_file_size(const char *p_format, size_t p_size) {
    uint64_t cap = get_max_file_size();
    if (cap == 0) return true; // unlimited
    if ((uint64_t)p_size > cap) {
        print_decode_error(String("extended_images: ") + p_format + String(" file is ") +
            String::num_int64((int64_t)(p_size / (1024ULL * 1024ULL))) + String(" MiB, exceeding the limit of ") +
            String::num_int64((int64_t)(cap / (1024ULL * 1024ULL))) +
            String(" MiB. Raise 'extended_images/max_file_size' in Project Settings if this file is legitimate."));
        return false;
    }
    return true;
}

} // namespace extimg

// ============================================================
// AVIF
// ============================================================
Ref<Image> decode_avif(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    if (!extimg::check_file_size("AVIF", size)) return nullptr;
    avifDecoder *dec = avifDecoderCreate();
    if (!dec) return nullptr;
    dec->maxThreads = 0; dec->allowProgressive = AVIF_FALSE;
    dec->ignoreExif = AVIF_TRUE; dec->ignoreXMP = AVIF_TRUE;
    avifResult r = avifDecoderSetIOMemory(dec, data, size);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: cannot read input: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return nullptr;
    }
    r = avifDecoderParse(dec);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: parse failed: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return nullptr;
    }
    r = avifDecoderNextImage(dec);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: no image: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return nullptr;
    }
    const avifImage *d = dec->image;
    uint32_t w = d->width, h = d->height;
    if (!extimg::check_dimensions("AVIF", w, h)) { avifDecoderDestroy(dec); return nullptr; }
    bool alpha = (d->alphaPlane && d->alphaRowBytes > 0);
    avifRGBImage rgb; avifRGBImageSetDefaults(&rgb, d);
    rgb.depth = 8; rgb.format = alpha ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
    rgb.alphaPremultiplied = AVIF_FALSE; rgb.isFloat = AVIF_FALSE;
    r = avifRGBImageAllocatePixels(&rgb);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: allocation failed: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return nullptr;
    }
    r = avifImageYUVToRGB(d, &rgb);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: YUV->RGB failed: ") + avifResultToString(r));
        avifRGBImageFreePixels(&rgb); avifDecoderDestroy(dec); return nullptr;
    }
    size_t bpp = alpha ? 4 : 3;
    size_t rb = (size_t)w * bpp;
    PackedByteArray px; px.resize((int64_t)(rb * h));
    if (rgb.rowBytes == rb) memcpy(px.ptrw(), rgb.pixels, rb * h);
    else { uint8_t *dst = px.ptrw(); for (uint32_t y=0;y<h;y++) memcpy(dst+y*rb, rgb.pixels+y*rgb.rowBytes, rb); }
    avifRGBImageFreePixels(&rgb); avifDecoderDestroy(dec);
    Ref<Image> img = memnew(Image);
    img->set_data(w, h, false, alpha ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8, px);
    return img;
}

// ============================================================
// JPEG XL
// ============================================================
static String _jxl_status_name(JxlDecoderStatus st) {
    switch (st) {
        case JXL_DEC_SUCCESS: return "SUCCESS";
        case JXL_DEC_ERROR: return "ERROR (corrupt or unsupported stream)";
        case JXL_DEC_NEED_MORE_INPUT: return "NEED_MORE_INPUT (truncated file)";
        case JXL_DEC_BASIC_INFO: return "BASIC_INFO";
        case JXL_DEC_COLOR_ENCODING: return "COLOR_ENCODING";
        case JXL_DEC_NEED_IMAGE_OUT_BUFFER: return "NEED_IMAGE_OUT_BUFFER";
        case JXL_DEC_FULL_IMAGE: return "FULL_IMAGE";
        default: return String("status ") + String::num_int64((int64_t)st);
    }
}

Ref<Image> decode_jxl(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    if (!extimg::check_file_size("JXL", size)) return nullptr;
    auto dec = JxlDecoderMake(nullptr);
    if (!dec) return nullptr;
    if (JxlDecoderSetInput(dec.get(), data, size) != JXL_DEC_SUCCESS) {
        extimg::print_decode_error("extended_images: JXL: cannot set input.");
        return nullptr;
    }
    if (JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
        extimg::print_decode_error("extended_images: JXL: cannot subscribe events.");
        return nullptr;
    }
    JxlBasicInfo info; bool have_info = false;
    JxlPixelFormat pf = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    PackedByteArray px; bool decoded = false;
    while (true) {
        JxlDecoderStatus st = JxlDecoderProcessInput(dec.get());
        if (st == JXL_DEC_ERROR || st == JXL_DEC_NEED_MORE_INPUT) {
            extimg::print_decode_error(String("extended_images: JXL: decode failed: ") + _jxl_status_name(st));
            return nullptr;
        }
        if (st == JXL_DEC_BASIC_INFO) {
            if (JxlDecoderGetBasicInfo(dec.get(), &info) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot read basic info.");
                return nullptr;
            }
            have_info = true;
            // Enforce the pixel cap BEFORE the decoder asks for the output buffer.
            if (!extimg::check_dimensions("JXL", info.xsize, info.ysize)) return nullptr;
            continue;
        }
        if (st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            if (!have_info) {
                extimg::print_decode_error("extended_images: JXL: output buffer requested before basic info.");
                return nullptr;
            }
            size_t bs;
            if (JxlDecoderImageOutBufferSize(dec.get(), &pf, &bs) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot get output buffer size.");
                return nullptr;
            }
            px.resize((int64_t)bs);
            if (JxlDecoderSetImageOutBuffer(dec.get(), &pf, px.ptrw(), bs) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot set output buffer.");
                return nullptr;
            }
            continue;
        }
        if (st == JXL_DEC_FULL_IMAGE) { decoded = true; continue; }
        if (st == JXL_DEC_SUCCESS) break;
        extimg::print_decode_error(String("extended_images: JXL: unexpected status: ") + _jxl_status_name(st));
        return nullptr;
    }
    if (!decoded || !have_info) {
        extimg::print_decode_error("extended_images: JXL: stream ended without an image.");
        return nullptr;
    }
    Ref<Image> img = memnew(Image);
    img->set_data(info.xsize, info.ysize, false, Image::FORMAT_RGBA8, px);
    return img;
}

// ============================================================
// HEIC / HEIF (via libheif + libde265)
// ============================================================
Ref<Image> decode_heic(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    if (!extimg::check_file_size("HEIC", size)) return nullptr;
    struct heif_context *ctx = heif_context_alloc();
    if (!ctx) return nullptr;
    struct heif_error err = heif_context_read_from_memory(ctx, data, size, nullptr);
    if (err.code != heif_error_Ok) {
        extimg::print_decode_error(String("extended_images: HEIC: cannot read container: ") + err.message);
        heif_context_free(ctx); return nullptr;
    }
    struct heif_image_handle *handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok) {
        extimg::print_decode_error(String("extended_images: HEIC: no primary image: ") + err.message);
        heif_context_free(ctx); return nullptr;
    }
    // Enforce the pixel cap BEFORE heif_decode_image allocates its buffers.
    int pw = heif_image_handle_get_width(handle);
    int ph = heif_image_handle_get_height(handle);
    if (!extimg::check_dimensions("HEIC", (uint32_t)pw, (uint32_t)ph)) {
        heif_image_handle_release(handle); heif_context_free(ctx); return nullptr;
    }
    struct heif_image *img_heif = nullptr;
    err = heif_decode_image(handle, &img_heif, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
    if (err.code != heif_error_Ok) {
        extimg::print_decode_error(String("extended_images: HEIC: decode failed: ") + err.message);
        heif_image_handle_release(handle); heif_context_free(ctx); return nullptr;
    }
    int w = heif_image_get_width(img_heif, heif_channel_interleaved);
    int h = heif_image_get_height(img_heif, heif_channel_interleaved);
    if (!extimg::check_dimensions("HEIC", (uint32_t)w, (uint32_t)h)) {
        heif_image_release(img_heif); heif_image_handle_release(handle); heif_context_free(ctx); return nullptr;
    }
    int stride = 0;
    const uint8_t *plane = heif_image_get_plane_readonly(img_heif, heif_channel_interleaved, &stride);
    if (!plane) {
        extimg::print_decode_error("extended_images: HEIC: no pixel plane returned.");
        heif_image_release(img_heif); heif_image_handle_release(handle); heif_context_free(ctx); return nullptr;
    }
    size_t row_bytes = (size_t)w * 4;
    PackedByteArray px; px.resize((int64_t)(row_bytes * h));
    if (stride == (int)row_bytes) {
        memcpy(px.ptrw(), plane, row_bytes * h);
    } else {
        uint8_t *dst = px.ptrw();
        for (int y = 0; y < h; y++) memcpy(dst + y * row_bytes, plane + y * stride, row_bytes);
    }
    heif_image_release(img_heif);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    Ref<Image> img = memnew(Image);
    img->set_data(w, h, false, Image::FORMAT_RGBA8, px);
    return img;
}

// ============================================================
// QOI (Quite OK Image format)
// ============================================================
Ref<Image> decode_qoi(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    if (!extimg::check_file_size("QOI", size)) return nullptr;
    qoi_desc desc;
    void *pixels = qoi_decode(data, (int)size, &desc, 4); // 4 = RGBA
    if (!pixels) {
        extimg::print_decode_error("extended_images: QOI: decode failed (corrupt or truncated file).");
        return nullptr;
    }
    if (!extimg::check_dimensions("QOI", desc.width, desc.height)) {
        free(pixels); return nullptr;
    }
    size_t pixel_count = (size_t)desc.width * desc.height;
    PackedByteArray px; px.resize((int64_t)(pixel_count * 4));
    memcpy(px.ptrw(), pixels, pixel_count * 4);
    free(pixels);
    Ref<Image> img = memnew(Image);
    img->set_data(desc.width, desc.height, false, Image::FORMAT_RGBA8, px);
    return img;
}
