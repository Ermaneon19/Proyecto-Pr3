#include "game.h"

#include "game_data.h"
#include "hud.h"
#include "mesh_util.h"
#include "player.h"
#include "util.h"

#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/world_environment.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace cc {

static const Vector3 CAM_OFFSET(0, 15, 10);

const UpgradeDef Game::UPGRADES[U_COUNT] = {
	{ "Calibre mayor", "+20% daño de todas las armas", 99, false },
	{ "Gatillo ligero", "+15% cadencia del rifle", 10, false },
	{ "Ráfaga", "+1 proyectil por disparo", 5, false },
	{ "Perforación", "Las balas atraviesan +1 enemigo", 5, false },
	{ "Munición crítica", "+10% probabilidad de crítico (daño x2)", 5, false },
	{ "Servo-piernas", "+10% velocidad", 5, false },
	{ "Placas de blindaje", "+25 vida máxima", 99, false },
	{ "Imán", "+30% radio de recolección", 5, false },
	{ "Bombardeo orbital", "Explosiones desde la órbita", 8, true },
	{ "Drones de escolta", "+1 dron que gira a tu alrededor", 6, true },
	{ "Pulso de choque", "Onda que daña y empuja a los enemigos cercanos", 6, true },
	{ "Rayo en cadena", "Descarga que salta de enemigo en enemigo", 6, true },
	{ "Misiles buscadores", "Misiles que persiguen y explotan", 6, true },
	{ "Escudo de energía", "Barrera que absorbe daño y se recarga sola", 5, true },
	{ "Botiquín", "Recupera 50% de vida", 99, false },
};

// ---------------------------------------------------------------- shaders de los lotes
// Opaco con luz: el color de instancia es el albedo; INSTANCE_CUSTOM lleva
// (destello, emision, metalico). Un borde de luz (rim) separa las siluetas del suelo.
static const char *SHADER_LIT = R"(
shader_type spatial;
varying vec4 cust;
void vertex() {
	cust = INSTANCE_CUSTOM;
}
void fragment() {
	ALBEDO = COLOR.rgb;
	METALLIC = cust.b;
	ROUGHNESS = 0.45;
	float rim = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 2.5);
	// Borde de luz bicolor (turquesa arriba/izquierda, magenta abajo/derecha) como en
	// el arte conceptual; cust.a decide cuanto se aplica (solo enemigos).
	vec3 rim_color = mix(vec3(0.1, 0.95, 0.8), vec3(1.0, 0.2, 0.75), smoothstep(-0.35, 0.35, NORMAL.x - NORMAL.y * 0.6));
	// La emision se atenua para que el bloom no queme los colores hasta el blanco.
	EMISSION = COLOR.rgb * (cust.g * 0.4 + rim * 0.3) + rim_color * rim * cust.a * 0.85 + vec3(cust.r * 1.1);
}
)";

// Aditivo sin luz: balas, rayos, explosiones, alas.
static const char *SHADER_ADD = R"(
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;
varying float energy;
void vertex() {
	energy = INSTANCE_CUSTOM.g;
}
void fragment() {
	ALBEDO = COLOR.rgb * energy;
	ALPHA = COLOR.a;
}
)";

// Mezcla alfa sin luz: sombras falsas bajo los enemigos.
static const char *SHADER_MIX = R"(
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, shadows_disabled;
void fragment() {
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
}
)";

static const char *SHADER_GROUND = R"(
// Terreno alienigena procedural en coordenadas de mundo (el plano sigue al jugador
// y el dibujo no se mueve): tierra con ruido fractal, losas agrietadas (voronoi),
// vetas que laten con luz violeta, manchas de musgo luminiscente y relieve falso.
shader_type spatial;
varying vec3 wpos;

float hash21(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
			mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}
float fbm(vec2 p) {
	float a = 0.5;
	float s = 0.0;
	for (int i = 0; i < 4; i++) {
		s += a * vnoise(p);
		p = p * 2.03 + vec2(17.0, 9.0);
		a *= 0.5;
	}
	return s;
}
// Distancias al punto mas cercano y al segundo: su diferencia marca las grietas.
vec2 voronoi(vec2 p) {
	vec2 n = floor(p);
	vec2 f = fract(p);
	float d1 = 8.0;
	float d2 = 8.0;
	for (int j = -1; j <= 1; j++) {
		for (int i = -1; i <= 1; i++) {
			vec2 g = vec2(float(i), float(j));
			vec2 o = vec2(hash21(n + g), hash21(n + g + vec2(31.7, 12.3)));
			float d = length(g + o - f);
			if (d < d1) {
				d2 = d1;
				d1 = d;
			} else if (d < d2) {
				d2 = d;
			}
		}
	}
	return vec2(d1, d2);
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = wpos.xz;
	float n = fbm(p * 0.35);
	float fine = fbm(p * 2.2);
	vec2 v = voronoi(p * 0.42 + n * 0.7);
	float edge = v.y - v.x;
	float crack = 1.0 - smoothstep(0.0, 0.07, edge);
	float bevel = 1.0 - smoothstep(0.0, 0.3, edge);

	vec3 soil = mix(vec3(0.03, 0.034, 0.058), vec3(0.085, 0.08, 0.125), n);
	soil *= 0.7 + 0.6 * fine;
	float moss = smoothstep(0.52, 0.72, fbm(p * 0.11 + vec2(40.0, 3.0)));
	soil = mix(soil, vec3(0.03, 0.12, 0.12), moss * 0.65);
	ALBEDO = soil * (1.0 - crack * 0.75);

	// Solo algunas zonas tienen vetas encendidas, asi el suelo no se ve repetido.
	float region = smoothstep(0.45, 0.62, fbm(p * 0.07 + vec2(7.0, 21.0)));
	float pulse = 0.55 + 0.45 * sin(TIME * 1.4 + n * 14.0);
	EMISSION = vec3(0.7, 0.14, 0.95) * crack * region * pulse * 1.3
			+ vec3(0.05, 0.55, 0.5) * moss * fine * 0.22;

	ROUGHNESS = mix(0.95, 0.5, smoothstep(0.5, 0.8, fine) * 0.6);
	METALLIC = 0.12;

	// Relieve por derivadas en pantalla (sin texturas ni tangentes).
	float h = fine * 0.5 + n * 0.4 - bevel * 0.5 - crack * 0.6;
	vec3 dpdx = dFdx(VERTEX);
	vec3 dpdy = dFdy(VERTEX);
	float dhx = dFdx(h);
	float dhy = dFdy(h);
	vec3 r1 = cross(dpdy, NORMAL);
	vec3 r2 = cross(NORMAL, dpdx);
	float det = dot(dpdx, r1);
	vec3 grad = sign(det) * (dhx * r1 + dhy * r2);
	NORMAL = normalize(abs(det) * NORMAL - 0.55 * grad);
}
)";

// ---------------------------------------------------------------- modelos de enemigos
// Cada enemigo se describe como una lista de piezas (primitiva + transform local)
// que se envian a los lotes. Nada de nodos por enemigo.
enum { B_SPHERE, B_BOX, B_CONE, B_WING };
enum { A_NONE, A_WING_L, A_WING_R, A_BLADE_L, A_BLADE_R, A_LEG };

struct Part {
	Transform3D local;
	Vector3 pivot;
	Color color;
	float emis = 0.0f;
	float metal = 0.35f;
	float phase = 0.0f;
	uint8_t batch = B_SPHERE;
	uint8_t anim = A_NONE;
};

static const int MODEL_ELITE = E_TYPE_COUNT; // modelo extra: insecto elite
static std::vector<Part> g_models[E_TYPE_COUNT + 1];

static void add_part(std::vector<Part> &m, uint8_t batch, const Vector3 &pos, const Vector3 &rot, const Vector3 &scl,
		const Color &col, float emis = 0.0f, float metal = 0.35f) {
	Part p;
	p.batch = batch;
	p.local = make_xf(pos, rot, scl);
	p.color = col;
	p.emis = emis;
	p.metal = metal;
	m.push_back(p);
}

static void add_anim(std::vector<Part> &m, uint8_t batch, uint8_t anim, float phase, const Vector3 &pivot, const Vector3 &pos,
		const Vector3 &rot, const Vector3 &scl, const Color &col, float emis = 0.0f, float metal = 0.35f) {
	Part p;
	p.batch = batch;
	p.anim = anim;
	p.phase = phase;
	p.pivot = pivot;
	p.local = make_xf(pos, rot, scl);
	p.color = col;
	p.emis = emis;
	p.metal = metal;
	m.push_back(p);
}

static void add_legs(std::vector<Part> &m, int pairs, float x, float y, float z0, float dz, const Vector3 &size, float tilt, const Color &col) {
	for (int side = -1; side <= 1; side += 2) {
		for (int i = 0; i < pairs; i++) {
			float phase = i * 2.1f + (side > 0 ? PI : 0.0f);
			add_anim(m, B_BOX, A_LEG, phase, Vector3(x * side, y, z0 + i * dz), Vector3(), Vector3(0, 0, tilt * side), size, col, 0.0f, 0.3f);
		}
	}
}

