#ifndef OC_STATION_H
#define OC_STATION_H

#include "../../interaction/interactable.h"
#include "oc_recipe.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>

namespace godot {

class OCIngredient;
class OCPlate;

class OCStation : public Interactable {
	GDCLASS(OCStation, Interactable)

public:
	enum StationType {
		TYPE_COUNTER = 0,
		TYPE_CUTTING,
		TYPE_COOKING,
		TYPE_BLENDER,
		TYPE_CRATE,
		TYPE_DELIVERY,
		TYPE_TRASH
	};

protected:
	Interactable *held_item = nullptr;
	Node3D *item_slot = nullptr;

	StationType station_type = TYPE_COUNTER;
	std::vector<OCProcessStep> steps;

	class OvercookedManager *om = nullptr;

	void update_held_item_position();
	void _process_ingredient(OCIngredient *p_ingredient, float p_delta);
	bool _interact_ingredient(OCIngredient *p_ingredient);

	static void _bind_methods();

public:
	OCStation();
	~OCStation();

	void _ready() override;
	void _exit_tree() override;
	void _process(double delta) override;

	// Interaction Interface
	virtual void interact(Node3D *p_actor) override;
	virtual void pickup(Node3D *p_actor) override;

	// Item Management
	virtual bool can_place_item(Interactable *item);
	virtual void place_item(Interactable *item);
	virtual Interactable *take_item();
	virtual bool has_item() const { return station_type == TYPE_CRATE || held_item != nullptr; }
	virtual Interactable *get_held_item() const { return held_item; }

	bool can_process(OCIngredient *p_ingredient) const;

	// Getters/Setters
	void set_station_name(const String &p_name);
	String get_station_name() const;

	void set_station_type(StationType p_type);
	StationType get_station_type() const;

	void add_step(int p_input, int p_output, float p_speed, bool p_auto, ProcessOperation p_op = PROCESS_NONE);
	void clear_steps();
};

} // namespace godot

VARIANT_ENUM_CAST(OCStation::StationType);

#endif // OC_STATION_H
