#ifndef PHYSICS_PUSHER_H
#define PHYSICS_PUSHER_H

#include <godot_cpp/classes/node.hpp>

namespace godot {

class PhysicsPusher : public Node {
	GDCLASS(PhysicsPusher, Node)

private:
	float push_force = 5.0f;

protected:
	static void _bind_methods();

public:
	PhysicsPusher();
	~PhysicsPusher();

	void _physics_process(double delta) override;

	void set_push_force(float p_force);
	float get_push_force() const;
};

} // namespace godot

#endif // PHYSICS_PUSHER_H
