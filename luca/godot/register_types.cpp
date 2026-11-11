// GDExtension entry point: tells Godot which classes this library adds.

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "luca_world.h"

using namespace godot;

namespace {

void initialize_luca(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(LucaWorld);
}

void uninitialize_luca(ModuleInitializationLevel) {}

}  // namespace

extern "C" {
GDExtensionBool GDE_EXPORT luca_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                             GDExtensionClassLibraryPtr library,
                                             GDExtensionInitialization* initialization) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_luca);
    init.register_terminator(uninitialize_luca);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
}
