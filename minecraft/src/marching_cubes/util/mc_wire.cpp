// Wireframe pieces for hover previews and debug views (see mc_wire.h).
#include "marching_cubes/util/mc_wire.h"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot {

Ref<ArrayMesh> mc_wire_box(const Vector3 &p_size) {
	Vector3 h = p_size * 0.5f;
	// the 8 corners: bit 0 = +x, bit 1 = +y, bit 2 = +z
	Vector3 corners[8];
	for (int i = 0; i < 8; i++) {
		corners[i] = Vector3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
	}
	// an edge joins two corners that differ in one bit
	PackedVector3Array lines;
	for (int i = 0; i < 8; i++) {
		for (int bit : { 1, 2, 4 }) {
			if (!(i & bit)) {
				lines.push_back(corners[i]);
				lines.push_back(corners[i | bit]);
			}
		}
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = lines;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	return mesh;
}

namespace {
// Append the 6 faces of the axis-aligned box from p_min to p_max (two triangles each).
void append_box(
		PackedVector3Array &r_triangles,
		const Vector3 &p_min,
		const Vector3 &p_max
) {
	for (int axis = 0; axis < 3; axis++) {
		for (int side = 0; side < 2; side++) {
			int u = (axis + 1) % 3;
			int v = (axis + 2) % 3;
			Vector3 c[4];
			const int su[4] = { 0, 1, 1, 0 };
			const int sv[4] = { 0, 0, 1, 1 };
			for (int k = 0; k < 4; k++) {
				Vector3 p;
				p[axis] = side ? p_max[axis] : p_min[axis];
				p[u] = su[k] ? p_max[u] : p_min[u];
				p[v] = sv[k] ? p_max[v] : p_min[v];
				c[k] = p;
			}
			// Godot's front faces are clockwise seen from outside: (u, v) runs counter-clockwise seen
			// from +axis, so the + side takes the reversed order and the - side the plain one
			const int order[2][6] = { { 0, 1, 2, 0, 2, 3 }, { 0, 2, 1, 0, 3, 2 } };
			for (int i : order[side]) {
				r_triangles.push_back(c[i]);
			}
		}
	}
}
} // namespace

Ref<ArrayMesh> mc_wire_cube(
		const Vector3 &p_size,
		float p_thickness
) {
	Vector3 h = p_size * 0.5f;
	float t = p_thickness * 0.5f;
	// the edges: along each axis, at the 4 combinations of the other two axes' sides
	PackedVector3Array bars;
	for (int axis = 0; axis < 3; axis++) {
		int u = (axis + 1) % 3;
		int v = (axis + 2) % 3;
		for (int k = 0; k < 4; k++) {
			Vector3 lo, hi;
			lo[axis] = -h[axis] - t;
			hi[axis] = h[axis] + t;
			float cu = (k & 1) ? h[u] : -h[u];
			float cv = (k & 2) ? h[v] : -h[v];
			lo[u] = cu - t;
			hi[u] = cu + t;
			lo[v] = cv - t;
			hi[v] = cv + t;
			append_box(bars, lo, hi);
		}
	}
	PackedVector3Array faces;
	append_box(faces, -h, h);
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = bars;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	arrays[Mesh::ARRAY_VERTEX] = faces;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

Ref<ArrayMesh> mc_wire_face(float p_size) {
	float h = p_size * 0.5f;
	const Vector3 square[4] = { Vector3(-h, -h, 0), Vector3(h, -h, 0), Vector3(h, h, 0), Vector3(-h, h, 0) };
	PackedVector3Array outline;
	for (int i = 0; i < 4; i++) {
		outline.push_back(square[i]);
		outline.push_back(square[(i + 1) % 4]);
	}
	PackedVector3Array fill;
	for (int i : { 0, 1, 2, 0, 2, 3 }) {
		fill.push_back(square[i]);
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = outline;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	arrays[Mesh::ARRAY_VERTEX] = fill;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

Ref<StandardMaterial3D> mc_wire_material(
		const Color &p_color,
		bool p_show_hidden
) {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(p_color);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	material->set_render_priority(1);
	if (p_show_hidden) {
		Ref<StandardMaterial3D> hidden;
		hidden.instantiate();
		hidden->set_albedo(Color(p_color.r, p_color.g, p_color.b, p_color.a * 0.25f));
		hidden->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		hidden->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		hidden->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		hidden->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
		material->set_next_pass(hidden);
	}
	return material;
}

} // namespace godot
