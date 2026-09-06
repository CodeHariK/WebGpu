/**
 * @file tr_cliff.cpp
 * @brief TerrainSplineCliff: bindings, properties, rebuild scheduling and the internal nodes.
 */
#include "tr_cliff.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define TR_CLIFF_BIND(m_variant, m_name, m_hint, m_hint_str)                                                           \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &TerrainSplineCliff::set_##m_name);                        \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &TerrainSplineCliff::get_##m_name);                                 \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void TerrainSplineCliff::_bind_methods() {
	ADD_GROUP("Shape", "");
	TR_CLIFF_BIND(FLOAT, height, PROPERTY_HINT_RANGE, "0.1,200,0.1,suffix:m");
	ClassDB::bind_method(D_METHOD("set_height_curve", "curve"), &TerrainSplineCliff::set_height_curve);
	ClassDB::bind_method(D_METHOD("get_height_curve"), &TerrainSplineCliff::get_height_curve);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "height_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_height_curve",
			"get_height_curve"
	);
	TR_CLIFF_BIND(FLOAT, base_offset, PROPERTY_HINT_RANGE, "-50,50,0.1,suffix:m");
	TR_CLIFF_BIND(BOOL, rim_from_deformer, PROPERTY_HINT_NONE, "");
	TR_CLIFF_BIND(BOOL, flip_side, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("set_profile_curve", "curve"), &TerrainSplineCliff::set_profile_curve);
	ClassDB::bind_method(D_METHOD("get_profile_curve"), &TerrainSplineCliff::get_profile_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "profile_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_profile_curve", "get_profile_curve");
	TR_CLIFF_BIND(FLOAT, profile_amount, PROPERTY_HINT_RANGE, "-50,50,0.1,suffix:m");
	TR_CLIFF_BIND(FLOAT, segment_length, PROPERTY_HINT_RANGE, "0.5,20,0.1,suffix:m");
	ClassDB::bind_method(D_METHOD("set_bottom_mode", "mode"), &TerrainSplineCliff::set_bottom_mode);
	ClassDB::bind_method(D_METHOD("get_bottom_mode"), &TerrainSplineCliff::get_bottom_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bottom_mode", PROPERTY_HINT_ENUM, "Relative (height below top),Absolute (bottom_y level),Ground (raycast)"), "set_bottom_mode", "get_bottom_mode");
	TR_CLIFF_BIND(FLOAT, bottom_y, PROPERTY_HINT_NONE, "suffix:m");
	TR_CLIFF_BIND(BOOL, cap_ends, PROPERTY_HINT_NONE, "");
	TR_CLIFF_BIND(BOOL, cap_top, PROPERTY_HINT_NONE, "");
	TR_CLIFF_BIND(FLOAT, cap_resolution, PROPERTY_HINT_RANGE, "0.5,50,0.5,suffix:m");
	TR_CLIFF_BIND(FLOAT, cap_dome, PROPERTY_HINT_RANGE, "-20,20,0.1,suffix:m");
	TR_CLIFF_BIND(INT, cap_smoothing, PROPERTY_HINT_RANGE, "0,500,1");
	TR_CLIFF_BIND(INT, seed, PROPERTY_HINT_NONE, "");

	ADD_GROUP("Strata", "");
	TR_CLIFF_BIND(INT, strata, PROPERTY_HINT_RANGE, "1,32,1");
	TR_CLIFF_BIND(FLOAT, strata_variation, PROPERTY_HINT_RANGE, "0,1,0.01");
	TR_CLIFF_BIND(FLOAT, step_out, PROPERTY_HINT_RANGE, "0,10,0.05,suffix:m");
	TR_CLIFF_BIND(FLOAT, ledge_chance, PROPERTY_HINT_RANGE, "0,1,0.01");
	TR_CLIFF_BIND(FLOAT, ledge_depth, PROPERTY_HINT_RANGE, "0,10,0.05,suffix:m");
	TR_CLIFF_BIND(FLOAT, bevel, PROPERTY_HINT_RANGE, "0,3,0.01,suffix:m");
	TR_CLIFF_BIND(FLOAT, lip, PROPERTY_HINT_RANGE, "0,5,0.05,suffix:m");
	TR_CLIFF_BIND(FLOAT, skirt, PROPERTY_HINT_RANGE, "0,20,0.1,suffix:m");

	ADD_GROUP("Wobble", "noise_");
	TR_CLIFF_BIND(FLOAT, noise_amplitude, PROPERTY_HINT_RANGE, "0,5,0.05,suffix:m");
	TR_CLIFF_BIND(FLOAT, noise_frequency, PROPERTY_HINT_RANGE, "0,1,0.005");
	TR_CLIFF_BIND(FLOAT, noise_quantize, PROPERTY_HINT_RANGE, "0,2,0.05,suffix:m");
	TR_CLIFF_BIND(FLOAT, column_width, PROPERTY_HINT_RANGE, "0,50,0.5,suffix:m");
	TR_CLIFF_BIND(FLOAT, column_coherence, PROPERTY_HINT_RANGE, "0,1,0.01");
	TR_CLIFF_BIND(FLOAT, cleft_depth, PROPERTY_HINT_RANGE, "0,10,0.05,suffix:m");
	TR_CLIFF_BIND(FLOAT, cleft_width, PROPERTY_HINT_RANGE, "0,20,0.1,suffix:m");
	TR_CLIFF_BIND(FLOAT, cleft_shade, PROPERTY_HINT_RANGE, "0,1,0.01");
	TR_CLIFF_BIND(FLOAT, column_shade, PROPERTY_HINT_RANGE, "0,1,0.01");
	TR_CLIFF_BIND(FLOAT, talus_start, PROPERTY_HINT_RANGE, "0,1,0.01");

	ADD_GROUP("Look", "");
	ClassDB::bind_method(D_METHOD("set_colors", "gradient"), &TerrainSplineCliff::set_colors);
	ClassDB::bind_method(D_METHOD("get_colors"), &TerrainSplineCliff::get_colors);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "colors", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_colors", "get_colors"
	);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &TerrainSplineCliff::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &TerrainSplineCliff::get_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material",
			"get_material"
	);
	TR_CLIFF_BIND(COLOR, top_color, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("set_top_material", "material"), &TerrainSplineCliff::set_top_material);
	ClassDB::bind_method(D_METHOD("get_top_material"), &TerrainSplineCliff::get_top_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "top_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_top_material", "get_top_material");
	TR_CLIFF_BIND(BOOL, color_by_depth, PROPERTY_HINT_NONE, "");
	TR_CLIFF_BIND(BOOL, collision_enabled, PROPERTY_HINT_NONE, "");

	BIND_ENUM_CONSTANT(BOTTOM_RELATIVE);
	BIND_ENUM_CONSTANT(BOTTOM_ABSOLUTE);
	BIND_ENUM_CONSTANT(BOTTOM_GROUND);

	ClassDB::bind_method(D_METHOD("rebuild"), &TerrainSplineCliff::rebuild);
	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineCliff::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineCliff::_on_spline_changed);
	ClassDB::bind_method(D_METHOD("_on_resource_changed"), &TerrainSplineCliff::_on_resource_changed);
}
#undef TR_CLIFF_BIND
// clang-format on

