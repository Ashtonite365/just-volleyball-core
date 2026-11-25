#include "mod_api.hpp"
#include "mod_gate.hpp"
#include "menu_ui.hpp"
#include "picker_ui.hpp"
#include "menu_perf.hpp"
#include "devtools.hpp"
#include "settings_ui.hpp"
#include "player_consts_boot.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void init(ModuleInitializationLevel level) {
	if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
	GDREGISTER_CLASS(JVModGate);
	GDREGISTER_CLASS(JVModAPI);
	GDREGISTER_CLASS(JVMenuUI);
	GDREGISTER_CLASS(JVPickerUI);
	GDREGISTER_CLASS(JVMenuPerf);
	GDREGISTER_CLASS(JVDevTools);
	GDREGISTER_CLASS(JVSettingsUI);
	GDREGISTER_CLASS(JVPlayerConsts);
}
static void deinit(ModuleInitializationLevel) {}

extern "C" GDExtensionBool GDE_EXPORT jvm_core_init(GDExtensionInterfaceGetProcAddress p, const GDExtensionClassLibraryPtr l, GDExtensionInitialization *i) {
	GDExtensionBinding::InitObject o(p, l, i);
	o.register_initializer(init);
	o.register_terminator(deinit);
	o.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return o.init();
}
