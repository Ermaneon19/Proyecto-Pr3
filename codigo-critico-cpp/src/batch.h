// Batch: dibujo "inmediato" con MultiMesh. Cada frame se llena un buffer con
// (transform, color, datos) y se sube de una sola vez a la GPU, de modo que
// cientos de enemigos, balas y efectos cuestan UNA llamada de dibujo por batch
// en lugar de un nodo MeshInstance3D por pieza.
#pragma once

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace cc {

class Batch {
public:
	static const int STRIDE = 20; // 12 transform + 4 color + 4 custom

	void init(godot::Node *parent, const godot::Ref<godot::Mesh> &mesh, const godot::Ref<godot::Material> &material, int capacity);

	inline void begin() {
		count = 0;
		w = buffer.ptrw();
	}

	// custom: r = destello de golpe, g = energia de emision, b = metalico, a = libre
	inline void add(const godot::Transform3D &t, const godot::Color &c, float cr = 0.0f, float cg = 0.0f, float cb = 0.0f, float ca = 0.0f) {
		if (count >= capacity) {
			return;
		}
		float *p = w + count * STRIDE;
		const godot::Basis &b = t.basis;
		p[0] = b.rows[0][0];
		p[1] = b.rows[0][1];
		p[2] = b.rows[0][2];
		p[3] = t.origin.x;
		p[4] = b.rows[1][0];
		p[5] = b.rows[1][1];
		p[6] = b.rows[1][2];
		p[7] = t.origin.y;
		p[8] = b.rows[2][0];
		p[9] = b.rows[2][1];
		p[10] = b.rows[2][2];
		p[11] = t.origin.z;
		p[12] = c.r;
		p[13] = c.g;
		p[14] = c.b;
		p[15] = c.a;
		p[16] = cr;
		p[17] = cg;
		p[18] = cb;
		p[19] = ca;
		count++;
	}

	void flush();

	int count = 0;

private:
	godot::Ref<godot::MultiMesh> multimesh;
	godot::MultiMeshInstance3D *node = nullptr;
	godot::PackedFloat32Array buffer;
	float *w = nullptr;
	int capacity = 0;
};

} // namespace cc
