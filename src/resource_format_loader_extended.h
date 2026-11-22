#ifndef RESOURCE_FORMAT_LOADER_EXTENDED_H
#define RESOURCE_FORMAT_LOADER_EXTENDED_H
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
using namespace godot;
class ResourceFormatLoaderExtended : public ResourceFormatLoader {
    GDCLASS(ResourceFormatLoaderExtended, ResourceFormatLoader);
protected:
    static void _bind_methods();
public:
    virtual PackedStringArray _get_recognized_extensions() const override;
    virtual bool _recognize_path(const String &p_path, const StringName &p_type) const override;
    virtual bool _handles_type(const StringName &p_type) const override;
    virtual String _get_resource_type(const String &p_path) const override;
    virtual bool _exists(const String &p_path) const override;
    virtual Variant _load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const override;
};
#endif
