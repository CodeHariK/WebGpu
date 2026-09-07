/**
 * @file tr_road.cpp
 * @brief TerrainSplineRoad: bindings, properties, rebuild scheduling and the internal nodes.
 */
#include "tr_road.h"
#include "tr_toon.h"
#include "tr_water.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define TR_ROAD_BIND(m_variant, m_name, m_hint, m_hint_str)                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &TerrainSplineRoad::set_##m_name);              \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &TerrainSplineRoad::get_##m_name);                       \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void TerrainSplineRoad::_bind_methods() {
	ADD_GROUP("Cross Section", "");
	TR_ROAD_BIND(INT, profile, PROPERTY_HINT_ENUM, "Slab,Slab with rails,Half pipe,Custom (cross_section),Water");
	TR_ROAD_BIND(FLOAT, width, PROPERTY_HINT_RANGE, "0.2,100,0.1,suffix:m");
	TR_ROAD_BIND(FLOAT, thickness, PROPERTY_HINT_RANGE, "0.02,10,0.02,suffix:m");
	TR_ROAD_BIND(FLOAT, edge_radius, PROPERTY_HINT_RANGE, "0,3,0.01,suffix:m");
	TR_ROAD_BIND(FLOAT, rail_height, PROPERTY_HINT_RANGE, "0,5,0.05,suffix:m");
	TR_ROAD_BIND(FLOAT, rail_width, PROPERTY_HINT_RANGE, "0.05,3,0.05,suffix:m");
	TR_ROAD_BIND(FLOAT, pipe_depth, PROPERTY_HINT_RANGE, "0,30,0.1,suffix:m");
	TR_ROAD_BIND(INT, pipe_segments, PROPERTY_HINT_RANGE, "2,64,1");
	ClassDB::bind_method(D_METHOD("set_cross_section", "curve"), &TerrainSplineRoad::set_cross_section);
	ClassDB::bind_method(D_METHOD("get_cross_section"), &TerrainSplineRoad::get_cross_section);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "cross_section", PROPERTY_HINT_RESOURCE_TYPE, "Curve2D"), "set_cross_section", "get_cross_section");

	ADD_GROUP("Along Spline", "");
	TR_ROAD_BIND(INT, height_source, PROPERTY_HINT_ENUM, "Spline (floating; spline Y and tilt are the road),Terrain (follow a sibling deformer's profile)");
	TR_ROAD_BIND(INT, sampling, PROPERTY_HINT_ENUM, "Fixed (segment_length),Adaptive (by angle)");
	TR_ROAD_BIND(FLOAT, segment_length, PROPERTY_HINT_RANGE, "0.25,50,0.25,suffix:m");
	TR_ROAD_BIND(FLOAT, adaptive_max_step, PROPERTY_HINT_RANGE, "0.1,50,0.1,suffix:m");
	TR_ROAD_BIND(FLOAT, adaptive_min_step, PROPERTY_HINT_RANGE, "0.05,10,0.05,suffix:m");
	TR_ROAD_BIND(FLOAT, adaptive_angle_tol, PROPERTY_HINT_RANGE, "0.1,90,0.1,suffix:°");
	TR_ROAD_BIND(FLOAT, section_start, PROPERTY_HINT_RANGE, "0,100000,0.5,suffix:m");
	TR_ROAD_BIND(FLOAT, section_end, PROPERTY_HINT_RANGE, "0,100000,0.5,suffix:m");
	TR_ROAD_BIND(BOOL, cap_ends, PROPERTY_HINT_NONE, "");
	TR_ROAD_BIND(FLOAT, surface_offset, PROPERTY_HINT_RANGE, "-5,5,0.01,suffix:m");

	ADD_GROUP("Look", "");
	TR_ROAD_BIND(FLOAT, texture_length, PROPERTY_HINT_RANGE, "0.1,200,0.1,suffix:m");
	TR_ROAD_BIND(COLOR, deck_color, PROPERTY_HINT_NONE, "");
	TR_ROAD_BIND(COLOR, edge_color, PROPERTY_HINT_NONE, "");
	TR_ROAD_BIND(COLOR, rail_color, PROPERTY_HINT_NONE, "");
	TR_ROAD_BIND(COLOR, underside_color, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("set_material", "material"), &TerrainSplineRoad::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &TerrainSplineRoad::get_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
	TR_ROAD_BIND(BOOL, collision_enabled, PROPERTY_HINT_NONE, "");
	ADD_GROUP("Water", "water_");
	TR_ROAD_BIND(FLOAT, water_speed, PROPERTY_HINT_RANGE, "-10,10,0.05,suffix:m/s");
	TR_ROAD_BIND(FLOAT, water_alpha, PROPERTY_HINT_RANGE, "0,1,0.01");
	ADD_GROUP("Markings", "marking_");
	TR_ROAD_BIND(INT, marking_lanes, PROPERTY_HINT_RANGE, "0,8,1");
	TR_ROAD_BIND(BOOL, marking_dashed, PROPERTY_HINT_NONE, "");
	TR_ROAD_BIND(BOOL, marking_edges, PROPERTY_HINT_NONE, "");
	TR_ROAD_BIND(FLOAT, marking_width, PROPERTY_HINT_RANGE, "0.01,3,0.01,suffix:m");
	TR_ROAD_BIND(FLOAT, marking_dash, PROPERTY_HINT_RANGE, "0.1,50,0.1,suffix:m");
	TR_ROAD_BIND(FLOAT, marking_edge_inset, PROPERTY_HINT_RANGE, "0,10,0.05,suffix:m");
	TR_ROAD_BIND(COLOR, marking_color, PROPERTY_HINT_NONE, "");

	BIND_ENUM_CONSTANT(PROFILE_SLAB);
	BIND_ENUM_CONSTANT(PROFILE_SLAB_RAILS);
	BIND_ENUM_CONSTANT(PROFILE_HALF_PIPE);
	BIND_ENUM_CONSTANT(PROFILE_CUSTOM);
	BIND_ENUM_CONSTANT(PROFILE_WATER);
	BIND_ENUM_CONSTANT(SAMPLING_FIXED);
	BIND_ENUM_CONSTANT(SAMPLING_ADAPTIVE);
	BIND_ENUM_CONSTANT(HEIGHT_SPLINE);
	BIND_ENUM_CONSTANT(HEIGHT_TERRAIN);

	ClassDB::bind_method(D_METHOD("get_water_area"), &TerrainSplineRoad::get_water_area);
	ClassDB::bind_method(D_METHOD("rebuild"), &TerrainSplineRoad::rebuild);
	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineRoad::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineRoad::_on_spline_changed);
	ClassDB::bind_method(D_METHOD("_on_resource_changed"), &TerrainSplineRoad::_on_resource_changed);
}
#undef TR_ROAD_BIND
// clang-format on