// Insecto mecanico: blindaje gris acero, ojos rasgados naranjas, antenas, mandibulas,
// abdomen segmentado con aguijon y dos guadanas en media luna que van de magenta a naranja.
static void build_insect(std::vector<Part> &m, bool elite) {
	Color plate = elite ? Color(0.36f, 0.12f, 0.14f) : Color(0.3f, 0.33f, 0.37f);
	Color dark = elite ? Color(0.15f, 0.05f, 0.07f) : Color(0.11f, 0.12f, 0.15f);
	Color teal(0.15f, 0.85f, 0.75f);
	Color magenta(1.0f, 0.15f, 0.8f);
	Color orange(1.0f, 0.55f, 0.08f);
	Color eye = elite ? Color(1.0f, 0.08f, 0.05f) : Color(1.0f, 0.38f, 0.04f);
	Color bone(0.78f, 0.74f, 0.66f);
	Color blade_a = elite ? Color(1.0f, 0.1f, 0.25f) : magenta;
	Color blade_b = elite ? Color(1.0f, 0.75f, 0.15f) : orange;

	// --- Torax blindado (frente = -Z) ---
	add_part(m, B_SPHERE, Vector3(0, 1.0f, 0), Vector3(), Vector3(0.85f, 0.7f, 1.0f), plate, 0.0f, 0.75f);
	for (int i = 0; i < 3; i++) {
		// Placas dorsales superpuestas y una espina sobre cada una.
		add_part(m, B_BOX, Vector3(0, 1.32f - i * 0.06f, -0.25f + i * 0.32f), Vector3(-18, 0, 0), Vector3(0.6f - i * 0.1f, 0.08f, 0.36f), dark, 0.0f, 0.8f);
		add_part(m, B_CONE, Vector3(0, 1.5f - i * 0.07f, -0.2f + i * 0.32f), Vector3(30, 0, 0), Vector3(0.12f, elite ? 0.55f : 0.34f, 0.12f),
				elite ? blade_a : plate, elite ? 2.5f : 0.0f, 0.8f);
	}
	for (int s = -1; s <= 1; s += 2) {
		add_part(m, B_SPHERE, Vector3(0.42f * s, 1.08f, -0.3f), Vector3(), Vector3(0.36f, 0.32f, 0.42f), plate, 0.0f, 0.8f); // hombrera
		add_part(m, B_BOX, Vector3(0.52f * s, 1.02f, -0.3f), Vector3(0, 0, -20.0f * s), Vector3(0.04f, 0.2f, 0.36f), teal, 1.4f, 0.0f); // canto turquesa
	}

	// --- Abdomen segmentado que se curva hacia abajo, con aguijon ---
	add_part(m, B_SPHERE, Vector3(0, 0.88f, 0.7f), Vector3(-20, 0, 0), Vector3(0.7f, 0.6f, 0.85f), dark, 0.0f, 0.7f);
	add_part(m, B_SPHERE, Vector3(0, 0.68f, 1.22f), Vector3(-35, 0, 0), Vector3(0.56f, 0.48f, 0.7f), plate, 0.0f, 0.7f);
	add_part(m, B_SPHERE, Vector3(0, 0.46f, 1.62f), Vector3(-50, 0, 0), Vector3(0.4f, 0.36f, 0.55f), dark, 0.0f, 0.7f);
	add_part(m, B_CONE, Vector3(0, 0.26f, 1.95f), Vector3(118, 0, 0), Vector3(0.2f, 0.55f, 0.2f), plate, 0.0f, 0.8f);
	add_part(m, B_BOX, Vector3(0, 0.8f, 0.98f), Vector3(-28, 0, 0), Vector3(0.5f, 0.36f, 0.04f), magenta, 2.2f, 0.0f); // juntas encendidas
	add_part(m, B_BOX, Vector3(0, 0.58f, 1.44f), Vector3(-42, 0, 0), Vector3(0.38f, 0.3f, 0.04f), magenta, 2.2f, 0.0f);

	// --- Cabeza: ceno fruncido, ojos rasgados, mandibulas y antenas ---
	add_part(m, B_SPHERE, Vector3(0, 1.12f, -0.68f), Vector3(), Vector3(0.62f, 0.52f, 0.6f), dark, 0.0f, 0.6f);
	add_part(m, B_BOX, Vector3(0, 1.32f, -0.76f), Vector3(15, 0, 0), Vector3(0.5f, 0.08f, 0.3f), plate, 0.0f, 0.8f);
	for (int s = -1; s <= 1; s += 2) {
		add_part(m, B_SPHERE, Vector3(0.15f * s, 1.17f, -0.93f), Vector3(0, 0, 24.0f * s), Vector3(0.24f, 0.1f, 0.13f), eye, elite ? 8.0f : 6.0f, 0.0f);
		add_part(m, B_BOX, Vector3(0.17f * s, 1.26f, -0.95f), Vector3(0, 0, 24.0f * s), Vector3(0.28f, 0.05f, 0.1f), dark, 0.0f, 0.6f); // ceja
		add_part(m, B_CONE, Vector3(0.12f * s, 0.9f, -0.95f), Vector3(180, 0, 12.0f * s), Vector3(0.09f, 0.3f, 0.09f), bone, 0.1f, 0.2f); // mandibula
		add_part(m, B_BOX, Vector3(0.14f * s, 1.55f, -0.85f), Vector3(-40, 0, -12.0f * s), Vector3(0.035f, 0.5f, 0.035f), dark, 0.0f, 0.6f); // antena
		add_part(m, B_SPHERE, Vector3(0.18f * s, 1.75f, -1.02f), Vector3(), Vector3(0.08f, 0.08f, 0.08f), eye, 3.0f, 0.0f);
	}

	// --- Patas ---
	add_legs(m, 3, 0.42f, 0.6f, -0.35f, 0.32f, Vector3(0.06f, 0.75f, 0.06f), 38.0f, dark);

	// --- Guadanas en media luna ---
	// Cada hoja es un arco de segmentos (mas ancho en el centro, afilado en las puntas)
	// inclinado hacia fuera para que se lea bien desde la camara cenital.
	const int SEGMENTS = 7;
	const float R = 0.52f;
	for (int s = -1; s <= 1; s += 2) {
		uint8_t anim = s < 0 ? A_BLADE_L : A_BLADE_R;
		Vector3 pivot(0.36f * s, 1.0f, -0.5f);
		add_anim(m, B_BOX, anim, 0, pivot, Vector3(0, -0.05f, -0.3f), Vector3(25, 0, 0), Vector3(0.11f, 0.11f, 0.6f), plate, 0.0f, 0.8f); // brazo
		add_anim(m, B_SPHERE, anim, 0, pivot, Vector3(0, -0.18f, -0.6f), Vector3(), Vector3(0.2f, 0.2f, 0.2f), dark, 0.0f, 0.7f); // codo
		Transform3D tilt(Basis(Vector3(0, 0, 1), -s * 0.95f), Vector3(0, -0.2f, -0.64f));
		for (int k = 0; k < SEGMENTS; k++) {
			float t = (k + 0.5f) / SEGMENTS;
			float theta_deg = lerpf(95.0f, -105.0f, t);
			float th = theta_deg * PI / 180.0f;
			Vector3 pos(0, -R + R * std::sin(th), -R * std::cos(th));
			float width = 0.04f + 0.17f * std::sin(PI * t);
			float seg_len = R * (200.0f / SEGMENTS) * (PI / 180.0f) * 1.25f;
			Color col = blade_a.lerp(blade_b, t);
			Part p;
			p.batch = B_BOX;
			p.anim = anim;
			p.pivot = pivot;
			p.local = tilt * make_xf(pos, Vector3(theta_deg - 90.0f, 0, 0), Vector3(0.045f, width, seg_len));
			p.color = col;
			p.emis = 3.4f;
			p.metal = 0.0f;
			m.push_back(p);
			if (k % 2 == 1) {
				// Halo aditivo alrededor del filo.
				Part h = p;
				h.batch = B_WING;
				h.local = tilt * make_xf(pos, Vector3(theta_deg - 90.0f, 0, 0), Vector3(0.02f, width * 2.6f, seg_len * 1.6f));
				h.color = Color(col.r, col.g, col.b, 0.16f);
				h.emis = 2.0f;
				m.push_back(h);
			}
		}
	}

	// --- Alas con venas: dos pares ---
	Color wing(0.55f, 0.75f, 0.85f, 0.09f);
	Color vein(0.7f, 0.95f, 1.0f, 0.3f);
	for (int pair = 0; pair < 2; pair++) {
		float len = pair == 0 ? 1.05f : 0.75f;
		float sweep = pair == 0 ? 28.0f : 52.0f;
		float phase = pair * 0.7f;
		for (int s = -1; s <= 1; s += 2) {
			uint8_t anim = s < 0 ? A_WING_L : A_WING_R;
			Vector3 pivot(0.18f * s, 1.36f, 0.0f + pair * 0.18f);
			Vector3 pos(len * 0.5f * s, 0.05f, len * 0.22f);
			add_anim(m, B_WING, anim, phase, pivot, pos, Vector3(0, -sweep * s, 0), Vector3(len, 0.012f, 0.3f), wing, 1.2f);
			add_anim(m, B_WING, anim, phase, pivot, pos, Vector3(0, -(sweep - 7.0f) * s, 0), Vector3(len * 0.95f, 0.014f, 0.022f), vein, 1.5f);
			add_anim(m, B_WING, anim, phase, pivot, pos, Vector3(0, -(sweep + 7.0f) * s, 0), Vector3(len * 0.85f, 0.014f, 0.022f), vein, 1.5f);
		}
	}
}

static void build_wasp(std::vector<Part> &m) {
	Color yellow(0.95f, 0.72f, 0.08f);
	Color black(0.1f, 0.09f, 0.08f);
	add_part(m, B_SPHERE, Vector3(0, 1.0f, 0), Vector3(), Vector3(0.6f, 0.55f, 0.8f), yellow, 0.15f, 0.2f);
	add_part(m, B_SPHERE, Vector3(0, 0.9f, 0.62f), Vector3(-20, 0, 0), Vector3(0.55f, 0.5f, 0.9f), black, 0.0f, 0.3f);
	add_part(m, B_SPHERE, Vector3(0, 0.78f, 1.15f), Vector3(-25, 0, 0), Vector3(0.42f, 0.4f, 0.7f), yellow, 0.15f, 0.2f);
	add_part(m, B_BOX, Vector3(0, 0.62f, 1.6f), Vector3(-25, 0, 0), Vector3(0.06f, 0.06f, 0.5f), Color(1.0f, 0.3f, 0.1f), 4.0f, 0.0f); // aguijon
	add_part(m, B_SPHERE, Vector3(0, 1.08f, -0.5f), Vector3(), Vector3(0.45f, 0.42f, 0.45f), black, 0.0f, 0.3f);
	add_part(m, B_SPHERE, Vector3(-0.15f, 1.14f, -0.68f), Vector3(), Vector3(0.2f, 0.2f, 0.2f), Color(0.4f, 1.0f, 0.2f), 5.0f, 0.0f);
	add_part(m, B_SPHERE, Vector3(0.15f, 1.14f, -0.68f), Vector3(), Vector3(0.2f, 0.2f, 0.2f), Color(0.4f, 1.0f, 0.2f), 5.0f, 0.0f);
	add_legs(m, 2, 0.3f, 0.6f, -0.1f, 0.35f, Vector3(0.04f, 0.5f, 0.04f), 30.0f, black);
	Color wing(1.0f, 0.9f, 0.5f, 0.1f);
	for (int i = 0; i < 2; i++) {
		float z = 0.05f + i * 0.3f;
		add_anim(m, B_WING, A_WING_L, i * 0.8f, Vector3(-0.12f, 1.3f, z), Vector3(-0.5f, 0, 0.12f), Vector3(0, 25, 0), Vector3(1.0f, 0.01f, 0.3f), wing, 1.2f);
		add_anim(m, B_WING, A_WING_R, i * 0.8f, Vector3(0.12f, 1.3f, z), Vector3(0.5f, 0, 0.12f), Vector3(0, -25, 0), Vector3(1.0f, 0.01f, 0.3f), wing, 1.2f);
	}
}

static void build_beetle(std::vector<Part> &m) {
	Color shell(0.08f, 0.28f, 0.2f);
	Color dark(0.1f, 0.12f, 0.12f);
	Color ridge(0.25f, 0.95f, 0.6f);
	add_part(m, B_SPHERE, Vector3(0, 0.85f, 0.15f), Vector3(), Vector3(1.5f, 0.95f, 1.8f), shell, 0.0f, 0.75f);
	add_part(m, B_BOX, Vector3(0, 1.3f, 0.15f), Vector3(), Vector3(0.07f, 0.12f, 1.4f), ridge, 1.6f, 0.0f); // union de los elitros
	add_part(m, B_BOX, Vector3(-0.42f, 1.12f, 0.2f), Vector3(0, 0, 25), Vector3(0.05f, 0.06f, 1.0f), ridge, 0.8f, 0.0f);
	add_part(m, B_BOX, Vector3(0.42f, 1.12f, 0.2f), Vector3(0, 0, -25), Vector3(0.05f, 0.06f, 1.0f), ridge, 0.8f, 0.0f);
	add_part(m, B_SPHERE, Vector3(0, 0.7f, -0.85f), Vector3(), Vector3(0.8f, 0.6f, 0.7f), dark, 0.0f, 0.6f);
	add_part(m, B_BOX, Vector3(0, 0.98f, -1.3f), Vector3(25, 0, 0), Vector3(0.12f, 0.12f, 0.8f), Color(0.55f, 0.57f, 0.6f), 0.0f, 0.8f); // cuerno
	add_part(m, B_SPHERE, Vector3(-0.22f, 0.8f, -1.14f), Vector3(), Vector3(0.16f, 0.16f, 0.16f), Color(1.0f, 0.1f, 0.05f), 5.0f, 0.0f);
	add_part(m, B_SPHERE, Vector3(0.22f, 0.8f, -1.14f), Vector3(), Vector3(0.16f, 0.16f, 0.16f), Color(1.0f, 0.1f, 0.05f), 5.0f, 0.0f);
	add_legs(m, 3, 0.75f, 0.4f, -0.45f, 0.5f, Vector3(0.1f, 0.65f, 0.1f), 40.0f, dark);
	for (int s = -1; s <= 1; s += 2) {
		add_anim(m, B_BOX, s < 0 ? A_BLADE_L : A_BLADE_R, 0, Vector3(0.25f * s, 0.6f, -1.1f), Vector3(0, 0, -0.2f), Vector3(0, -20.0f * s, 0),
				Vector3(0.08f, 0.08f, 0.45f), Color(1.0f, 0.55f, 0.1f), 2.5f, 0.0f); // mandibulas
	}
}

