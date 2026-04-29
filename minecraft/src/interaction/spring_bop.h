#ifndef SPRING_BOP_H
#define SPRING_BOP_H

#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

class SpringBop : public RigidBody3D {
	GDCLASS(SpringBop, RigidBody3D)

private:
	float stiffness = 40.0f;
	float damping = 4.0f;
	bool lock_y_rotation = false;
	
	Vector3 rest_rotation;
	Vector3 hinge_offset = Vector3(0, 0, 0);

protected:
	static void _bind_methods();

public:
	SpringBop();
	~SpringBop();

	void _ready() override;
	void _physics_process(double delta) override;

	void set_stiffness(float p_stiffness);
	float get_stiffness() const;

	void set_damping(float p_damping);
	float get_damping() const;

	void set_lock_y_rotation(bool p_lock);
	bool get_lock_y_rotation() const;

	void set_hinge_offset(Vector3 p_offset);
	Vector3 get_hinge_offset() const;
};

} // namespace godot

#endif // SPRING_BOP_H
