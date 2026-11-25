#ifndef EXTENDED_IMAGE_INFO_H
#define EXTENDED_IMAGE_INFO_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

// v1.2.0 — Describes a decoded extended image file: still frames or a full
// animation, plus source bit depth / HDR flags and loop metadata.
//
// Usage:
//   var info = ExtendedImageInfo.new()
//   if info.load_file("res://photo.avif"):
//       var img = info.get_frame(0)              # first frame
//       if info.is_animated():
//           for i in info.get_frame_count():
//               var frame = info.get_frame(i)
//               var dur  = info.get_frame_duration(i)
class ExtendedImageInfo : public RefCounted {
    GDCLASS(ExtendedImageInfo, RefCounted)

private:
    // Filled by load_file / load_bytes.
    Array frames;                  // Array of Ref<Image>
    PackedFloat32Array durations;  // seconds per frame
    int loop_count = 0;            // 0 = infinite (both AVIF & JXL semantics)
    float total_duration = 0.0f;
    int bit_depth = 8;
    bool is_float = false;
    bool has_alpha = false;
    bool animated = false;
    String error = String();

protected:
    static void _bind_methods();

public:
    ExtendedImageInfo();
    ~ExtendedImageInfo();

    // Loads and decodes a file. On failure returns false and get_error()
    // holds a message. AVIF and JXL decode every frame; HEIC/HEIF and QOI
    // decode the single still image.
    bool load_file(const String &p_path);
    // Same, from an existing buffer with a format hint ("avif", "jxl",
    // "heic", "heif" or "qoi").
    bool load_bytes(const PackedByteArray &p_bytes, const String &p_format);

    // --- animation ---
    bool is_animated() const;
    int get_frame_count() const;
    Ref<Image> get_frame(int p_index) const;
    float get_frame_duration(int p_index) const;
    PackedFloat32Array get_frame_durations() const;
    // 0 = infinite, n = play n+1 times (AVIF) / n times (JXL).
    int get_loop_count() const;
    // Seconds of one full playback pass.
    float get_total_duration() const;

    // --- source fidelity ---
    // Source bit depth (8, 10, 12, 16).
    int get_bit_depth() const;
    // True when the source is floating point HDR (half float).
    bool is_hdr() const;
    bool get_has_alpha() const;

    // Last error message, empty when the last load succeeded.
    String get_error() const;
};

} // namespace godot

#endif // EXTENDED_IMAGE_INFO_H
