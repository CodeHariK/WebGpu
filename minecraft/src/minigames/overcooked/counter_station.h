#ifndef COUNTER_STATION_H
#define COUNTER_STATION_H

#include "../../interaction/interactable.h"

namespace godot {

class CounterStation : public Interactable {
	GDCLASS(CounterStation, Interactable)

protected:
	Interactable *held_item = nullptr;
	Node3D *item_slot = nullptr;

	static void _bind_methods();

public:
	CounterStation();
	~CounterStation();

	void _ready() override;

	// Overriding Interactable methods
	void interact(Node3D *p_actor) override;
	void pickup(Node3D *p_actor) override;

	virtual bool can_place_item(Interactable *item);
	virtual void place_item(Interactable *item);
	virtual Interactable *take_item();

	bool has_item() const { return held_item != nullptr; }
};

} // namespace godot

#endif // COUNTER_STATION_H
