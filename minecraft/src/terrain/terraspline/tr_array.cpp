/**
 * @file tr_array.cpp
 * @brief TerrainSplineArray: bindings, properties, rebuild scheduling and the internal nodes.
 */
#include "tr_array.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define TR_ARRAY_BIND(m_variant, m_name, m_hint, m_hint_str)                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &TerrainSplineArray::set_##m_name);              \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &TerrainSplineArray::get_##m_name);                       \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void TerrainSplineArray::_bind_methods() {
	ADD_GROUP("What", "");
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &TerrainSplineArray::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &TerrainSplineArray::get_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
	ClassDB::bind_method(D_METHOD("set_collision_shape", "shape"), &TerrainSplineArray::set_collision_shape);
	ClassDB::bind_method(D_METHOD("get_collision_shape"), &TerrainSplineArray::get_collision_shape);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "collision_shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_collision_shape", "get_collision_shape");
	TR_ARRAY_BIND(BOOL, collision_enabled, PROPERTY_HINT_NONE, "");
	TR_ARRAY_BIND(BOOL, mesh_centered, PROPERTY_HINT_NONE, "");

	ADD_GROUP("Where", "");
	TR_ARRAY_BIND(FLOAT, spacing, PROPERTY_HINT_RANGE, "0.1,500,0.1,suffix:m");
	TR_ARRAY_BIND(FLOAT, start_offset, PROPERTY_HINT_RANGE, "0,500,0.1,suffix:m");
	TR_ARRAY_BIND(FLOAT, section_start, PROPERTY_HINT_RANGE, "0,100000,0.5,suffix:m");
	TR_ARRAY_BIND(FLOAT, section_end, PROPERTY_HINT_RANGE, "0,100000,0.5,suffix:m");
	TR_ARRAY_BIND(INT, side, PROPERTY_HINT_ENUM, "Center,Left,Right,Both");
	TR_ARRAY_BIND(FLOAT, lateral_offset, PROPERTY_HINT_RANGE, "-100,100,0.1,suffix:m");
	TR_ARRAY_BIND(FLOAT, vertical_offset, PROPERTY_HINT_RANGE, "-100,100,0.1,suffix:m");
	TR_ARRAY_BIND(BOOL, align_to_tangent, PROPERTY_HINT_NONE, "");
	TR_ARRAY_BIND(BOOL, follow_tilt, PROPERTY_HINT_NONE, "");
	TR_ARRAY_BIND(BOOL, face_inward, PROPERTY_HINT_NONE, "");
	TR_ARRAY_BIND(BOOL, stretch_to_ground, PROPERTY_HINT_NONE, "");
	TR_ARRAY_BIND(FLOAT, ground_max_distance, PROPERTY_HINT_RANGE, "1,5000,1,suffix:m");

	ADD_GROUP("Variation", "");
	TR_ARRAY_BIND(VECTOR3, scale, PROPERTY_HINT_NONE, "");
	TR_ARRAY_BIND(FLOAT, scale_jitter, PROPERTY_HINT_RANGE, "0,1,0.01");
	TR_ARRAY_BIND(FLOAT, yaw_jitter_deg, PROPERTY_HINT_RANGE, "0,180,1,suffix:°");
	TR_ARRAY_BIND(FLOAT, spacing_jitter, PROPERTY_HINT_RANGE, "0,0.9,0.01");
	TR_ARRAY_BIND(INT, seed, PROPERTY_HINT_NONE, "");

	BIND_ENUM_CONSTANT(SIDE_CENTER);
	BIND_ENUM_CONSTANT(SIDE_LEFT);
	BIND_ENUM_CONSTANT(SIDE_RIGHT);
	BIND_ENUM_CONSTANT(SIDE_BOTH);

	ClassDB::bind_method(D_METHOD("rebuild"), &TerrainSplineArray::rebuild);
	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineArray::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineArray::_on_spline_changed);
	ClassDB::bind_method(D_METHOD("_on_resource_changed"), &TerrainSplineArray::_on_resource_changed);
}
#undef TR_ARRAY_BIND
// clang-format on

