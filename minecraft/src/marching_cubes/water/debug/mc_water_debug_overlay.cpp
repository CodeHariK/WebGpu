// MCGrid: the water debug overlay (Debug water) and the pour step-through (set_pour_step).
//
//   water     an overlay above the water, drawn on top of everything:
//               - a dot per wet sample of the sample grid, coloured by height
//               - a square per wet column: blue = held one layer deep, green = stream, navy = sea
//               - a white arrow along the flow (toward the nearest fall)
//               - a line along the top of each fall: green when it is cut into the land (framed),
//                 red when it pours off an open edge
//               - the last pour (fill_water): a dot per column it reached, coloured by the order the
//                 flood fill reached it (yellow first -> magenta last); replay_water_debug() replays
//                 it, the dots appearing in that order over two seconds
//               - or one step of that pour (set_pour_step; the Debug tab's pour step buttons): done columns yellow,
//                 the queue cyan, the column the step takes outlined white, a line to each neighbour
//                 (green queued, grey already queued, red + cross higher ground) and a label
//                 explaining the step
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/water/mc_water_steps.h"
#include "marching_cubes/util/mc_debug_mesh.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <algorithm>
#include <cmath>

namespace godot {

using mc_water::SIDE_NAMES;
using mc_water::STEPS;

namespace {
const Color HELD(0.25f, 0.55f, 1.0f, 0.55f);
const Color STREAM_COLOR(0.35f, 1.0f, 0.55f, 0.55f);
const Color SEA(0.1f, 0.2f, 0.55f, 0.35f);
const Color ARROW(1.0f, 1.0f, 1.0f, 0.9f);
const Color FRAMED(0.3f, 1.0f, 0.4f, 1.0f);
const Color OPEN_EDGE(1.0f, 0.3f, 0.25f, 1.0f);
const Color FIRST(1.0f, 0.9f, 0.2f, 1.0f);
const Color LAST(1.0f, 0.2f, 0.85f, 1.0f);
const float TURN = 6.2831853f; // a full turn in radians
const float LIFT = 0.04f; // m the water overlay floats above the surface
const float REPLAY_SECONDS = 2.0f;
// The water overlay: vertex colours, unshaded, drawn over everything; UV.x is the order a pour
// reached the vertex (0..1, or -1 for always shown); `reveal` hides what was reached later.
const char *WATER_DEBUG_SHADER = R"(shader_type spatial;
render_mode unshaded, cull_disabled, depth_test_disabled, depth_draw_never;
uniform float reveal = 1.0;
void fragment() {
	if (UV.x > reveal) {
		discard;
	}
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
}
)";
Ref<ShaderMaterial> material_from_code(const char *p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}
const Color DONE_DOT(1.0f, 0.85f, 0.3f, 0.9f); // taken off the queue already
const Color QUEUE_DOT(0.3f, 0.9f, 1.0f, 1.0f); // waiting in the queue
const Color CURRENT(1.0f, 1.0f, 1.0f, 1.0f); // the column this step takes
const Color RESULT_COLOR[4] = {
	Color(0.3f, 1.0f, 0.4f, 1.0f), // QUEUED
	Color(0.6f, 0.6f, 0.6f, 1.0f), // SEEN
	Color(1.0f, 0.3f, 0.25f, 1.0f), // HIGHER
	Color(0.6f, 0.6f, 0.6f, 0.0f), // OUTSIDE (not drawn)
};
const char *RESULT_TEXT[4] = { "queued", "already queued", "higher ground, rejected", "off the grid" };
} // namespace

void MCGrid::set_debug_water(bool p_on) {
	debug_water = p_on;
	if (!p_on && water_debug_node) {
		water_debug_node->hide();
	}
	_rebuild_water();
}

// Replay the last pour: its dots appear in the order the flood fill reached them.
void MCGrid::replay_water_debug() {
	if (!debug_water) {
		set_debug_water(true);
	}
	if (water_debug_material.is_null()) {
		return;
	}
	water_debug_material->set_shader_parameter("reveal", 0.0f);
	Ref<Tween> tween = create_tween();
	tween->tween_property(water_debug_material.ptr(), "shader_parameter/reveal", 1.0f, REPLAY_SECONDS);
}

