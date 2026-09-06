/**
 * @file race_track.h
 * @brief RaceTrack: checkpoint gates, laps, wrong-way detection and respawn along a track spline.
 */
#ifndef RACE_TRACK_H
#define RACE_TRACK_H

#include "utils/spline3d/procedural_spline3d.h"
#include <cstdint>
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

namespace godot {

/**
 * @class RaceTrack
 * @brief SplineComponent that turns its parent ProceduralSpline3D into a race course: one checkpoint
 * gate (Area3D box, `gate_width` × `gate_height` × `gate_depth`, following the spline's banking) every
 * `checkpoint_spacing` metres, gate 0 being start / finish. Any body that drives through gate 0 is
 * tracked from then on: it must cross the gates in order (skipping or reversing does nothing), a full
 * ring is a lap, `laps` laps finish the race. Per body it also watches for driving against the track
 * direction (`wrong_way`) and for falling `kill_depth` below the last checkpoint (respawn onto it,
 * upright and facing forward, velocities cleared). The gates are internal children; `show_gates`
 * draws them as translucent boxes for tuning. Nothing here touches the terrain or the road mesh.
 *
 * Signals: checkpoint_passed(body, index), lap_completed(body, lap, lap_msec),
 * race_finished(body, total_msec), wrong_way(body, is_wrong), respawned(body), fell_off(body).
 */
class RaceTrack : public SplineComponent {
	GDCLASS(RaceTrack,
			SplineComponent)

private:
	struct Gate {
		Transform3D frame; // Global: origin on the spline, +Z = track direction, Y = deck up
		Area3D *area = nullptr;
		MeshInstance3D *debug_mesh = nullptr;
	};

	struct BodyState {
		int next = 0; // Gate the body must cross next
		int lap = 0; // Completed laps
		bool started = false; // Crossed gate 0 at least once
		bool finished = false;
		uint64_t race_start_msec = 0;
		uint64_t lap_start_msec = 0;
		float wrong_timer = 0.0f;
		bool is_wrong = false;
	};

	// ---- Course ----
	float checkpoint_spacing = 60.0f; // Metres between gates; gate 0 at the spline start
	float gate_width = 24.0f;
	float gate_height = 8.0f;
	float gate_depth = 2.0f;
	int laps = 3; // 0 = endless
	uint32_t body_mask = 1; // Collision layers the gates listen to
	bool show_gates = false;

	// ---- Rules ----
	float wrong_way_angle = 100.0f; // Degrees between velocity and track direction that counts as wrong
	float wrong_way_time = 1.0f; // Seconds of wrong driving before the signal
	float wrong_way_min_speed = 3.0f; // m/s; slower bodies are never "wrong"
	float kill_depth = 30.0f; // Metres below the last checkpoint that triggers a respawn (0 = off)
	bool auto_respawn = true; // Else only fell_off is emitted
	float respawn_height = 1.0f; // Above the checkpoint
	float respawn_back = 4.0f; // Metres behind the checkpoint so the body re-crosses it cleanly

	std::vector<Gate> _gates;
	HashMap<uint64_t, BodyState> _bodies; // By instance id
	bool _rebuild_queued = false;
	ProceduralSpline3D *_watched_spline = nullptr;

	// ---- Gates (race_track.cpp) ----
	void _connect_spline();
	void _disconnect_spline();
	void _clear_gates();
	bool _gate_frames(std::vector<Transform3D> &r_frames) const;
	void _make_gate_nodes();
	void _on_gate_body_entered(
			Node3D *p_body,
			int p_index
	);

	// ---- Progress (race_track_progress.cpp) ----
	void _advance(
			Node3D *p_body,
			BodyState &p_state,
			int p_index
	);
	void _watch_body(
			Node3D *p_body,
			BodyState &p_state,
			float p_delta
	);
	Transform3D _respawn_frame(const BodyState &p_state) const;
	Vector3 _track_direction(int p_index) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	RaceTrack();
	~RaceTrack();

	// ---- Course ----
	void set_checkpoint_spacing(float p_metres) {
		checkpoint_spacing = MAX(5.0f, p_metres);
		queue_rebuild();
	}
	float get_checkpoint_spacing() const { return checkpoint_spacing; }
	void set_gate_width(float p_metres) {
		gate_width = MAX(1.0f, p_metres);
		queue_rebuild();
	}
	float get_gate_width() const { return gate_width; }
	void set_gate_height(float p_metres) {
		gate_height = MAX(0.5f, p_metres);
		queue_rebuild();
	}
	float get_gate_height() const { return gate_height; }
	void set_gate_depth(float p_metres) {
		gate_depth = MAX(0.2f, p_metres);
		queue_rebuild();
	}
	float get_gate_depth() const { return gate_depth; }
	void set_laps(int p_laps) { laps = MAX(0, p_laps); }
	int get_laps() const { return laps; }
	void set_body_mask(uint32_t p_mask) {
		body_mask = p_mask;
		queue_rebuild();
	}
	uint32_t get_body_mask() const { return body_mask; }
	void set_show_gates(bool p_show) {
		show_gates = p_show;
		queue_rebuild();
	}
	bool get_show_gates() const { return show_gates; }

	// ---- Rules ----
	void set_wrong_way_angle(float p_deg) { wrong_way_angle = CLAMP(p_deg, 0.0f, 180.0f); }
	float get_wrong_way_angle() const { return wrong_way_angle; }
	void set_wrong_way_time(float p_sec) { wrong_way_time = MAX(0.0f, p_sec); }
	float get_wrong_way_time() const { return wrong_way_time; }
	void set_wrong_way_min_speed(float p_speed) { wrong_way_min_speed = MAX(0.0f, p_speed); }
	float get_wrong_way_min_speed() const { return wrong_way_min_speed; }
	void set_kill_depth(float p_metres) { kill_depth = MAX(0.0f, p_metres); }
	float get_kill_depth() const { return kill_depth; }
	void set_auto_respawn(bool p_auto) { auto_respawn = p_auto; }
	bool get_auto_respawn() const { return auto_respawn; }
	void set_respawn_height(float p_metres) { respawn_height = p_metres; }
	float get_respawn_height() const { return respawn_height; }
	void set_respawn_back(float p_metres) { respawn_back = MAX(0.0f, p_metres); }
	float get_respawn_back() const { return respawn_back; }

	// ---- Runtime API ----
	void queue_rebuild();
	void rebuild();
	void _on_spline_changed();

	int get_checkpoint_count() const { return (int)_gates.size(); }
	Transform3D get_checkpoint_transform(int p_index) const;
	/// Starts tracking p_body at gate 0 without it having crossed the line (grid start).
	void track_body(Node3D *p_body);
	void untrack_body(Node3D *p_body);
	int get_next_checkpoint(Node3D *p_body) const;
	int get_lap(Node3D *p_body) const;
	/// 0..1 along the current lap: passed gates plus the fraction of the way to the next one.
	float get_progress(Node3D *p_body) const;
	uint64_t get_race_time_msec(Node3D *p_body) const;
	Transform3D get_respawn_transform(Node3D *p_body) const;
	/// Teleports p_body to its respawn transform (RigidBody3D velocities cleared) and emits respawned.
	void respawn(Node3D *p_body);
};

} // namespace godot

#endif // RACE_TRACK_H
