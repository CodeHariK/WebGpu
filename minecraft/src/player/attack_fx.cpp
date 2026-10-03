#include "attack_fx.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static const float RING_HEIGHT = -0.4f; // waist height relative to the body centre
static const float RING_START = 0.4f; // spin ring starts at this fraction of the hit radius
static const float SQUASH = 0.18f; // peak spin squash & stretch
static const float DIVE_STRETCH = 0.18f; // lengthwise stretch at full dive speed
static const float BURST_TIME = 0.22f;
static const float BURST_RADIUS = 1.8f;

void AttackFx::setup(
		Node3D *p_owner,
		Node3D *p_skin
) {
	owner = p_owner;
	skin = p_skin;
	if (skin) {
		skin_base = skin->get_transform();
	}

	Ref<TorusMesh> torus; // unit radius, scaled per use
	torus.instantiate();
	torus->set_inner_radius(0.85f);
	torus->set_outer_radius(1.0f);
	torus->set_rings(32);
	torus->set_ring_segments(6);

	ring_mat.instantiate();
	ring_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	ring_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	ring_mat->set_albedo(Color(1, 1, 1, 0));

	ring = memnew(MeshInstance3D);
	ring->set_mesh(torus);
	ring->set_material_override(ring_mat);
	ring->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	ring->set_as_top_level(true); // world space: a burst stays where it happened
	ring->set_visible(false);
	owner->add_child(ring);
}

void AttackFx::_begin_pose() {
	if (skin && !posing) {
		skin_base = skin->get_transform(); // in case something else moved it since setup
	}
	posing = true;
}

void AttackFx::_set_skin_pose(
		const Basis &p_extra_before,
		const Vector3 &p_scale
) {
	if (skin) {
		skin->set_transform(Transform3D(p_extra_before * skin_base.basis * Basis().scaled(p_scale), skin_base.origin));
	}
}

void AttackFx::_place_ring(
		const Vector3 &p_world,
		float p_radius,
		float p_alpha
) {
	if (!ring) {
		return;
	}
	ring->set_global_transform(Transform3D(Basis().scaled(Vector3(p_radius, 0.25f, p_radius)), p_world));
	ring_mat->set_albedo(Color(1, 1, 1, p_alpha));
	ring->set_visible(true);
}

void AttackFx::play_spin(float p_radius) {
	radius = p_radius;
	_begin_pose();
	update_spin(0.0f);
}

void AttackFx::update_spin(float p_progress) {
	if (!posing) {
		return;
	}
	float p = CLAMP(p_progress, 0.0f, 1.0f);
	float out = 1.0f - (1.0f - p) * (1.0f - p); // ease-out: whips round, then settles
	float s = SQUASH * std::sin(p * Math::PI); // 0 -> peak -> 0
	_set_skin_pose(Basis(Vector3(0, 1, 0), Math::TAU * out), Vector3(1.0f + s, 1.0f - s, 1.0f + s));
	if (burst_t < 0.0f && owner) {
		_place_ring(owner->get_global_position() + Vector3(0, RING_HEIGHT, 0), radius * Math::lerp(RING_START, 1.0f, out), 0.85f * (1.0f - p));
	}
}

void AttackFx::play_dive() {
	_begin_pose();
	update_dive(0.0f, 0.0f);
}

void AttackFx::update_dive(
		float p_lean,
		float p_stretch
) {
	if (!posing) {
		return;
	}
	// Pitch about the body's X axis: positive tips the head back (feet forward).
	float k = DIVE_STRETCH * CLAMP(p_stretch, 0.0f, 1.0f);
	_set_skin_pose(Basis(Vector3(1, 0, 0), p_lean), Vector3(1.0f - k * 0.5f, 1.0f + k, 1.0f - k * 0.5f));
}

void AttackFx::burst(const Vector3 &p_at) {
	burst_t = 0.0f;
	_place_ring(p_at, BURST_RADIUS * 0.3f, 1.0f);
}

void AttackFx::stop() {
	if (posing && skin) {
		skin->set_transform(skin_base);
	}
	posing = false;
	if (ring && burst_t < 0.0f) {
		ring->set_visible(false);
	}
}

void AttackFx::tick(float p_dt) {
	if (burst_t < 0.0f || !ring) {
		return;
	}
	burst_t += p_dt;
	float p = burst_t / BURST_TIME;
	if (p >= 1.0f) {
		burst_t = -1.0f;
		ring->set_visible(false);
		return;
	}
	float out = 1.0f - (1.0f - p) * (1.0f - p);
	_place_ring(ring->get_global_position(), BURST_RADIUS * Math::lerp(0.3f, 1.0f, out), 1.0f - p);
}

} // namespace godot
