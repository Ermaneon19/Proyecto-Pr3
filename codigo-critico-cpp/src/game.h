// Orquestador de la partida: mundo, camara, oleadas, colisiones (grid espacial),
// armas/habilidades, experiencia/niveles y fin de partida.
//
// Enemigos, balas, recolectables y efectos NO son nodos: son structs en vectores
// contiguos que se simulan en C++ y se dibujan con MultiMesh (ver batch.h).
#pragma once

#include "batch.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace cc {

class Player;
class Hud;

// Mejoras que se eligen al subir de nivel.
enum Upgrade {
	U_DMG,
	U_RATE,
	U_MULTI,
	U_PIERCE,
	U_CRIT,
	U_SPEED,
	U_HP,
	U_MAGNET,
	U_STRIKE,
	U_DRONES,
	U_NOVA,
	U_CHAIN,
	U_MISSILE,
	U_SHIELD,
	U_HEAL, // solo aparece cuando todo lo demas esta al maximo
	U_COUNT
};

struct UpgradeDef {
	const char *title;
	const char *desc;
	int max;
	bool ability; // true = habilidad/arma nueva (se etiqueta "NUEVO" al nivel 0)
};

enum EnemyType {
	E_INSECT, // basico
	E_WASP, // rapido y fragil
	E_BEETLE, // lento y blindado
	E_SPITTER, // dispara acido a distancia
	E_TYPE_COUNT
};

struct Enemy {
	godot::Vector3 pos;
	godot::Vector3 knock;
	float yaw = 0.0f;
	float hp = 25.0f;
	float max_hp = 25.0f;
	float speed = 3.0f;
	float radius = 0.5f;
	float contact_dps = 10.0f;
	float scale = 1.0f;
	float t = 0.0f;
	float age = 0.0f;
	float flash = 0.0f;
	float drone_cd = 0.0f;
	float shoot_cd = 0.0f;
	float xp = 1.0f;
	int id = 0;
	uint8_t type = E_INSECT;
	bool elite = false;
	bool alive = true;
};

struct Bullet {
	godot::Vector3 pos;
	godot::Vector3 vel;
	float damage = 10.0f;
	float life = 1.1f;
	int pierce_left = 0;
	bool crit = false;
	int hit_ids[8];
	int hit_count = 0;
};

struct Missile {
	godot::Vector3 pos;
	godot::Vector3 vel;
	float damage = 10.0f;
	float radius = 2.2f;
	float life = 3.0f;
};

struct EnemyShot {
	godot::Vector3 pos;
	godot::Vector3 vel;
	float damage = 8.0f;
	float life = 4.0f;
};

struct Pickup {
	godot::Vector3 pos;
	float value = 1.0f;
	float vel = 0.0f;
	float t = 0.0f;
	bool core = false;
	bool heal = false;
	bool attracted = false;
};

enum PickupKind {
	PK_XP,
	PK_CORE,
	PK_HEAL,
};

// Jarron rompible: al golpearlo suelta botiquines.
struct Jar {
	godot::Vector3 pos;
	float hp = 2.0f; // golpes que aguanta
	float flash = 0.0f;
	float age = 0.0f;
	int id = 0;
};

struct Strike {
	godot::Vector3 pos;
	float radius = 2.6f;
	float damage = 10.0f;
	float t = 0.0f;
};

enum FxKind {
	FX_SPHERE, // esfera que crece y se desvanece (explosiones, muertes)
	FX_RING, // disco plano que se expande (pulso de choque)
	FX_BEAM, // segmento entre dos puntos (rayo en cadena, estela)
};

struct Fx {
	godot::Vector3 a;
	godot::Vector3 b;
	godot::Color color;
	float age = 0.0f;
	float life = 0.3f;
	float r0 = 0.3f;
	float r1 = 1.0f;
	float energy = 3.0f;
	uint8_t kind = FX_SPHERE;
};

class Game : public godot::Node3D {
	GDCLASS(Game, godot::Node3D)

public:
	enum State {
		PLAYING,
		LEVELUP,
		PAUSED,
		DEAD,
	};

	static const UpgradeDef UPGRADES[U_COUNT];
	static const int MAX_ENEMIES = 320;

	void _ready() override;
	void _process(double delta) override;

	// --- Estado que lee el HUD ---
	Player *player = nullptr;
	godot::Camera3D *camera = nullptr;
	State state = PLAYING;
	float run_time = 0.0f;
	int kills = 0;
	int elite_kills = 0;
	int level = 1;
	float xp = 0.0f;
	float xp_next = 4.0f;
	float run_cores = 0.0f;
	int taken[U_COUNT] = {};
	std::vector<int> levelup_options;
	float strike_timer = 2.0f, strike_interval = 6.0f;
	float nova_timer = 2.0f, nova_interval = 5.0f;
	float chain_timer = 1.5f, chain_interval = 2.6f;
	float missile_timer = 2.5f, missile_interval = 3.5f;

