#include "mesh_util.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/shader.hpp>

using namespace godot;

namespace cc {
namespace mu {

Ref<StandardMaterial3D> mat(const Color &color, float metallic, float roughness) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(color);
	m->set_metallic(metallic);
	m->set_roughness(roughness);
	return m;
}

Ref<StandardMaterial3D> glow(const Color &color, float energy) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(color);
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
	m->set_emission(color);
	m->set_emission_energy_multiplier(energy);
	return m;
}

Ref<StandardMaterial3D> transparent(const Color &color, float energy) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(color);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	if (energy > 0.0f) {
		m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		m->set_emission(Color(color.r, color.g, color.b));
		m->set_emission_energy_multiplier(energy);
	}
	return m;
}

Ref<ShaderMaterial> shader_material(const char *code) {
	Ref<Shader> sh;
	sh.instantiate();
	sh->set_code(String::utf8(code));
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(sh);
	return m;
}

MeshInstance3D *part(Node3D *parent, const Ref<Mesh> &mesh, const Ref<Material> &material, const Vector3 &pos,
		const Vector3 &rot_deg, const Vector3 &scl) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesh);
	mi->set_material_override(material);
	mi->set_position(pos);
	mi->set_rotation_degrees(rot_deg);
	mi->set_scale(scl);
	parent->add_child(mi);
	return mi;
}

Node3D *pivot(Node3D *parent, const Vector3 &pos) {
	Node3D *p = memnew(Node3D);
	p->set_position(pos);
	parent->add_child(p);
	return p;
}

Ref<BoxMesh> box(const Vector3 &size) {
	Ref<BoxMesh> b;
	b.instantiate();
	b->set_size(size);
	return b;
}

Ref<SphereMesh> sphere(float r, int segments, int rings) {
	Ref<SphereMesh> s;
	s.instantiate();
	s->set_radius(r);
	s->set_height(r * 2.0f);
	s->set_radial_segments(segments);
	s->set_rings(rings);
	return s;
}

Ref<CylinderMesh> cylinder(float top, float bottom, float h, int segments) {
	Ref<CylinderMesh> c;
	c.instantiate();
	c->set_top_radius(top);
	c->set_bottom_radius(bottom);
	c->set_height(h);
	c->set_radial_segments(segments);
	return c;
}

} // namespace mu
} // namespace cc