TerrainSplineRoad::TerrainSplineRoad() {}
TerrainSplineRoad::~TerrainSplineRoad() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void TerrainSplineRoad::_notification(int p_what) {
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

void TerrainSplineRoad::_connect_spline() {
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

void TerrainSplineRoad::_disconnect_spline() {
	if (_watched_spline) {
		const Callable cb(this, "_on_spline_changed");
		if (_watched_spline->is_connected("spline_changed", cb)) {
			_watched_spline->disconnect("spline_changed", cb);
		}
		_watched_spline = nullptr;
	}
}

void TerrainSplineRoad::_on_spline_changed() { queue_rebuild(); }

void TerrainSplineRoad::_on_resource_changed() { queue_rebuild(); }

void TerrainSplineRoad::queue_rebuild() {
	if (!_rebuild_queued && is_inside_tree()) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

// ---------------------------------------------------------------------------------------------
// Resource properties
// ---------------------------------------------------------------------------------------------

void TerrainSplineRoad::set_cross_section(const Ref<Curve2D> &p_curve) {
	const Callable cb(this, "_on_resource_changed");
	if (cross_section.is_valid() && cross_section->is_connected("changed", cb)) {
		cross_section->disconnect("changed", cb);
	}
	cross_section = p_curve;
	if (cross_section.is_valid()) {
		cross_section->connect("changed", cb);
	}
	queue_rebuild();
}

void TerrainSplineRoad::set_material(const Ref<Material> &p_material) {
	material = p_material;
	_apply_material();
}

// ---------------------------------------------------------------------------------------------
// Rebuild
// ---------------------------------------------------------------------------------------------

/// Unowned internal children: rebuilt on every load, never saved.
void TerrainSplineRoad::_ensure_nodes() {
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("RoadMesh");
		add_child(mesh_instance, false, INTERNAL_MODE_BACK);
	}
}

/// `material` if set; else the water shader for WATER, else the toon road material with the marking uniforms.
void TerrainSplineRoad::_apply_material() {
	if (!mesh_instance) {
		return;
	}
	if (material.is_valid()) {
		mesh_instance->set_material_override(material);
		return;
	}
	if (profile == PROFILE_WATER) {
		if (_water_material.is_null()) {
			_water_material = make_toon_water_material();
		}
		_water_material->set_shader_parameter("speed", water_speed);
		_water_material->set_shader_parameter("alpha", water_alpha);
		_water_material->set_shader_parameter("foam_color", edge_color);
		mesh_instance->set_material_override(_water_material);
		return;
	}
	if (_fallback_material.is_null()) {
		_fallback_material = make_toon_road_material();
	}
	Ref<ShaderMaterial> road_mat = _fallback_material;
	road_mat->set_shader_parameter("deck_width", _deck_width);
	road_mat->set_shader_parameter("texture_length", texture_length);
	road_mat->set_shader_parameter("lanes", marking_lanes);
	road_mat->set_shader_parameter("dashed", marking_dashed);
	road_mat->set_shader_parameter("edge_lines", marking_edges);
	road_mat->set_shader_parameter("line_width", marking_width);
	road_mat->set_shader_parameter("dash_length", marking_dash);
	road_mat->set_shader_parameter("edge_inset", marking_edge_inset);
	road_mat->set_shader_parameter("line_color", marking_color);
	mesh_instance->set_material_override(_fallback_material);
}

/// WATER: an Area3D (group "water") over the water volume so gameplay can react to entering it.
void TerrainSplineRoad::_update_water_area(const Ref<ArrayMesh> &p_mesh) {
	const bool want = profile == PROFILE_WATER && p_mesh.is_valid() && p_mesh->get_surface_count() > 0;
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
	water_shape->set_shape(p_mesh->create_trimesh_shape());
}

void TerrainSplineRoad::_update_collision(const Ref<ArrayMesh> &p_mesh) {
	const bool want =
			collision_enabled && profile != PROFILE_WATER && p_mesh.is_valid() && p_mesh->get_surface_count() > 0;
	if (!want) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}
	if (!static_body) {
		static_body = memnew(StaticBody3D);
		static_body->set_name("RoadBody");
		add_child(static_body, false, INTERNAL_MODE_BACK);
		collision_shape = memnew(CollisionShape3D);
		collision_shape->set_name("RoadShape");
		static_body->add_child(collision_shape, false, INTERNAL_MODE_BACK);
	}
	collision_shape->set_shape(p_mesh->create_trimesh_shape());
}

void TerrainSplineRoad::rebuild() {
	_rebuild_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	_ensure_nodes();

	Ref<ArrayMesh> mesh;
	std::vector<Transform3D> stations;
	std::vector<float> distances;
	bool loop = false;
	if (_build_stations(stations, distances, loop) && stations.size() >= 2) {
		std::vector<ProfilePoint> profile;
		bool closed = false;
		_build_profile(profile, closed);
		if (profile.size() >= 2) {
			mesh = _build_mesh(stations, distances, loop, profile, closed);
		}
	}
	mesh_instance->set_mesh(mesh);
	_apply_material();
	_update_collision(mesh);
	_update_water_area(mesh);
}

} // namespace godot
