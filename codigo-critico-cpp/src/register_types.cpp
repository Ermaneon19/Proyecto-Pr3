// Punto de entrada de la GDExtension: registra las clases C++ en Godot.
#include "game.h"
#include "hud.h"
#include "player.h"
#include "screens.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void initialize_codigo_critico(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(cc::Player);
	GDREGISTER_CLASS(cc::Hud);
	GDREGISTER_CLASS(cc::Game);
	GDREGISTER_ABSTRACT_CLASS(cc::ScreenBase);
	GDREGISTER_CLASS(cc::MainMenu);
	GDREGISTER_CLASS(cc::RegisterScreen);
	GDREGISTER_CLASS(cc::GameOverScreen);
	GDREGISTER_CLASS(cc::RankingScreen);
	GDREGISTER_CLASS(cc::ShopScreen);
}

static void uninitialize_codigo_critico(ModuleInitializationLevel p_level) {
}

extern "C" {
GDExtensionBool GDE_EXPORT codigo_critico_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
		const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_codigo_critico);
	init_obj.register_terminator(uninitialize_codigo_critico);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}
