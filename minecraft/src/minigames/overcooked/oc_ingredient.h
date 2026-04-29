#ifndef INGREDIENT_H
#define INGREDIENT_H

#include "../../interaction/interactable.h"
#include "oc_manager.h"

namespace godot {

class OCIngredient : public Interactable {
	GDCLASS(OCIngredient, Interactable)

public:
	enum State {
		STATE_RAW = 0,
		STATE_CHOPPED,
		STATE_COOKED,
		STATE_BURNT
	};

private:
	State current_state = STATE_RAW;
	float process_progress = 0.0f;

protected:
	static void _bind_methods();
	OvercookedManager *om = nullptr;

public:
	OCIngredient();
	~OCIngredient();

	void _ready() override;
	void _exit_tree() override;
	void _process(double delta) override;

	void drop(Node3D *p_actor) override;

	void set_state(State p_state);
	State get_state() const;

	void set_process_progress(float p_progress);
	float get_process_progress() const;

	void update_visuals();
};

} // namespace godot

VARIANT_ENUM_CAST(OCIngredient::State);

#endif // INGREDIENT_H
