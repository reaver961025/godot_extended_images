#ifndef EXTENDED_IMAGES_EDITOR_PLUGIN_H
#define EXTENDED_IMAGES_EDITOR_PLUGIN_H
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_resource_preview.hpp>
#include "editor_preview_generator_extended.h"
using namespace godot;
class ExtendedImagesEditorPlugin : public EditorPlugin {
    GDCLASS(ExtendedImagesEditorPlugin, EditorPlugin);
protected:
    static void _bind_methods();
public:
    void _enter_tree() override;
    void _exit_tree() override;
private:
    Ref<EditorPreviewGeneratorExtended> preview_generator;
};
#endif
