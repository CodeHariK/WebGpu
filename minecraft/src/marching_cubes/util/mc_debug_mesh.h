// MCDebugMesh: the water debug overlay's geometry (mc_grid_debug_view.cpp,
// mc_water_debug_stages.cpp), built up piece by piece: flat squares (triangles) and lines,
// each with a vertex colour. UV.x of a square is the order a pour reached it (0..1, the overlay
// shader's replay hides later ones), -1 for always shown.
#ifndef MC_DEBUG_MESH_H
#define MC_DEBUG_MESH_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot {

// The water overlay's geometry, built up piece by piece.
struct MCDebugMesh {
	PackedVector3Array tri_vertices, line_vertices;
	PackedColorArray tri_colors, line_colors;
	PackedVector2Array tri_uvs, line_uvs;

	void square(const Vector3 &p_centre, float p_half, const Color &p_color, float p_order = -1.0f) {
		const Vector3 c[4] = { p_centre + Vector3(-p_half, 0, -p_half), p_centre + Vector3(p_half, 0, -p_half),
			p_centre + Vector3(p_half, 0, p_half), p_centre + Vector3(-p_half, 0, p_half) };
		for (int i : { 0, 1, 2, 0, 2, 3 }) {
			tri_vertices.push_back(c[i]);
			tri_colors.push_back(p_color);
			tri_uvs.push_back(Vector2(p_order, 0));
		}
	}

	// A flat filled arrow at p_at pointing along p_dir (unit, horizontal), p_length long. Drawn with
	// the squares (triangles, in order), so an arrow added after a square shows on top of it.
	void arrow(const Vector3 &p_at, const Vector3 &p_dir, float p_length, const Color &p_color) {
		Vector3 side(-p_dir.z, 0, p_dir.x);
		Vector3 back = p_at - p_dir * (p_length * 0.5f);
		Vector3 neck = p_at + p_dir * (p_length * 0.1f);
		Vector3 tip = p_at + p_dir * (p_length * 0.5f);
		float shaft = p_length * 0.08f;
		float head = p_length * 0.25f;
		const Vector3 points[9] = { back - side * shaft, neck - side * shaft, neck + side * shaft, // shaft
			back - side * shaft, neck + side * shaft, back + side * shaft, neck - side * head, tip, neck + side * head }; // head
		for (const Vector3 &p : points) {
			tri_vertices.push_back(p);
			tri_colors.push_back(p_color);
			tri_uvs.push_back(Vector2(-1, 0));
		}
	}

	// One filled triangle (drawn in order with the squares).
	void triangle(const Vector3 &p_a, const Vector3 &p_b, const Vector3 &p_c, const Color &p_color) {
		for (const Vector3 &p : { p_a, p_b, p_c }) {
			tri_vertices.push_back(p);
			tri_colors.push_back(p_color);
			tri_uvs.push_back(Vector2(-1, 0));
		}
	}

	// Everything of p_other, drawn after what is here.
	void append(const MCDebugMesh &p_other) {
		tri_vertices.append_array(p_other.tri_vertices);
		tri_colors.append_array(p_other.tri_colors);
		tri_uvs.append_array(p_other.tri_uvs);
		line_vertices.append_array(p_other.line_vertices);
		line_colors.append_array(p_other.line_colors);
		line_uvs.append_array(p_other.line_uvs);
	}

	void line(const Vector3 &p_a, const Vector3 &p_b, const Color &p_color) {
		line_vertices.push_back(p_a);
		line_vertices.push_back(p_b);
		line_colors.push_back(p_color);
		line_colors.push_back(p_color);
		line_uvs.push_back(Vector2(-1, 0));
		line_uvs.push_back(Vector2(-1, 0));
	}

	Ref<ArrayMesh> commit() const {
		Ref<ArrayMesh> mesh;
		mesh.instantiate();
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		if (!tri_vertices.is_empty()) {
			arrays[Mesh::ARRAY_VERTEX] = tri_vertices;
			arrays[Mesh::ARRAY_COLOR] = tri_colors;
			arrays[Mesh::ARRAY_TEX_UV] = tri_uvs;
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		}
		if (!line_vertices.is_empty()) {
			arrays[Mesh::ARRAY_VERTEX] = line_vertices;
			arrays[Mesh::ARRAY_COLOR] = line_colors;
			arrays[Mesh::ARRAY_TEX_UV] = line_uvs;
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
		}
		return mesh;
	}
};

} // namespace godot

#endif // MC_DEBUG_MESH_H
