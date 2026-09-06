/**
 * @file race_track.cpp
 * @brief RaceTrack: bindings, lifecycle and the checkpoint gates along the spline.
 */
#include "race_track.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// clang-format off
#define RT_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &RaceTrack::set_##m_name);                                 \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &RaceTrack::get_##m_name);                                          \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void RaceTrack::_bind_methods() {
	ADD_GROUP("Course", "");
	RT_BIND(FLOAT, checkpoint_spacing, PROPERTY_HINT_RANGE, "5,500,1,suffix:m");
	RT_BIND(FLOAT, gate_width, PROPERTY_HINT_RANGE, "1,200,0.5,suffix:m");
	RT_BIND(FLOAT, gate_height, PROPERTY_HINT_RANGE, "0.5,50,0.5,suffix:m");
	RT_BIND(FLOAT, gate_depth, PROPERTY_HINT_RANGE, "0.2,20,0.1,suffix:m");
	RT_BIND(INT, laps, PROPERTY_HINT_RANGE, "0,99,1");
	RT_BIND(INT, body_mask, PROPERTY_HINT_LAYERS_3D_PHYSICS, "");
	RT_BIND(BOOL, show_gates, PROPERTY_HINT_NONE, "");

	ADD_GROUP("Rules", "");
	RT_BIND(FLOAT, wrong_way_angle, PROPERTY_HINT_RANGE, "0,180,1,suffix:°");
	RT_BIND(FLOAT, wrong_way_time, PROPERTY_HINT_RANGE, "0,10,0.1,suffix:s");
	RT_BIND(FLOAT, wrong_way_min_speed, PROPERTY_HINT_RANGE, "0,50,0.5,suffix:m/s");
	RT_BIND(FLOAT, kill_depth, PROPERTY_HINT_RANGE, "0,500,1,suffix:m");
	RT_BIND(BOOL, auto_respawn, PROPERTY_HINT_NONE, "");
	RT_BIND(FLOAT, respawn_height, PROPERTY_HINT_RANGE, "-5,20,0.1,suffix:m");
	RT_BIND(FLOAT, respawn_back, PROPERTY_HINT_RANGE, "0,50,0.5,suffix:m");

	ClassDB::bind_method(D_METHOD("rebuild"), &RaceTrack::rebuild);
	ClassDB::bind_method(D_METHOD("queue_rebuild"), &RaceTrack::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &RaceTrack::_on_spline_changed);
	ClassDB::bind_method(D_METHOD("_on_gate_body_entered", "body", "index"), &RaceTrack::_on_gate_body_entered);
	ClassDB::bind_method(D_METHOD("get_checkpoint_count"), &RaceTrack::get_checkpoint_count);
	ClassDB::bind_method(D_METHOD("get_checkpoint_transform", "index"), &RaceTrack::get_checkpoint_transform);
	ClassDB::bind_method(D_METHOD("track_body", "body"), &RaceTrack::track_body);
	ClassDB::bind_method(D_METHOD("untrack_body", "body"), &RaceTrack::untrack_body);
	ClassDB::bind_method(D_METHOD("get_next_checkpoint", "body"), &RaceTrack::get_next_checkpoint);
	ClassDB::bind_method(D_METHOD("get_lap", "body"), &RaceTrack::get_lap);
	ClassDB::bind_method(D_METHOD("get_progress", "body"), &RaceTrack::get_progress);
	ClassDB::bind_method(D_METHOD("get_race_time_msec", "body"), &RaceTrack::get_race_time_msec);
	ClassDB::bind_method(D_METHOD("get_respawn_transform", "body"), &RaceTrack::get_respawn_transform);
	ClassDB::bind_method(D_METHOD("respawn", "body"), &RaceTrack::respawn);

	ADD_SIGNAL(MethodInfo("checkpoint_passed", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_NODE_TYPE, "Node3D"), PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("lap_completed", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_NODE_TYPE, "Node3D"), PropertyInfo(Variant::INT, "lap"), PropertyInfo(Variant::INT, "lap_msec")));
	ADD_SIGNAL(MethodInfo("race_finished", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_NODE_TYPE, "Node3D"), PropertyInfo(Variant::INT, "total_msec")));
	ADD_SIGNAL(MethodInfo("wrong_way", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_NODE_TYPE, "Node3D"), PropertyInfo(Variant::BOOL, "is_wrong")));
	ADD_SIGNAL(MethodInfo("respawned", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_NODE_TYPE, "Node3D")));
	ADD_SIGNAL(MethodInfo("fell_off", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_NODE_TYPE, "Node3D")));
}
#undef RT_BIND
// clang-format on

RaceTrack::RaceTrack() {}
RaceTrack::~RaceTrack() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void RaceTrack::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			_connect_spline();
			queue_rebuild();
			set_physics_process(!Engine::get_singleton()->is_editor_hint());
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
		case NOTIFICATION_PHYSICS_PROCESS: {
			const float delta = (float)get_physics_process_delta_time();
			for (KeyValue<uint64_t, BodyState> &kv : _bodies) {
				Node3D *body = Object::cast_to<Node3D>(ObjectDB::get_instance(kv.key));
				if (body && body->is_inside_tree()) {
					_watch_body(body, kv.value, delta);
				}
			}
			break;
		}
		default:
			break;
	}
}

