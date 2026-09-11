/**
 * @file cliff_mesh.cpp
 * @brief CliffMesh: Procedural Voronoi cliff mesh generator with connected box topology,
 * slope masking, and concave collision generation.
 */
#include "cliff_mesh.h"
#include "prop_geometry.h"
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {

void CliffMesh::_bind_methods() {
	// clang-format off
#define CL_BIND(m_type, m_name) \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &CliffMesh::set_##m_name); \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &CliffMesh::get_##m_name);

	CL_BIND(int, seed)
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	CL_BIND(Vector3, extents)
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "extents"), "set_extents", "get_extents");

	CL_BIND(int, subdivisions_x)
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivisions_x"), "set_subdivisions_x", "get_subdivisions_x");

	CL_BIND(int, subdivisions_y)
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivisions_y"), "set_subdivisions_y", "get_subdivisions_y");

	CL_BIND(int, subdivisions_z)
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivisions_z"), "set_subdivisions_z", "get_subdivisions_z");

	CL_BIND(float, corner_radius)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "corner_radius"), "set_corner_radius", "get_corner_radius");

	CL_BIND(float, edge_radius)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "edge_radius"), "set_edge_radius", "get_edge_radius");

	ClassDB::bind_method(D_METHOD("set_style", "value"), &CliffMesh::set_style);
	ClassDB::bind_method(D_METHOD("get_style"), &CliffMesh::get_style);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::INT, "style", PROPERTY_HINT_ENUM,
					"FacetedBlocks,Crevassed,ColumnarBasalt,Stratified"
			),
			"set_style", "get_style"
	);

	CL_BIND(float, wall_noise_amp)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_noise_amp"), "set_wall_noise_amp", "get_wall_noise_amp");

	CL_BIND(float, wall_noise_freq)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_noise_freq"), "set_wall_noise_freq", "get_wall_noise_freq");

	CL_BIND(float, warp_strength)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "warp_strength"), "set_warp_strength", "get_warp_strength");

	CL_BIND(float, top_noise_amp)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "top_noise_amp"), "set_top_noise_amp", "get_top_noise_amp");

	CL_BIND(float, slope_mask_steepness)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slope_mask_steepness"), "set_slope_mask_steepness", "get_slope_mask_steepness");

	CL_BIND(bool, flat_shaded)
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat_shaded"), "set_flat_shaded", "get_flat_shaded");

	CL_BIND(int, strata_count)
	ADD_PROPERTY(PropertyInfo(Variant::INT, "strata_count"), "set_strata_count", "get_strata_count");

	CL_BIND(float, strata_depth)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "strata_depth"), "set_strata_depth", "get_strata_depth");

	CL_BIND(Color, base_color)
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "base_color"), "set_base_color", "get_base_color");

	CL_BIND(Color, top_color)
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "top_color"), "set_top_color", "get_top_color");

	CL_BIND(Color, crevice_color)
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "crevice_color"), "set_crevice_color", "get_crevice_color");

	CL_BIND(Color, moss_color)
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "moss_color"), "set_moss_color", "get_moss_color");

	CL_BIND(float, moss_amount)
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "moss_amount"), "set_moss_amount", "get_moss_amount");

	ClassDB::bind_method(D_METHOD("rebuild"), &CliffMesh::rebuild);

	BIND_ENUM_CONSTANT(STYLE_FACETED_BLOCKS);
	BIND_ENUM_CONSTANT(STYLE_CREVASSED);
	BIND_ENUM_CONSTANT(STYLE_COLUMNAR_BASALT);
	BIND_ENUM_CONSTANT(STYLE_STRATIFIED);
}
#undef CL_BIND
// clang-format on

CliffMesh::CliffMesh() {
	_queue_rebuild();
}

CliffMesh::~CliffMesh() {}

void CliffMesh::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("_surfaces")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void CliffMesh::_queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

