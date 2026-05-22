#ifndef VEHICLE_STATE_H
#define VEHICLE_STATE_H

#include "godot_cpp/variant/string.hpp"
#include <godot_cpp/core/defs.hpp>

namespace godot {

class ArcadeVehicle;

/**
 * Base class for all vehicle states in the Hierarchical State Machine.
 * Child states can delegate unhandled logic to their parent states.
 */
class VehicleState {
protected:
	ArcadeVehicle *vehicle = nullptr;
	VehicleState *parent = nullptr;

public:
	String state_name = "";

	VehicleState(String p_name, ArcadeVehicle *p_vehicle, VehicleState *p_parent = nullptr) :
			state_name(p_name), vehicle(p_vehicle), parent(p_parent) {}
	virtual ~VehicleState() {}

	// Called when this specific state is entered
	virtual void enter() {}

	// Called when this specific state is exited
	virtual void exit() {}

	// Standard frame update
	virtual void update(float delta) {
		if (parent) {
			parent->update(delta);
		}
	}

	// Physics frame update
	virtual void physics_update(float delta) {
		if (parent) {
			parent->physics_update(delta);
		}
	}

	// Helper to check if this state or any parent is of a certain type
	// (Useful for high-level logic checks)
	virtual bool is_in_state(VehicleState *p_other) const {
		if (this == p_other) {
			return true;
		}
		return parent ? parent->is_in_state(p_other) : false;
	}
};

} // namespace godot

#endif // VEHICLE_STATE_H
