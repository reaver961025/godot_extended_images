#ifndef IMAGE_LOADER_AVIF_H
#define IMAGE_LOADER_AVIF_H
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_format_loader_extension.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/type_info.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
using namespace godot;
class ImageFormatLoaderAVIF : public ImageFormatLoaderExtension {
    GDCLASS(ImageFormatLoaderAVIF, ImageFormatLoaderExtension);
protected:
    static void _bind_methods();
public:
    virtual PackedStringArray _get_recognized_extensions() const override;
    virtual Error _load_image(const Ref<Image> &p_image, const Ref<FileAccess> &p_fileaccess,
        BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) override;
};
#endif