void CliffMesh::_generate_subdivided_box(
		std::vector<Vector3> &r_verts,
		std::vector<Vector3> &r_normals,
		std::vector<Vector2> &r_uvs,
		std::vector<int> &r_indices
) const {
	r_verts.clear();
	r_normals.clear();
	r_uvs.clear();
	r_indices.clear();

	const int nx = subdivisions_x;
	const int ny = subdivisions_y;
	const int nz = subdivisions_z;

	const int dim_x = nx + 1;
	const int dim_y = ny + 1;
	const int dim_z = nz + 1;

	const Vector3 h = extents * 0.5f;
	const float rx = corner_radius * Math::min(h.x, h.z);
	const float ry = edge_radius * Math::min(h.y, Math::min(h.x, h.z));

	std::vector<int> vert_map(dim_x * dim_y * dim_z, -1);

	auto get_map_idx = [=](int i, int j, int k) {
		return i * (dim_y * dim_z) + j * dim_z + k;
	};

	// 1. Generate unique surface vertices (shared across edges and corners so top and walls stay seamlessly connected)
	for (int i = 0; i <= nx; ++i) {
		float u = (float)i / (float)nx;
		float px = (u - 0.5f) * extents.x;
		float nx_dir = (i == nx ? 1.0f : (i == 0 ? -1.0f : 0.0f));

		for (int j = 0; j <= ny; ++j) {
			float v = (float)j / (float)ny;
			float py = (v - 0.5f) * extents.y;
			float ny_dir = (j == ny ? 1.0f : (j == 0 ? -1.0f : 0.0f));

			for (int k = 0; k <= nz; ++k) {
				// Skip interior points that are not on any box face
				if (i > 0 && i < nx && j > 0 && j < ny && k > 0 && k < nz) {
					continue;
				}

				float w = (float)k / (float)nz;
				float pz = (w - 0.5f) * extents.z;
				float nz_dir = (k == nz ? 1.0f : (k == 0 ? -1.0f : 0.0f));

				// Smooth rounding of horizontal corners and vertical edges to eliminate sharp wall seams
				float cx = Math::clamp(px, -h.x + rx, h.x - rx);
				float cz = Math::clamp(pz, -h.z + rx, h.z - rx);
				float cy = Math::clamp(py, -h.y + ry, h.y - ry);

				Vector2 delta_xz(px - cx, pz - cz);
				if (delta_xz.length_squared() > 1e-8f && rx > 1e-4f) {
					Vector2 n_xz = delta_xz.normalized();
					px = cx + n_xz.x * rx;
					pz = cz + n_xz.y * rx;
				}

				float delta_y = py - cy;
				if (Math::abs(delta_y) > 1e-4f && ry > 1e-4f) {
					float n_y = (delta_y > 0.0f ? 1.0f : -1.0f);
					py = cy + n_y * ry;
				}

				Vector3 norm = Vector3(px - cx, py - cy, pz - cz);
				if (norm.length_squared() > 1e-6f) {
					norm.normalize();
				} else {
					norm = Vector3(nx_dir, ny_dir, nz_dir);
					if (norm.length_squared() > 1e-6f) {
						norm.normalize();
					} else {
						norm = Vector3(0, 1, 0);
					}
				}

				int idx = (int)r_verts.size();
				vert_map[get_map_idx(i, j, k)] = idx;
				r_verts.push_back(Vector3(px, py, pz));
				r_normals.push_back(norm);
				r_uvs.push_back(Vector2(u, v));
			}
		}
	}

	auto add_quad = [&](int i0, int i1, int i2, int i3) {
		int a = vert_map[i0];
		int b = vert_map[i1];
		int c = vert_map[i2];
		int d = vert_map[i3];
		if (a < 0 || b < 0 || c < 0 || d < 0) {
			return;
		}
		// Clockwise winding when viewed from outside
		r_indices.push_back(a);
		r_indices.push_back(d);
		r_indices.push_back(c);

		r_indices.push_back(a);
		r_indices.push_back(c);
		r_indices.push_back(b);
	};

	// 2. Build 6 faces connecting the shared grid vertices
	// Front face (+Z): k = nz
	for (int j = 0; j < ny; ++j) {
		for (int i = 0; i < nx; ++i) {
			add_quad(
					get_map_idx(i, j, nz),
					get_map_idx(i + 1, j, nz),
					get_map_idx(i + 1, j + 1, nz),
					get_map_idx(i, j + 1, nz)
			);
		}
	}

	// Back face (-Z): k = 0
	for (int j = 0; j < ny; ++j) {
		for (int i = 0; i < nx; ++i) {
			add_quad(
					get_map_idx(i + 1, j, 0),
					get_map_idx(i, j, 0),
					get_map_idx(i, j + 1, 0),
					get_map_idx(i + 1, j + 1, 0)
			);
		}
	}

	// Right wall (+X): i = nx
	for (int j = 0; j < ny; ++j) {
		for (int k = 0; k < nz; ++k) {
			add_quad(
					get_map_idx(nx, j, k + 1),
					get_map_idx(nx, j, k),
					get_map_idx(nx, j + 1, k),
					get_map_idx(nx, j + 1, k + 1)
			);
		}
	}

	// Left wall (-X): i = 0
	for (int j = 0; j < ny; ++j) {
		for (int k = 0; k < nz; ++k) {
			add_quad(
					get_map_idx(0, j, k),
					get_map_idx(0, j, k + 1),
					get_map_idx(0, j + 1, k + 1),
					get_map_idx(0, j + 1, k)
			);
		}
	}

	// Top plateau (+Y): j = ny
	for (int k = 0; k < nz; ++k) {
		for (int i = 0; i < nx; ++i) {
			add_quad(
					get_map_idx(i, ny, k + 1),
					get_map_idx(i + 1, ny, k + 1),
					get_map_idx(i + 1, ny, k),
					get_map_idx(i, ny, k)
			);
		}
	}

	// Bottom face (-Y): j = 0
	for (int k = 0; k < nz; ++k) {
		for (int i = 0; i < nx; ++i) {
			add_quad(
					get_map_idx(i, 0, k),
					get_map_idx(i + 1, 0, k),
					get_map_idx(i + 1, 0, k + 1),
					get_map_idx(i, 0, k + 1)
			);
		}
	}
}

