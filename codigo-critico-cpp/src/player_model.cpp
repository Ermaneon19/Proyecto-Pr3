// Modelo del soldado: armadura pesada azul pizarra con marcas amarillas, casco
// cerrado con visor negro y respirador, hombreras grandes, capa y rifle a dos manos.
// Todo con primitivas; frente = -Z.
#include "player.h"

#include "mesh_util.h"

using namespace godot;

namespace cc {

void Player::build() {
	model = mu::pivot(this);

	armor_mat = mu::mat(Color(0.17f, 0.2f, 0.27f), 0.55f, 0.42f); // parpadea en rojo al recibir dano
	Ref<StandardMaterial3D> under = mu::mat(Color(0.05f, 0.055f, 0.07f), 0.1f, 0.8f); // traje interior
	Ref<StandardMaterial3D> steel = mu::mat(Color(0.35f, 0.37f, 0.42f), 0.8f, 0.35f);
	Ref<StandardMaterial3D> yellow = mu::mat(Color(0.95f, 0.74f, 0.08f), 0.3f, 0.45f);
	yellow->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	yellow->set_emission(Color(0.95f, 0.7f, 0.08f));
	yellow->set_emission_energy_multiplier(0.35f);
	Ref<StandardMaterial3D> visor = mu::mat(Color(0.02f, 0.03f, 0.05f), 0.9f, 0.12f);
	Ref<StandardMaterial3D> lens = mu::glow(Color(0.3f, 0.8f, 1.0f), 2.0f);
	Ref<StandardMaterial3D> cape_mat = mu::mat(Color(0.06f, 0.06f, 0.08f), 0.0f, 0.9f);
	cape_mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);

	// --- Piernas (pivotan en la cadera para animar la caminata) ---
	leg_l = mu::pivot(model, Vector3(-0.2f, 0.9f, 0));
	leg_r = mu::pivot(model, Vector3(0.2f, 0.9f, 0));
	Ref<BoxMesh> thigh = mu::box(Vector3(0.24f, 0.42f, 0.26f));
	Ref<BoxMesh> thigh_plate = mu::box(Vector3(0.27f, 0.3f, 0.1f));
	Ref<SphereMesh> knee = mu::sphere(0.13f, 10, 5);
	Ref<BoxMesh> knee_mark = mu::box(Vector3(0.12f, 0.05f, 0.03f));
	Ref<BoxMesh> shin = mu::box(Vector3(0.25f, 0.38f, 0.28f));
	Ref<BoxMesh> boot = mu::box(Vector3(0.27f, 0.14f, 0.4f));
	for (Node3D *leg : { leg_l, leg_r }) {
		mu::part(leg, thigh, under, Vector3(0, -0.22f, 0));
		mu::part(leg, thigh_plate, armor_mat, Vector3(0, -0.2f, -0.12f));
		mu::part(leg, knee, armor_mat, Vector3(0, -0.45f, -0.13f));
		mu::part(leg, knee_mark, yellow, Vector3(0, -0.45f, -0.26f));
		mu::part(leg, shin, armor_mat, Vector3(0, -0.66f, 0));
		mu::part(leg, boot, under, Vector3(0, -0.84f, -0.06f));
	}

	// --- Cadera y cinturon ---
	mu::part(model, mu::box(Vector3(0.62f, 0.22f, 0.36f)), under, Vector3(0, 0.96f, 0));
	mu::part(model, mu::box(Vector3(0.66f, 0.08f, 0.4f)), steel, Vector3(0, 1.07f, 0));
	Ref<BoxMesh> pouch = mu::box(Vector3(0.14f, 0.16f, 0.1f));
	mu::part(model, pouch, armor_mat, Vector3(-0.26f, 1.0f, -0.2f));
	mu::part(model, pouch, armor_mat, Vector3(0.26f, 1.0f, -0.2f));

	// --- Torso ---
	mu::part(model, mu::box(Vector3(0.6f, 0.3f, 0.36f)), under, Vector3(0, 1.2f, 0)); // abdomen
	mu::part(model, mu::box(Vector3(0.86f, 0.5f, 0.5f)), armor_mat, Vector3(0, 1.5f, 0)); // peto
	mu::part(model, mu::box(Vector3(0.44f, 0.14f, 0.42f)), armor_mat, Vector3(0, 1.78f, 0)); // cuello
	Ref<BoxMesh> stripe = mu::box(Vector3(0.07f, 0.3f, 0.02f));
	mu::part(model, stripe, yellow, Vector3(-0.24f, 1.52f, -0.26f));
	mu::part(model, stripe, yellow, Vector3(-0.13f, 1.52f, -0.26f));
	mu::part(model, mu::box(Vector3(0.3f, 0.26f, 0.02f)), steel, Vector3(0.16f, 1.5f, -0.256f)); // placa central
	mu::part(model, mu::box(Vector3(0.5f, 0.5f, 0.16f)), under, Vector3(0, 1.48f, 0.33f)); // mochila
	mu::part(model, mu::box(Vector3(0.52f, 0.06f, 0.17f)), yellow, Vector3(0, 1.66f, 0.33f));

	// --- Hombreras grandes con franja amarilla ---
	Ref<SphereMesh> pauldron = mu::sphere(0.3f, 14, 7);
	Ref<BoxMesh> pauldron_mark = mu::box(Vector3(0.09f, 0.05f, 0.54f));
	for (float s : { -1.0f, 1.0f }) {
		mu::part(model, pauldron, armor_mat, Vector3(0.58f * s, 1.72f, 0), Vector3(0, 0, -12.0f * s), Vector3(1.0f, 0.72f, 1.1f));
		mu::part(model, pauldron_mark, yellow, Vector3(0.6f * s, 1.925f, 0), Vector3(0, 0, -12.0f * s));
	}

