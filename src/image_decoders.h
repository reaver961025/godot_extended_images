#ifndef IMAGE_DECODERS_H
#define IMAGE_DECODERS_H
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
using namespace godot;
Ref<Image> decode_avif(const uint8_t *data, size_t size);
Ref<Image> decode_jxl(const uint8_t *data, size_t size);
Ref<Image> decode_heic(const uint8_t *data, size_t size);
Ref<Image> decode_qoi(const uint8_t *data, size_t size);
#endif
