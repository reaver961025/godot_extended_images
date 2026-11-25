#include "image_decoders.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <avif/avif.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <libheif/heif.h>
#include <libheif/heif_properties.h>

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
static bool g_hdr_output = true;
static bool g_hdr_output_read = false;
static bool g_setting_warned = false;

static void _read_settings() {
    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (ps) {
        Variant v = ps->get_setting("extended_images/max_pixels", Variant());
        if (v.get_type() != Variant::NIL) g_max_pixels = (uint64_t)(int64_t)v;
        else g_max_pixels = 268435456ULL; // 268 MP, same cap as Godot's WebP
        v = ps->get_setting("extended_images/max_file_size", Variant());
        if (v.get_type() != Variant::NIL) g_max_file_size = (uint64_t)(int64_t)v;
        else g_max_file_size = 268435456ULL; // 256 MiB
        v = ps->get_setting("extended_images/hdr_output", Variant());
        if (v.get_type() != Variant::NIL) g_hdr_output = (bool)v;
        else g_hdr_output = true;
    } else {
        g_max_pixels = 268435456ULL;
        g_max_file_size = 268435456ULL;
        g_hdr_output = true;
        warn_setting_once("extended_images/*");
    }
    g_max_pixels_read = g_max_file_size_read = g_hdr_output_read = true;
}

uint64_t get_max_pixels() {
    if (!g_max_pixels_read) _read_settings();
    return g_max_pixels;
}

uint64_t get_max_file_size() {
    if (!g_max_file_size_read) _read_settings();
    return g_max_file_size;
}

bool get_hdr_output() {
    if (!g_hdr_output_read) _read_settings();
    return g_hdr_output;
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
// Shared helpers (v1.2.0)
// ============================================================

// Applies the EXIF-style orientation (1-8) to a Godot Image in place,
// matching JEITA CP-3451C (Exif 2.3) semantics:
// 1 normal, 2 flip-horizontal, 3 rotate-180, 4 flip-vertical,
// 5 transpose, 6 rotate-90cw, 7 transverse, 8 rotate-90ccw.
static void _apply_orientation(const Ref<Image> &img, int orient) {
    if (orient <= 1 || orient > 8) return;
    switch (orient) {
        case 2: img->flip_x(); break;
        case 3: img->rotate_180(); break;
        case 4: img->flip_y(); break;
        case 5: img->flip_y(); img->rotate_90(ClockDirection::CLOCKWISE); break;     // transpose
        case 6: img->rotate_90(ClockDirection::CLOCKWISE); break;                    // rotate 90 CW
        case 7: img->flip_x(); img->rotate_90(ClockDirection::CLOCKWISE); break;     // transverse
        case 8: img->rotate_90(ClockDirection::COUNTERCLOCKWISE); break;             // rotate 90 CCW
        default: break;
    }
}

// Packs an interleaved 16-bit-per-channel buffer into a Godot Image.
// half_float: samples are IEEE754 half precision -> RGBH/RGBAH,
// otherwise uint16 -> RGB16/RGBA16.
static Ref<Image> _image_from_16(uint32_t w, uint32_t h, const uint8_t *src, size_t row_bytes_src,
        int channels_src, bool half_float) {
    Image::Format fmt = half_float
        ? (channels_src == 4 ? Image::FORMAT_RGBAH : Image::FORMAT_RGBH)
        : (channels_src == 4 ? Image::FORMAT_RGBA16 : Image::FORMAT_RGB16);
    size_t row_bytes_out = (size_t)w * channels_src * 2;
    PackedByteArray px; px.resize((int64_t)(row_bytes_out * h));
    if (row_bytes_src == row_bytes_out) {
        memcpy(px.ptrw(), src, row_bytes_out * h);
    } else {
        uint8_t *dst = px.ptrw();
        for (uint32_t y = 0; y < h; y++)
            memcpy(dst + y * row_bytes_out, src + y * row_bytes_src, row_bytes_out);
    }
    Ref<Image> img = memnew(Image);
    img->set_data((int)w, (int)h, false, fmt, px);
    return img;
}

// ============================================================
// AVIF (v1.2.0: 16-bit output, orientation, animation info)
// ============================================================

// Shared worker: decodes the CURRENT frame of `dec` into a Godot Image.
// Keeps 16-bit when the source is > 8 bits and hdr_output is enabled.
static Ref<Image> _avif_frame_to_image(avifDecoder *dec) {
    const avifImage *d = dec->image;
    uint32_t w = d->width, h = d->height;
    if (!extimg::check_dimensions("AVIF", w, h)) return nullptr;
    bool alpha = (d->alphaPlane && d->alphaRowBytes > 0);
    bool want16 = extimg::get_hdr_output() && (d->depth > 8);
    avifRGBImage rgb; avifRGBImageSetDefaults(&rgb, d);
    rgb.depth = want16 ? 16 : 8;
    rgb.format = alpha ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
    rgb.alphaPremultiplied = AVIF_FALSE; rgb.isFloat = AVIF_FALSE;
    avifResult r = avifRGBImageAllocatePixels(&rgb);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: allocation failed: ") + avifResultToString(r));
        return nullptr;
    }
    r = avifImageYUVToRGB(d, &rgb);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: YUV->RGB failed: ") + avifResultToString(r));
        avifRGBImageFreePixels(&rgb); return nullptr;
    }
    Ref<Image> img;
    if (want16) {
        img = _image_from_16(w, h, rgb.pixels, rgb.rowBytes, alpha ? 4 : 3, false);
    } else {
        size_t bpp = alpha ? 4 : 3;
        size_t rb = (size_t)w * bpp;
        PackedByteArray px; px.resize((int64_t)(rb * h));
        if (rgb.rowBytes == rb) memcpy(px.ptrw(), rgb.pixels, rb * h);
        else { uint8_t *dst = px.ptrw(); for (uint32_t y=0;y<h;y++) memcpy(dst+y*rb, rgb.pixels+y*rgb.rowBytes, rb); }
        img = memnew(Image);
        img->set_data((int)w, (int)h, false, alpha ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8, px);
    }
    avifRGBImageFreePixels(&rgb);
    // Orientation: irot is anti-clockwise in units of 90 degrees; imir axis
    // 0 exchanges top/bottom, 1 exchanges left/right.
    if (d->transformFlags & AVIF_TRANSFORM_IROT) {
        switch (d->irot.angle) {
            case 1: img->rotate_90(ClockDirection::COUNTERCLOCKWISE); break;
            case 2: img->rotate_180(); break;
            case 3: img->rotate_90(ClockDirection::CLOCKWISE); break;
            default: break; // 0 = no rotation
        }
    }
    if (d->transformFlags & AVIF_TRANSFORM_IMIR) {
        if (d->imir.axis == 0) img->flip_y();
        else img->flip_x();
    }
    return img;
}

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
    Ref<Image> img = _avif_frame_to_image(dec);
    avifDecoderDestroy(dec);
    return img;
}