	int score() const;
	int enemy_count() const { return int(enemies.size()); }
	int rifle_level() const;

	// --- Acciones desde el HUD ---
	void choose_upgrade(int option_index);
	void toggle_pause();
	void quit_to_menu();

protected:
	static void _bind_methods() {}

private:
	// mundo
	godot::Node3D *world = nullptr;
	godot::MeshInstance3D *ground = nullptr;
	Hud *hud = nullptr;
	float shake = 0.0f;

	// entidades
	std::vector<Enemy> enemies;
	std::vector<Bullet> bullets;
	std::vector<Missile> missiles;
	std::vector<EnemyShot> shots;
	std::vector<Pickup> pickups;
	std::vector<Strike> strikes;
	std::vector<Fx> fxs;
	std::vector<Jar> jars;
	int next_enemy_id = 1;
	int next_jar_id = 1;
	float jar_timer = 0.0f;

	// Modo demo para pruebas (variable de entorno CC_AUTOPLAY): el soldado camina
	// solo y elige mejoras automaticamente. Con CC_AUTOPLAY=god ademas no recibe dano.
	bool autoplay = false;
	bool autoplay_god = false;

	// oleadas
	float spawn_timer = 1.0f;
	float next_elite = 60.0f;
	int pending_levels = 0;
	float dead_timer = 0.0f;

	// armas
	float fire_timer = 0.0f;
	float drone_angle = 0.0f;
	float dash_trail_timer = 0.0f;
	int target_index = -1;

	// grid espacial (listas enlazadas por celda, se reconstruye cada frame)
	static const int GRID_N = 64;
	static constexpr float CELL = 2.0f;
	std::vector<int> cell_head;
	std::vector<int> cell_next;
	float grid_x0 = 0.0f, grid_z0 = 0.0f;

	// render por lotes
	Batch lit_sphere, lit_box, lit_gem, lit_cone;
	Batch add_sphere, add_box, add_cyl;
	Batch mix_disc;

	// numeros de dano
	static const int DAMAGE_LABELS = 14;
	godot::Label3D *damage_labels[DAMAGE_LABELS] = {};
	float damage_label_age[DAMAGE_LABELS];
	float damage_label_timer = 0.0f;

	void build_world();
	void build_batches();
	void init_damage_labels();

	void update_camera(float delta);
	void update_player(float delta);
	void update_weapons(float delta);
	void update_spawning(float delta);
	void update_enemies(float delta);
	void update_bullets(float delta);
	void update_missiles(float delta);
	void update_shots(float delta);
	void update_pickups(float delta);
	void update_strikes(float delta);
	void update_jars(float delta);
	void spawn_jar(float min_dist, float max_dist);
	void hit_jar(int index);
	void damage_jars(const godot::Vector3 &pos, float radius);
	void update_fx(float delta);
	void update_damage_labels(float delta);
	void remove_dead();
	void render();
	void render_enemy(const Enemy &e);

	void spawn_enemy(bool elite, float minutes);
	void damage_enemy(int index, float amount, bool crit = false);
	void spawn_pickup(const godot::Vector3 &pos, int kind, float value);
	void collect(const Pickup &p);
	void open_levelup();
	void apply_upgrade(int id);
	void on_player_died();
	void explode(const godot::Vector3 &pos, float radius, float dmg, const godot::Color &color);
	void add_fx(uint8_t kind, const godot::Vector3 &a, const godot::Vector3 &b, const godot::Color &color, float life, float r0, float r1, float energy = 3.0f);
	// style: 0 = normal, 1 = critico, 2 = curacion
	void damage_number(const godot::Vector3 &pos, float amount, int style);

	void rebuild_grid();
	int nearest_enemy(const godot::Vector3 &pos, float max_dist, const int *exclude_ids = nullptr, int exclude_count = 0) const;
	int random_enemy_near(const godot::Vector3 &pos, float max_dist) const;

	template <typename F>
	inline void query(const godot::Vector3 &pos, float r, F &&fn) const {
		int x0 = int(std::floor((pos.x - r - grid_x0) / CELL));
		int x1 = int(std::floor((pos.x + r - grid_x0) / CELL));
		int z0 = int(std::floor((pos.z - r - grid_z0) / CELL));
		int z1 = int(std::floor((pos.z + r - grid_z0) / CELL));
		if (x0 < 0) x0 = 0;
		if (z0 < 0) z0 = 0;
		if (x1 >= GRID_N) x1 = GRID_N - 1;
		if (z1 >= GRID_N) z1 = GRID_N - 1;
		for (int z = z0; z <= z1; z++) {
			for (int x = x0; x <= x1; x++) {
				for (int i = cell_head[z * GRID_N + x]; i >= 0; i = cell_next[i]) {
					fn(i);
				}
			}
		}
	}
};

} // namespace cc
