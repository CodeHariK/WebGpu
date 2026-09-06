/**
 * @file tr_lake.cpp
 * @brief TerrainSplineLake: rim sampling, flat triangulated surface, Area3D, material.
 */
#include "tr_lake.h"
#include "tr_water.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/geometry2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// clang-format off
#define TR_LAKE_BIND(m_variant, m_name, m_hint, m_hint_str)                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &TerrainSplineLake::set_##m_name);              \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &TerrainSplineLake::get_##m_name);                       \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void TerrainSplineLake::_bind_methods() {
	ADD_GROUP("Level", "");
	TR_LAKE_BIND(BOOL, level_from_spline, PROPERTY_HINT_NONE, "");
	TR_LAKE_BIND(FLOAT, level_offset, PROPERTY_HINT_RANGE, "-20,20,0.05,suffix:m");
	TR_LAKE_BIND(FLOAT, water_level, PROPERTY_HINT_RANGE, "-1000,1000,0.1,suffix:m");
	TR_LAKE_BIND(FLOAT, depth, PROPERTY_HINT_RANGE, "0.1,100,0.1,suffix:m");
	ADD_GROUP("Shape", "");
	TR_LAKE_BIND(FLOAT, shore_offset, PROPERTY_HINT_RANGE, "-50,50,0.1,suffix:m");
	TR_LAKE_BIND(FLOAT, segment_length, PROPERTY_HINT_RANGE, "0.5,50,0.5,suffix:m");
	TR_LAKE_BIND(FLOAT, resolution, PROPERTY_HINT_RANGE, "0.5,50,0.5,suffix:m");
	ADD_GROUP("Look", "");
	TR_LAKE_BIND(FLOAT, texture_scale, PROPERTY_HINT_RANGE, "0.1,200,0.1,suffix:m");
	TR_LAKE_BIND(COLOR, water_color, PROPERTY_HINT_NONE, "");
	TR_LAKE_BIND(COLOR, foam_color, PROPERTY_HINT_NONE, "");
	TR_LAKE_BIND(FLOAT, water_speed, PROPERTY_HINT_RANGE, "-5,5,0.05");
	TR_LAKE_BIND(FLOAT, water_alpha, PROPERTY_HINT_RANGE, "0,1,0.01");
	ClassDB::bind_method(D_METHOD("set_material", "material"), &TerrainSplineLake::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &TerrainSplineLake::get_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");

	ClassDB::bind_method(D_METHOD("get_water_area"), &TerrainSplineLake::get_water_area);
	ClassDB::bind_method(D_METHOD("rebuild"), &TerrainSplineLake::rebuild);
	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineLake::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineLake::_on_spline_changed);
}
#undef TR_LAKE_BIND
// clang-format on

TerrainSplineLake::TerrainSplineLake() {}
TerrainSplineLake::~TerrainSplineLake() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void TerrainSplineLake::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			_connect_spline();
			queue_rebuild();
			break;
		case NOTIFICATION_PARENTED:
			if (is_inside_tree()) {
				_connect_spline();
				queue_rebuild();
			}
			break;
		case NOTIFICATION_UNPARENTED:
		case NOTIFICATION_EXIT_TREE:
			_disconnect_spline();
			break;
		default:
			break;
	}
}

void TerrainSplineLake::_connect_spline() {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (spline == _watched_spline) {
		return;
	}
	_disconnect_spline();
	if (spline) {
		spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
		_watched_spline = spline;
	}
}

void TerrainSplineLake::_disconnect_spline() {
	if (_watched_spline) {
		const Callable cb(this, "_on_spline_changed");
		if (_watched_spline->is_connected("spline_changed", cb)) {
			_watched_spline->disconnect("spline_changed", cb);
		}
		_watched_spline = nullptr;
	}
}

void TerrainSplineLake::_on_spline_changed() { queue_rebuild(); }

