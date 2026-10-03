#include "projectile_visual.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

static const float POP_SWELL_TIME = 0.12f; // model swells, then vanishes
static const float POP_TOTAL_TIME = 0.7f; // burst puffs finish fading
static const float STICK_TIME = 1.2f; // arrows stay stuck where they hit, then shrink away
static const float STICK_FADE = 0.25f;
static const float PUFF_INTERVAL = 0.05f; // seconds between trail puffs
static const float PUFF_LIFE = 0.55f;
static const float BANK_GAIN = 0.35f; // radians of bank per rad/s of turn
static const float BANK_MAX = 0.9f;

Ref<StandardMaterial3D> ProjectileVisual::_toon(const Color &p_color) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(p_color);
	m->set_diffuse_mode(BaseMaterial3D::DIFFUSE_TOON);
	m->set_specular_mode(BaseMaterial3D::SPECULAR_TOON);
	m->set_roughness(0.5f);
	m->set_feature(BaseMaterial3D::FEATURE_RIM, true); // soft cartoon edge light
	m->set_rim(0.4f);
	return m;
}

MeshInstance3D *ProjectileVisual::_part(
		Node3D *p_parent,
		const Ref<Mesh> &p_mesh,
		const Ref<StandardMaterial3D> &p_mat,
		const Vector3 &p_pos,
		const Vector3 &p_rot
) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(p_mesh);
	mi->set_material_override(p_mat);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mi->set_position(p_pos);
	mi->set_rotation(p_rot);
	p_parent->add_child(mi);
	return mi;
}

void ProjectileVisual::build(
		Node3D *p_owner,
		const Ref<ProjectileProfile> &p_profile
) {
	owner = p_owner;
	style = p_profile->get_style();
	wobble_style = p_profile->get_wobble_style();
	amp = p_profile->get_wobble_amp();
	freq = p_profile->get_wobble_freq();
	model_scale = p_profile->get_model_scale();
	phase = (float)UtilityFunctions::randf_range(0.0, Math::TAU);

	body_mat = _toon(p_profile->get_body_color());
	accent_mat = _toon(style == ProjectileProfile::STYLE_ARROW ? Color(0.55f, 0.35f, 0.2f) : Color(1.0f, 0.85f, 0.25f));
	dark_mat = _toon(Color(0.1f, 0.1f, 0.12f));

	pivot = memnew(Node3D);
	owner->add_child(pivot);
	if (style == ProjectileProfile::STYLE_ARROW) {
		_build_arrow();
	} else {
		_build_missile();
		puffs.build(owner, PUFF_COUNT, 0.16f, Color(0.95f, 0.95f, 0.95f));
	}
	pivot->set_scale(Vector3(0.3f, 0.3f, 0.3f) * model_scale); // pops in from small
}

// Forward is -Z everywhere (Godot's look_at convention).
void ProjectileVisual::_build_missile() {
	Ref<CapsuleMesh> body;
	body.instantiate();
	body->set_radius(0.2f);
	body->set_height(0.8f);
	body->set_radial_segments(12);
	body->set_rings(4);
	_part(pivot, body, body_mat, Vector3(), Vector3(Math::PI * 0.5f, 0, 0));

	Ref<CylinderMesh> stripe; // white belly band
	stripe.instantiate();
	stripe->set_top_radius(0.205f);
	stripe->set_bottom_radius(0.205f);
	stripe->set_height(0.1f);
	stripe->set_radial_segments(12);
	_part(pivot, stripe, _toon(Color(1, 1, 1)), Vector3(0, 0, 0.05f), Vector3(Math::PI * 0.5f, 0, 0));

	Ref<SphereMesh> eye_white; // big cartoon eyes near the nose
	eye_white.instantiate();
	eye_white->set_radius(0.075f);
	eye_white->set_height(0.15f);
	eye_white->set_radial_segments(8);
	eye_white->set_rings(4);
	Ref<SphereMesh> pupil;
	pupil.instantiate();
	pupil->set_radius(0.04f);
	pupil->set_height(0.08f);
	pupil->set_radial_segments(8);
	pupil->set_rings(4);
	Ref<StandardMaterial3D> white = _toon(Color(1, 1, 1));
	for (int side = -1; side <= 1; side += 2) {
		_part(pivot, eye_white, white, Vector3(0.085f * side, 0.11f, -0.24f));
		_part(pivot, pupil, dark_mat, Vector3(0.085f * side, 0.125f, -0.3f));
	}

	Ref<BoxMesh> fin_h; // cross fins at the tail
	fin_h.instantiate();
	fin_h->set_size(Vector3(0.62f, 0.03f, 0.18f));
	Ref<BoxMesh> fin_v;
	fin_v.instantiate();
	fin_v->set_size(Vector3(0.03f, 0.62f, 0.18f));
	_part(pivot, fin_h, accent_mat, Vector3(0, 0, 0.3f));
	_part(pivot, fin_v, accent_mat, Vector3(0, 0, 0.3f));
}