bool decode_avif_info(const uint8_t *data, size_t size, AnimInfo &out) {
    if (!data || !size) return false;
    if (!extimg::check_file_size("AVIF", size)) return false;
    avifDecoder *dec = avifDecoderCreate();
    if (!dec) return false;
    dec->maxThreads = 0; dec->allowProgressive = AVIF_FALSE;
    dec->ignoreExif = AVIF_TRUE; dec->ignoreXMP = AVIF_TRUE;
    avifResult r = avifDecoderSetIOMemory(dec, data, size);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: cannot read input: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return false;
    }
    r = avifDecoderParse(dec);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: parse failed: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return false;
    }
    // A real animation sequence (not a progressive image).
    out.animated = (dec->imageCount > 1) && (dec->progressiveState != AVIF_PROGRESSIVE_STATE_ACTIVE);
    out.loop_count = dec->repetitionCount; // -1 = infinite, n = play n+1 times
    out.total_duration = (float)dec->duration;
    r = avifDecoderNextImage(dec);
    if (r != AVIF_RESULT_OK) {
        extimg::print_decode_error(String("extended_images: AVIF: no image: ") + avifResultToString(r));
        avifDecoderDestroy(dec); return false;
    }
    out.bit_depth = (int)dec->image->depth;
    out.has_alpha = dec->alphaPresent ? true : (dec->image->alphaPlane != nullptr);
    Ref<Image> first = _avif_frame_to_image(dec);
    if (first.is_null()) { avifDecoderDestroy(dec); return false; }
    out.frames.push_back(first);
    out.frame_durations.push_back((float)dec->imageTiming.duration);
    if (out.animated) {
        while (true) {
            r = avifDecoderNextImage(dec);
            if (r != AVIF_RESULT_OK) {
                if (r != AVIF_RESULT_NO_IMAGES_REMAINING && r != AVIF_RESULT_NO_CONTENT) {
                    extimg::print_decode_error(String("extended_images: AVIF: frame ") +
                        String::num_int64((int64_t)(out.frames.size() - 1)) + String(" failed: ") + avifResultToString(r));
                }
                break;
            }
            Ref<Image> frame = _avif_frame_to_image(dec);
            if (frame.is_null()) break;
            out.frames.push_back(frame);
            out.frame_durations.push_back((float)dec->imageTiming.duration);
        }
    }
    avifDecoderDestroy(dec);
    return true;
}