// Build the water overlay from what _rebuild_water worked out (surfaces, held, flow).
void MCGrid::_rebuild_water_debug(
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow
) {
	if (!water_debug_node) {
		water_debug_node = memnew(MeshInstance3D);
		water_debug_node->set_name("WaterDebug");
		water_debug_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(water_debug_node);
		water_debug_material = material_from_code(WATER_DEBUG_SHADER);
		water_debug_material->set_render_priority(2);
	}
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	MCDebugMesh debug;
	bool tracing = water_trace_kind != 0 && pour_step >= 0; // step 2's trace draws the columns itself
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			size_t i = static_cast<size_t>(_column_index(x, z));
			if (!_is_wet(static_cast<int>(i)) || tracing) {
				continue;
			}
			float y = p_surface[i] + LIFT;
			Vector3 centre(static_cast<float>(x), y, static_cast<float>(z));
			bool sea = sea_level > 0 && water_levels[i] == sea_level && column_heights[i] < sea_level;
			debug.square(centre, 0.4f, sea ? SEA : (p_held[i] ? HELD : STREAM_COLOR));
			// the flow arrow: a shaft and two barbs
			const Color &flow = p_flow[i];
			if (flow.b > 0.5f) {
				Vector3 dir(flow.r, 0, flow.g);
				Vector3 side(-dir.z, 0, dir.x);
				Vector3 tip = centre + dir * 0.35f;
				debug.line(centre - dir * 0.25f, tip, ARROW);
				debug.line(tip, tip - dir * 0.15f + side * 0.1f, ARROW);
				debug.line(tip, tip - dir * 0.15f - side * 0.1f, ARROW);
			}
			// the top edge of each fall from this column: framed (green) or open (red)
			int level = water_levels[i];
			for (const Vector2i &step : STEPS) {
				Vector2i n(x + step.x, z + step.y);
				if (!inside(n)) {
					continue;
				}
				size_t j = static_cast<size_t>(_column_index(n.x, n.y));
				if (!_is_wet(static_cast<int>(j)) || p_surface[j] > p_surface[i] - 0.02f) {
					continue;
				}
				Vector3 out(static_cast<float>(step.x), 0, static_cast<float>(step.y));
				Vector3 across(std::abs(out.z), 0, std::abs(out.x));
				Vector3 mid = centre + out * 0.5f;
				bool framed = water_levels[j] < level && _fall_framed(Vector2i(x, z), step, level);
				debug.line(mid - across * 0.45f, mid + across * 0.45f, framed ? FRAMED : OPEN_EDGE);
			}
		}
	}
	// the debug stages (how the shape is built; mc_water_debug_stages.cpp)
	_add_water_stage_debug(debug, p_surface, p_held, p_flow);
	// the sample grid: a dot per wet sample, coloured by its height (stage 0 and 3)
	if (!water_debug_samples.empty() && !tracing && (water_debug_stage == 0 || water_debug_stage == 3)) {
		float lo = 1e6f;
		float hi = -1e6f;
		for (const Vector3 &s : water_debug_samples) {
			lo = std::min(lo, s.y);
			hi = std::max(hi, s.y);
		}
		for (const Vector3 &s : water_debug_samples) {
			float t = hi > lo ? (s.y - lo) / (hi - lo) : 0.0f;
			Color c = Color(0.2f, 0.4f, 1.0f).lerp(Color(1.0f, 0.4f, 0.2f), t);
			debug.square(s + Vector3(0, LIFT * 1.5f, 0), 0.012f, c);
		}
	}
	// the last pour: one step of it (step-by-step debug), or a dot per column in the order it was reached
	if (pour_step >= 0 && water_trace_kind != 0) {
		_add_water_trace_debug(debug, p_surface); // step 2 (mc_water_trace.cpp)
	} else if (pour_step >= 0) {
		_add_pour_step_debug(debug, p_surface);
	} else if (pour_step_label) {
		pour_step_label->hide();
	}
	size_t count = pour_step >= 0 || water_trace_kind != 0 ? 0 : last_pour.size();
	for (size_t k = 0; k < count; k++) {
		size_t i = static_cast<size_t>(last_pour[k]);
		if (i >= p_surface.size() || !_is_wet(static_cast<int>(i))) {
			continue;
		}
		float order = count > 1 ? static_cast<float>(k) / static_cast<float>(count - 1) : 0.0f;
		Vector2i columns_xy = _column_counts();
		Vector3 centre(static_cast<float>(static_cast<int>(i) % columns_xy.x), p_surface[i] + LIFT * 2.0f,
				static_cast<float>(static_cast<int>(i) / columns_xy.x));
		debug.square(centre, 0.12f, FIRST.lerp(LAST, order), order * 0.999f);
	}
	Ref<ArrayMesh> mesh = debug.commit();
	for (int s = 0; s < mesh->get_surface_count(); s++) {
		mesh->surface_set_material(s, water_debug_material);
	}
	water_debug_node->set_mesh(mesh);
	water_debug_node->show();
}

// --- the pour, step by step ---

void MCGrid::set_pour_step(int p_step) {
	int count = get_pour_step_count();
	pour_step = count == 0 ? -1 : std::clamp(p_step, -1, count - 1);
	if (!debug_water) {
		set_debug_water(true); // rebuilds
	} else {
		_rebuild_water();
	}
}