void ProjectileVisual::_build_arrow() {
	Ref<CylinderMesh> shaft;
	shaft.instantiate();
	shaft->set_top_radius(0.025f);
	shaft->set_bottom_radius(0.025f);
	shaft->set_height(1.0f);
	shaft->set_radial_segments(6);
	_part(pivot, shaft, accent_mat, Vector3(), Vector3(Math::PI * 0.5f, 0, 0));

	Ref<CylinderMesh> tip; // cone, +Y rotated to -Z
	tip.instantiate();
	tip->set_top_radius(0.0f);
	tip->set_bottom_radius(0.07f);
	tip->set_height(0.18f);
	tip->set_radial_segments(8);
	_part(pivot, tip, _toon(Color(0.75f, 0.78f, 0.85f)), Vector3(0, 0, -0.58f), Vector3(-Math::PI * 0.5f, 0, 0));

	fletching = memnew(Node3D);
	fletching->set_position(Vector3(0, 0, 0.4f));
	pivot->add_child(fletching);
	Ref<BoxMesh> feather_h;
	feather_h.instantiate();
	feather_h->set_size(Vector3(0.2f, 0.01f, 0.22f));
	Ref<BoxMesh> feather_v;
	feather_v.instantiate();
	feather_v->set_size(Vector3(0.01f, 0.2f, 0.22f));
	_part(fletching, feather_h, body_mat, Vector3());
	_part(fletching, feather_v, body_mat, Vector3());
}




void ProjectileVisual::update(
		float p_dt,
		float p_age,
		float p_turn_rate,
		float p_shake
) {
	if (!pivot || pop_t >= 0.0f) {
		return;
	}
	float a = p_age * freq * Math::TAU + phase;
	float settle = CLAMP(p_age / 0.25f, 0.0f, 1.0f); // clean launch, then wobble fades in

	// Bank into turns (eased so it leans, not snaps).
	float bank_target = CLAMP(-p_turn_rate * BANK_GAIN, -BANK_MAX, BANK_MAX);
	bank += (bank_target - bank) * (1.0f - std::exp(-8.0f * p_dt));

	// Pop-in: ease-out-back from 0.3 to 1 over 0.2 s (same curve as the bubble).
	float g_t = CLAMP(p_age / 0.2f, 0.0f, 1.0f);
	float c = 1.70158f;
	float grow = 1.0f + (c + 1.0f) * std::pow(g_t - 1.0f, 3.0f) + c * std::pow(g_t - 1.0f, 2.0f);
	grow = Math::lerp(0.3f, 1.0f, grow) * model_scale;

	if (style == ProjectileProfile::STYLE_ARROW) {
		// Fishtail: strong on release, settling to a gentle wiggle.
		float decay = 0.25f + 0.75f * std::exp(-p_age * 1.5f);
		float yaw = std::sin(a) * amp * decay * p_shake * settle;
		pivot->set_rotation(Vector3(0, yaw, bank));
		pivot->set_scale(Vector3(grow, grow, grow));
		if (fletching) {
			fletching->set_rotation(Vector3(0, 0, std::sin(p_age * 35.0f) * 0.25f));
		}
		return;
	}

	float k = amp * p_shake * settle;
	if (wobble_style == ProjectileProfile::WOBBLE_CORKSCREW) {
		_apply_corkscrew(a, k, settle, grow);
	} else {
		_apply_shake(a, k, p_shake, grow);
	}

	// Smoke trail from the tail.
	puff_timer += p_dt;
	if (puff_timer >= PUFF_INTERVAL) {
		puff_timer = 0.0f;
		Transform3D xf = pivot->get_global_transform();
		Vector3 tail = xf.origin + xf.basis.get_column(2).normalized() * 0.45f * model_scale;
		Vector3 jitter(
				(float)UtilityFunctions::randf_range(-0.3, 0.3),
				(float)UtilityFunctions::randf_range(0.2, 0.6),
				(float)UtilityFunctions::randf_range(-0.3, 0.3)
		);
		puffs.emit(tail, jitter, PUFF_LIFE, model_scale);
	}
	puffs.update(p_dt);
}