static void build_spitter(std::vector<Part> &m) {
	Color purple(0.4f, 0.18f, 0.55f);
	Color dark(0.16f, 0.1f, 0.22f);
	Color acid(0.4f, 1.0f, 0.25f);
	add_part(m, B_SPHERE, Vector3(0, 0.95f, 0), Vector3(), Vector3(0.8f, 0.7f, 0.9f), purple, 0.0f, 0.3f);
	add_part(m, B_SPHERE, Vector3(0, 1.05f, 0.75f), Vector3(), Vector3(1.0f, 0.95f, 1.1f), acid, 1.4f, 0.0f); // saco de acido
	add_part(m, B_SPHERE, Vector3(0, 1.05f, -0.55f), Vector3(), Vector3(0.5f, 0.48f, 0.5f), dark, 0.0f, 0.3f);
	add_part(m, B_BOX, Vector3(0, 1.0f, -0.9f), Vector3(), Vector3(0.16f, 0.16f, 0.5f), dark, 0.0f, 0.5f); // boquilla
	add_part(m, B_SPHERE, Vector3(0, 1.0f, -1.17f), Vector3(), Vector3(0.16f, 0.16f, 0.16f), acid, 4.0f, 0.0f);
	add_part(m, B_SPHERE, Vector3(-0.17f, 1.2f, -0.7f), Vector3(), Vector3(0.14f, 0.14f, 0.14f), acid, 5.0f, 0.0f);
	add_part(m, B_SPHERE, Vector3(0.17f, 1.2f, -0.7f), Vector3(), Vector3(0.14f, 0.14f, 0.14f), acid, 5.0f, 0.0f);
	add_legs(m, 3, 0.42f, 0.55f, -0.2f, 0.35f, Vector3(0.05f, 0.6f, 0.05f), 35.0f, dark);
	Color wing(0.8f, 0.5f, 1.0f, 0.1f);
	add_anim(m, B_WING, A_WING_L, 0, Vector3(-0.15f, 1.3f, 0.0f), Vector3(-0.3f, 0, 0.05f), Vector3(0, 20, 0), Vector3(0.55f, 0.01f, 0.22f), wing, 1.2f);
	add_anim(m, B_WING, A_WING_R, 0, Vector3(0.15f, 1.3f, 0.0f), Vector3(0.3f, 0, 0.05f), Vector3(0, -20, 0), Vector3(0.55f, 0.01f, 0.22f), wing, 1.2f);
}

static void ensure_models() {
	if (!g_models[E_INSECT].empty()) {
		return;
	}
	build_insect(g_models[E_INSECT], false);
	build_wasp(g_models[E_WASP]);
	build_beetle(g_models[E_BEETLE]);
	build_spitter(g_models[E_SPITTER]);
	build_insect(g_models[MODEL_ELITE], true);
}

// ---------------------------------------------------------------- inicio
void Game::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		set_process(false);
		return;
	}
	GameData::setup_input();
	rng_seed(uint32_t(Time::get_singleton()->get_ticks_usec()) ^ 0xA53C9E11u);
	ensure_models();
	if (OS::get_singleton()->has_environment("CC_AUTOPLAY")) {
		autoplay = true;
		autoplay_god = OS::get_singleton()->get_environment("CC_AUTOPLAY") == "god";
	}
	const bool demo_magnet = autoplay;

	enemies.reserve(MAX_ENEMIES + 8);
	bullets.reserve(256);
	pickups.reserve(512);
	fxs.reserve(128);
	cell_head.assign(GRID_N * GRID_N, -1);

	build_world();
	build_batches();
	init_damage_labels();

	player = memnew(Player);
	player->set_name("Player");
	world->add_child(player);
	player->build();
	player->apply_meta();
	if (demo_magnet) {
		player->magnet_radius = 30.0f; // la demo recoge todo para subir de nivel rapido
	}

	// Luz que acompana al soldado: da volumen a los enemigos cercanos.
	OmniLight3D *lamp = memnew(OmniLight3D);
	lamp->set_position(Vector3(0, 3.5f, 0));
	lamp->set_color(Color(0.75f, 0.9f, 1.0f));
	lamp->set_param(Light3D::PARAM_ENERGY, 2.2f);
	lamp->set_param(Light3D::PARAM_RANGE, 13.0f);
	lamp->set_param(Light3D::PARAM_ATTENUATION, 1.4f);
	lamp->set_shadow(false);
	player->add_child(lamp);

	// Algunos jarrones ya a la vista al empezar.
	for (int i = 0; i < 5; i++) {
		spawn_jar(7.0f, 20.0f);
	}

	hud = memnew(Hud);
	hud->game = this;
	add_child(hud);

	get_tree()->set_pause(false);
	state = PLAYING;
}

void Game::build_world() {
	Ref<Environment> env;
	env.instantiate();
	env->set_background(Environment::BG_COLOR);
	// Ambiente nocturno violeta-azulado con niebla.
	env->set_bg_color(Color(0.03f, 0.025f, 0.06f));
	env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	env->set_ambient_light_color(Color(0.42f, 0.42f, 0.62f));
	env->set_ambient_light_energy(0.6f);
	env->set_tonemapper(Environment::TONE_MAPPER_ACES);
	// El resplandor (bloom) hace brillar ojos, guadanas, balas y explosiones.
	env->set_glow_enabled(true);
	env->set_glow_intensity(0.9f);
	env->set_glow_strength(1.0f);
	env->set_glow_bloom(0.05f);
	env->set_glow_hdr_bleed_threshold(1.0f);
	env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_ADDITIVE);
	env->set_fog_enabled(true);
	env->set_fog_light_color(Color(0.09f, 0.06f, 0.16f));
	env->set_fog_density(0.016f);
	WorldEnvironment *we = memnew(WorldEnvironment);
	we->set_environment(env);
	add_child(we);

	DirectionalLight3D *sun = memnew(DirectionalLight3D);
	sun->set_rotation_degrees(Vector3(-55, -35, 0));
	sun->set_param(Light3D::PARAM_ENERGY, 1.1f);
	sun->set_color(Color(0.8f, 0.82f, 1.0f));
	sun->set_shadow(false);
	add_child(sun);

	world = memnew(Node3D);
	world->set_name("World");
	add_child(world);

	// Suelo infinito: un plano que sigue al jugador con un shader de cuadricula en coordenadas de mundo.
	Ref<PlaneMesh> plane;
	plane.instantiate();
	plane->set_size(Vector2(240, 240));
	ground = mu::part(this, plane, mu::shader_material(SHADER_GROUND));

	// Rocas/escombros decorativos con MultiMesh (muy barato de dibujar).
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(mu::box(Vector3(1, 1, 1)));
	mm->set_instance_count(260);
	for (int i = 0; i < 260; i++) {
		Vector3 p(frange(-120, 120), 0, frange(-120, 120));
		if (p.length() < 6.0f) {
			p += Vector3(10, 0, 10);
		}
		Vector3 s(frange(0.4f, 2.2f), frange(0.2f, 1.4f), frange(0.4f, 2.2f));
		Vector3 rot_deg(frange(-11, 11), frand() * 360.0f, frange(-11, 11));
		mm->set_instance_transform(i, make_xf(p + Vector3(0, s.y * 0.3f, 0), rot_deg, s));
	}
	MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
	mmi->set_multimesh(mm);
	mmi->set_material_override(mu::mat(Color(0.09f, 0.09f, 0.13f), 0.3f, 0.8f));
	add_child(mmi);

	camera = memnew(Camera3D);
	camera->set_fov(50);
	camera->set_far(200);
	add_child(camera);
	camera->set_position(CAM_OFFSET);
	camera->set_rotation_degrees(Vector3(-54.46f, 0, 0)); // mira a (0, 1, 0)
	camera->make_current();
}

void Game::build_batches() {
	Ref<ShaderMaterial> lit = mu::shader_material(SHADER_LIT);
	Ref<ShaderMaterial> add = mu::shader_material(SHADER_ADD);
	Ref<ShaderMaterial> mix = mu::shader_material(SHADER_MIX);

	// Primitivas unitarias (diametro/lado 1): el tamano real va en el transform de cada instancia.
	Ref<SphereMesh> sphere = mu::sphere(0.5f, 12, 6);
	Ref<BoxMesh> box = mu::box(Vector3(1, 1, 1));
	Ref<SphereMesh> gem = mu::sphere(0.5f, 4, 1);
	Ref<SphereMesh> fx_sphere = mu::sphere(0.5f, 16, 8);
	Ref<CylinderMesh> cyl = mu::cylinder(0.5f, 0.5f, 1.0f, 28);
	Ref<CylinderMesh> disc = mu::cylinder(0.5f, 0.5f, 1.0f, 14);

	// Las sombras van primero para que queden por debajo del resto de transparencias.
	mix_disc.init(world, disc, mix, MAX_ENEMIES + 16);
	Ref<CylinderMesh> cone = mu::cylinder(0.0f, 0.5f, 1.0f, 6); // punta hacia +Y
	lit_sphere.init(world, sphere, lit, MAX_ENEMIES * 13 + 400);
	lit_box.init(world, box, lit, MAX_ENEMIES * 36 + 512);
	lit_cone.init(world, cone, lit, MAX_ENEMIES * 8 + 64);
	lit_gem.init(world, gem, lit, 700);
	add_sphere.init(world, fx_sphere, add, 500);
	add_box.init(world, box, add, MAX_ENEMIES * 22 + 800);
	add_cyl.init(world, cyl, add, 128);
}

void Game::init_damage_labels() {
	for (int i = 0; i < DAMAGE_LABELS; i++) {
		Label3D *label = memnew(Label3D);
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
		label->set_font_size(36);
		label->set_outline_size(8);
		label->set_pixel_size(0.008f);
		label->set_modulate(Color(1, 0.9f, 0.85f));
		label->set_outline_modulate(Color(0.5f, 0.05f, 0.05f));
		label->set_visible(false);
		world->add_child(label);
		damage_labels[i] = label;
		damage_label_age[i] = -1.0f;
	}
}