// ============================================================
// JPEG XL (v1.2.0: float16/16-bit output, orientation, animation)
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
        case JXL_DEC_FRAME: return "FRAME";
        default: return String("status ") + String::num_int64((int64_t)st);
    }
}

// Decodes JXL frames into `out`. first_frame_only stops after the first
// JXL_DEC_FULL_IMAGE (plain decode_avif-style use).
static bool _jxl_decode_frames(const uint8_t *data, size_t size, AnimInfo &out, bool first_frame_only) {
    auto dec = JxlDecoderMake(nullptr);
    if (!dec) return false;
    if (JxlDecoderSetInput(dec.get(), data, size) != JXL_DEC_SUCCESS) {
        extimg::print_decode_error("extended_images: JXL: cannot set input.");
        return false;
    }
    if (JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
        extimg::print_decode_error("extended_images: JXL: cannot subscribe events.");
        return false;
    }
    JxlBasicInfo info; bool have_info = false;
    bool want16 = false;
    JxlPixelFormat pf = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    PackedByteArray px; bool decoded_any = false;
    JxlFrameHeader fheader;
    bool have_frame_header = false;
    while (true) {
        JxlDecoderStatus st = JxlDecoderProcessInput(dec.get());
        if (st == JXL_DEC_ERROR || st == JXL_DEC_NEED_MORE_INPUT) {
            extimg::print_decode_error(String("extended_images: JXL: decode failed: ") + _jxl_status_name(st));
            return false;
        }
        if (st == JXL_DEC_BASIC_INFO) {
            if (JxlDecoderGetBasicInfo(dec.get(), &info) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot read basic info.");
                return false;
            }
            have_info = true;
            out.bit_depth = (int)info.bits_per_sample;
            out.is_float = (info.exponent_bits_per_sample > 0);
            out.has_alpha = (info.alpha_bits > 0);
            // Enforce the pixel cap BEFORE the out-buffer is requested.
            if (!extimg::check_dimensions("JXL", info.xsize, info.ysize)) return false;
            want16 = extimg::get_hdr_output() && (info.bits_per_sample > 8);
            pf.data_type = want16
                ? (out.is_float ? JXL_TYPE_FLOAT16 : JXL_TYPE_UINT16)
                : JXL_TYPE_UINT8;
            pf.num_channels = want16 ? (info.alpha_bits > 0 ? 4 : 3) : 4;
            continue;
        }
        if (st == JXL_DEC_FRAME) {
            if (JxlDecoderGetFrameHeader(dec.get(), &fheader) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot read frame header.");
                return false;
            }
            have_frame_header = true;
            if (info.have_animation) out.animated = true;
            continue;
        }
        if (st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            if (!have_info) {
                extimg::print_decode_error("extended_images: JXL: output buffer requested before basic info.");
                return false;
            }
            size_t bs;
            if (JxlDecoderImageOutBufferSize(dec.get(), &pf, &bs) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot get output buffer size.");
                return false;
            }
            px.resize((int64_t)bs);
            if (JxlDecoderSetImageOutBuffer(dec.get(), &pf, px.ptrw(), bs) != JXL_DEC_SUCCESS) {
                extimg::print_decode_error("extended_images: JXL: cannot set output buffer.");
                return false;
            }
            continue;
        }
        if (st == JXL_DEC_FULL_IMAGE) {
            if (!have_info) return false;
            Ref<Image> img;
            if (want16) {
                int chans = (info.alpha_bits > 0) ? 4 : 3;
                size_t rb = (size_t)info.xsize * chans * 2;
                img = _image_from_16(info.xsize, info.ysize, px.ptr(), rb, chans, out.is_float);
            } else {
                PackedByteArray q; q.resize((int64_t)((size_t)info.xsize * info.ysize * 4));
                memcpy(q.ptrw(), px.ptr(), (size_t)info.xsize * info.ysize * 4);
                img = memnew(Image);
                img->set_data((int)info.xsize, (int)info.ysize, false, Image::FORMAT_RGBA8, q);
            }
            if (img.is_null()) return false;
            // JXL orientation 1-8 uses the same EXIF encoding.
            _apply_orientation(img, (int)info.orientation);
            out.frames.push_back(img);
            if (info.have_animation && have_frame_header) {
                // duration is in ticks; tps = ticks per second of a time unit.
                float tps = (info.animation.tps_denominator > 0)
                    ? (float)info.animation.tps_numerator / (float)info.animation.tps_denominator
                    : 10.0f;
                float dur = (tps > 0.0f) ? (float)fheader.duration / tps : 0.0f;
                out.frame_durations.push_back(dur);
                out.total_duration += dur;
            }
            decoded_any = true;
            if (first_frame_only) return true;
            continue;
        }
        if (st == JXL_DEC_SUCCESS) break;
        extimg::print_decode_error(String("extended_images: JXL: unexpected status: ") + _jxl_status_name(st));
        return false;
    }
    if (info.have_animation) out.loop_count = (int)info.animation.num_loops;
    return decoded_any;
}

