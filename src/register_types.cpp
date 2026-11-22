#include "register_types.h"
#include "image_loader_avif.h"
#include "image_loader_jxl.h"
#include "image_loader_heic.h"
#include "image_loader_qoi.h"
#include "resource_format_loader_extended.h"
#include "editor_preview_generator_extended.h"
#include "extended_images_editor_plugin.h"
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/godot.hpp>
using namespace godot;
static Ref<ImageFormatLoaderAVIF> g_avif;
static Ref<ImageFormatLoaderJXL> g_jxl;
static Ref<ImageFormatLoaderHEIC> g_heic;
static Ref<ImageFormatLoaderQOI> g_qoi;
static Ref<ResourceFormatLoaderExtended> g_rfl;
void register_types(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<ImageFormatLoaderAVIF>();
        ClassDB::register_class<ImageFormatLoaderJXL>();
        ClassDB::register_class<ImageFormatLoaderHEIC>();
        ClassDB::register_class<ImageFormatLoaderQOI>();
        ClassDB::register_class<ResourceFormatLoaderExtended>();
        g_avif.instantiate(); if (g_avif.is_valid()) g_avif->add_format_loader();
        g_jxl.instantiate(); if (g_jxl.is_valid()) g_jxl->add_format_loader();
        g_heic.instantiate(); if (g_heic.is_valid()) g_heic->add_format_loader();
        g_qoi.instantiate(); if (g_qoi.is_valid()) g_qoi->add_format_loader();
        g_rfl.instantiate(); if (g_rfl.is_valid())
            ResourceLoader::get_singleton()->add_resource_format_loader(g_rfl, true);
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        ClassDB::register_class<EditorPreviewGeneratorExtended>();
        ClassDB::register_class<ExtendedImagesEditorPlugin>();
        EditorPlugins::add_by_type<ExtendedImagesEditorPlugin>();
    }
}
void unregister_types(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        if (g_avif.is_valid()) { g_avif->remove_format_loader(); g_avif.unref(); }
        if (g_jxl.is_valid()) { g_jxl->remove_format_loader(); g_jxl.unref(); }
        if (g_heic.is_valid()) { g_heic->remove_format_loader(); g_heic.unref(); }
        if (g_qoi.is_valid()) { g_qoi->remove_format_loader(); g_qoi.unref(); }
        if (g_rfl.is_valid()) { ResourceLoader::get_singleton()->remove_resource_format_loader(g_rfl); g_rfl.unref(); }
    } else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        EditorPlugins::remove_by_type<ExtendedImagesEditorPlugin>();
    }
}
extern "C" {
GDExtensionBool GDE_EXPORT extended_images_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(register_types);
    init_obj.register_terminator(unregister_types);
    init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
