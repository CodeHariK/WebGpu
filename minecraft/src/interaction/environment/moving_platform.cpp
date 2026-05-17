#include "moving_platform.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MovingPlatform::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_preset", "preset"), &MovingPlatform::set_preset);
	ClassDB::bind_method(D_METHOD("get_preset"), &MovingPlatform::get_preset);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "preset", PROPERTY_HINT_ENUM, "Ping-Pong,Circular,Helios (Spiral),Custom Expressions"), "set_preset", "get_preset");

	ClassDB::bind_method(D_METHOD("set_x_expression", "expr"), &MovingPlatform::set_x_expression);
	ClassDB::bind_method(D_METHOD("get_x_expression"), &MovingPlatform::get_x_expression);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "x_expression"), "set_x_expression", "get_x_expression");

	ClassDB::bind_method(D_METHOD("set_y_expression", "expr"), &MovingPlatform::set_y_expression);
	ClassDB::bind_method(D_METHOD("get_y_expression"), &MovingPlatform::get_y_expression);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "y_expression"), "set_y_expression", "get_y_expression");

	ClassDB::bind_method(D_METHOD("set_z_expression", "expr"), &MovingPlatform::set_z_expression);
	ClassDB::bind_method(D_METHOD("get_z_expression"), &MovingPlatform::get_z_expression);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "z_expression"), "set_z_expression", "get_z_expression");

	ClassDB::bind_method(D_METHOD("set_time_scale", "scale"), &MovingPlatform::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &MovingPlatform::get_time_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_scale"), "set_time_scale", "get_time_scale");

	ClassDB::bind_method(D_METHOD("set_ping_pong_offset", "offset"), &MovingPlatform::set_ping_pong_offset);
	ClassDB::bind_method(D_METHOD("get_ping_pong_offset"), &MovingPlatform::get_ping_pong_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "ping_pong_offset"), "set_ping_pong_offset", "get_ping_pong_offset");

	ClassDB::bind_method(D_METHOD("set_circular_radius", "radius"), &MovingPlatform::set_circular_radius);
	ClassDB::bind_method(D_METHOD("get_circular_radius"), &MovingPlatform::get_circular_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "circular_radius"), "set_circular_radius", "get_circular_radius");
}

MovingPlatform::MovingPlatform() {}
MovingPlatform::~MovingPlatform() {}

void MovingPlatform::_ready() {
	start_position = get_global_position();
	compile_expressions();
	is_initialized = true;
}

void MovingPlatform::compile_expressions() {
	x_expr.instantiate();
	y_expr.instantiate();
	z_expr.instantiate();

	PackedStringArray input_names;
	input_names.append("t");

	Error err_x = x_expr->parse(x_expr_str, input_names);
	Error err_y = y_expr->parse(y_expr_str, input_names);
	Error err_z = z_expr->parse(z_expr_str, input_names);

	if (err_x != OK || err_y != OK || err_z != OK) {
		UtilityFunctions::print("MovingPlatform: Parse error in custom expressions!");
		if (err_x != OK) UtilityFunctions::print("  X error: ", x_expr->get_error_text());
		if (err_y != OK) UtilityFunctions::print("  Y error: ", y_expr->get_error_text());
		if (err_z != OK) UtilityFunctions::print("  Z error: ", z_expr->get_error_text());
		expr_parse_success = false;
	} else {
		expr_parse_success = true;
	}
}

Vector3 MovingPlatform::calculate_relative_position(float t) {
	Vector3 offset(0, 0, 0);

	switch (preset) {
		case PRESET_PING_PONG: {
			// Ping-pong smoothly between start_position and start_position + ping_pong_offset
			float factor = (Math::sin(t) + 1.0f) * 0.5f;
			offset = ping_pong_offset * factor;
			break;
		}
		case PRESET_CIRCULAR: {
			// Circular orbit in XZ plane
			offset.x = Math::cos(t) * circular_radius;
			offset.z = Math::sin(t) * circular_radius;
			break;
		}
		case PRESET_HELIOS: {
			// Helix: Circular in XZ, upward climbing in Y (resetting to repeat or continuous)
			offset.x = Math::cos(t) * circular_radius;
			offset.y = t * 0.5f; // Climbing upward over time
			offset.z = Math::sin(t) * circular_radius;
			break;
		}
		case PRESET_CUSTOM_EXPRESSIONS: {
			if (!expr_parse_success) {
				break;
			}
			
			Array inputs;
			inputs.append(t);

			Variant x_val = x_expr->execute(inputs, nullptr, false);
			Variant y_val = y_expr->execute(inputs, nullptr, false);
			Variant z_val = z_expr->execute(inputs, nullptr, false);

			if (!x_expr->has_execute_failed()) offset.x = (float)x_val;
			if (!y_expr->has_execute_failed()) offset.y = (float)y_val;
			if (!z_expr->has_execute_failed()) offset.z = (float)z_val;
			break;
		}
	}

	return offset;
}

void MovingPlatform::_physics_process(double delta) {
	running_time += (float)delta * time_scale;

	// Calculate the next target relative position
	Vector3 rel_pos = calculate_relative_position(running_time);

	// Update our global position. Because AnimatableBody3D computes linear velocity 
	// based on frame-to-frame position delta, setting the position here automatically 
	// provides correct physical velocity to carrying/colliding CharacterBody3D actors.
	set_global_position(start_position + rel_pos);
}

// Getters and Setters
void MovingPlatform::set_preset(int p_preset) {
	preset = (MovementPreset)p_preset;
}

int MovingPlatform::get_preset() const {
	return (int)preset;
}

void MovingPlatform::set_x_expression(const String &p_expr) {
	x_expr_str = p_expr;
	if (is_initialized) {
		compile_expressions();
	}
}

String MovingPlatform::get_x_expression() const {
	return x_expr_str;
}

void MovingPlatform::set_y_expression(const String &p_expr) {
	y_expr_str = p_expr;
	if (is_initialized) {
		compile_expressions();
	}
}

String MovingPlatform::get_y_expression() const {
	return y_expr_str;
}

void MovingPlatform::set_z_expression(const String &p_expr) {
	z_expr_str = p_expr;
	if (is_initialized) {
		compile_expressions();
	}
}

String MovingPlatform::get_z_expression() const {
	return z_expr_str;
}

void MovingPlatform::set_time_scale(float p_scale) {
	time_scale = p_scale;
}

float MovingPlatform::get_time_scale() const {
	return time_scale;
}

void MovingPlatform::set_ping_pong_offset(Vector3 p_offset) {
	ping_pong_offset = p_offset;
}

Vector3 MovingPlatform::get_ping_pong_offset() const {
	return ping_pong_offset;
}

void MovingPlatform::set_circular_radius(float p_radius) {
	circular_radius = p_radius;
}

float MovingPlatform::get_circular_radius() const {
	return circular_radius;
}

} // namespace godot
