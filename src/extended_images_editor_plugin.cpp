#include "extended_images_editor_plugin.h"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/core/class_db.hpp>
using namespace godot;
void ExtendedImagesEditorPlugin::_bind_methods() {}
void ExtendedImagesEditorPlugin::_enter_tree() {
    preview_generator.instantiate();
    if (preview_generator.is_valid()) {
        EditorResourcePreview *p = EditorInterface::get_singleton()->get_resource_previewer();
        if (p) p->add_preview_generator(preview_generator);
    }
}
void ExtendedImagesEditorPlugin::_exit_tree() {
    if (preview_generator.is_valid()) {
        EditorResourcePreview *p = EditorInterface::get_singleton()->get_resource_previewer();
        if (p) p->remove_preview_generator(preview_generator);
        preview_generator.unref();
    }
}
