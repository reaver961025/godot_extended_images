#ifndef IMAGE_DECODERS_H
#define IMAGE_DECODERS_H
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
using namespace godot;

// v1.1.0 — limits enforced BEFORE the decoders allocate pixel buffers.
// Both are read from ProjectSettings the first time a decode happens:
//   extended_images/max_pixels   (int, default 268435456 = 268 MP)
//   extended_images/max_file_size (int, default 268435456 = 256 MiB)
// Setting either to 0 disables that limit.

// v1.2.0 — fidelity:
//   extended_images/hdr_output (bool, default true): keep >= 10-bit sources
//   as 16-bit Godot Images (RGBH/RGBAH, half-float for float sources)
//   instead of quantizing to 8 bits.

namespace extimg {

// Returns the pixel cap (0 = unlimited). Cached after first read.
uint64_t get_max_pixels();
// Returns the input file size cap (0 = unlimited). Cached after first read.
uint64_t get_max_file_size();
// True when >= 10-bit / float sources should stay 16-bit. Cached.
bool get_hdr_output();
// Logs `p_what` once to remind the user a ProjectSetting could not be read.
void warn_setting_once(const char *p_setting);
// True if w*h is within the cap (and each side <= 65535). Prints a clear
// error naming the file dimensions and the active limit when it rejects.
bool check_dimensions(const char *p_format, uint32_t p_w, uint32_t p_h);
// True if the buffer size is within the file-size cap.
bool check_file_size(const char *p_format, size_t p_size);
// Prints a decode failure with the underlying library message.
void print_decode_error(const String &p_msg);

} // namespace extimg

// ------------------------------------------------------------
// Single-image decode (v1.0 API, now fidelity-aware):
//   - >= 10-bit or float sources decode to RGBH/RGBAH (16-bit or
//     half-float) when extended_images/hdr_output is enabled,
//     8-bit otherwise.
//   - Orientation (irot/imir for AVIF, JxlBasicInfo::orientation for
//     JXL, irot+imir properties for HEIC) is applied before returning.
// ------------------------------------------------------------
Ref<Image> decode_avif(const uint8_t *data, size_t size);
Ref<Image> decode_jxl(const uint8_t *data, size_t size);
Ref<Image> decode_heic(const uint8_t *data, size_t size);
Ref<Image> decode_qoi(const uint8_t *data, size_t size);

// ------------------------------------------------------------
// v1.2.0 animation / metadata API. Everything the Godot-side
// ExtendedImageInfo class needs to describe a decoded file:
//   - still image (any of the four formats)
//   - animated file (AVIF sequence or JXL animation): all frames +
//     per-frame durations in seconds + loop count (0 = infinite)
// Success leaves the output fields filled and returns true.
// On failure returns false after printing the library error.
// ------------------------------------------------------------
struct AnimInfo {
    bool animated = false;
    Vector<Ref<Image>> frames;          // every frame, orientation applied
    Vector<float> frame_durations;      // seconds; empty for non-animated
    int loop_count = 0;                 // 0 = infinite, n = play n+1 times (AVIF)
                                        // JXL num_loops: 0 = infinite, n = n loops
    float total_duration = 0.0f;       // seconds of one full playback
    int bit_depth = 8;                  // source bit depth
    bool is_float = false;              // source is floating point HDR
    bool has_alpha = false;
};
bool decode_avif_info(const uint8_t *data, size_t size, AnimInfo &out);
bool decode_jxl_info(const uint8_t *data, size_t size, AnimInfo &out);

#endif