TerrainSplineArray::TerrainSplineArray() {}
TerrainSplineArray::~TerrainSplineArray() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void TerrainSplineArray::_notification(int p_what) {
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

void TerrainSplineArray::_connect_spline() {
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

void TerrainSplineArray::_disconnect_spline() {
	if (_watched_spline) {
		const Callable cb(this, "_on_spline_changed");
		if (_watched_spline->is_connected("spline_changed", cb)) {
			_watched_spline->disconnect("spline_changed", cb);
		}
		_watched_spline = nullptr;
	}
}

void TerrainSplineArray::_on_spline_changed() { queue_rebuild(); }

void TerrainSplineArray::_on_resource_changed() { queue_rebuild(); }

void TerrainSplineArray::queue_rebuild() {
	if (!_rebuild_queued && is_inside_tree()) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

void TerrainSplineArray::set_mesh(const Ref<Mesh> &p_mesh) {
	const Callable cb(this, "_on_resource_changed");
	if (mesh.is_valid() && mesh->is_connected("changed", cb)) {
		mesh->disconnect("changed", cb);
	}
	mesh = p_mesh;
	if (mesh.is_valid()) {
		mesh->connect("changed", cb);
	}
	queue_rebuild();
}

void TerrainSplineArray::set_collision_shape(const Ref<Shape3D> &p_shape) {
	collision_shape = p_shape;
	queue_rebuild();
}

// ---------------------------------------------------------------------------------------------
// Rebuild
// ---------------------------------------------------------------------------------------------

void TerrainSplineArray::_ensure_nodes() {
	if (!mmi) {
		mmi = memnew(MultiMeshInstance3D);
		mmi->set_name("ArrayInstances");
		add_child(mmi, false, INTERNAL_MODE_BACK);
	}
}

/// One MultiMesh with every transform (12 floats per instance: the 3x4 transform by rows).
void TerrainSplineArray::_apply_transforms(const std::vector<Transform3D> &p_transforms) {
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(mesh);
	mm->set_instance_count((int)p_transforms.size());
	if (!p_transforms.empty()) {
		PackedFloat32Array buf;
		buf.resize((int64_t)p_transforms.size() * 12);
		float *p = buf.ptrw();
		for (size_t i = 0; i < p_transforms.size(); ++i) {
			const Transform3D &t = p_transforms[i];
			for (int r = 0; r < 3; ++r) {
				p[i * 12 + r * 4 + 0] = t.basis.rows[r].x;
				p[i * 12 + r * 4 + 1] = t.basis.rows[r].y;
				p[i * 12 + r * 4 + 2] = t.basis.rows[r].z;
				p[i * 12 + r * 4 + 3] = t.origin[r];
			}
		}
		mm->set_buffer(buf);
	}
	mmi->set_multimesh(mm);
}

/// A StaticBody3D with one CollisionShape3D per instance (hand-placed levels: a few hundred at most).
void TerrainSplineArray::_update_collision(const std::vector<Transform3D> &p_transforms) {
	if (static_body) {
		static_body->queue_free();
		static_body = nullptr;
	}
	if (!collision_enabled || collision_shape.is_null() || p_transforms.empty()) {
		return;
	}
	static_body = memnew(StaticBody3D);
	static_body->set_name("ArrayBody");
	add_child(static_body, false, INTERNAL_MODE_BACK);
	for (const Transform3D &t : p_transforms) {
		CollisionShape3D *cs = memnew(CollisionShape3D);
		cs->set_shape(collision_shape);
		static_body->add_child(cs, false, INTERNAL_MODE_BACK);
		cs->set_transform(t);
	}
}

void TerrainSplineArray::rebuild() {
	_rebuild_queued = false;
	if (!is_inside_tree()) {
		return;
	}
	_ensure_nodes();
	std::vector<Transform3D> transforms;
	_ground_missing = false;
	if (mesh.is_valid()) {
		_build_transforms(transforms);
	}
	_apply_transforms(transforms);
	_update_collision(transforms);

	// Under a streaming compositor the terrain arrives after we first build; poll for it briefly.
	if (stretch_to_ground && _ground_missing && _ground_retries < 20 && get_tree()) {
		++_ground_retries;
		Ref<SceneTreeTimer> timer = get_tree()->create_timer(0.5);
		timer->connect("timeout", Callable(this, "queue_rebuild"), CONNECT_ONE_SHOT);
	} else if (!_ground_missing) {
		_ground_retries = 0;
	}
}

} // namespace godot
