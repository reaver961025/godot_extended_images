#include "image_decoders.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>
#include <cstdlib>
#include <avif/avif.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <libheif/heif.h>

// QOI: define implementation in this translation unit
#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

// ============================================================
// AVIF
// ============================================================
Ref<Image> decode_avif(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    avifDecoder *dec = avifDecoderCreate();
    if (!dec) return nullptr;
    dec->maxThreads = 0; dec->allowProgressive = AVIF_FALSE;
    dec->ignoreExif = AVIF_TRUE; dec->ignoreXMP = AVIF_TRUE;
    avifResult r = avifDecoderSetIOMemory(dec, data, size);
    if (r != AVIF_RESULT_OK) { avifDecoderDestroy(dec); return nullptr; }
    r = avifDecoderParse(dec);
    if (r != AVIF_RESULT_OK) { avifDecoderDestroy(dec); return nullptr; }
    r = avifDecoderNextImage(dec);
    if (r != AVIF_RESULT_OK) { avifDecoderDestroy(dec); return nullptr; }
    const avifImage *d = dec->image;
    uint32_t w = d->width, h = d->height;
    if (!w || !h || w > 65535 || h > 65535) { avifDecoderDestroy(dec); return nullptr; }
    bool alpha = (d->alphaPlane && d->alphaRowBytes > 0);
    avifRGBImage rgb; avifRGBImageSetDefaults(&rgb, d);
    rgb.depth = 8; rgb.format = alpha ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
    rgb.alphaPremultiplied = AVIF_FALSE; rgb.isFloat = AVIF_FALSE;
    r = avifRGBImageAllocatePixels(&rgb);
    if (r != AVIF_RESULT_OK) { avifDecoderDestroy(dec); return nullptr; }
    r = avifImageYUVToRGB(d, &rgb);
    if (r != AVIF_RESULT_OK) { avifRGBImageFreePixels(&rgb); avifDecoderDestroy(dec); return nullptr; }
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
Ref<Image> decode_jxl(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    auto dec = JxlDecoderMake(nullptr);
    if (!dec) return nullptr;
    if (JxlDecoderSetInput(dec.get(), data, size) != JXL_DEC_SUCCESS) return nullptr;
    if (JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) return nullptr;
    JxlBasicInfo info; bool have_info = false;
    JxlPixelFormat pf = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    PackedByteArray px; bool decoded = false;
    while (true) {
        JxlDecoderStatus st = JxlDecoderProcessInput(dec.get());
        if (st == JXL_DEC_ERROR || st == JXL_DEC_NEED_MORE_INPUT) return nullptr;
        if (st == JXL_DEC_BASIC_INFO) { if (JxlDecoderGetBasicInfo(dec.get(), &info) != JXL_DEC_SUCCESS) return nullptr; have_info = true; continue; }
        if (st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            if (!have_info) return nullptr;
            size_t bs; if (JxlDecoderImageOutBufferSize(dec.get(), &pf, &bs) != JXL_DEC_SUCCESS) return nullptr;
            px.resize((int64_t)bs);
            if (JxlDecoderSetImageOutBuffer(dec.get(), &pf, px.ptrw(), bs) != JXL_DEC_SUCCESS) return nullptr;
            continue;
        }
        if (st == JXL_DEC_FULL_IMAGE) { decoded = true; continue; }
        if (st == JXL_DEC_SUCCESS) break;
        return nullptr;
    }
    if (!decoded || !have_info) return nullptr;
    if (!info.xsize || !info.ysize || info.xsize > 65535 || info.ysize > 65535) return nullptr;
    Ref<Image> img = memnew(Image);
    img->set_data(info.xsize, info.ysize, false, Image::FORMAT_RGBA8, px);
    return img;
}

// ============================================================
// HEIC / HEIF (via libheif + libde265)
// ============================================================
Ref<Image> decode_heic(const uint8_t *data, size_t size) {
    if (!data || !size) return nullptr;
    struct heif_context *ctx = heif_context_alloc();
    if (!ctx) return nullptr;
    struct heif_error err = heif_context_read_from_memory(ctx, data, size, nullptr);
    if (err.code != heif_error_Ok) { heif_context_free(ctx); return nullptr; }
    struct heif_image_handle *handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok) { heif_context_free(ctx); return nullptr; }
    struct heif_image *img_heif = nullptr;
    err = heif_decode_image(handle, &img_heif, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
    if (err.code != heif_error_Ok) { heif_image_handle_release(handle); heif_context_free(ctx); return nullptr; }
    int w = heif_image_get_width(img_heif, heif_channel_interleaved);
    int h = heif_image_get_height(img_heif, heif_channel_interleaved);
    if (!w || !h || w > 65535 || h > 65535) { heif_image_release(img_heif); heif_image_handle_release(handle); heif_context_free(ctx); return nullptr; }
    int stride = 0;
    const uint8_t *plane = heif_image_get_plane_readonly(img_heif, heif_channel_interleaved, &stride);
    if (!plane) { heif_image_release(img_heif); heif_image_handle_release(handle); heif_context_free(ctx); return nullptr; }
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
    qoi_desc desc;
    void *pixels = qoi_decode(data, (int)size, &desc, 4); // 4 = RGBA
    if (!pixels) return nullptr;
    if (!desc.width || !desc.height || desc.width > 65535 || desc.height > 65535) {
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
