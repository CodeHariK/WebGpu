#ifndef WHEEL_CONFIG_H
#define WHEEL_CONFIG_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class WheelConfig : public Resource {
	GDCLASS(WheelConfig, Resource)

private:
	Vector3 hardpoint_offset;
	float radius = 0.5f;
	float suspension_rest_length = 0.6f;
	float suspension_stiffness = 20000.0f;
	float compression_damping = 4000.0f;
	float rebound_damping = 1500.0f;

protected:
	static void _bind_methods();

public:
	WheelConfig();
	~WheelConfig();

	void set_hardpoint_offset(const Vector3 &p_offset);
	Vector3 get_hardpoint_offset() const;

	void set_radius(float p_radius);
	float get_radius() const;

	void set_suspension_rest_length(float p_length);
	float get_suspension_rest_length() const;

	void set_suspension_stiffness(float p_stiffness);
	float get_suspension_stiffness() const;

	void set_compression_damping(float p_damping);
	float get_compression_damping() const;

	void set_rebound_damping(float p_damping);
	float get_rebound_damping() const;
};

} // namespace godot

#endif // WHEEL_CONFIG_H