void TerrainSplineLake::queue_rebuild() {
	if (!_rebuild_queued && is_inside_tree()) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

void TerrainSplineLake::set_material(const Ref<Material> &p_material) {
	material = p_material;
	_apply_material();
}

// ---------------------------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------------------------

/// Rim polygon (local space) every segment_length along the closed curve, grown by shore_offset, and the
/// water level: the spline's mean Y + level_offset, or the absolute water_level.
bool TerrainSplineLake::_rim_points(
		std::vector<Vector3> &r_rim,
		float &r_level
) const {
	r_rim.clear();
	const ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline) {
		return false;
	}
	Ref<Curve3D> curve = spline->get_curve();
	if (curve.is_null() || curve->get_point_count() < 3 || !curve->is_closed()) {
		UtilityFunctions::push_warning("[TerrainSplineLake] needs a closed spline with at least 3 points.");
		return false;
	}
	const Transform3D to_local = get_global_transform().affine_inverse() * spline->get_global_transform();
	const float total = curve->get_baked_length();
	const int n = MAX(3, (int)Math::round(total / segment_length));
	float y_sum = 0.0f;
	for (int i = 0; i < n; ++i) {
		const Vector3 p = to_local.xform(curve->sample_baked(total * i / n));
		r_rim.push_back(p);
		y_sum += p.y;
	}
	// Grow / shrink: push each rim point along the outward normal of the polygon at that point.
	if (Math::abs(shore_offset) > 1e-4f) {
		float area2 = 0.0f;
		for (int i = 0, j = n - 1; i < n; j = i++) {
			area2 += r_rim[j].x * r_rim[i].z - r_rim[i].x * r_rim[j].z;
		}
		const float ccw = area2 > 0.0f ? 1.0f : -1.0f; // In (x, z) with y up, this sign is what it is
		std::vector<Vector3> grown(n);
		for (int i = 0; i < n; ++i) {
			const Vector3 &a = r_rim[(i + n - 1) % n], &b = r_rim[(i + 1) % n];
			Vector2 t(b.x - a.x, b.z - a.z);
			t.normalize();
			const Vector2 nrm(t.y * ccw, -t.x * ccw);
			grown[i] = r_rim[i] + Vector3(nrm.x, 0, nrm.y) * shore_offset;
		}
		r_rim.swap(grown);
	}
	if (level_from_spline) {
		r_level = y_sum / (float)n + level_offset;
	} else {
		r_level = get_global_transform().affine_inverse().xform(Vector3(0, water_level, 0)).y;
	}
	return true;
}

/// Flat surface at p_level: rim + interior grid samples, Delaunay clipped to the polygon, UVs in XZ.
Ref<ArrayMesh> TerrainSplineLake::_build_mesh(
		const std::vector<Vector3> &p_rim,
		float p_level
) const {
	std::vector<Vector3> rim(p_rim);
	PackedVector2Array poly;
	for (Vector3 &v : rim) {
		v.y = p_level;
		poly.push_back(Vector2(v.x, v.z));
	}
	std::vector<Vector2> pts;
	for (const Vector2 &p : poly) {
		pts.push_back(p);
	}
	Rect2 bounds(poly[0], Vector2());
	for (const Vector2 &p : poly) {
		bounds = bounds.expand(p);
	}
	Geometry2D *g = Geometry2D::get_singleton();
	for (float z = bounds.position.y; z <= bounds.get_end().y; z += resolution) {
		for (float x = bounds.position.x; x <= bounds.get_end().x; x += resolution) {
			const Vector2 p(x, z);
			if (!g->is_point_in_polygon(p, poly)) {
				continue;
			}
			bool near_rim = false;
			for (int i = 0, j = poly.size() - 1; i < poly.size() && !near_rim; j = i++) {
				near_rim = g->get_closest_point_to_segment(p, poly[j], poly[i]).distance_to(p) < resolution * 0.45f;
			}
			if (!near_rim) {
				pts.push_back(p);
			}
		}
	}
	PackedVector2Array flat;
	for (const Vector2 &p : pts) {
		flat.push_back(p);
	}
	const PackedInt32Array tris = g->triangulate_delaunay(flat);

	PackedVector3Array vertices, normals;
	PackedVector2Array uvs;
	PackedColorArray colors;
	PackedInt32Array indices;
	for (const Vector2 &p : pts) {
		vertices.push_back(Vector3(p.x, p_level, p.y));
		normals.push_back(Vector3(0, 1, 0));
		uvs.push_back(p / texture_scale);
		colors.push_back(water_color);
	}
	for (int t = 0; t + 2 < tris.size(); t += 3) {
		const Vector2 c = (pts[tris[t]] + pts[tris[t + 1]] + pts[tris[t + 2]]) / 3.0f;
		if (!g->is_point_in_polygon(c, poly)) {
			continue;
		}
		int i0 = tris[t], i1 = tris[t + 1], i2 = tris[t + 2];
		// Clockwise seen from above (Godot front face).
		const Vector2 e1 = pts[i1] - pts[i0], e2 = pts[i2] - pts[i0];
		if (e1.x * e2.y - e1.y * e2.x < 0.0f) {
			std::swap(i1, i2);
		}
		indices.push_back(i0);
		indices.push_back(i1);
		indices.push_back(i2);
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (indices.size() >= 3) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_COLOR] = colors;
		arrays[Mesh::ARRAY_INDEX] = indices;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	return mesh;
}