	// --- Rifle sujeto con las dos manos ---
	Node3D *gun = mu::pivot(model, Vector3(0.14f, 1.28f, -0.5f));
	mu::part(gun, mu::box(Vector3(0.14f, 0.2f, 0.6f)), under); // cajon
	mu::part(gun, mu::box(Vector3(0.12f, 0.14f, 0.45f)), steel, Vector3(0, 0, -0.5f)); // guardamanos
	mu::part(gun, mu::cylinder(0.035f, 0.035f, 0.4f, 8), steel, Vector3(0, 0.02f, -0.9f), Vector3(90, 0, 0)); // canon
	mu::part(gun, mu::box(Vector3(0.09f, 0.09f, 0.12f)), under, Vector3(0, 0.02f, -1.1f)); // bocacha
	mu::part(gun, mu::box(Vector3(0.1f, 0.16f, 0.35f)), under, Vector3(0, -0.03f, 0.45f)); // culata
	mu::part(gun, mu::box(Vector3(0.08f, 0.26f, 0.14f)), steel, Vector3(0, -0.2f, -0.1f), Vector3(10, 0, 0)); // cargador
	mu::part(gun, mu::cylinder(0.05f, 0.05f, 0.32f, 10), steel, Vector3(0, 0.17f, -0.1f), Vector3(90, 0, 0)); // mira
	mu::part(gun, mu::cylinder(0.042f, 0.042f, 0.02f, 10), lens, Vector3(0, 0.17f, -0.265f), Vector3(90, 0, 0));
	mu::part(gun, mu::box(Vector3(0.145f, 0.05f, 0.2f)), yellow, Vector3(0, 0.03f, 0.1f));
	muzzle = mu::pivot(gun, Vector3(0, 0.02f, -1.18f));

	// --- Brazos ---
	Ref<BoxMesh> upper = mu::box(Vector3(0.19f, 0.4f, 0.2f));
	Ref<BoxMesh> forearm = mu::box(Vector3(0.21f, 0.4f, 0.23f));
	arm_l = mu::pivot(model, Vector3(-0.52f, 1.62f, 0)); // el izquierdo cruza hasta el guardamanos
	mu::part(arm_l, upper, under, Vector3(0.08f, -0.14f, -0.18f), Vector3(-60, -25, 0));
	mu::part(arm_l, forearm, armor_mat, Vector3(0.36f, -0.28f, -0.55f), Vector3(-80, -48, 0));
	Node3D *arm_r = mu::pivot(model, Vector3(0.52f, 1.62f, 0)); // el derecho va a la empunadura
	mu::part(arm_r, upper, under, Vector3(-0.03f, -0.2f, -0.08f), Vector3(-40, 12, 0));
	mu::part(arm_r, forearm, armor_mat, Vector3(-0.16f, -0.36f, -0.34f), Vector3(-82, 25, 0));

	// --- Casco cerrado: visor negro, respirador y cresta amarilla ---
	mu::part(model, mu::sphere(0.31f, 16, 8), armor_mat, Vector3(0, 2.0f, 0), Vector3(), Vector3(1, 1, 1.1f));
	mu::part(model, mu::box(Vector3(0.5f, 0.2f, 0.32f)), armor_mat, Vector3(0, 1.85f, -0.1f)); // mandibula
	mu::part(model, mu::box(Vector3(0.46f, 0.15f, 0.14f)), visor, Vector3(0, 2.01f, -0.27f));
	mu::part(model, mu::box(Vector3(0.3f, 0.02f, 0.01f)), lens, Vector3(0, 2.04f, -0.343f)); // reflejo del visor
	mu::part(model, mu::box(Vector3(0.22f, 0.2f, 0.18f)), under, Vector3(0, 1.82f, -0.28f)); // respirador
	Ref<CylinderMesh> filter = mu::cylinder(0.06f, 0.06f, 0.1f, 8);
	Ref<CylinderMesh> ear = mu::cylinder(0.09f, 0.09f, 0.06f, 10);
	for (float s : { -1.0f, 1.0f }) {
		mu::part(model, filter, steel, Vector3(0.16f * s, 1.8f, -0.27f), Vector3(0, 0, 90));
		mu::part(model, ear, steel, Vector3(0.32f * s, 2.0f, 0), Vector3(0, 0, 90));
	}
	mu::part(model, mu::box(Vector3(0.12f, 0.05f, 0.58f)), yellow, Vector3(0, 2.305f, 0.0f)); // cresta
	mu::part(model, mu::box(Vector3(0.12f, 0.16f, 0.05f)), yellow, Vector3(0, 2.2f, -0.31f), Vector3(-25, 0, 0));

	// --- Capa (pivota en los hombros) ---
	cape = mu::pivot(model, Vector3(0, 1.8f, 0.44f));
	mu::part(cape, mu::box(Vector3(0.7f, 0.12f, 0.14f)), under, Vector3(0, 0, -0.04f));
	mu::part(cape, mu::box(Vector3(0.9f, 1.25f, 0.03f)), cape_mat, Vector3(0, -0.63f, 0));
	mu::part(cape, mu::box(Vector3(0.91f, 0.07f, 0.035f)), yellow, Vector3(0, -1.22f, 0));

	// Sombra falsa (disco oscuro bajo los pies)
	mu::part(this, mu::cylinder(0.6f, 0.6f, 0.01f, 20), mu::transparent(Color(0, 0, 0, 0.45f)), Vector3(0, 0.02f, 0));
}

} // namespace cc
