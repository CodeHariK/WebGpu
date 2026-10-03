#include "trajectory_preview.h"

#include "../utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const float SIM_STEP = 1.0f / 30.0f; // arc resolution (s); a ray per step
static const float RING_RADIUS = 0.5f;
static const float END_SHRINK = 0.5f; // last dots shrink to this fraction (taper)

void TrajectoryPreview::_bind_methods() {
	ClassDB::bind_method(D_METHOD("show_arc", "origin", "velocity", "gravity", "dt", "mask", "exclude"), &TrajectoryPreview::show_arc, DEFVAL(TypedArray<RID>()));
	ClassDB::bind_method(D_METHOD("hide_arc"), &TrajectoryPreview::hide_arc);
	ClassDB::bind_method(D_METHOD("get_landing"), &TrajectoryPreview::get_landing);
	ClassDB::bind_method(D_METHOD("get_has_hit"), &TrajectoryPreview::get_has_hit);
	ClassDB::bind_method(D_METHOD("set_color", "color"), &TrajectoryPreview::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &TrajectoryPreview::get_color);
	ClassDB::bind_method(D_METHOD("set_dot_spacing", "spacing"), &TrajectoryPreview::set_dot_spacing);
	ClassDB::bind_method(D_METHOD("get_dot_spacing"), &TrajectoryPreview::get_dot_spacing);
	ClassDB::bind_method(D_METHOD("set_max_time", "seconds"), &TrajectoryPreview::set_max_time);
	ClassDB::bind_method(D_METHOD("get_max_time"), &TrajectoryPreview::get_max_time);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dot_spacing", PROPERTY_HINT_RANGE, "0.1,2,0.05,suffix:m"), "set_dot_spacing", "get_dot_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_time", PROPERTY_HINT_RANGE, "0.5,8,0.1,suffix:s"), "set_max_time", "get_max_time");
}

void TrajectoryPreview::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	_build();
}

void TrajectoryPreview::_build() {
	dot_mat.instantiate();
	dot_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dot_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dot_mat->set_albedo(color);

	Ref<SphereMesh> dot;
	dot.instantiate();
	dot->set_radius(dot_radius);
	dot->set_height(dot_radius * 2.0f);
	dot->set_radial_segments(8);
	dot->set_rings(4);
	dot->set_material(dot_mat);

	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(dot);
	mm->set_instance_count(max_dots);
	mm->set_visible_instance_count(0);

	dots = memnew(MultiMeshInstance3D);
	dots->set_multimesh(mm);
	dots->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	dots->set_as_top_level(true);
	dots->set_visible(false);
	add_child(dots);

	ring_mat.instantiate();
	ring_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	ring_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	ring_mat->set_albedo(color);

	Ref<TorusMesh> torus;
	torus.instantiate();
	torus->set_inner_radius(RING_RADIUS * 0.8f);
	torus->set_outer_radius(RING_RADIUS);
	torus->set_rings(24);
	torus->set_ring_segments(6);

	ring = memnew(MeshInstance3D);
	ring->set_mesh(torus);
	ring->set_material_override(ring_mat);
	ring->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	ring->set_as_top_level(true);
	ring->set_visible(false);
	add_child(ring);
}

bool TrajectoryPreview::show_arc(
		const Vector3 &p_origin,
		const Vector3 &p_velocity,
		float p_gravity,
		float p_dt,
		uint32_t p_mask,
		const TypedArray<RID> &p_exclude
) {
	if (!dots) {
		return false;
	}
	clock += p_dt;
	flow = std::fmod(flow + flow_speed * p_dt, dot_spacing);

	// Step the same motion the projectile integrates, stopping at the first hit.
	PackedVector3Array path;
	path.push_back(p_origin);
	Vector3 p = p_origin;
	Vector3 v = p_velocity;
	Vector3 normal(0, 1, 0);
	has_hit = false;
	int steps = (int)(max_time / SIM_STEP);
	for (int i = 0; i < steps; i++) {
		v.y -= p_gravity * SIM_STEP;
		Vector3 next = p + v * SIM_STEP;
		MCRaycastHit hit = raycast_3d(this, p, next, p_mask, p_exclude);
		if (hit.is_hit) {
			path.push_back(hit.position);
			normal = hit.normal;
			has_hit = true;
			break;
		}
		path.push_back(next);
		p = next;
	}
	landing = path[path.size() - 1];

	_place_dots(path);
	_place_ring(landing, normal, has_hit);
	return has_hit;
}

void TrajectoryPreview::_place_dots(const PackedVector3Array &p_path) {
	Ref<MultiMesh> mm = dots->get_multimesh();
	dot_mat->set_albedo(color);

	// Total length, so dots near the end can taper.
	float total = 0.0f;
	for (int i = 1; i < p_path.size(); i++) {
		total += p_path[i - 1].distance_to(p_path[i]);
	}

	int count = 0;
	float next_at = flow; // first dot offset by the flow phase -> dots drift forward
	float walked = 0.0f;
	for (int i = 1; i < p_path.size() && count < max_dots; i++) {
		Vector3 a = p_path[i - 1];
		Vector3 b = p_path[i];
		float seg = a.distance_to(b);
		while (next_at <= walked + seg && count < max_dots) {
			float t = (seg > 1e-5f) ? (next_at - walked) / seg : 0.0f;
			float along = (total > 1e-5f) ? next_at / total : 0.0f;
			float s = Math::lerp(1.0f, END_SHRINK, along);
			mm->set_instance_transform(count, Transform3D(Basis().scaled(Vector3(s, s, s)), a.lerp(b, t)));
			count++;
			next_at += dot_spacing;
		}
		walked += seg;
	}
	mm->set_visible_instance_count(count);
	dots->set_visible(count > 0);
}

void TrajectoryPreview::_place_ring(
		const Vector3 &p_point,
		const Vector3 &p_normal,
		bool p_hit
) {
	ring->set_visible(p_hit);
	if (!p_hit) {
		return;
	}
	// Tilt the ring onto the surface: its local Y (the torus axis) along the normal.
	Vector3 up = p_normal.normalized();
	Vector3 ref = (std::abs(up.y) > 0.9f) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
	Vector3 x = ref.cross(up).normalized();
	Vector3 z = x.cross(up).normalized();
	float pulse = 1.0f + 0.12f * std::sin(clock * 8.0f);
	Basis b(x, up, z);
	ring->set_global_transform(Transform3D(b * Basis().scaled(Vector3(pulse, 0.4f, pulse)), p_point + up * 0.04f));
	ring_mat->set_albedo(color);
}

void TrajectoryPreview::hide_arc() {
	if (dots) {
		dots->set_visible(false);
	}
	if (ring) {
		ring->set_visible(false);
	}
	has_hit = false;
}

} // namespace godot
