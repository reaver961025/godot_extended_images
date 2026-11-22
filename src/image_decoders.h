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
//   extended_images/max_pixels   (int, default 268435456 = 256 Mi pixels ~ 268 MP)
//   extended_images/max_file_size (int, default 268435456 = 256 MiB)
// Setting either to 0 disables that limit.
namespace extimg {

// Returns the pixel cap (0 = unlimited). Cached after first read.
uint64_t get_max_pixels();
// Returns the input file size cap (0 = unlimited). Cached after first read.
uint64_t get_max_file_size();
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

Ref<Image> decode_avif(const uint8_t *data, size_t size);
Ref<Image> decode_jxl(const uint8_t *data, size_t size);
Ref<Image> decode_heic(const uint8_t *data, size_t size);
Ref<Image> decode_qoi(const uint8_t *data, size_t size);
#endif