// ---------------------------------------------------------------- bucle
void Game::_process(double p_delta) {
	float delta = std::min(float(p_delta), 0.05f); // evita saltos enormes tras un tiron
	damage_label_timer = std::max(0.0f, damage_label_timer - delta);

	if (state == PLAYING) {
		run_time += delta;
		rebuild_grid();
		update_player(delta);
		update_weapons(delta);
		update_spawning(delta);
		update_enemies(delta);
		update_bullets(delta);
		update_missiles(delta);
		update_shots(delta);
		update_strikes(delta);
		update_jars(delta);
		update_pickups(delta);
		remove_dead();
	} else if (state == DEAD) {
		player->animate(Vector3(), false, Vector3(), delta);
		for (Enemy &e : enemies) {
			e.t += delta;
			e.age += delta;
		}
		dead_timer -= delta;
		if (dead_timer <= 0.0f) {
			set_process(false);
			get_tree()->change_scene_to_file("res://scenes/game_over.tscn");
			return;
		}
	}

	update_fx(delta);
	update_damage_labels(delta);
	update_camera(delta);
	render();
}

void Game::update_camera(float delta) {
	Vector3 pp = player->get_position();
	// CC_ZOOM (solo pruebas) acerca la camara para inspeccionar los modelos.
	static const float zoom = OS::get_singleton()->has_environment("CC_ZOOM") ? float(OS::get_singleton()->get_environment("CC_ZOOM").to_float()) : 1.0f;
	Vector3 target = pp + Vector3(0, 1.0f, 0) * (1.0f - zoom) + CAM_OFFSET * zoom;
	Vector3 cp = camera->get_position();
	cp = cp.lerp(target, std::min(1.0f, 8.0f * delta));
	if (shake > 0.0f) {
		shake = std::max(0.0f, shake - delta * 2.5f);
		cp += Vector3(frange(-1, 1), frange(-1, 1), frange(-1, 1)) * shake * 0.5f;
	}
	camera->set_position(cp);
	ground->set_position(Vector3(std::round(pp.x / 4.0f) * 4.0f, 0, std::round(pp.z / 4.0f) * 4.0f));
}

void Game::update_player(float delta) {
	Input *in = Input::get_singleton();
	Vector2 v = in->get_vector("move_left", "move_right", "move_up", "move_down");
	Vector3 dir(v.x, 0, v.y);
	Vector3 pos = player->get_position();
	if (autoplay) {
		// Camina en circulos; pasado el primer minuto sin "god" se queda quieto para probar el final.
		dir = (autoplay_god || run_time < 60.0f) ? Vector3(std::cos(run_time * 0.5f), 0, std::sin(run_time * 0.5f)) : Vector3();
		if (autoplay_god) {
			player->hp = player->max_hp;
		}
	}

	player->dash_cd = std::max(0.0f, player->dash_cd - delta);
	if (in->is_action_just_pressed("dash") && player->try_dash(dir)) {
		add_fx(FX_RING, pos, pos, Color(0.4f, 0.9f, 1.0f, 0.5f), 0.3f, 0.3f, 2.2f, 2.5f);
		dash_trail_timer = 0.0f;
	}

	if (player->dash_time > 0.0f) {
		player->dash_time -= delta;
		Vector3 before = pos;
		pos += player->dash_dir * (player->speed * 4.2f * delta);
		dash_trail_timer -= delta;
		if (dash_trail_timer <= 0.0f) {
			dash_trail_timer = 0.02f;
			Vector3 back = before - player->dash_dir * 0.9f;
			add_fx(FX_BEAM, back + Vector3(0, 1.1f, 0), pos + Vector3(0, 1.1f, 0), Color(0.3f, 0.85f, 1.0f, 0.7f), 0.25f, 0.5f, 0.0f, 2.5f);
		}
	} else {
		pos += dir * (player->speed * delta);
	}
	player->set_position(pos);

	player->hp = std::min(player->max_hp, player->hp + player->regen * delta);
	if (player->shield_max > 0.0f) {
		player->shield_delay -= delta;
		if (player->shield_delay <= 0.0f && player->shield < player->shield_max) {
			player->shield = std::min(player->shield_max, player->shield + player->shield_max * 0.25f * delta);
		}
	}

	target_index = nearest_enemy(pos, 18.0f);
	bool has_target = target_index >= 0;
	player->animate(dir, has_target, has_target ? enemies[target_index].pos : Vector3(), delta);
}

// ---------------------------------------------------------------- armas / habilidades
int Game::rifle_level() const {
	return 1 + taken[U_DMG] + taken[U_RATE] + taken[U_MULTI] + taken[U_PIERCE] + taken[U_CRIT];
}

void Game::update_weapons(float delta) {
	Vector3 pos = player->get_position();
	float dmg = player->damage;

	// Rifle con auto-apuntado
	fire_timer -= delta;
	if (fire_timer <= 0.0f && target_index >= 0) {
		fire_timer = player->fire_interval;
		Vector3 base = enemies[target_index].pos - pos;
		base.y = 0;
		float len = base.length();
		if (len > 0.001f) {
			base /= len;
			float base_ang = std::atan2(base.x, base.z);
			Vector3 from = player->muzzle_position();
			for (int i = 0; i < player->projectiles; i++) {
				float off = (i - (player->projectiles - 1) * 0.5f) * 0.14f;
				float a = base_ang + off;
				Bullet b;
				b.pos = from;
				b.vel = Vector3(std::sin(a), 0, std::cos(a)) * 26.0f;
				b.crit = frand() < player->crit_chance;
				b.damage = dmg * (b.crit ? 2.0f : 1.0f);
				b.pierce_left = player->pierce;
				bullets.push_back(b);
			}
			add_fx(FX_SPHERE, from, from, Color(1.0f, 0.8f, 0.4f, 0.8f), 0.06f, 0.12f, 0.3f, 4.0f); // fogonazo
		}
	}

	// Bombardeo orbital
	int strike_lv = taken[U_STRIKE];
	if (strike_lv > 0) {
		strike_interval = std::max(1.5f, 6.0f - strike_lv * 0.5f);
		strike_timer -= delta;
		if (strike_timer <= 0.0f) {
			strike_timer = strike_interval;
			int bursts = 1 + strike_lv / 3;
			for (int i = 0; i < bursts; i++) {
				int t = random_enemy_near(pos, 16.0f);
				Strike s;
				s.pos = t >= 0 ? enemies[t].pos : pos + Vector3(frange(-8, 8), 0, frange(-8, 8));
				s.pos.y = 0;
				s.radius = 2.6f + strike_lv * 0.35f;
				s.damage = dmg * (2.5f + strike_lv * 0.6f);
				strikes.push_back(s);
			}
		}
	}

	// Drones de escolta
	int drone_lv = taken[U_DRONES];
	if (drone_lv > 0) {
		drone_angle += delta * (2.4f + drone_lv * 0.2f);
		float radius = 2.6f + drone_lv * 0.15f;
		float drone_dmg = dmg * (0.7f + drone_lv * 0.15f);
		for (int d = 0; d < drone_lv; d++) {
			float a = drone_angle + TAU * d / drone_lv;
			Vector3 dpos = pos + Vector3(std::cos(a) * radius, 1.2f, std::sin(a) * radius);
			query(dpos, 1.6f, [&](int i) {
				Enemy &e = enemies[i];
				float reach = 0.6f + e.radius;
				if (e.alive && e.drone_cd <= 0.0f && dist2_xz(e.pos, dpos) < reach * reach) {
					e.drone_cd = 0.45f;
					damage_enemy(i, drone_dmg);
				}
			});
			for (int j = int(jars.size()) - 1; j >= 0; j--) {
				if (jars[j].flash <= 0.0f && dist2_xz(jars[j].pos, dpos) < 0.9f * 0.9f) {
					hit_jar(j);
				}
			}
		}
	}

	// Pulso de choque: onda que dana y empuja
	int nova_lv = taken[U_NOVA];
	if (nova_lv > 0) {
		nova_interval = std::max(2.0f, 5.0f - nova_lv * 0.4f);
		nova_timer -= delta;
		if (nova_timer <= 0.0f) {
			nova_timer = nova_interval;
			float radius = 4.0f + nova_lv * 0.6f;
			float nova_dmg = dmg * (1.2f + nova_lv * 0.3f);
			query(pos, radius + 1.0f, [&](int i) {
				Enemy &e = enemies[i];
				if (!e.alive) {
					return;
				}
				float d2 = dist2_xz(e.pos, pos);
				if (d2 < radius * radius) {
					float d = std::sqrt(d2);
					if (d > 0.01f) {
						Vector3 away((e.pos.x - pos.x) / d, 0, (e.pos.z - pos.z) / d);
						e.knock += away * (e.elite ? 4.0f : 11.0f);
					}
					damage_enemy(i, nova_dmg);
				}
			});
			damage_jars(pos, radius);
			add_fx(FX_RING, pos, pos, Color(0.35f, 0.75f, 1.0f, 0.55f), 0.4f, 0.6f, radius, 2.5f);
			add_fx(FX_SPHERE, pos + Vector3(0, 1, 0), pos, Color(0.4f, 0.8f, 1.0f, 0.35f), 0.25f, 0.5f, radius * 0.6f, 2.0f);
			shake = std::max(shake, 0.15f);
		}
	}

	// Rayo en cadena: salta entre enemigos cercanos
	int chain_lv = taken[U_CHAIN];
	if (chain_lv > 0) {
		chain_interval = std::max(0.8f, 2.6f - chain_lv * 0.25f);
		chain_timer -= delta;
		if (chain_timer <= 0.0f) {
			int hit_ids[12];
			int hit_n = 0;
			int jumps = std::min(12, 2 + chain_lv);
			float chain_dmg = dmg * (0.9f + chain_lv * 0.2f);
			Vector3 from = pos + Vector3(0, 1.4f, 0);
			Vector3 search = pos;
			float range = 12.0f;
			for (int j = 0; j < jumps; j++) {
				int t = nearest_enemy(search, range, hit_ids, hit_n);
				if (t < 0) {
					break;
				}
				Vector3 to = enemies[t].pos + Vector3(0, 0.9f * enemies[t].scale, 0);
				// Zigzag: dos tramos con el punto medio desplazado.
				Vector3 mid = (from + to) * 0.5f + Vector3(frange(-0.5f, 0.5f), frange(-0.2f, 0.4f), frange(-0.5f, 0.5f));
				Color c(0.55f, 0.8f, 1.0f, 0.95f);
				add_fx(FX_BEAM, from, mid, c, 0.18f, 0.14f, 0.0f, 5.0f);
				add_fx(FX_BEAM, mid, to, c, 0.18f, 0.14f, 0.0f, 5.0f);
				add_fx(FX_SPHERE, to, to, Color(0.7f, 0.9f, 1.0f, 0.8f), 0.15f, 0.15f, 0.5f, 4.0f);
				hit_ids[hit_n++] = enemies[t].id;
				search = enemies[t].pos;
				from = to;
				range = 6.5f;
				damage_enemy(t, chain_dmg);
			}
			// Sin objetivos: reintenta pronto en lugar de gastar el enfriamiento.
			chain_timer = hit_n > 0 ? chain_interval : 0.25f;
		}
	}

	// Misiles buscadores
	int missile_lv = taken[U_MISSILE];
	if (missile_lv > 0) {
		missile_interval = std::max(1.2f, 3.5f - missile_lv * 0.3f);
		missile_timer -= delta;
		if (missile_timer <= 0.0f) {
			if (target_index >= 0) {
				missile_timer = missile_interval;
				int n = 1 + missile_lv / 2;
				for (int i = 0; i < n; i++) {
					float a = frand() * TAU;
					Missile m;
					m.pos = pos + Vector3(0, 1.6f, 0);
					m.vel = Vector3(std::cos(a), 0, std::sin(a)) * 7.0f;
					m.damage = dmg * (1.8f + missile_lv * 0.3f);
					m.radius = 2.2f + missile_lv * 0.15f;
					missiles.push_back(m);
				}
			} else {
				missile_timer = 0.25f;
			}
		}
	}
}

