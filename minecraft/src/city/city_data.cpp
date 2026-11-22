/**
 * @file city_data.cpp
 * @brief Reading the Map 26 JSON export: points, numbers, transforms.
 */
#include "city_data.h"

namespace godot::city {

PackedVector2Array read_line(const Array &p_points) {
	PackedVector2Array out;
	out.resize(p_points.size());
	for (int64_t i = 0; i < p_points.size(); i++) {
		const Array p = p_points[i];
		out.set(i, Vector2(double(p[0]), double(p[1])));
	}
	return out;
}

float read_float(
		const Dictionary &p_dict,
		const String &p_key,
		float p_fallback
) {
	return p_dict.has(p_key) ? float(double(p_dict[p_key])) : p_fallback;
}

Transform3D standing(
		float p_x,
		float p_z,
		float p_angle,
		const Vector3 &p_size,
		float p_base
) {
	Basis basis = Basis(Vector3(0, 1, 0), -p_angle).scaled_local(p_size);
	return Transform3D(basis, Vector3(p_x, p_base + p_size.y * 0.5f, p_z));
}

void add_face(
		const Ref<SurfaceTool> &p_st,
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_c,
		const Vector3 &p_normal,
		const Color &p_colour,
		PackedVector3Array *r_faces
) {
	const bool clockwise = (p_b - p_a).cross(p_c - p_a).dot(p_normal) < 0.0f;
	const Vector3 corners[3] = { p_a, clockwise ? p_b : p_c, clockwise ? p_c : p_b };
	for (const Vector3 &v : corners) {
		p_st->set_color(p_colour);
		p_st->set_normal(p_normal);
		p_st->add_vertex(v);
		if (r_faces) {
			r_faces->push_back(v);
		}
	}
}

Ref<StandardMaterial3D> vertex_colour_material(float p_roughness) {
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	material->set_roughness(p_roughness);
	return material;
}

float signed_area(const PackedVector2Array &p_loop) {
	float area = 0.0f;
	const int64_t n = p_loop.size();
	for (int64_t i = 0; i < n; i++) {
		const Vector2 a = p_loop[i];
		const Vector2 b = p_loop[(i + 1) % n];
		area += a.x * b.y - b.x * a.y;
	}
	return area;
}

} // namespace godot::city
