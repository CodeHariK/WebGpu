#include "target_telegraph.h"

#include "../../game_manager/game_constants.h"
#include "../../utils/raycast/mc_raycast.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const Color TRACK_COLOR(1.0f, 0.85f, 0.2f); // yellow: it's following you
static const Color LOCK_COLOR(1.0f, 0.2f, 0.15f); // red: about to hit
static const float GROUND_PROBE = 6.0f; // how far below the target to look for floor
static const float FALLBACK_DROP = 1.0f; // no floor found: draw this far below the centre

Ref<StandardMaterial3D> TargetTelegraph::_glow(float p_alpha) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_albedo(Color(TRACK_COLOR, p_alpha));
	return m;
}

MeshInstance3D *TargetTelegraph::_top_level_mesh(
		const Ref<Mesh> &p_mesh,
		const Ref<StandardMaterial3D> &p_mat
) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(p_mesh);
	mi->set_material_override(p_mat);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mi->set_as_top_level(true);
	mi->set_visible(false);
	owner->add_child(mi);
	return mi;
}

void TargetTelegraph::_build_zone() {
	Ref<TorusMesh> ring; // unit radius; scaled to the blast radius
	ring.instantiate();
	ring->set_inner_radius(0.9f);
	ring->set_outer_radius(1.0f);
	ring->set_rings(32);
	ring->set_ring_segments(6);
	zone_ring_mat = _glow(0.9f);
	zone_ring = _top_level_mesh(ring, zone_ring_mat);

	Ref<CylinderMesh> disc; // flat unit disc that grows to fill the ring
	disc.instantiate();
	disc->set_top_radius(1.0f);
	disc->set_bottom_radius(1.0f);
	disc->set_height(0.02f);
	disc->set_radial_segments(32);
	disc->set_rings(1);
	zone_fill_mat = _glow(0.35f);
	zone_fill = _top_level_mesh(disc, zone_fill_mat);
}

void TargetTelegraph::update_zone(
		float p_dt,
		const Vector3 &p_center,
		float p_radius,
		float p_progress
) {
	if (!owner) {
		return;
	}
	if (!zone_ring) {
		_build_zone();
	}
	clock += p_dt;
	float progress = CLAMP(p_progress, 0.0f, 1.0f);
	Color col = TRACK_COLOR.lerp(LOCK_COLOR, MIN(1.0f, progress * 1.5f));

	// Blink rate climbs from 2 Hz to 12 Hz as impact approaches.
	float blink_hz = Math::lerp(2.0f, 12.0f, progress * progress);
	blink_phase += p_dt * blink_hz; // accumulated, so speeding up doesn't jump the phase
	float ring_alpha = (std::sin(blink_phase * Math::TAU) > -0.3f) ? 0.95f : 0.35f;
	zone_ring_mat->set_albedo(Color(col, ring_alpha));
	zone_fill_mat->set_albedo(Color(col, 0.3f + 0.2f * progress));

	Vector3 c = p_center + Vector3(0, 0.06f, 0);
	float r = MAX(0.3f, p_radius);
	zone_ring->set_global_position(c);
	zone_ring->set_global_basis(Basis().scaled(Vector3(r, 0.3f, r)));
	zone_ring->set_visible(true);

	float f = MAX(0.02f, r * progress);
	zone_fill->set_global_position(c - Vector3(0, 0.02f, 0));
	zone_fill->set_global_basis(Basis().scaled(Vector3(f, 1.0f, f)));
	zone_fill->set_visible(true);
}

void TargetTelegraph::build(Node3D *p_owner) {
	owner = p_owner;

	Ref<TorusMesh> ring;
	ring.instantiate();
	ring->set_inner_radius(0.75f);
	ring->set_outer_radius(0.95f);
	ring->set_rings(24);
	ring->set_ring_segments(6);
	reticle_mat = _glow(0.85f);
	reticle = memnew(MeshInstance3D);
	reticle->set_mesh(ring);
	reticle->set_material_override(reticle_mat);
	reticle->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	reticle->set_as_top_level(true);
	reticle->set_visible(false);
	owner->add_child(reticle);

	Ref<SphereMesh> dot;
	dot.instantiate();
	dot->set_radius(0.2f);
	dot->set_height(0.4f);
	dot->set_radial_segments(8);
	dot->set_rings(4);
	marker_mat = _glow(0.45f);
	marker = memnew(MeshInstance3D);
	marker->set_mesh(dot);
	marker->set_material_override(marker_mat);
	marker->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	marker->set_as_top_level(true);
	marker->set_visible(false);
	owner->add_child(marker);
}

float TargetTelegraph::_ground_y(Node3D *p_target) const {
	Vector3 p = p_target->get_global_position();
	TypedArray<RID> exclude;
	if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_target)) {
		exclude.push_back(co->get_rid());
	}
	MCRaycastHit hit = raycast_3d(owner, p, p - Vector3(0, GROUND_PROBE, 0), toLayer(LAYER_TERRAIN), exclude);
	return hit.is_hit ? hit.position.y : p.y - FALLBACK_DROP;
}

void TargetTelegraph::update(
		float p_dt,
		Node3D *p_target,
		const Vector3 &p_aim,
		float p_lock,
		bool p_committed,
		bool p_show_aim
) {
	if (!reticle || !p_target) {
		hide();
		return;
	}
	clock += p_dt;
	spin += p_dt * (2.0f + 6.0f * p_lock); // spins faster as it closes in

	float lock = CLAMP(p_lock, 0.0f, 1.0f);
	Color col = TRACK_COLOR.lerp(LOCK_COLOR, lock);
	float alpha = 0.85f;
	if (p_committed) { // blink: "move now"
		alpha = (std::sin(clock * 30.0f) > 0.0f) ? 1.0f : 0.25f;
	}
	reticle_mat->set_albedo(Color(col, alpha));

	Vector3 tp = p_target->get_global_position();
	float s = Math::lerp(1.6f, 0.8f, lock);
	reticle->set_global_position(Vector3(tp.x, _ground_y(p_target) + 0.05f, tp.z));
	reticle->set_rotation(Vector3(0, spin, 0));
	reticle->set_scale(Vector3(s, 0.4f, s)); // flattened torus = ground ring
	reticle->set_visible(true);

	marker->set_visible(p_show_aim && !p_committed);
	if (marker->is_visible()) {
		marker_mat->set_albedo(Color(col, 0.45f));
		marker->set_global_position(p_aim);
	}
}

void TargetTelegraph::hide() {
	if (reticle) {
		reticle->set_visible(false);
	}
	if (marker) {
		marker->set_visible(false);
	}
	if (zone_ring) {
		zone_ring->set_visible(false);
		zone_fill->set_visible(false);
	}
}

} // namespace godot