void Game::explode(const Vector3 &pos, float radius, float dmg, const Color &color) {
	query(pos, radius + 1.5f, [&](int i) {
		Enemy &e = enemies[i];
		float reach = radius + e.radius * 0.5f;
		if (e.alive && dist2_xz(e.pos, pos) < reach * reach) {
			damage_enemy(i, dmg);
		}
	});
	damage_jars(pos, radius);
	Vector3 c(pos.x, 0.5f, pos.z);
	add_fx(FX_SPHERE, c, c, color, 0.35f, 0.3f, radius, 5.0f);
	add_fx(FX_RING, c, c, Color(color.r, color.g, color.b, 0.4f), 0.4f, 0.4f, radius * 1.25f, 2.0f);
	shake = std::max(shake, 0.35f);
}

void Game::add_fx(uint8_t kind, const Vector3 &a, const Vector3 &b, const Color &color, float life, float r0, float r1, float energy) {
	if (fxs.size() >= 300) {
		return;
	}
	Fx f;
	f.kind = kind;
	f.a = a;
	f.b = b;
	f.color = color;
	f.life = life;
	f.r0 = r0;
	f.r1 = r1;
	f.energy = energy;
	fxs.push_back(f);
}

// ---------------------------------------------------------------- oleadas
void Game::update_spawning(float delta) {
	float minutes = run_time / 60.0f;
	spawn_timer -= delta;
	if (spawn_timer <= 0.0f) {
		spawn_timer = std::max(0.22f, 1.0f - minutes * 0.15f);
		int count = 1 + int(minutes * 1.5f);
		for (int i = 0; i < count; i++) {
			if (int(enemies.size()) < MAX_ENEMIES) {
				spawn_enemy(false, minutes);
			}
		}
	}
	if (run_time >= next_elite) {
		next_elite += 60.0f;
		spawn_enemy(true, minutes);
		hud->toast(U8("¡Se acerca un insecto élite!"));
	}
}

void Game::spawn_enemy(bool elite, float minutes) {
	Enemy e;
	e.id = next_enemy_id++;
	e.elite = elite;
	float hp_mult = 1.0f + minutes * 0.4f;
	float speed = std::min(5.6f, 2.8f + minutes * 0.25f) * frange(0.9f, 1.1f);

	uint8_t type = E_INSECT;
	if (!elite) {
		float r = frand();
		if (minutes >= 2.0f && r < 0.12f) {
			type = E_SPITTER;
		} else if (minutes >= 1.25f && r < 0.24f) {
			type = E_BEETLE;
		} else if (minutes >= 0.4f && r < 0.48f) {
			type = E_WASP;
		}
	}
	e.type = type;
	switch (type) {
		case E_WASP:
			e.max_hp = 12.0f * hp_mult;
			e.speed = std::min(7.6f, speed * 1.55f);
			e.radius = 0.38f;
			e.contact_dps = 7.0f;
			e.scale = 0.75f;
			e.xp = 1.0f;
			break;
		case E_BEETLE:
			e.max_hp = 110.0f * hp_mult;
			e.speed = speed * 0.55f;
			e.radius = 0.9f;
			e.contact_dps = 18.0f;
			e.scale = 1.45f;
			e.xp = 4.0f;
			break;
		case E_SPITTER:
			e.max_hp = 30.0f * hp_mult;
			e.speed = speed * 0.8f;
			e.radius = 0.5f;
			e.contact_dps = 6.0f;
			e.scale = 1.0f;
			e.xp = 2.0f;
			e.shoot_cd = frange(1.0f, 2.5f);
			break;
		default:
			e.max_hp = 25.0f * hp_mult;
			e.speed = speed;
			e.radius = 0.5f;
			e.contact_dps = 10.0f;
			e.scale = 1.0f;
			e.xp = 1.0f;
			break;
	}
	if (elite) {
		e.max_hp *= 14.0f;
		e.speed *= 0.8f;
		e.radius = 0.95f;
		e.contact_dps = 30.0f;
		e.scale = 1.9f;
		e.xp = 10.0f;
	}
	e.hp = e.max_hp;
	e.t = frand() * 10.0f;
	float a = frand() * TAU;
	float r = frange(22.0f, 27.0f);
	e.pos = player->get_position() + Vector3(std::cos(a) * r, 0, std::sin(a) * r);
	e.pos.y = 0;
	e.yaw = std::atan2(std::cos(a), std::sin(a));
	enemies.push_back(e);
}

void Game::update_enemies(float delta) {
	Vector3 p = player->get_position();
	float minutes = run_time / 60.0f;
	int n = int(enemies.size());
	int grid_n = int(cell_next.size());
	for (int i = 0; i < n; i++) {
		Enemy &e = enemies[i];
		if (!e.alive) {
			continue;
		}
		e.t += delta;
		e.age += delta;
		e.drone_cd = std::max(0.0f, e.drone_cd - delta);
		e.flash = std::max(0.0f, e.flash - delta);

		float dx = p.x - e.pos.x, dz = p.z - e.pos.z;
		float dist = std::sqrt(dx * dx + dz * dz);

		// Si el jugador lo dejo muy atras, reaparece en el borde (como en los survivors).
		if (dist > 42.0f) {
			float a = frand() * TAU;
			e.pos = p + Vector3(std::cos(a) * 24.0f, 0, std::sin(a) * 24.0f);
			e.pos.y = 0;
			continue;
		}

		Vector3 dir = dist > 0.001f ? Vector3(dx / dist, 0, dz / dist) : Vector3();
		Vector3 move = dir;

		if (e.type == E_SPITTER) {
			// Mantiene la distancia y escupe acido.
			if (dist < 7.0f) {
				move = dir * -0.7f;
			} else if (dist < 10.0f) {
				move = Vector3(-dir.z, 0, dir.x) * 0.5f;
			}
			e.shoot_cd -= delta;
			if (e.shoot_cd <= 0.0f && dist < 14.0f) {
				e.shoot_cd = 2.6f;
				EnemyShot s;
				s.pos = e.pos + Vector3(0, 0.9f, 0) + dir * 0.8f;
				s.vel = dir * 9.0f;
				s.damage = 8.0f + minutes * 1.5f;
				shots.push_back(s);
			}
		}

		// Separacion ligera para que no se apilen todos en el mismo punto.
		Vector3 push;
		if (i < grid_n) {
			query(e.pos, e.radius + 1.0f, [&](int j) {
				if (j == i) {
					return;
				}
				const Enemy &o = enemies[j];
				if (!o.alive) {
					return;
				}
				float ox = e.pos.x - o.pos.x, oz = e.pos.z - o.pos.z;
				float l2 = ox * ox + oz * oz;
				float min_d = e.radius + o.radius;
				if (l2 < min_d * min_d && l2 > 0.000001f) {
					float l = std::sqrt(l2);
					float k = (min_d - l) / l;
					push.x += ox * k;
					push.z += oz * k;
				}
			});
		}

		e.pos += (move * e.speed + push * 4.0f + e.knock) * delta;
		e.knock *= std::max(0.0f, 1.0f - 6.0f * delta);

		if (dist > 0.01f) {
			e.yaw = lerp_angle(e.yaw, std::atan2(-dir.x, -dir.z), std::min(1.0f, 10.0f * delta));
		}

		if (dist < e.radius + 0.45f && player->alive) {
			if (player->take_damage(e.contact_dps * delta)) {
				on_player_died();
			}
		}
	}
}

void Game::damage_enemy(int index, float amount, bool crit) {
	Enemy &e = enemies[index];
	if (!e.alive) {
		return;
	}
	e.hp -= amount;
	e.flash = 0.08f;
	damage_number(e.pos + Vector3(0, 1.4f * e.scale, 0), amount, crit ? 1 : 0);
	if (e.hp > 0.0f) {
		return;
	}

	e.alive = false;
	kills++;
	Vector3 pos = e.pos;
	if (e.elite) {
		elite_kills++;
		spawn_pickup(pos, PK_XP, 10.0f);
		for (int i = 0; i < 8; i++) {
			spawn_pickup(pos + Vector3(frange(-1.5f, 1.5f), 0, frange(-1.5f, 1.5f)), PK_CORE, 1.0f);
		}
		shake = 0.8f;
		add_fx(FX_RING, pos, pos, Color(1.0f, 0.3f, 0.3f, 0.6f), 0.5f, 0.5f, 6.0f, 3.0f);
	} else {
		spawn_pickup(pos, PK_XP, e.xp);
		if (frand() < (e.type == E_BEETLE ? 0.35f : 0.12f)) {
			spawn_pickup(pos + Vector3(0.4f, 0, 0.3f), PK_CORE, 1.0f);
		}
	}
	static const Color burst[E_TYPE_COUNT] = {
		Color(1.0f, 0.35f, 0.8f, 0.7f),
		Color(1.0f, 0.8f, 0.2f, 0.7f),
		Color(0.3f, 1.0f, 0.6f, 0.7f),
		Color(0.5f, 1.0f, 0.3f, 0.7f),
	};
	Vector3 c = pos + Vector3(0, 0.9f * e.scale, 0);
	add_fx(FX_SPHERE, c, c, burst[e.type], 0.22f, 0.25f * e.scale, 1.2f * e.scale, 3.0f);
}

// ---------------------------------------------------------------- proyectiles
void Game::update_bullets(float delta) {
	for (int b = int(bullets.size()) - 1; b >= 0; b--) {
		Bullet &bl = bullets[b];
		bl.pos += bl.vel * delta;
		bl.life -= delta;
		bool dead = bl.life <= 0.0f;
		if (!dead) {
			query(bl.pos, 2.0f, [&](int i) {
				if (dead) {
					return;
				}
				Enemy &e = enemies[i];
				if (!e.alive) {
					return;
				}
				float reach = e.radius + 0.2f;
				if (dist2_xz(e.pos, bl.pos) >= reach * reach) {
					return;
				}
				for (int h = 0; h < bl.hit_count; h++) {
					if (bl.hit_ids[h] == e.id) {
						return;
					}
				}
				if (bl.hit_count < 8) {
					bl.hit_ids[bl.hit_count++] = e.id;
				}
				damage_enemy(i, bl.damage, bl.crit);
				bl.pierce_left--;
				if (bl.pierce_left < 0) {
					dead = true;
				}
			});
			// Jarrones: se registran con id negativo para que una bala perforante no los golpee dos veces.
			for (int j = int(jars.size()) - 1; j >= 0 && !dead; j--) {
				if (dist2_xz(jars[j].pos, bl.pos) >= 0.75f * 0.75f) {
					continue;
				}
				int jid = -jars[j].id;
				bool seen = false;
				for (int h = 0; h < bl.hit_count; h++) {
					seen = seen || bl.hit_ids[h] == jid;
				}
				if (seen) {
					continue;
				}
				if (bl.hit_count < 8) {
					bl.hit_ids[bl.hit_count++] = jid;
				}
				hit_jar(j);
				bl.pierce_left--;
				if (bl.pierce_left < 0) {
					dead = true;
				}
			}
		}
		if (dead) {
			bullets[b] = bullets.back();
			bullets.pop_back();
		}
	}
}

