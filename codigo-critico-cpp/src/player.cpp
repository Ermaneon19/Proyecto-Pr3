#include "player.h"

#include "game_data.h"
#include "mesh_util.h"
#include "util.h"

#include <algorithm>
#include <cmath>

using namespace godot;

namespace cc {

void Player::apply_meta() {
	const GameData &gd = GameData::get();
	damage = 10.0f * (1.0f + 0.10f * gd.get_level(M_DAMAGE));
	fire_interval = 0.45f / (1.0f + 0.08f * gd.get_level(M_FIRE_RATE));
	max_hp = 100.0f + 20.0f * gd.get_level(M_MAX_HP);
	speed = 7.0f * (1.0f + 0.05f * gd.get_level(M_SPEED));
	magnet_radius = 3.0f * (1.0f + 0.15f * gd.get_level(M_MAGNET));
	core_mult = 1.0f + 0.10f * gd.get_level(M_GREED);
	regen = 0.3f * gd.get_level(M_REGEN);
	hp = max_hp;
}

// El modelo se construye en player_model.cpp (Player::build).

// ---------------------------------------------------------------- animacion
void Player::animate(const Vector3 &move_dir, bool has_target, const Vector3 &target_pos, float delta) {
	anim_t += delta;

	if (!alive) {
		// Caida al morir: el modelo se desploma hacia atras.
		death_t = std::min(1.0f, death_t + delta * 2.2f);
		Vector3 r = model->get_rotation();
		r.x = lerpf(0.0f, PI * 0.5f, death_t * death_t);
		model->set_rotation(r);
		model->set_position(Vector3(0, 0.25f * death_t, 0));
		return;
	}

	bool moving = move_dir.length() > 0.1f;
	bool dashing = dash_time > 0.0f;
	float swing = moving ? std::sin(anim_t * 12.0f) * 0.6f : 0.0f;
	float k = std::min(1.0f, 12.0f * delta);

	Vector3 r = leg_l->get_rotation();
	r.x = lerpf(r.x, swing, k);
	leg_l->set_rotation(r);
	r = leg_r->get_rotation();
	r.x = lerpf(r.x, -swing, k);
	leg_r->set_rotation(r);
	r = arm_l->get_rotation();
	r.x = lerpf(r.x, -swing * 0.1f, k); // el brazo izquierdo sujeta el rifle: apenas se mueve
	arm_l->set_rotation(r);

	float cape_target = dashing ? 1.1f : (moving ? 0.45f + std::sin(anim_t * 9.0f) * 0.08f : 0.08f + std::sin(anim_t * 2.0f) * 0.04f);
	r = cape->get_rotation();
	r.x = lerpf(r.x, cape_target, std::min(1.0f, 6.0f * delta));
	cape->set_rotation(r);

	model->set_position(Vector3(0, moving ? std::fabs(std::sin(anim_t * 12.0f)) * 0.06f : 0.0f, 0));

	// Mira hacia el enemigo mas cercano; si no hay, hacia donde camina.
	Vector3 look;
	if (has_target) {
		look = target_pos - get_position();
	} else if (moving) {
		look = move_dir;
	}
	r = model->get_rotation();
	if (look.x * look.x + look.z * look.z > 0.0001f) {
		float ang = std::atan2(-look.x, -look.z);
		r.y = lerp_angle(r.y, ang, std::min(1.0f, 14.0f * delta));
	}
	// Inclinacion hacia delante durante el impulso.
	r.x = lerpf(r.x, dashing ? -0.35f : 0.0f, std::min(1.0f, 16.0f * delta));
	model->set_rotation(r);

	if (hurt_flash > 0.0f) {
		hurt_flash -= delta;
		armor_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		armor_mat->set_emission(Color(1, 0.1f, 0.1f));
		armor_mat->set_emission_energy_multiplier(std::max(0.0f, hurt_flash) * 6.0f);
	} else if (armor_mat->get_feature(BaseMaterial3D::FEATURE_EMISSION)) {
		armor_mat->set_feature(BaseMaterial3D::FEATURE_EMISSION, false);
	}
}

Vector3 Player::muzzle_position() const {
	Vector3 p = muzzle->get_global_position();
	p.y = 1.25f;
	return p;
}

float Player::facing() const {
	return model->get_rotation().y;
}

// ---------------------------------------------------------------- vida
bool Player::take_damage(float amount) {
	if (!alive || dash_time > 0.0f) {
		return false; // invulnerable durante el impulso
	}
	shield_delay = 4.0f;
	if (shield > 0.0f) {
		float absorbed = std::min(shield, amount);
		shield -= absorbed;
		amount -= absorbed;
		if (amount <= 0.0f) {
			return false;
		}
	}
	hp -= amount;
	hurt_flash = 0.25f;
	if (hp <= 0.0f) {
		hp = 0.0f;
		alive = false;
		return true;
	}
	return false;
}

void Player::heal(float amount) {
	hp = std::min(max_hp, hp + amount);
}

bool Player::try_dash(const Vector3 &dir) {
	if (!alive || dash_cd > 0.0f || dash_time > 0.0f) {
		return false;
	}
	Vector3 d = dir;
	if (d.length() < 0.1f) {
		// Sin direccion: impulso hacia donde mira el soldado.
		float y = facing();
		d = Vector3(-std::sin(y), 0, -std::cos(y));
	}
	dash_dir = d.normalized();
	dash_time = DASH_DURATION;
	dash_cd = DASH_COOLDOWN;
	return true;
}

} // namespace cc
