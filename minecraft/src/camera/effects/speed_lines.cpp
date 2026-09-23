#include "speed_lines.h"

#include "../../utils/spring/spring_dynamics.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void SpeedLines::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_path", "path"), &SpeedLines::set_target_path);
	ClassDB::bind_method(D_METHOD("get_target_path"), &SpeedLines::get_target_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_path"), "set_target_path", "get_target_path");

	ClassDB::bind_method(D_METHOD("set_speed_ratio", "ratio"), &SpeedLines::set_speed_ratio);

	ClassDB::bind_method(D_METHOD("set_speed_min", "v"), &SpeedLines::set_speed_min);
	ClassDB::bind_method(D_METHOD("get_speed_min"), &SpeedLines::get_speed_min);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_min"), "set_speed_min", "get_speed_min");

	ClassDB::bind_method(D_METHOD("set_speed_max", "v"), &SpeedLines::set_speed_max);
	ClassDB::bind_method(D_METHOD("get_speed_max"), &SpeedLines::get_speed_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_max"), "set_speed_max", "get_speed_max");

	ClassDB::bind_method(D_METHOD("set_intensity_max", "v"), &SpeedLines::set_intensity_max);
	ClassDB::bind_method(D_METHOD("get_intensity_max"), &SpeedLines::get_intensity_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "intensity_max"), "set_intensity_max", "get_intensity_max");

	ClassDB::bind_method(D_METHOD("set_line_color", "c"), &SpeedLines::set_line_color);
	ClassDB::bind_method(D_METHOD("get_line_color"), &SpeedLines::get_line_color);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "line_color"), "set_line_color", "get_line_color");

	ClassDB::bind_method(D_METHOD("set_vignette_strength", "v"), &SpeedLines::set_vignette_strength);
	ClassDB::bind_method(D_METHOD("get_vignette_strength"), &SpeedLines::get_vignette_strength);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vignette_strength"), "set_vignette_strength", "get_vignette_strength");
}

SpeedLines::SpeedLines() {}
SpeedLines::~SpeedLines() {}

void SpeedLines::_build_overlay() {
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/game/speed_lines.gdshader");

	material.instantiate();
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("intensity", 0.0f);
	material->set_shader_parameter("line_color", line_color);
	material->set_shader_parameter("vignette_strength", vignette_strength);

	rect = memnew(ColorRect);
	rect->set_name("SpeedLinesRect");
	rect->set_anchors_preset(Control::PRESET_FULL_RECT);
	rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	// The shader paints its own alpha; the base rect must be transparent.
	rect->set_color(Color(0, 0, 0, 0));
	rect->set_material(material);
	add_child(rect);
}

void SpeedLines::_ready() {
	set_layer(layer_index);
	_build_overlay();

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	_resolve_target();

	// Debug screenshot: `--fxshot` forces the effect on and grabs a frame.
	PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
	if (args.has("--fxshot")) {
		debug_force = true;
		Ref<SceneTreeTimer> t = get_tree()->create_timer(1.2);
		t->connect("timeout", callable_mp(this, &SpeedLines::_fx_capture));
	}
}

void SpeedLines::_resolve_target() {
	if (target_path.is_empty()) {
		target_node = nullptr;
		return;
	}
	target_node = Object::cast_to<Node3D>(get_node_or_null(target_path));
}

void SpeedLines::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint() || material.is_null()) {
		return;
	}

	float ratio = external_ratio;
	if (target_node || !target_path.is_empty()) {
		if (!target_node) {
			_resolve_target();
		}
		RigidBody3D *rb = Object::cast_to<RigidBody3D>(target_node);
		if (rb) {
			float speed = rb->get_linear_velocity().length();
			float span = MAX(0.001f, speed_max - speed_min);
			ratio = CLAMP((speed - speed_min) / span, 0.0f, 1.0f);
		}
	}

	if (debug_force) {
		ratio = MAX(ratio, 0.9f);
	}

	float target_intensity = ratio * intensity_max;
	current_intensity = Math::lerp(current_intensity, target_intensity, spring_damp_factor(smooth_rate, (float)p_delta));

	material->set_shader_parameter("intensity", current_intensity);
	material->set_shader_parameter("line_color", line_color);
	material->set_shader_parameter("vignette_strength", vignette_strength);
}

void SpeedLines::set_target_path(const NodePath &p_path) {
	target_path = p_path;
	_resolve_target();
}

void SpeedLines::set_speed_ratio(float p_ratio) {
	external_ratio = CLAMP(p_ratio, 0.0f, 1.0f);
}

void SpeedLines::_fx_capture() {
	Ref<Image> img = get_viewport()->get_texture()->get_image();
	if (img.is_valid()) {
		img->save_png("res://../docs/png/speed_lines.png");
	}
	get_tree()->quit();
}

} // namespace godot
