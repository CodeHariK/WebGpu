#include "jump_reach_gizmo.h"

#include "debug_manager.h"

#include <godot_cpp/classes/label3d.hpp>

namespace godot {

static const int ARC_SAMPLES = 32;
static const char *RING_IDS[] = { "jump_reach_comfort", "jump_reach_precise", "jump_reach_limit" };
static const char *DOUBLE_RING_ID = "jump_reach_double";
static const Color DOUBLE_COLOR = Color(0.35f, 0.6f, 1.0f, 0.55f); // faint: outer bounds
static const char *DASH_RING_ID = "jump_reach_dash";
static const Color DASH_COLOR = Color(0.8f, 0.4f, 1.0f, 0.55f);
static const Color RING_COLORS[] = {
	Color(0.3f, 0.95f, 0.4f, 0.9f), // comfortable
	Color(1.0f, 0.9f, 0.2f, 0.9f), // precise
	Color(1.0f, 0.55f, 0.15f, 0.9f), // run limit
};
static const float RING_FRACTIONS[] = { JumpMetrics::COMFORT, JumpMetrics::PRECISE, 1.0f };
static const Color ARC_COLOR = Color(1.0f, 1.0f, 1.0f, 0.9f);

JumpReachGizmo::JumpReachGizmo() {
	mesh.instantiate();
	set_mesh(mesh);
	set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);

	material.instantiate();
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true); // readable through platforms
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	material->set_albedo(ARC_COLOR);

	label = memnew(Label3D);
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
	label->set_font_size(40);
	label->set_outline_size(10);
	label->set_pixel_size(0.025f);
	add_child(label);
}

void JumpReachGizmo::update_for(Node3D *p_target) {
	JumpMetrics m;
	if (!p_target || !JumpMetrics::from_node(p_target, m)) {
		clear();
		return;
	}
	_update_anchor(p_target);
	_draw_rings(m);
	_draw_arc(m);

	label->set_visible(true);
	label->set_global_position(anchor + forward * m.reach(0.0f) + Vector3(0, 1.2f, 0));
	String text = vformat(
			"flat %.1f m (sprint %.1f)\napex %.1f m   air %.2f s\n+%.1f m ledge: %.1f m",
			m.reach(0.0f), m.reach(0.0f, true), m.height, m.airtime(0.0f), 0.6f * m.height, m.reach(0.6f * m.height)
	);
	if (m.air_jumps > 0) {
		text += vformat("\ndouble: %.1f m far / %.1f m high", m.double_reach(0.0f), m.double_apex());
	}
	if (m.air_dash) {
		text += vformat("\njump+double+dash: %.1f m", m.full_reach(0.0f));
	}
	label->set_text(text);
}

void JumpReachGizmo::clear() {
	mesh->clear_surfaces();
	label->set_visible(false);
	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		for (const char *id : RING_IDS) {
			dm->clear_ring(id);
		}
		dm->clear_ring(DOUBLE_RING_ID);
		dm->clear_ring(DASH_RING_ID);
	}
}

// Follow the character while grounded; freeze at take-off while airborne.
void JumpReachGizmo::_update_anchor(Node3D *p_target) {
	bool grounded = !p_target->has_method("is_grounded") || bool(p_target->call("is_grounded"));
	if (!grounded && has_anchor) {
		return;
	}
	anchor = p_target->get_global_position();
	has_anchor = true;
	Vector3 vel;
	if (p_target->has_method("get_velocity")) {
		vel = p_target->call("get_velocity");
	} else if (p_target->has_method("get_linear_velocity")) {
		vel = p_target->call("get_linear_velocity");
	}
	vel.y = 0.0f;
	if (vel.length() > 1.0f) {
		forward = vel.normalized();
	}
}

void JumpReachGizmo::_draw_rings(const JumpMetrics &p_m) {
	DebugManager *dm = DebugManager::get_singleton();
	if (!dm) {
		return;
	}
	const float run = p_m.reach(0.0f);
	for (int i = 0; i < 3; ++i) {
		dm->draw_ring(RING_IDS[i], anchor, run * RING_FRACTIONS[i], RING_COLORS[i]);
	}
	if (p_m.air_jumps > 0) {
		dm->draw_ring(DOUBLE_RING_ID, anchor, p_m.double_reach(0.0f), DOUBLE_COLOR);
	} else {
		dm->clear_ring(DOUBLE_RING_ID);
	}
	if (p_m.air_dash) {
		dm->draw_ring(DASH_RING_ID, anchor, p_m.full_reach(0.0f), DASH_COLOR);
	} else {
		dm->clear_ring(DASH_RING_ID);
	}
}

// The run trajectory straight ahead, from take-off down to a full apex-height drop.
void JumpReachGizmo::_draw_arc(const JumpMetrics &p_m) {
	set_global_transform(Transform3D()); // arc vertices are world space
	mesh->clear_surfaces();
	mesh->surface_begin(Mesh::PRIMITIVE_LINE_STRIP, material);
	const float t_end = p_m.airtime(-p_m.height);
	for (int i = 0; i <= ARC_SAMPLES; ++i) {
		const float t = t_end * i / ARC_SAMPLES;
		mesh->surface_add_vertex(anchor + forward * (p_m.run_speed * t) + Vector3(0, p_m.height_at(t), 0));
	}
	mesh->surface_end();
}

} // namespace godot
