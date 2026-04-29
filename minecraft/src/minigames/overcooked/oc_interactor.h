#ifndef OVERCOOKED_INTERACTOR_H
#define OVERCOOKED_INTERACTOR_H

#include "../../game_manager/game_constants.h"
#include "../../interaction/interactable.h"
#include "oc_manager.h"
#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class GameManager;
class PlayerInput;

class OCInteractor : public Node3D {
	GDCLASS(OCInteractor, Node3D)

private:
	float interaction_range = InteractionDefaults::RANGE;
	uint32_t interaction_mask = InteractionDefaults::MASK;

	Interactable *held_item = nullptr;
	Node3D *hand_marker = nullptr;

	OvercookedManager *om = nullptr;
	GameManager *gm = nullptr;
	PlayerInput *player_input = nullptr;

protected:
	static void _bind_methods();

public:
	OCInteractor();
	~OCInteractor();

	void _ready() override;
	void _physics_process(double delta) override;

	void grab_or_drop();
	void interact(Node3D *p_actor = nullptr);

	void set_interaction_range(float p_range);
	float get_interaction_range() const;

	void set_interaction_mask(uint32_t p_mask);
	uint32_t get_interaction_mask() const;

	Interactable *get_held_item() const;
};

} // namespace godot

#endif // OVERCOOKED_INTERACTOR_H