void TerrainSplineLake::_apply_material() {
	if (!mesh_instance) {
		return;
	}
	if (material.is_valid()) {
		mesh_instance->set_material_override(material);
		return;
	}
	if (_water_material.is_null()) {
		_water_material = make_toon_water_material();
	}
	_water_material->set_shader_parameter("speed", water_speed);
	_water_material->set_shader_parameter("alpha", water_alpha);
	_water_material->set_shader_parameter("foam_color", foam_color);
	_water_material->set_shader_parameter("bank_foam", 0.0f); // No lateral axis on a lake
	mesh_instance->set_material_override(_water_material);
}

void TerrainSplineLake::rebuild() {
	_rebuild_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("LakeMesh");
		add_child(mesh_instance, false, INTERNAL_MODE_BACK);
	}
	std::vector<Vector3> rim;
	float level = 0.0f;
	Ref<ArrayMesh> mesh;
	if (_rim_points(rim, level)) {
		mesh = _build_mesh(rim, level);
	}
	mesh_instance->set_mesh(mesh);
	_apply_material();

	// Area3D (group "water"): the surface sheet plus a copy `depth` below as one trimesh. Trimesh areas
	// report overlap when a body touches the triangles, so this catches anything crossing the surface or
	// sinking to the bed - which is what floating cars and characters do.
	const bool want = mesh.is_valid() && mesh->get_surface_count() > 0;
	if (!want) {
		if (water_area) {
			water_area->queue_free();
			water_area = nullptr;
			water_shape = nullptr;
		}
		return;
	}
	if (!water_area) {
		water_area = memnew(Area3D);
		water_area->set_name("WaterArea");
		water_area->add_to_group("water");
		add_child(water_area, false, INTERNAL_MODE_BACK);
		water_shape = memnew(CollisionShape3D);
		water_shape->set_name("WaterShape");
		water_area->add_child(water_shape, false, INTERNAL_MODE_BACK);
	}
	Ref<ArrayMesh> volume;
	volume.instantiate();
	Array top = mesh->surface_get_arrays(0);
	volume->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, top);
	PackedVector3Array bottom_v = top[Mesh::ARRAY_VERTEX];
	for (int i = 0; i < bottom_v.size(); ++i) {
		bottom_v[i].y -= depth;
	}
	Array bottom = top.duplicate();
	bottom[Mesh::ARRAY_VERTEX] = bottom_v;
	volume->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, bottom);
	water_shape->set_shape(volume->create_trimesh_shape());
}

} // namespace godot