void ProjectileVisual::_apply_shake(
		float p_a,
		float p_k,
		float p_shake,
		float p_grow
) {
	// Two layered sines per axis at non-harmonic ratios -> jittery, never-repeating nose
	// twitch (pitch and yaw), plus a little roll rattle.
	float yaw = p_k * (0.6f * std::sin(p_a) + 0.4f * std::sin(p_a * 2.37f + 1.3f));
	float pitch = p_k * (0.6f * std::sin(p_a * 1.71f + 0.7f) + 0.4f * std::sin(p_a * 3.13f + 2.1f));
	float roll = p_k * 0.8f * std::sin(p_a * 2.9f + 0.4f);
	pivot->set_position(Vector3());
	pivot->set_rotation(Vector3(pitch, yaw, bank + roll));
	// Engine "throb": a quick squash & stretch along the flight axis.
	float throb = std::sin(p_a * 2.0f) * 0.06f * MIN(p_shake, 2.0f);
	pivot->set_scale(Vector3(1.0f - throb * 0.5f, 1.0f - throb * 0.5f, 1.0f + throb) * p_grow);
}

void ProjectileVisual::_apply_corkscrew(
		float p_a,
		float p_k,
		float p_settle,
		float p_grow
) {
	// The model circles the true path (radius p_k), nose swaying with the circle, and rolls
	// a little with it.
	pivot->set_position(Vector3(std::cos(p_a), std::sin(p_a), 0.0f) * p_k);
	pivot->set_rotation(Vector3(
			std::sin(p_a) * 0.12f * p_settle,
			-std::cos(p_a) * 0.12f * p_settle,
			bank + std::sin(p_a) * 0.25f * p_settle
	));
	// Breathing squash & stretch along the flight axis.
	float breathe = std::sin(p_a * 2.0f) * 0.1f;
	pivot->set_scale(Vector3(1.0f - breathe * 0.5f, 1.0f - breathe * 0.5f, 1.0f + breathe) * p_grow);
}

void ProjectileVisual::start_pop() {
	if (pop_t >= 0.0f) {
		return;
	}
	pop_t = 0.0f;
	if (!owner) {
		return;
	}
	// Burst: a ring of puffs flying outward.
	puffs.burst(owner->get_global_position(), 6, 3.0f, POP_TOTAL_TIME - 0.05f, model_scale);
}

bool ProjectileVisual::update_pop(float p_dt) {
	pop_t += p_dt;
	if (style == ProjectileProfile::STYLE_ARROW) {
		// Thunk: stuck in place with a quick quiver, then shrinks away.
		if (pivot) {
			float quiver = std::sin(pop_t * 60.0f) * 0.15f * std::exp(-pop_t * 8.0f);
			pivot->set_rotation(Vector3(quiver, 0, 0));
			float s = model_scale * CLAMP(1.0f - (pop_t - STICK_TIME) / STICK_FADE, 0.0f, 1.0f);
			pivot->set_scale(Vector3(s, s, s));
		}
		return pop_t >= STICK_TIME + STICK_FADE;
	}
	if (pivot) {
		if (pop_t < POP_SWELL_TIME) {
			float s = (1.0f + 0.8f * pop_t / POP_SWELL_TIME) * model_scale;
			pivot->set_scale(Vector3(s, s, s));
		} else {
			pivot->set_visible(false);
		}
	}
	puffs.update(p_dt);
	return pop_t >= POP_TOTAL_TIME;
}

} // namespace godot