void Game::update_missiles(float delta) {
	for (int m = int(missiles.size()) - 1; m >= 0; m--) {
		Missile &ms = missiles[m];
		ms.life -= delta;
		bool boom = ms.life <= 0.0f;
		int t = nearest_enemy(ms.pos, 30.0f);
		if (t >= 0) {
			const Enemy &e = enemies[t];
			Vector3 to(e.pos.x - ms.pos.x, 0, e.pos.z - ms.pos.z);
			float d = to.length();
			if (d < e.radius + 0.45f) {
				boom = true;
			} else {
				Vector3 want = to / d * 17.0f;
				ms.vel = ms.vel.lerp(want, std::min(1.0f, 5.0f * delta));
			}
		}
		ms.pos += ms.vel * delta;
		ms.pos.y = lerpf(ms.pos.y, 1.1f, std::min(1.0f, 4.0f * delta));
		if (boom) {
			explode(ms.pos, ms.radius, ms.damage, Color(1.0f, 0.7f, 0.25f, 0.85f));
			missiles[m] = missiles.back();
			missiles.pop_back();
		}
	}
}

void Game::update_shots(float delta) {
	Vector3 p = player->get_position();
	for (int s = int(shots.size()) - 1; s >= 0; s--) {
		EnemyShot &sh = shots[s];
		sh.pos += sh.vel * delta;
		sh.life -= delta;
		bool dead = sh.life <= 0.0f;
		if (!dead && dist2_xz(sh.pos, p) < 0.65f * 0.65f) {
			dead = true;
			if (player->alive && player->dash_time <= 0.0f) {
				add_fx(FX_SPHERE, sh.pos, sh.pos, Color(0.5f, 1.0f, 0.3f, 0.7f), 0.2f, 0.2f, 0.9f, 3.0f);
				if (player->take_damage(sh.damage)) {
					on_player_died();
				}
			}
		}
		if (dead) {
			shots[s] = shots.back();
			shots.pop_back();
		}
	}
}

void Game::update_strikes(float delta) {
	for (int s = int(strikes.size()) - 1; s >= 0; s--) {
		Strike &st = strikes[s];
		st.t += delta;
		if (st.t >= 0.7f) {
			Strike done = st;
			strikes[s] = strikes.back();
			strikes.pop_back();
			explode(done.pos, done.radius, done.damage, Color(1.0f, 0.6f, 0.15f, 0.85f));
		}
	}
}

// ---------------------------------------------------------------- jarrones rompibles
void Game::spawn_jar(float min_dist, float max_dist) {
	float a = frand() * TAU;
	float r = frange(min_dist, max_dist);
	Jar j;
	j.pos = player->get_position() + Vector3(std::cos(a) * r, 0, std::sin(a) * r);
	j.pos.y = 0;
	j.id = next_jar_id++;
	jars.push_back(j);
}

void Game::update_jars(float delta) {
	Vector3 p = player->get_position();
	for (int i = int(jars.size()) - 1; i >= 0; i--) {
		Jar &j = jars[i];
		j.flash = std::max(0.0f, j.flash - delta);
		j.age += delta;
		// Los que quedaron muy lejos se retiran para que siempre haya alguno cerca.
		if (dist2_xz(j.pos, p) > 50.0f * 50.0f) {
			jars[i] = jars.back();
			jars.pop_back();
		}
	}
	jar_timer -= delta;
	if (jar_timer <= 0.0f) {
		jar_timer = 2.5f;
		if (jars.size() < 9) {
			spawn_jar(14.0f, 26.0f);
		}
	}
}

void Game::hit_jar(int index) {
	Jar &j = jars[index];
	j.hp -= 1.0f;
	j.flash = 0.14f;
	Vector3 pos = j.pos;
	Vector3 c = pos + Vector3(0, 0.6f, 0);
	if (j.hp > 0.0f) {
		add_fx(FX_SPHERE, c, c, Color(0.9f, 0.6f, 0.3f, 0.6f), 0.15f, 0.2f, 0.7f, 2.0f);
		return;
	}
	jars[index] = jars.back();
	jars.pop_back();

	// Se rompe: siempre un botiquin, a veces dos, y algo de botin extra.
	spawn_pickup(pos, PK_HEAL, 15.0f);
	if (frand() < 0.3f) {
		spawn_pickup(pos + Vector3(frange(-0.8f, 0.8f), 0, frange(-0.8f, 0.8f)), PK_HEAL, 15.0f);
	}
	if (frand() < 0.4f) {
		spawn_pickup(pos + Vector3(frange(-0.8f, 0.8f), 0, frange(-0.8f, 0.8f)), PK_CORE, 1.0f);
	}
	spawn_pickup(pos + Vector3(frange(-0.8f, 0.8f), 0, frange(-0.8f, 0.8f)), PK_XP, 2.0f);
	add_fx(FX_SPHERE, c, c, Color(0.85f, 0.5f, 0.25f, 0.8f), 0.3f, 0.3f, 1.5f, 2.5f);
	add_fx(FX_RING, pos, pos, Color(0.2f, 1.0f, 0.45f, 0.5f), 0.4f, 0.3f, 2.2f, 2.5f);
	// Fragmentos que salen disparados.
	for (int i = 0; i < 6; i++) {
		float a = frand() * TAU;
		Vector3 out(std::cos(a) * frange(0.8f, 1.6f), frange(0.2f, 1.0f), std::sin(a) * frange(0.8f, 1.6f));
		add_fx(FX_BEAM, c, c + out, Color(0.8f, 0.45f, 0.22f, 0.9f), 0.3f, 0.12f, 0.0f, 1.5f);
	}
}

void Game::damage_jars(const Vector3 &pos, float radius) {
	float reach = radius + 0.5f;
	for (int i = int(jars.size()) - 1; i >= 0; i--) {
		if (dist2_xz(jars[i].pos, pos) < reach * reach) {
			hit_jar(i);
		}
	}
}

// ---------------------------------------------------------------- recolectables / niveles
void Game::spawn_pickup(const Vector3 &pos, int kind, float value) {
	Pickup p;
	p.pos = Vector3(pos.x, 0, pos.z);
	p.core = kind == PK_CORE;
	p.heal = kind == PK_HEAL;
	p.value = value;
	p.t = frand() * 6.0f;
	pickups.push_back(p);
	// Con demasiados cristales en el suelo, los mas viejos vuelan solos al jugador.
	if (pickups.size() > 450) {
		pickups[irand(64)].attracted = true;
	}
}

void Game::update_pickups(float delta) {
	if (!player->alive) {
		return;
	}
	Vector3 p = player->get_position();
	float magnet2 = player->magnet_radius * player->magnet_radius;
	for (int i = int(pickups.size()) - 1; i >= 0; i--) {
		Pickup &pk = pickups[i];
		pk.t += delta;
		float dx = p.x - pk.pos.x, dz = p.z - pk.pos.z;
		float d2 = dx * dx + dz * dz;
		if (!pk.attracted && d2 < magnet2) {
			pk.attracted = true;
		}
		if (pk.attracted && d2 > 0.0001f) {
			pk.vel = std::min(pk.vel + 40.0f * delta, 25.0f);
			float d = std::sqrt(d2);
			float step = std::min(d, pk.vel * delta);
			pk.pos.x += dx / d * step;
			pk.pos.z += dz / d * step;
		}
		if (d2 < 0.6f * 0.6f) {
			Pickup got = pk;
			pickups[i] = pickups.back();
			pickups.pop_back();
			collect(got);
		}
	}
}

void Game::collect(const Pickup &p) {
	if (p.heal) {
		// Cura una cantidad fija mas un 5% de la vida maxima.
		float amount = p.value + player->max_hp * 0.05f;
		player->heal(amount);
		Vector3 pp = player->get_position();
		add_fx(FX_RING, pp, pp, Color(0.2f, 1.0f, 0.45f, 0.5f), 0.4f, 0.3f, 1.8f, 2.5f);
		damage_number(pp + Vector3(0, 2.6f, 0), amount, 2);
		return;
	}
	if (p.core) {
		run_cores += p.value * player->core_mult;
		return;
	}
	xp += p.value;
	while (xp >= xp_next) {
		xp -= xp_next;
		level++;
		xp_next = 4.0f + (level - 1) * 5.0f;
		pending_levels++;
	}
	if (pending_levels > 0 && state == PLAYING) {
		open_levelup();
	}
}

void Game::open_levelup() {
	levelup_options.clear();
	for (int i = 0; i < U_COUNT; i++) {
		if (i != U_HEAL && taken[i] < UPGRADES[i].max) {
			levelup_options.push_back(i);
		}
	}
	// Barajar (Fisher-Yates) y quedarse con 3.
	for (int i = int(levelup_options.size()) - 1; i > 0; i--) {
		std::swap(levelup_options[i], levelup_options[irand(i + 1)]);
	}
	if (levelup_options.size() > 3) {
		levelup_options.resize(3);
	}
	if (levelup_options.empty()) {
		levelup_options.push_back(U_HEAL);
	}
	state = LEVELUP;
	if (autoplay) {
		// Demo: prioriza las habilidades para que se vean todas.
		int pick = 0;
		for (int i = 0; i < int(levelup_options.size()); i++) {
			if (UPGRADES[levelup_options[i]].ability) {
				pick = i;
				break;
			}
		}
		choose_upgrade(pick);
		return;
	}
	get_tree()->set_pause(true);
	hud->show_levelup();
}

void Game::choose_upgrade(int option_index) {
	if (state != LEVELUP || option_index < 0 || option_index >= int(levelup_options.size())) {
		return;
	}
	int id = levelup_options[option_index];
	taken[id]++;
	apply_upgrade(id);
	pending_levels--;
	hud->hide_levelup();
	get_tree()->set_pause(false);
	state = PLAYING;
	if (pending_levels > 0) {
		open_levelup();
	}
}

void Game::apply_upgrade(int id) {
	switch (id) {
		case U_DMG:
			player->damage *= 1.2f;
			break;
		case U_RATE:
			player->fire_interval /= 1.15f;
			break;
		case U_MULTI:
			player->projectiles += 1;
			break;
		case U_PIERCE:
			player->pierce += 1;
			break;
		case U_CRIT:
			player->crit_chance += 0.10f;
			break;
		case U_SPEED:
			player->speed *= 1.1f;
			break;
		case U_HP:
			player->max_hp += 25.0f;
			player->heal(25.0f);
			break;
		case U_MAGNET:
			player->magnet_radius *= 1.3f;
			break;
		case U_SHIELD:
			player->shield_max = 25.0f * taken[U_SHIELD];
			player->shield = player->shield_max;
			break;
		case U_HEAL:
			player->heal(player->max_hp * 0.5f);
			break;
		default:
			break; // las habilidades leen su nivel de taken[]
	}
}

// ---------------------------------------------------------------- fin de partida / pausa
int Game::score() const {
	return kills * 10 + elite_kills * 150 + int(run_time) * 2 + (level - 1) * 25;
}