Ref<Image> decode_jxl(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    if (!extimg::check_file_size("JXL", size)) return nullptr;
    AnimInfo info;
    if (!_jxl_decode_frames(data, size, info, true)) return nullptr;
    if (info.frames.is_empty()) {
        extimg::print_decode_error("extended_images: JXL: stream ended without an image.");
        return nullptr;
    }
    return info.frames[0];
}

bool decode_jxl_info(const uint8_t *data, size_t size, AnimInfo &out) {
    if (!data || !size) return false;
    if (!extimg::check_file_size("JXL", size)) return false;
    return _jxl_decode_frames(data, size, out, false);
}

// ============================================================
// HEIC / HEIF (libheif + libde265) — v1.2.0: irot/imir orientation
// ============================================================

// Orientation from the item's irot/imir properties, as an EXIF 1-8 value.
// irot is anti-clockwise: 90 -> EXIF 8, 180 -> 3, 270 -> 6.
// imir vertical (0) flips top/bottom; horizontal (1) flips left/right.
static int _heif_orientation(heif_context *ctx, heif_item_id item_id) {
    int rot_ccw = heif_item_get_property_transform_rotation_ccw(ctx, item_id, 0);
    enum heif_transform_mirror_direction mir = heif_item_get_property_transform_mirror(ctx, item_id, 0);
    if (rot_ccw < 0 && mir == heif_transform_mirror_direction_invalid) return 1;
    int orient = 1;
    switch (rot_ccw) {
        case 90: orient = 8; break;    // 90 anti-clockwise
        case 180: orient = 3; break;
        case 270: orient = 6; break;  // 270 CCW == 90 CW
        default: break;               // 0 or error -> unchanged
    }
    if (mir == heif_transform_mirror_direction_vertical) {
        static const int with_v[9] = {0, 4, 5, 6, 1, 2, 7, 8, 3}; // rot then vertical flip
        orient = with_v[orient];
    } else if (mir == heif_transform_mirror_direction_horizontal) {
        static const int with_h[9] = {0, 2, 1, 4, 3, 8, 5, 6, 7};
        orient = with_h[orient];
    }
    return orient;
}

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
    heif_item_id primary_id = 0;
    err = heif_context_get_primary_image_ID(ctx, &primary_id);
    if (err.code != heif_error_Ok) {
        extimg::print_decode_error(String("extended_images: HEIC: no primary image: ") + err.message);
        heif_context_free(ctx); return nullptr;
    }
    int orientation = _heif_orientation(ctx, primary_id);
    struct heif_image_handle *handle = nullptr;
    err = heif_context_get_image_handle(ctx, primary_id, &handle);
    if (err.code != heif_error_Ok) {
        extimg::print_decode_error(String("extended_images: HEIC: cannot get handle: ") + err.message);
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
    _apply_orientation(img, orientation);
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