void RaceTrack::_connect_spline() {
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

void RaceTrack::_disconnect_spline() {
	if (_watched_spline) {
		const Callable cb(this, "_on_spline_changed");
		if (_watched_spline->is_connected("spline_changed", cb)) {
			_watched_spline->disconnect("spline_changed", cb);
		}
		_watched_spline = nullptr;
	}
}

void RaceTrack::_on_spline_changed() { queue_rebuild(); }

void RaceTrack::queue_rebuild() {
	if (!_rebuild_queued && is_inside_tree()) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

// ---------------------------------------------------------------------------------------------
// Gates
// ---------------------------------------------------------------------------------------------

void RaceTrack::_clear_gates() {
	for (Gate &g : _gates) {
		if (g.area) {
			g.area->queue_free();
		}
		if (g.debug_mesh) {
			g.debug_mesh->queue_free();
		}
	}
	_gates.clear();
}

/**
 * @brief One frame every checkpoint_spacing metres from the spline start (global space): origin on
 * the spline, +Z along the direction of travel, Y the banked up vector. A closed loop gets no gate on
 * the last few metres so the finish line is not duplicated.
 */
bool RaceTrack::_gate_frames(std::vector<Transform3D> &r_frames) const {
	r_frames.clear();
	const ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (!spline) {
		return false;
	}
	Ref<Curve3D> curve = spline->get_curve();
	if (curve.is_null() || curve->get_point_count() < 2) {
		return false;
	}
	const float total = curve->get_baked_length();
	if (total < checkpoint_spacing) {
		return false;
	}
	const Transform3D to_global = spline->get_global_transform();
	const float last = spline->get_is_closed() ? total - checkpoint_spacing * 0.5f : total;
	for (float d = 0.0f; d <= last; d += checkpoint_spacing) {
		Transform3D frame = curve->sample_baked_with_rotation(MIN(d, total), false, true);
		const Vector3 ahead = curve->sample_baked(MIN(d + 0.5f, total), false);
		const Vector3 behind = curve->sample_baked(MAX(d - 0.5f, 0.0f), false);
		Vector3 forward = ahead - behind;
		if (forward.length_squared() < 1e-8f) {
			forward = -frame.basis.get_column(2);
		}
		forward.normalize();
		Vector3 up = frame.basis.get_column(1).normalized();
		Vector3 lateral = up.cross(forward).normalized();
		up = forward.cross(lateral).normalized();
		frame.basis = Basis(lateral, up, forward);
		r_frames.push_back(to_global * frame);
	}
	return r_frames.size() >= 2;
}

/// Area3D (box, listens to body_mask) per gate, plus a translucent box when show_gates. Internal children.
void RaceTrack::_make_gate_nodes() {
	Ref<StandardMaterial3D> debug_mat;
	if (show_gates) {
		debug_mat.instantiate();
		debug_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		debug_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		debug_mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		debug_mat->set_albedo(Color(0.2f, 0.9f, 0.4f, 0.25f));
	}
	Ref<StandardMaterial3D> start_mat;
	if (show_gates) {
		start_mat = debug_mat->duplicate();
		start_mat->set_albedo(Color(1.0f, 1.0f, 1.0f, 0.35f));
	}
	const Vector3 size(gate_width, gate_height, gate_depth);

	for (size_t i = 0; i < _gates.size(); ++i) {
		Gate &g = _gates[i];
		Transform3D t = g.frame;
		t.origin += t.basis.get_column(1) * (gate_height * 0.5f);

		Area3D *area = memnew(Area3D);
		area->set_name(String("Gate") + String::num_int64((int64_t)i));
		area->set_collision_layer(0);
		area->set_collision_mask(body_mask);
		area->set_monitorable(false);
		Ref<BoxShape3D> box;
		box.instantiate();
		box->set_size(size);
		CollisionShape3D *cs = memnew(CollisionShape3D);
		cs->set_shape(box);
		area->add_child(cs, false, INTERNAL_MODE_BACK);
		add_child(area, false, INTERNAL_MODE_BACK);
		area->set_global_transform(t);
		area->connect("body_entered", Callable(this, "_on_gate_body_entered").bind((int)i));
		g.area = area;

		if (show_gates) {
			Ref<BoxMesh> mesh;
			mesh.instantiate();
			mesh->set_size(size);
			mesh->set_material(i == 0 ? start_mat : debug_mat);
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(mesh);
			add_child(mi, false, INTERNAL_MODE_BACK);
			mi->set_global_transform(t);
			g.debug_mesh = mi;
		}
	}
}

void RaceTrack::rebuild() {
	_rebuild_queued = false;
	_clear_gates();
	std::vector<Transform3D> frames;
	if (!_gate_frames(frames)) {
		return;
	}
	_gates.resize(frames.size());
	for (size_t i = 0; i < frames.size(); ++i) {
		_gates[i].frame = frames[i];
	}
	_make_gate_nodes();
	// Gate indices may have shifted: every tracked body restarts from its lap's first gate.
	for (KeyValue<uint64_t, BodyState> &kv : _bodies) {
		kv.value.next = 0;
	}
}

Transform3D RaceTrack::get_checkpoint_transform(int p_index) const {
	if (p_index < 0 || p_index >= (int)_gates.size()) {
		return Transform3D();
	}
	return _gates[p_index].frame;
}

void RaceTrack::_on_gate_body_entered(
		Node3D *p_body,
		int p_index
) {
	if (!p_body || p_index < 0 || p_index >= (int)_gates.size()) {
		return;
	}
	const uint64_t id = p_body->get_instance_id();
	if (!_bodies.has(id)) {
		if (p_index != 0) {
			return; // Only the start line enrols a body
		}
		_bodies[id] = BodyState();
	}
	_advance(p_body, _bodies[id], p_index);
}

} // namespace godot
