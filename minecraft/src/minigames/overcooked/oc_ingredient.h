#ifndef INGREDIENT_H
#define INGREDIENT_H

#include "../../interaction/interactable.h"
#include "oc_manager.h"
#include "oc_types.h"
#include <cstdint>

namespace godot {

class OCIngredient : public Interactable {
	GDCLASS(OCIngredient, Interactable)

public:

private:
	IngredientState current_state = INGREDIENT_STATE_RAW;
	float process_progress = 0.0f;
	IngredientType ingredient_type = INGREDIENT_GENERIC;

	OvercookedManager *om = nullptr;

protected:
	static void _bind_methods();

public:
	OCIngredient();
	~OCIngredient();

	void _enter_tree() override;
	void _ready() override;
	void _exit_tree() override;
	void _process(double delta) override;

	void drop(Node3D *p_actor) override;

	void set_state(IngredientState p_state);
	IngredientState get_state() const;

	void set_process_progress(float p_progress);
	float get_process_progress() const;

	void set_ingredient_type(IngredientType p_type);
	IngredientType get_ingredient_type() const;

	bool can_be_chopped() const { return current_state == INGREDIENT_STATE_RAW; }
	bool can_be_cooked() const { return current_state == INGREDIENT_STATE_CHOPPED; }

	void update_visuals();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::IngredientState);
VARIANT_ENUM_CAST(godot::IngredientType);

#endif // INGREDIENT_H
