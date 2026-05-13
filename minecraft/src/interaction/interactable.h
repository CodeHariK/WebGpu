#ifndef INTERACTABLE_H
#define INTERACTABLE_H

#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

class Interactable : public RigidBody3D {
	GDCLASS(Interactable, RigidBody3D)

private:
	bool is_interactable = true;
	bool is_picked_up = false;
	Node3D *current_owner = nullptr;

protected:
	static void _bind_methods();

public:
	Interactable();
	~Interactable();

	virtual void interact(Node3D *p_actor);
	virtual void pickup(Node3D *p_actor);
	virtual void drop(Node3D *p_actor);

	void set_is_interactable(bool p_interactable);
	bool get_is_interactable() const;

	void set_is_picked_up(bool p_picked_up);
	bool get_is_picked_up() const;

	void set_current_owner(Node3D *p_owner);
	Node3D *get_current_owner() const;

	void attach_to(Node3D *p_parent);
};

} // namespace godot

#endif // INTERACTABLE_H
