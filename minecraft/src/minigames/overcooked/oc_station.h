#ifndef OC_STATION_H
#define OC_STATION_H

#include "../../interaction/interactable.h"
#include <godot_cpp/classes/node3d.hpp>

namespace godot {

/**
 * @brief Base class for all stations in the Overcooked minigame.
 * Handles item slotting, registration, and basic interaction.
 */
class OCStation : public Interactable {
	GDCLASS(OCStation, Interactable)

protected:
	String station_name = "OCStation";
	Interactable *held_item = nullptr;
	Node3D *item_slot = nullptr;

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
	bool has_item() const { return held_item != nullptr; }

	// Getters/Setters
	void set_station_name(const String &p_name);
	String get_station_name() const;
};

} // namespace godot

#endif // OC_STATION_H
