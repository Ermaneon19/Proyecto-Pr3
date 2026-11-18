// Utilidades comunes: texto UTF-8, aleatorios rapidos y matematicas pequenas.
#pragma once

#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cmath>
#include <cstdint>

// Los literales con tildes/enie deben pasar por UTF-8.
#define U8(s) godot::String::utf8(s)

namespace cc {

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;

// xorshift32: mucho mas barato que llamar al motor por cada numero aleatorio.
inline uint32_t &rng_state() {
	static uint32_t s = 0x9E3779B9u;
	return s;
}
inline void rng_seed(uint32_t seed) {
	rng_state() = seed ? seed : 0x9E3779B9u;
}
inline uint32_t rng_u32() {
	uint32_t &x = rng_state();
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return x;
}
inline float frand() {
	return (rng_u32() >> 8) * (1.0f / 16777216.0f);
}
inline float frange(float a, float b) {
	return a + (b - a) * frand();
}
inline int irand(int n) {
	return n <= 0 ? 0 : int(rng_u32() % uint32_t(n));
}

inline float lerpf(float a, float b, float t) {
	return a + (b - a) * t;
}
inline float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
inline float lerp_angle(float from, float to, float t) {
	float diff = std::fmod(to - from, TAU);
	float shortest = std::fmod(2.0f * diff, TAU) - diff;
	return from + shortest * t;
}
inline float dist2_xz(const godot::Vector3 &a, const godot::Vector3 &b) {
	float dx = a.x - b.x, dz = a.z - b.z;
	return dx * dx + dz * dz;
}

// Transform con la misma semantica que Node3D: T * R(euler en grados) * S.
inline godot::Transform3D make_xf(const godot::Vector3 &pos, const godot::Vector3 &rot_deg = godot::Vector3(),
		const godot::Vector3 &scl = godot::Vector3(1, 1, 1)) {
	godot::Basis b = godot::Basis::from_euler(rot_deg * (PI / 180.0f));
	for (int r = 0; r < 3; r++) {
		b.rows[r][0] *= scl.x;
		b.rows[r][1] *= scl.y;
		b.rows[r][2] *= scl.z;
	}
	return godot::Transform3D(b, pos);
}

} // namespace cc