void Game::on_player_died() {
	if (state == DEAD) {
		return;
	}
	state = DEAD;
	int earned = int(std::floor(run_cores)) + kills / 25;
	// Aqui se empareja el nombre registrado con el puntaje final.
	GameData::get().add_run_result(score(), earned, run_time, kills, level);
	shake = 1.0f;
	dead_timer = 1.6f;
	Vector3 c = player->get_position() + Vector3(0, 1, 0);
	add_fx(FX_SPHERE, c, c, Color(1.0f, 0.3f, 0.2f, 0.8f), 0.6f, 0.4f, 3.5f, 4.0f);
	add_fx(FX_RING, c, c, Color(1.0f, 0.3f, 0.2f, 0.6f), 0.7f, 0.5f, 7.0f, 3.0f);
}

void Game::toggle_pause() {
	if (state == PLAYING) {
		state = PAUSED;
		get_tree()->set_pause(true);
		hud->show_pause(true);
	} else if (state == PAUSED) {
		state = PLAYING;
		get_tree()->set_pause(false);
		hud->show_pause(false);
	}
}

void Game::quit_to_menu() {
	get_tree()->set_pause(false);
	set_process(false);
	get_tree()->change_scene_to_file("res://scenes/main_menu.tscn");
}

// ---------------------------------------------------------------- efectos
void Game::update_fx(float delta) {
	for (int i = int(fxs.size()) - 1; i >= 0; i--) {
		fxs[i].age += delta;
		if (fxs[i].age >= fxs[i].life) {
			fxs[i] = fxs.back();
			fxs.pop_back();
		}
	}
}

void Game::update_damage_labels(float delta) {
	for (int i = 0; i < DAMAGE_LABELS; i++) {
		if (damage_label_age[i] < 0.0f) {
			continue;
		}
		damage_label_age[i] += delta;
		float age = damage_label_age[i];
		Label3D *label = damage_labels[i];
		Vector3 p = label->get_position();
		p.y += delta * 2.0f;
		label->set_position(p);
		Color c = label->get_modulate();
		c.a = 1.0f - clampf(age / 0.45f, 0.0f, 1.0f);
		label->set_modulate(c);
		if (age >= 0.45f) {
			label->set_visible(false);
			damage_label_age[i] = -1.0f;
		}
	}
}

void Game::damage_number(const Vector3 &pos, float amount, int style) {
	bool crit = style == 1;
	if (damage_label_timer > 0.0f && style == 0) {
		return;
	}
	for (int i = 0; i < DAMAGE_LABELS; i++) {
		if (damage_label_age[i] >= 0.0f) {
			continue;
		}
		Label3D *label = damage_labels[i];
		int value = int(std::round(amount));
		if (style == 2) {
			label->set_text(String("+") + String::num_int64(value));
			label->set_modulate(Color(0.3f, 1.0f, 0.45f));
			label->set_outline_modulate(Color(0.02f, 0.25f, 0.08f));
			label->set_font_size(52);
		} else {
			label->set_text(crit ? String::num_int64(value) + "!" : String::num_int64(value));
			label->set_modulate(crit ? Color(1.0f, 0.9f, 0.2f) : Color(1, 0.9f, 0.85f));
			label->set_outline_modulate(Color(0.5f, 0.05f, 0.05f));
			label->set_font_size(crit ? 52 : 36);
		}
		label->set_position(pos + Vector3(frange(-0.3f, 0.3f), 0, frange(-0.3f, 0.3f)));
		label->set_visible(true);
		damage_label_age[i] = 0.0f;
		damage_label_timer = 0.08f;
		return;
	}
}

void Game::remove_dead() {
	// Compacta el vector: los indices solo cambian aqui, al final del frame.
	size_t w = 0;
	for (size_t r = 0; r < enemies.size(); r++) {
		if (enemies[r].alive) {
			if (w != r) {
				enemies[w] = enemies[r];
			}
			w++;
		}
	}
	enemies.resize(w);
	target_index = -1;
}

// ---------------------------------------------------------------- consultas espaciales
void Game::rebuild_grid() {
	Vector3 p = player->get_position();
	float half = GRID_N * CELL * 0.5f;
	grid_x0 = std::floor(p.x / CELL) * CELL - half;
	grid_z0 = std::floor(p.z / CELL) * CELL - half;
	std::fill(cell_head.begin(), cell_head.end(), -1);
	int n = int(enemies.size());
	cell_next.resize(n);
	for (int i = 0; i < n; i++) {
		const Enemy &e = enemies[i];
		if (!e.alive) {
			continue;
		}
		int cx = std::clamp(int(std::floor((e.pos.x - grid_x0) / CELL)), 0, GRID_N - 1);
		int cz = std::clamp(int(std::floor((e.pos.z - grid_z0) / CELL)), 0, GRID_N - 1);
		int c = cz * GRID_N + cx;
		cell_next[i] = cell_head[c];
		cell_head[c] = i;
	}
}

int Game::nearest_enemy(const Vector3 &pos, float max_dist, const int *exclude_ids, int exclude_count) const {
	int best = -1;
	float best_d = max_dist * max_dist;
	int n = int(enemies.size());
	for (int i = 0; i < n; i++) {
		const Enemy &e = enemies[i];
		if (!e.alive) {
			continue;
		}
		float d = dist2_xz(e.pos, pos);
		if (d >= best_d) {
			continue;
		}
		bool skip = false;
		for (int k = 0; k < exclude_count; k++) {
			if (exclude_ids[k] == e.id) {
				skip = true;
				break;
			}
		}
		if (!skip) {
			best_d = d;
			best = i;
		}
	}
	return best;
}

int Game::random_enemy_near(const Vector3 &pos, float max_dist) const {
	// Muestreo de reserva: elige uno al azar sin crear una lista temporal.
	int chosen = -1;
	int seen = 0;
	float md2 = max_dist * max_dist;
	int n = int(enemies.size());
	for (int i = 0; i < n; i++) {
		const Enemy &e = enemies[i];
		if (e.alive && dist2_xz(e.pos, pos) < md2) {
			seen++;
			if (irand(seen) == 0) {
				chosen = i;
			}
		}
	}
	return chosen;
}

// ---------------------------------------------------------------- render por lotes
void Game::render_enemy(const Enemy &e) {
	const std::vector<Part> &model = g_models[e.elite ? MODEL_ELITE : e.type];
	float grow = std::min(1.0f, e.age / 0.3f); // aparece creciendo
	float s = e.scale * 0.7f * grow;

	Basis rb(Vector3(0, 1, 0), e.yaw);
	rb.rows[0] *= s;
	rb.rows[1] *= s;
	rb.rows[2] *= s;
	float bob = (0.25f + std::sin(e.t * 5.0f) * 0.1f) * e.scale;
	Transform3D M(rb, Vector3(e.pos.x, bob, e.pos.z));

	float flap_speed = e.type == E_WASP ? 55.0f : 40.0f;
	float slash = std::sin(e.t * 7.0f) * 0.5f;
	float flash = e.flash > 0.0f ? 1.0f : 0.0f;

	for (const Part &p : model) {
		Transform3D w;
		switch (p.anim) {
			case A_NONE:
				w = M * p.local;
				break;
			case A_WING_L:
				w = M * Transform3D(Basis(Vector3(0, 0, 1), std::sin(e.t * flap_speed + p.phase) * 0.6f), p.pivot) * p.local;
				break;
			case A_WING_R:
				w = M * Transform3D(Basis(Vector3(0, 0, 1), -std::sin(e.t * flap_speed + p.phase) * 0.6f), p.pivot) * p.local;
				break;
			case A_BLADE_L:
				w = M * Transform3D(Basis(Vector3(1, 0, 0), slash), p.pivot) * p.local;
				break;
			case A_BLADE_R:
				w = M * Transform3D(Basis(Vector3(1, 0, 0), -slash), p.pivot) * p.local;
				break;
			default: // A_LEG
				w = M * Transform3D(Basis(Vector3(1, 0, 0), std::sin(e.t * 14.0f + p.phase) * 0.4f), p.pivot) * p.local;
				break;
		}
		switch (p.batch) {
			case B_SPHERE:
				lit_sphere.add(w, p.color, flash, p.emis, p.metal, 1.0f);
				break;
			case B_BOX:
				lit_box.add(w, p.color, flash, p.emis, p.metal, 1.0f);
				break;
			case B_CONE:
				lit_cone.add(w, p.color, flash, p.emis, p.metal, 1.0f);
				break;
			default:
				add_box.add(w, p.color, 0.0f, p.emis);
				break;
		}
	}

	float sr = e.radius * 2.6f * grow;
	mix_disc.add(Transform3D(Basis(Vector3(sr, 0, 0), Vector3(0, 0.01f, 0), Vector3(0, 0, sr)), Vector3(e.pos.x, 0.03f, e.pos.z)), Color(0, 0, 0, 0.42f));

	if (e.elite) {
		// Barra de vida flotante, inclinada hacia la camara.
		float frac = clampf(e.hp / e.max_hp, 0.0f, 1.0f);
		Vector3 bp(e.pos.x, 2.9f * e.scale * 0.7f + 1.1f, e.pos.z);
		add_box.add(make_xf(bp, Vector3(-54, 0, 0), Vector3(2.0f, 0.2f, 0.02f)), Color(0.4f, 0.05f, 0.05f, 0.5f), 0.0f, 1.0f);
		add_box.add(make_xf(bp - Vector3((1.0f - frac) * 0.95f, 0, 0), Vector3(-54, 0, 0), Vector3(1.9f * frac, 0.12f, 0.03f)), Color(1.0f, 0.15f, 0.15f, 0.9f), 0.0f, 3.0f);
	}
}

