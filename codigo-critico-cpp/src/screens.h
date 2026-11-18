// Pantallas del juego (nodos Control): inicio, registro, game over, ranking y armeria.
// Cada una es la raiz de su propia escena en res://scenes/.
#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

namespace cc {

extern const char *SCENE_MENU;
extern const char *SCENE_REGISTER;
extern const char *SCENE_GAME;
extern const char *SCENE_GAME_OVER;
extern const char *SCENE_RANKING;
extern const char *SCENE_SHOP;

// Base comun: cambio de escena y tecla ESC para volver.
class ScreenBase : public godot::Control {
	GDCLASS(ScreenBase, godot::Control)

public:
	void _unhandled_input(const godot::Ref<godot::InputEvent> &event) override;

	// Publicos para poder conectarlos a los botones con callable_mp.
	void go_to(const char *scene);
	void go_menu();
	void go_register();
	void go_game();
	void go_ranking();
	void go_shop();

protected:
	static void _bind_methods() {}
	const char *back_scene = nullptr; // a donde lleva ESC (nullptr = nada)
	bool in_editor() const;
};

// Pantalla de inicio: Iniciar Juego, Ver Ranking, Armeria, Salir.
class MainMenu : public ScreenBase {
	GDCLASS(MainMenu, ScreenBase)

public:
	void _ready() override;

protected:
	static void _bind_methods() {}

private:
	void on_quit();
};

// Pantalla de registro: nombre, equipo o alias antes de entrar al nivel.
class RegisterScreen : public ScreenBase {
	GDCLASS(RegisterScreen, ScreenBase)

public:
	void _ready() override;

protected:
	static void _bind_methods() {}

private:
	godot::LineEdit *name_edit = nullptr;
	godot::Label *error_label = nullptr;
	void on_start();
	void on_submitted(const godot::String &text);
};

// Pantalla de fin de juego: puntaje total, Reintentar, Menu Principal.
class GameOverScreen : public ScreenBase {
	GDCLASS(GameOverScreen, ScreenBase)

public:
	void _ready() override;

protected:
	static void _bind_methods() {}
};

// Tabla de mejores puntajes (nombre registrado + puntaje final).
class RankingScreen : public ScreenBase {
	GDCLASS(RankingScreen, ScreenBase)

public:
	void _ready() override;

protected:
	static void _bind_methods() {}
};

// Armeria: mejoras permanentes que se compran con nucleos (parte incremental).
class ShopScreen : public ScreenBase {
	GDCLASS(ShopScreen, ScreenBase)

public:
	void _ready() override;

protected:
	static void _bind_methods() {}

private:
	godot::VBoxContainer *content = nullptr;
	void refresh(int focus_id);
	void on_buy(int id);
};

} // namespace cc
