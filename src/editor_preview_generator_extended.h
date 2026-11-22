#ifndef EDITOR_PREVIEW_GENERATOR_EXTENDED_H
#define EDITOR_PREVIEW_GENERATOR_EXTENDED_H
#include <godot_cpp/classes/editor_resource_preview_generator.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
using namespace godot;
class EditorPreviewGeneratorExtended : public EditorResourcePreviewGenerator {
    GDCLASS(EditorPreviewGeneratorExtended, EditorResourcePreviewGenerator);
protected:
    static void _bind_methods();
public:
    virtual bool _handles(const String &p_type) const override;
    virtual Ref<Texture2D> _generate_from_path(const String &p_path, const Vector2i &p_size, const Dictionary &p_metadata) const override;
    virtual Ref<Texture2D> _generate(const Ref<Resource> &p_resource, const Vector2i &p_size, const Dictionary &p_metadata) const override;
    virtual bool _generate_small_preview_automatically() const override;
    virtual bool _can_generate_small_preview() const override;
};
#endif