void CliffMesh::_apply_voronoi_displacement(
		std::vector<Vector3> &r_verts,
		std::vector<Vector3> &r_normals,
		std::vector<Color> &r_colors
) const {
	const size_t count = r_verts.size();
	r_colors.resize(count);
	const uint32_t seed_u = (uint32_t)seed;
	const float h_y = extents.y * 0.5f;

	for (size_t i = 0; i < count; ++i) {
		Vector3 p = r_verts[i];
		Vector3 n = r_normals[i];

		// Slope mask: 0 on top plateau (+Y normal), 1 on vertical cliff walls
		float wall_factor = 1.0f - Math::clamp(n.y, 0.0f, 1.0f);
		wall_factor = Math::pow(wall_factor, slope_mask_steepness);

		bool is_bottom = (p.y <= -h_y + 1e-4f);

		// Domain warp across all 3D coordinates
		Vector3 warp = Vector3(
				prop::value_noise(p * 0.15f + Vector3(0, 0, 0), seed_u + 11u),
				prop::value_noise(p * 0.15f + Vector3(5, 5, 5), seed_u + 23u),
				prop::value_noise(p * 0.15f + Vector3(9, 9, 9), seed_u + 37u)
		) * warp_strength;

		Vector3 sample_pos = p + warp;
		float displacement = 0.0f;
		float crevice_darkness = 0.0f;

		if (style == STYLE_COLUMNAR_BASALT) {
			Vector3 col_sample = Vector3(sample_pos.x, sample_pos.y * 0.05f, sample_pos.z) * wall_noise_freq;
			prop::VoronoiResult v = prop::voronoi_3d(col_sample, seed_u);
			displacement = (1.0f - v.f1 * 1.2f) * wall_noise_amp;
			crevice_darkness = 1.0f - v.crack;
		} else if (style == STYLE_STRATIFIED) {
			Vector3 strat_pos = Vector3(sample_pos.x * 0.5f, sample_pos.y * 2.0f, sample_pos.z * 0.5f) * wall_noise_freq;
			prop::VoronoiResult v = prop::voronoi_3d(strat_pos, seed_u);
			float strata_step = Math::sin(p.y * (float)strata_count * 0.5f);
			displacement = ((1.0f - v.f1) + strata_step * strata_depth) * wall_noise_amp;
			crevice_darkness = 1.0f - v.crack;
		} else if (style == STYLE_CREVASSED) {
			prop::VoronoiResult v = prop::voronoi_3d(sample_pos * wall_noise_freq, seed_u);
			displacement = (v.crack - 0.5f) * wall_noise_amp * 2.0f;
			crevice_darkness = 1.0f - v.crack;
		} else { // STYLE_FACETED_BLOCKS
			prop::VoronoiResult v = prop::voronoi_3d(sample_pos * wall_noise_freq, seed_u);
			float cell_d = (sample_pos * wall_noise_freq - v.cell_center).length();
			displacement = (0.8f - cell_d) * wall_noise_amp;
			crevice_darkness = 1.0f - v.crack;
		}

		float final_disp = Math::lerp(
				prop::value_noise(p * 0.4f, seed_u) * top_noise_amp,
				displacement,
				wall_factor
		);

		if (is_bottom) {
			final_disp = 0.0f;
		}

		r_verts[i] += n * final_disp;

		// Colors: height blend + crevice darkening + moss
		float height_t = Math::clamp((p.y + h_y) / extents.y, 0.0f, 1.0f);
		Color vert_col = base_color.lerp(top_color, height_t);
		vert_col = vert_col.lerp(crevice_color, crevice_darkness * 0.7f);

		if (n.y > 0.3f && moss_amount > 0.0f) {
			float moss_factor = Math::clamp((n.y - 0.3f) / 0.7f, 0.0f, 1.0f) * moss_amount;
			vert_col = vert_col.lerp(moss_color, moss_factor);
		}
		r_colors[i] = vert_col;
	}
}

