// Soldado: modelo, animacion y estadisticas. Las armas las dispara Game,
// que es quien conoce a los enemigos.
#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace cc {

class Player : public godot::Node3D {
	GDCLASS(Player, godot::Node3D)

public:
	// --- Estadisticas (base + mejoras permanentes de GameData) ---
	float max_hp = 100.0f;
	float hp = 100.0f;
	float speed = 7.0f;
	float damage = 10.0f;
	float fire_interval = 0.45f;
	int projectiles = 1;
	int pierce = 0;
	float magnet_radius = 3.0f;
	float regen = 0.0f;
	float core_mult = 1.0f;
	float crit_chance = 0.05f;

	// --- Escudo de energia (habilidad) ---
	float shield = 0.0f;
	float shield_max = 0.0f;
	float shield_delay = 0.0f;

	// --- Impulso / dash (habilidad activa) ---
	float dash_cd = 0.0f;
	float dash_time = 0.0f;
	godot::Vector3 dash_dir;
	static constexpr float DASH_COOLDOWN = 2.5f;
	static constexpr float DASH_DURATION = 0.18f;

	bool alive = true;

	void build();
	void apply_meta();
	// Devuelve true si este golpe mato al jugador.
	bool take_damage(float amount);
	void heal(float amount);
	bool try_dash(const godot::Vector3 &dir);
	void animate(const godot::Vector3 &move_dir, bool has_target, const godot::Vector3 &target_pos, float delta);
	godot::Vector3 muzzle_position() const;
	float facing() const;

protected:
	static void _bind_methods() {}

private:
	float anim_t = 0.0f;
	float hurt_flash = 0.0f;
	float death_t = 0.0f;
	godot::Node3D *model = nullptr;
	godot::Node3D *leg_l = nullptr;
	godot::Node3D *leg_r = nullptr;
	godot::Node3D *arm_l = nullptr;
	godot::Node3D *cape = nullptr;
	godot::Node3D *muzzle = nullptr;
	godot::Ref<godot::StandardMaterial3D> armor_mat;
};

} // namespace cc
