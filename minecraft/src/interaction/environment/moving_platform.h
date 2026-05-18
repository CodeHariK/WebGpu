#ifndef MOVING_PLATFORM_H
#define MOVING_PLATFORM_H

#include <cstdint>
#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/expression.hpp>

namespace godot {

class MovingPlatform : public AnimatableBody3D {
	GDCLASS(MovingPlatform, AnimatableBody3D)

public:
	enum MovementPreset : uint8_t {
		PRESET_PING_PONG = 0,
		PRESET_CIRCULAR = 1,
		PRESET_HELIOS = 2,
		PRESET_CUSTOM_EXPRESSIONS = 3
	};

private:
	MovementPreset preset = PRESET_PING_PONG;

	// Expression properties
	String x_expr_str = "sin(t) * 3.0";
	String y_expr_str = "0.0";
	String z_expr_str = "cos(t) * 3.0";

	Ref<Expression> x_expr;
	Ref<Expression> y_expr;
	Ref<Expression> z_expr;

	bool expr_parse_success = false;

	// Tuning Parameters
	float time_scale = 1.0f;
	Vector3 ping_pong_offset = Vector3(5.0f, 0.0f, 0.0f);
	float circular_radius = 3.0f;

	// Runtime variables
	float running_time = 0.0f;
	Vector3 start_position;
	bool is_initialized = false;
	Vector3 velocity;

	void compile_expressions();
	Vector3 calculate_relative_position(float t);

protected:
	static void _bind_methods();

public:
	MovingPlatform();
	~MovingPlatform();

	void _ready() override;
	void _physics_process(double delta) override;

	Vector3 get_velocity() const { return velocity; }

	// Getters and Setters
	void set_preset(int p_preset);
	int get_preset() const;

	void set_x_expression(const String &p_expr);
	String get_x_expression() const;

	void set_y_expression(const String &p_expr);
	String get_y_expression() const;

	void set_z_expression(const String &p_expr);
	String get_z_expression() const;

	void set_time_scale(float p_scale);
	float get_time_scale() const;

	void set_ping_pong_offset(Vector3 p_offset);
	Vector3 get_ping_pong_offset() const;

	void set_circular_radius(float p_radius);
	float get_circular_radius() const;
};

} // namespace godot

#endif // MOVING_PLATFORM_H
