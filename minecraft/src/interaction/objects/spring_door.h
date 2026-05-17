#ifndef SPRING_DOOR_H
#define SPRING_DOOR_H

#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

class SpringDoor : public RigidBody3D {
	GDCLASS(SpringDoor, RigidBody3D)

private:
	float stiffness = 30.0f;
	float damping = 5.0f;
	float rest_rotation_y = 0.0f;
	Vector3 hinge_offset = Vector3(0, 0, 0);

protected:
	static void _bind_methods();

public:
	SpringDoor();
	~SpringDoor();

	void _ready() override;
	void _physics_process(double delta) override;

	void set_stiffness(float p_stiffness);
	float get_stiffness() const;

	void set_damping(float p_damping);
	float get_damping() const;

	void set_hinge_offset(Vector3 p_offset);
	Vector3 get_hinge_offset() const;
};

} // namespace godot

#endif // SPRING_DOOR_H
