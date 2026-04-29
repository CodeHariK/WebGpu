#ifndef INTERACTOR_H
#define INTERACTOR_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ray_cast3d.hpp>
#include "interactable.h"

namespace godot {

class Interactor : public Node3D {
	GDCLASS(Interactor, Node3D)

private:
	float interaction_range = 2.0f;
	uint32_t interaction_mask = 1 << 3; // Default to Layer 4 (LAYER_OBJECTS)
	
	Interactable *held_item = nullptr;
	Node3D *hand_marker = nullptr;

protected:
	static void _bind_methods();

public:
	Interactor();
	~Interactor();

	void _ready() override;

	void grab_or_drop();
	void interact();

	void set_interaction_range(float p_range);
	float get_interaction_range() const;

	void set_interaction_mask(uint32_t p_mask);
	uint32_t get_interaction_mask() const;

	Interactable *get_held_item() const;
};

} // namespace godot

#endif // INTERACTOR_H