TerrainSplineCliff::TerrainSplineCliff() {
	_noise.instantiate();
	_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	_noise->set_fractal_type(FastNoiseLite::FRACTAL_NONE);
}

TerrainSplineCliff::~TerrainSplineCliff() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void TerrainSplineCliff::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (colors.is_null()) {
				_make_default_colors();
			}
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

void TerrainSplineCliff::_connect_spline() {
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

void TerrainSplineCliff::_disconnect_spline() {
	if (_watched_spline) {
		const Callable cb(this, "_on_spline_changed");
		if (_watched_spline->is_connected("spline_changed", cb)) {
			_watched_spline->disconnect("spline_changed", cb);
		}
		_watched_spline = nullptr;
	}
}

void TerrainSplineCliff::_on_spline_changed() { queue_rebuild(); }

void TerrainSplineCliff::_on_resource_changed() { queue_rebuild(); }

void TerrainSplineCliff::queue_rebuild() {
	if (!_rebuild_queued && is_inside_tree()) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

// ---------------------------------------------------------------------------------------------
// Resource properties
// ---------------------------------------------------------------------------------------------

static void swap_changed_listener(
		Ref<Resource> &r_slot,
		const Ref<Resource> &p_new,
		Object *p_listener
) {
	const Callable cb(p_listener, "_on_resource_changed");
	if (r_slot.is_valid() && r_slot->is_connected("changed", cb)) {
		r_slot->disconnect("changed", cb);
	}
	r_slot = p_new;
	if (r_slot.is_valid()) {
		r_slot->connect("changed", cb);
	}
}

void TerrainSplineCliff::set_height_curve(const Ref<Curve> &p_curve) {
	Ref<Resource> slot = height_curve;
	swap_changed_listener(slot, p_curve, this);
	height_curve = slot;
	queue_rebuild();
}

void TerrainSplineCliff::set_profile_curve(const Ref<Curve> &p_curve) {
	Ref<Resource> slot = profile_curve;
	swap_changed_listener(slot, p_curve, this);
	profile_curve = slot;
	queue_rebuild();
}

void TerrainSplineCliff::set_colors(const Ref<Gradient> &p_colors) {
	Ref<Resource> slot = colors;
	swap_changed_listener(slot, p_colors, this);
	colors = slot;
	queue_rebuild();
}

void TerrainSplineCliff::set_material(const Ref<Material> &p_material) {
	material = p_material;
	_apply_materials();
}

void TerrainSplineCliff::set_top_material(const Ref<Material> &p_material) {
	top_material = p_material;
	_apply_materials();
}

/// Surface 0 = wall, surface 1 = top cap. Unset slots fall back to vertex-colour materials.
void TerrainSplineCliff::_apply_materials() {
	if (!mesh_instance || mesh_instance->get_mesh().is_null()) {
		return;
	}
	if (_fallback_material.is_null()) {
		Ref<StandardMaterial3D> m;
		m.instantiate();
		m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		m->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		m->set_roughness(1.0f);
		_fallback_material = m;
	}
	const Ref<Material> &fallback = _fallback_material;
	const int surfaces = mesh_instance->get_mesh()->get_surface_count();
	if (surfaces > 0) {
		mesh_instance->set_surface_override_material(0, material.is_valid() ? material : fallback);
	}
	if (surfaces > 1) {
		mesh_instance->set_surface_override_material(1, top_material.is_valid() ? top_material : fallback);
	}
}

/// Warm sandstone bands, light at the top: a reasonable start for a toon cliff.
void TerrainSplineCliff::_make_default_colors() {
	Ref<Gradient> g;
	g.instantiate();
	PackedFloat32Array offsets;
	PackedColorArray cols;
	offsets.push_back(0.0f);
	cols.push_back(Color(0.93f, 0.80f, 0.58f));
	offsets.push_back(0.5f);
	cols.push_back(Color(0.80f, 0.60f, 0.42f));
	offsets.push_back(1.0f);
	cols.push_back(Color(0.58f, 0.40f, 0.32f));
	g->set_offsets(offsets);
	g->set_colors(cols);
	g->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_CONSTANT);
	set_colors(g);
}

// ---------------------------------------------------------------------------------------------
// Rebuild
// ---------------------------------------------------------------------------------------------

/// Unowned internal children: rebuilt on every load, never saved.
void TerrainSplineCliff::_ensure_nodes() {
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("CliffMesh");
		add_child(mesh_instance, false, INTERNAL_MODE_BACK);
	}
}

