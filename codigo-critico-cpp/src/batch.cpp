#include "batch.h"

#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/variant/aabb.hpp>

using namespace godot;

namespace cc {

void Batch::init(Node *parent, const Ref<Mesh> &mesh, const Ref<Material> &material, int p_capacity) {
	capacity = p_capacity;
	multimesh.instantiate();
	multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	multimesh->set_use_colors(true);
	multimesh->set_use_custom_data(true);
	multimesh->set_mesh(mesh);
	multimesh->set_instance_count(capacity);
	multimesh->set_visible_instance_count(0);

	buffer.resize(capacity * STRIDE);
	buffer.fill(0.0f);

	node = memnew(MultiMeshInstance3D);
	node->set_multimesh(multimesh);
	node->set_material_override(material);
	node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	// Las instancias se mueven por todo el mapa: AABB enorme para que no se recorte.
	node->set_custom_aabb(AABB(Vector3(-100000, -100, -100000), Vector3(200000, 200, 200000)));
	parent->add_child(node);
	count = 0;
	w = buffer.ptrw();
}

void Batch::flush() {
	if (multimesh.is_null()) {
		return;
	}
	multimesh->set_visible_instance_count(count);
	if (count > 0) {
		multimesh->set_buffer(buffer);
	}
}

} // namespace cc