void CliffMesh::rebuild() {
	_rebuild_queued = false;
	clear_surfaces();

	std::vector<Vector3> verts;
	std::vector<Vector3> normals;
	std::vector<Vector2> uvs;
	std::vector<int> indices;
	std::vector<Color> colors;

	_generate_subdivided_box(verts, normals, uvs, indices);
	_apply_voronoi_displacement(verts, normals, colors);

	PackedVector3Array p_verts;
	PackedVector3Array p_normals;
	PackedVector2Array p_uvs;
	PackedColorArray p_colors;
	PackedInt32Array p_indices;
	_collision_faces.clear();

	if (flat_shaded) {
		const size_t tri_count = indices.size() / 3;
		p_verts.resize((int)tri_count * 3);
		p_normals.resize((int)tri_count * 3);
		p_uvs.resize((int)tri_count * 3);
		p_colors.resize((int)tri_count * 3);
		p_indices.resize((int)tri_count * 3);
		_collision_faces.resize((int)tri_count * 3);

		for (size_t t = 0; t < tri_count; ++t) {
			int i0 = indices[t * 3 + 0];
			int i1 = indices[t * 3 + 1];
			int i2 = indices[t * 3 + 2];

			Vector3 v0 = verts[i0], v1 = verts[i1], v2 = verts[i2];
			// Outward pointing normal for CW winding
			Vector3 face_normal = (v2 - v0).cross(v1 - v0).normalized();

			for (int k = 0; k < 3; ++k) {
				int src_idx = indices[t * 3 + k];
				int dst_idx = (int)(t * 3 + k);
				Vector3 v = verts[src_idx];
				p_verts[dst_idx] = v;
				p_normals[dst_idx] = face_normal;
				p_uvs[dst_idx] = uvs[src_idx];
				p_colors[dst_idx] = colors[src_idx];
				p_indices[dst_idx] = dst_idx;
				_collision_faces[dst_idx] = v;
			}
		}
	} else {
		std::vector<Vector3> smooth_normals(verts.size(), Vector3());
		for (size_t t = 0; t < indices.size(); t += 3) {
			int i0 = indices[t], i1 = indices[t + 1], i2 = indices[t + 2];
			// Outward pointing normal for CW winding
			Vector3 fn = (verts[i2] - verts[i0]).cross(verts[i1] - verts[i0]);
			smooth_normals[i0] += fn;
			smooth_normals[i1] += fn;
			smooth_normals[i2] += fn;
		}
		for (Vector3 &sn : smooth_normals) {
			if (sn.length_squared() > 1e-8f) {
				sn.normalize();
			} else {
				sn = Vector3(0, 1, 0);
			}
		}

		p_verts.resize((int)verts.size());
		p_normals.resize((int)verts.size());
		p_uvs.resize((int)verts.size());
		p_colors.resize((int)verts.size());
		for (size_t i = 0; i < verts.size(); ++i) {
			p_verts[(int)i] = verts[i];
			p_normals[(int)i] = smooth_normals[i];
			p_uvs[(int)i] = uvs[i];
			p_colors[(int)i] = colors[i];
		}
		p_indices.resize((int)indices.size());
		_collision_faces.resize((int)indices.size());
		for (size_t i = 0; i < indices.size(); ++i) {
			p_indices[(int)i] = indices[i];
			_collision_faces[(int)i] = verts[indices[i]];
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = p_verts;
	arrays[Mesh::ARRAY_NORMAL] = p_normals;
	arrays[Mesh::ARRAY_TEX_UV] = p_uvs;
	arrays[Mesh::ARRAY_COLOR] = p_colors;
	arrays[Mesh::ARRAY_INDEX] = p_indices;

	add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	emit_changed();
}

Ref<Shape3D> CliffMesh::create_collision_shape() const {
	if (_collision_faces.is_empty()) {
		return Ref<Shape3D>();
	}
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(_collision_faces);
	return shape;
}

} // namespace godot