void TerrainSplineCliff::_update_collision(const Ref<ArrayMesh> &p_mesh) {
	const bool want = collision_enabled && p_mesh.is_valid() && p_mesh->get_surface_count() > 0;
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
		static_body->set_name("CliffBody");
		add_child(static_body, false, INTERNAL_MODE_BACK);
		collision_shape = memnew(CollisionShape3D);
		collision_shape->set_name("CliffShape");
		static_body->add_child(collision_shape, false, INTERNAL_MODE_BACK);
	}
	collision_shape->set_shape(p_mesh->create_trimesh_shape());
}

void TerrainSplineCliff::rebuild() {
	_rebuild_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	_ensure_nodes();

	std::vector<Station> stations;
	bool closed = false;
	Ref<ArrayMesh> mesh;
	if (_build_stations(stations, closed) && stations.size() >= 2) {
		const float total = stations.back().distance;
		std::vector<std::vector<ProfilePoint>> profiles(stations.size());
		for (size_t i = 0; i < stations.size(); ++i) {
			_build_profile(stations[i], total, closed, profiles[i]);
		}
		mesh = _build_mesh(stations, profiles, closed);
	}
	mesh_instance->set_mesh(mesh);
	_apply_materials();
	_update_collision(mesh);
}

} // namespace godot