String MCGrid::get_pour_step_text() const {
	if (pour_step < 0 || pour_step >= get_pour_step_count()) {
		return String();
	}
	if (water_trace_kind != 0) { // step 2's traces (mc_water_trace.cpp)
		return vformat("step %d / %d\n%s", pour_step + 1, get_pour_step_count(), water_trace[static_cast<size_t>(pour_step)].text);
	}
	const MCPourStep &step = last_pour_trace[static_cast<size_t>(pour_step)];
	Vector2i columns = _column_counts();
	Vector2i c(step.column % columns.x, step.column / columns.x);
	int ground = column_heights[static_cast<size_t>(step.column)];
	String text = vformat("step %d / %d: take (%d, %d) off the queue, ground %d", pour_step + 1,
			get_pour_step_count(), c.x, c.y, ground);
	int pours = last_pour_trace.back().pour + 1;
	if (pours > 1) { // trace_settle: which source this pour started from
		int source = last_pour[static_cast<size_t>(pour_step - step.head)];
		text += vformat("\npour %d / %d, from (%d, %d)", step.pour + 1, pours, source % columns.x, source / columns.x);
	}
	for (int k = 0; k < 4; k++) {
		Vector2i n = c + STEPS[k];
		if (step.result[k] == MCPourStep::OUTSIDE) {
			text += vformat("\n%s: off the grid", SIDE_NAMES[k]);
			continue;
		}
		int other = column_heights[static_cast<size_t>(_column_index(n.x, n.y))];
		text += vformat("\n%s (%d, %d) ground %d: %s", SIDE_NAMES[k], n.x, n.y, other, RESULT_TEXT[step.result[k]]);
	}
	int waiting = step.queue_size - step.head - 1;
	text += vformat("\nqueue: %d waiting", waiting);
	return text;
}

// Draw the pour as it stood at pour_step: the columns already done (yellow dots), the queue (cyan
// dots), the column this step takes (white square) with a line to each neighbour coloured by what
// happened to it (green queued, grey already queued, red higher ground), and a label explaining it.
void MCGrid::_add_pour_step_debug(
		MCDebugMesh &r_debug,
		const std::vector<float> &p_surface
) {
	if (pour_step < 0 || pour_step >= get_pour_step_count()) {
		return;
	}
	Vector2i columns = _column_counts();
	// where to draw over a column: its water, or its ground if dry
	auto top_of = [&](int p_index) {
		size_t i = static_cast<size_t>(p_index);
		float y = _is_wet(p_index) ? p_surface[i] : static_cast<float>(column_heights[i]) - 0.5f;
		return Vector3(static_cast<float>(p_index % columns.x), y + LIFT * 3.0f, static_cast<float>(p_index / columns.x));
	};
	const MCPourStep &step = last_pour_trace[static_cast<size_t>(pour_step)];
	// done: the columns taken before this step; queue: found but not taken yet
	for (int k = 0; k < pour_step; k++) {
		r_debug.square(top_of(last_pour[static_cast<size_t>(k)]), 0.1f, DONE_DOT);
	}
	int queue_end = pour_step - step.head + step.queue_size; // this pour's queue so far
	for (int k = pour_step + 1; k < queue_end && k < static_cast<int>(last_pour.size()); k++) {
		r_debug.square(top_of(last_pour[static_cast<size_t>(k)]), 0.16f, QUEUE_DOT);
	}
	// the column this step takes, outlined, and its four neighbours
	Vector3 centre = top_of(step.column);
	const Vector3 ring[4] = { Vector3(-0.45f, 0, -0.45f), Vector3(0.45f, 0, -0.45f), Vector3(0.45f, 0, 0.45f),
		Vector3(-0.45f, 0, 0.45f) };
	for (int k = 0; k < 4; k++) {
		r_debug.line(centre + ring[k], centre + ring[(k + 1) % 4], CURRENT);
	}
	Vector2i c(step.column % columns.x, step.column / columns.x);
	for (int k = 0; k < 4; k++) {
		if (step.result[k] == MCPourStep::OUTSIDE) {
			continue;
		}
		Vector2i n = c + STEPS[k];
		Vector3 to = top_of(_column_index(n.x, n.y));
		const Color &color = RESULT_COLOR[step.result[k]];
		r_debug.line(centre, to, color);
		if (step.result[k] == MCPourStep::HIGHER) { // a cross on the rejected column
			r_debug.line(to + Vector3(-0.2f, 0, -0.2f), to + Vector3(0.2f, 0, 0.2f), color);
			r_debug.line(to + Vector3(-0.2f, 0, 0.2f), to + Vector3(0.2f, 0, -0.2f), color);
		} else if (step.result[k] == MCPourStep::QUEUED) { // a square on the newly queued column
			r_debug.square(to, 0.16f, color);
		}
	}
}

} // namespace godot