void Game::render() {
	lit_sphere.begin();
	lit_box.begin();
	lit_gem.begin();
	lit_cone.begin();
	add_sphere.begin();
	add_box.begin();
	add_cyl.begin();
	mix_disc.begin();

	for (const Enemy &e : enemies) {
		if (e.alive) {
			render_enemy(e);
		}
	}

	// Balas
	for (const Bullet &b : bullets) {
		float yaw = std::atan2(-b.vel.x, -b.vel.z);
		float k = b.crit ? 1.7f : 1.0f;
		Transform3D t = make_xf(b.pos, Vector3(0, yaw * 180.0f / PI, 0), Vector3(0.1f * k, 0.1f * k, 0.75f * k));
		add_box.add(t, b.crit ? Color(1.0f, 0.95f, 0.6f, 1.0f) : Color(1.0f, 0.7f, 0.22f, 1.0f), 0.0f, b.crit ? 6.0f : 4.0f);
	}

	// Misiles
	for (const Missile &m : missiles) {
		float yaw = std::atan2(-m.vel.x, -m.vel.z) * 180.0f / PI;
		lit_box.add(make_xf(m.pos, Vector3(0, yaw, 0), Vector3(0.16f, 0.16f, 0.55f)), Color(0.85f, 0.87f, 0.9f), 0.0f, 0.3f, 0.6f);
		Vector3 back = m.pos - m.vel.normalized() * 0.4f;
		float fl = 0.3f + frand() * 0.15f;
		add_sphere.add(make_xf(back, Vector3(), Vector3(fl, fl, fl)), Color(1.0f, 0.6f, 0.2f, 0.9f), 0.0f, 5.0f);
	}

	// Acido enemigo
	for (const EnemyShot &s : shots) {
		float r = 0.42f + std::sin(s.life * 20.0f) * 0.05f;
		add_sphere.add(make_xf(s.pos, Vector3(), Vector3(r, r, r)), Color(0.45f, 1.0f, 0.25f, 0.95f), 0.0f, 4.0f);
		mix_disc.add(Transform3D(Basis(Vector3(0.5f, 0, 0), Vector3(0, 0.01f, 0), Vector3(0, 0, 0.5f)), Vector3(s.pos.x, 0.03f, s.pos.z)), Color(0, 0, 0, 0.3f));
	}

	// Cristales de experiencia y nucleos
	for (const Pickup &p : pickups) {
		float y = 0.45f + std::sin(p.t * 4.0f) * 0.08f;
		float yaw = p.t * 172.0f;
		if (p.heal) {
			// Botiquin: cruz verde que gira, con halo.
			Vector3 c(p.pos.x, y + 0.2f, p.pos.z);
			Color green(0.2f, 1.0f, 0.4f);
			lit_box.add(make_xf(c, Vector3(0, yaw, 0), Vector3(0.6f, 0.2f, 0.2f)), green, 0.0f, 3.5f, 0.0f);
			lit_box.add(make_xf(c, Vector3(0, yaw, 0), Vector3(0.2f, 0.6f, 0.2f)), green, 0.0f, 3.5f, 0.0f);
			float halo = 1.0f + std::sin(p.t * 5.0f) * 0.12f;
			add_sphere.add(make_xf(c, Vector3(), Vector3(halo, halo, halo)), Color(0.2f, 1.0f, 0.4f, 0.12f), 0.0f, 2.0f);
		} else if (p.core) {
			lit_sphere.add(make_xf(Vector3(p.pos.x, y, p.pos.z), Vector3(), Vector3(0.44f, 0.44f, 0.44f)), Color(1.0f, 0.85f, 0.15f), 0.0f, 4.0f, 0.0f);
		} else if (p.value >= 4.0f) {
			lit_gem.add(make_xf(Vector3(p.pos.x, y + 0.1f, p.pos.z), Vector3(0, yaw, 0), Vector3(0.6f, 0.96f, 0.6f)), Color(0.9f, 0.3f, 1.0f), 0.0f, 3.0f, 0.0f);
		} else {
			lit_gem.add(make_xf(Vector3(p.pos.x, y, p.pos.z), Vector3(0, yaw, 0), Vector3(0.36f, 0.58f, 0.36f)), Color(0.25f, 0.7f, 1.0f), 0.0f, 2.5f, 0.0f);
		}
	}

	// Jarrones rompibles
	for (const Jar &j : jars) {
		float grow = std::min(1.0f, j.age / 0.35f);
		float wob = j.flash > 0.0f ? 1.0f + j.flash * 1.2f : 1.0f; // se hincha un instante al recibir un golpe
		float sx = grow * wob, sy = grow / wob;
		float fl = j.flash > 0.0f ? 1.0f : 0.0f;
		Color clay(0.62f, 0.34f, 0.18f);
		Color dark(0.26f, 0.13f, 0.08f);
		Color glow(0.2f, 1.0f, 0.45f);
		Vector3 b(j.pos.x, 0, j.pos.z);
		lit_sphere.add(make_xf(b + Vector3(0, 0.08f * sy, 0), Vector3(), Vector3(0.5f * sx, 0.14f * sy, 0.5f * sx)), dark, fl, 0.0f, 0.1f);
		lit_sphere.add(make_xf(b + Vector3(0, 0.5f * sy, 0), Vector3(), Vector3(0.82f * sx, 0.86f * sy, 0.82f * sx)), clay, fl, 0.0f, 0.1f);
		lit_sphere.add(make_xf(b + Vector3(0, 0.55f * sy, 0), Vector3(), Vector3(0.86f * sx, 0.12f * sy, 0.86f * sx)), glow, fl, 2.6f, 0.0f); // franja que indica curacion
		lit_sphere.add(make_xf(b + Vector3(0, 0.88f * sy, 0), Vector3(), Vector3(0.56f * sx, 0.4f * sy, 0.56f * sx)), clay, fl, 0.0f, 0.1f);
		lit_sphere.add(make_xf(b + Vector3(0, 1.08f * sy, 0), Vector3(), Vector3(0.34f * sx, 0.3f * sy, 0.34f * sx)), dark, fl, 0.0f, 0.1f);
		lit_sphere.add(make_xf(b + Vector3(0, 1.22f * sy, 0), Vector3(), Vector3(0.5f * sx, 0.1f * sy, 0.5f * sx)), clay, fl, 0.0f, 0.1f);
		float halo = 1.7f + std::sin(run_time * 3.0f + j.id) * 0.15f;
		add_sphere.add(make_xf(b + Vector3(0, 0.6f, 0), Vector3(), Vector3(halo, halo, halo) * grow), Color(0.2f, 1.0f, 0.45f, 0.05f), 0.0f, 1.5f);
		mix_disc.add(Transform3D(Basis(Vector3(1.2f * grow, 0, 0), Vector3(0, 0.01f, 0), Vector3(0, 0, 1.2f * grow)), Vector3(j.pos.x, 0.03f, j.pos.z)), Color(0, 0, 0, 0.42f));
	}

	// Esporas luminosas flotando: posiciones fijas en el mundo calculadas con un hash.
	{
		Vector3 c = player->get_position();
		const float CELL_M = 5.0f;
		int cx0 = int(std::floor(c.x / CELL_M)), cz0 = int(std::floor(c.z / CELL_M));
		for (int dz = -5; dz <= 5; dz++) {
			for (int dx = -6; dx <= 6; dx++) {
				int cx = cx0 + dx, cz = cz0 + dz;
				uint32_t h = uint32_t(cx) * 73856093u ^ uint32_t(cz) * 19349663u;
				h ^= h >> 13;
				h *= 0x5bd1e995u;
				h ^= h >> 15;
				if ((h & 3u) != 0u) {
					continue;
				}
				float fx = ((h >> 8) & 255u) / 255.0f, fz = ((h >> 16) & 255u) / 255.0f, fy = ((h >> 24) & 255u) / 255.0f;
				float ph = fy * 40.0f;
				Vector3 mp((cx + fx) * CELL_M + std::sin(run_time * 0.4f + ph) * 0.5f, 0.5f + fy * 2.6f + std::sin(run_time * 0.7f + ph) * 0.3f, (cz + fz) * CELL_M);
				float sz = 0.07f + fx * 0.07f;
				bool violet = (h & 4u) != 0u;
				add_sphere.add(make_xf(mp, Vector3(), Vector3(sz, sz, sz)), violet ? Color(0.8f, 0.4f, 1.0f, 0.7f) : Color(0.35f, 0.85f, 1.0f, 0.7f), 0.0f, 2.5f);
			}
		}
	}

	// Bombardeos en camino: marca en el suelo + haz que se ensancha
	for (const Strike &s : strikes) {
		float k = s.t / 0.7f;
		float d = s.radius * 2.0f;
		add_cyl.add(make_xf(Vector3(s.pos.x, 0.05f, s.pos.z), Vector3(), Vector3(d, 0.02f, d)), Color(1.0f, 0.2f, 0.1f, 0.2f + 0.15f * k), 0.0f, 1.0f);
		float g = 0.24f * (1.0f + 3.0f * k);
		add_cyl.add(make_xf(Vector3(s.pos.x, 15.0f, s.pos.z), Vector3(), Vector3(g, 30.0f, g)), Color(1.0f, 0.25f, 0.1f, 0.6f), 0.0f, 4.0f);
	}

	// Efectos temporales
	for (const Fx &f : fxs) {
		float k = f.age / f.life;
		float ease = 1.0f - (1.0f - k) * (1.0f - k);
		Color c = f.color;
		c.a *= (1.0f - k);
		switch (f.kind) {
			case FX_SPHERE: {
				float d = lerpf(f.r0, f.r1, ease) * 2.0f;
				add_sphere.add(make_xf(f.a, Vector3(), Vector3(d, d * 0.8f, d)), c, 0.0f, f.energy);
			} break;
			case FX_RING: {
				float d = lerpf(f.r0, f.r1, ease) * 2.0f;
				add_cyl.add(make_xf(Vector3(f.a.x, 0.08f, f.a.z), Vector3(), Vector3(d, 0.03f, d)), c, 0.0f, f.energy);
			} break;
			default: { // FX_BEAM
				Vector3 dvec = f.b - f.a;
				float len = dvec.length();
				if (len < 0.001f) {
					break;
				}
				Vector3 z = dvec / len;
				Vector3 up = std::fabs(z.y) > 0.99f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
				Vector3 x = up.cross(z).normalized();
				Vector3 y = z.cross(x);
				float th = f.r0 * (1.0f - k);
				add_box.add(Transform3D(Basis(x * th, y * th, z * len), (f.a + f.b) * 0.5f), c, 0.0f, f.energy);
			} break;
		}
	}

	// Habilidades visibles alrededor del jugador
	Vector3 pp = player->get_position();
	if (player->alive) {
		int drone_lv = taken[U_DRONES];
		if (drone_lv > 0) {
			float radius = 2.6f + drone_lv * 0.15f;
			for (int d = 0; d < drone_lv; d++) {
				float a = drone_angle + TAU * d / drone_lv;
				Vector3 dp = pp + Vector3(std::cos(a) * radius, 1.2f, std::sin(a) * radius);
				float spin = run_time * 460.0f;
				lit_sphere.add(make_xf(dp, Vector3(), Vector3(0.44f, 0.44f, 0.44f)), Color(0.2f, 0.22f, 0.25f), 0.0f, 0.0f, 0.8f);
				add_box.add(make_xf(dp, Vector3(0, spin, 0), Vector3(0.75f, 0.04f, 0.12f)), Color(0.3f, 0.9f, 1.0f, 1.0f), 0.0f, 3.0f);
				add_box.add(make_xf(dp, Vector3(0, spin + 90.0f, 0), Vector3(0.75f, 0.04f, 0.12f)), Color(0.3f, 0.9f, 1.0f, 1.0f), 0.0f, 3.0f);
				mix_disc.add(Transform3D(Basis(Vector3(0.5f, 0, 0), Vector3(0, 0.01f, 0), Vector3(0, 0, 0.5f)), Vector3(dp.x, 0.03f, dp.z)), Color(0, 0, 0, 0.3f));
			}
		}
		if (player->shield_max > 0.0f && player->shield > 0.5f) {
			float frac = player->shield / player->shield_max;
			float d = 2.5f + std::sin(run_time * 3.0f) * 0.06f;
			add_sphere.add(make_xf(pp + Vector3(0, 1.1f, 0), Vector3(), Vector3(d, d * 1.1f, d)), Color(0.3f, 0.65f, 1.0f, 0.02f + 0.04f * frac), 0.0f, 1.2f);
		}
		// Anillo que indica el impulso disponible.
		if (player->dash_cd <= 0.0f) {
			add_cyl.add(make_xf(Vector3(pp.x, 0.04f, pp.z), Vector3(), Vector3(1.5f, 0.01f, 1.5f)), Color(0.3f, 0.85f, 1.0f, 0.12f), 0.0f, 1.5f);
		}
	}

	mix_disc.flush();
	lit_sphere.flush();
	lit_box.flush();
	lit_gem.flush();
	lit_cone.flush();
	add_sphere.flush();
	add_box.flush();
	add_cyl.flush();
}

} // namespace cc
