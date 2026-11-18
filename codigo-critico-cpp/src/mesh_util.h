// Utilidades para armar modelos low-poly con primitivas (sin archivos .glb).
#pragma once

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace cc {
namespace mu {

godot::Ref<godot::StandardMaterial3D> mat(const godot::Color &color, float metallic = 0.0f, float roughness = 0.7f);
godot::Ref<godot::StandardMaterial3D> glow(const godot::Color &color, float energy = 3.0f);
godot::Ref<godot::StandardMaterial3D> transparent(const godot::Color &color, float energy = 0.0f);
godot::Ref<godot::ShaderMaterial> shader_material(const char *code);

godot::MeshInstance3D *part(godot::Node3D *parent, const godot::Ref<godot::Mesh> &mesh, const godot::Ref<godot::Material> &material,
		const godot::Vector3 &pos = godot::Vector3(), const godot::Vector3 &rot_deg = godot::Vector3(),
		const godot::Vector3 &scl = godot::Vector3(1, 1, 1));
godot::Node3D *pivot(godot::Node3D *parent, const godot::Vector3 &pos = godot::Vector3());

godot::Ref<godot::BoxMesh> box(const godot::Vector3 &size);
godot::Ref<godot::SphereMesh> sphere(float r, int segments = 16, int rings = 8);
godot::Ref<godot::CylinderMesh> cylinder(float top, float bottom, float h, int segments = 12);

} // namespace mu
} // namespace cc
