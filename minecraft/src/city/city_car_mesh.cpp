/**
 * @file city_car_mesh.cpp
 * @brief The toy car built from boxes with SurfaceTool, one surface per material.
 */
#include "city_car_mesh.h"
#include "city_data.h"

namespace godot::city {

namespace {

// A box centred on p_centre, p_size big, into p_st.
void add_box(
		const Ref<SurfaceTool> &p_st,
		const Vector3 &p_centre,
		const Vector3 &p_size,
		const Color &p_colour
) {
	const Vector3 h = p_size * 0.5f;
	for (int axis = 0; axis < 3; axis++) {
		for (int side = -1; side <= 1; side += 2) {
			Vector3 normal;
			normal[axis] = float(side);
			const int u = (axis + 1) % 3;
			const int v = (axis + 2) % 3;
			Vector3 corners[4];
			for (int k = 0; k < 4; k++) {
				Vector3 c;
				c[axis] = h[axis] * float(side);
				c[u] = h[u] * (k == 0 || k == 3 ? -1.0f : 1.0f);
				c[v] = h[v] * (k < 2 ? -1.0f : 1.0f);
				corners[k] = p_centre + c;
			}
			add_face(p_st, corners[0], corners[1], corners[2], normal, p_colour, nullptr);
			add_face(p_st, corners[0], corners[2], corners[3], normal, p_colour, nullptr);
		}
	}
}

// p_st's triangles committed as a new surface of p_mesh, drawn with p_material.
void commit(
		const Ref<SurfaceTool> &p_st,
		const Ref<ArrayMesh> &p_mesh,
		const Ref<Material> &p_material
) {
	p_st->commit(p_mesh);
	p_mesh->surface_set_material(p_mesh->get_surface_count() - 1, p_material);
}

Ref<StandardMaterial3D> plain(
		const Color &p_colour,
		float p_roughness
) {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(p_colour);
	material->set_roughness(p_roughness);
	return material;
}

} // namespace

Ref<ArrayMesh> make_car_mesh(const Vector3 &p_size) {
	const float w = p_size.x;
	const float h = p_size.y;
	const float l = p_size.z;
	const float wheel = h * 0.4f; // a wheel's height (and length)
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES); // the body, tinted per car
	add_box(st, Vector3(0, wheel * 0.45f + h * 0.22f, 0), Vector3(w, h * 0.44f, l), Color(1, 1, 1));
	commit(st, mesh, vertex_colour_material(0.4f));
	st->begin(Mesh::PRIMITIVE_TRIANGLES); // the cabin's windows
	add_box(st, Vector3(0, wheel * 0.45f + h * 0.44f + h * 0.17f, l * 0.06f), Vector3(w * 0.84f, h * 0.34f, l * 0.5f), Color(1, 1, 1));
	commit(st, mesh, plain(Color(0.72f, 0.84f, 0.95f), 0.15f));
	st->begin(Mesh::PRIMITIVE_TRIANGLES); // the wheels
	for (int k = 0; k < 4; k++) {
		const float x = (k % 2 ? 1.0f : -1.0f) * (w * 0.5f - w * 0.08f);
		const float z = (k < 2 ? 1.0f : -1.0f) * l * 0.32f;
		add_box(st, Vector3(x, wheel * 0.5f, z), Vector3(w * 0.2f, wheel, wheel), Color(1, 1, 1));
	}
	commit(st, mesh, plain(Color(0.12f, 0.12f, 0.13f), 0.8f));
	return mesh;
}

} // namespace godot::city
