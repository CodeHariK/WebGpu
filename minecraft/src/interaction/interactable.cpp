#include "interactable.h"

namespace godot {

void Interactable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_is_interactable", "interactable"), &Interactable::set_is_interactable);
	ClassDB::bind_method(D_METHOD("get_is_interactable"), &Interactable::get_is_interactable);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_interactable"), "set_is_interactable", "get_is_interactable");

	ClassDB::bind_method(D_METHOD("set_is_picked_up", "picked_up"), &Interactable::set_is_picked_up);
	ClassDB::bind_method(D_METHOD("get_is_picked_up"), &Interactable::get_is_picked_up);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_picked_up"), "set_is_picked_up", "get_is_picked_up");

	ClassDB::bind_method(D_METHOD("interact", "actor"), &Interactable::interact);
	ClassDB::bind_method(D_METHOD("pickup", "actor"), &Interactable::pickup);
	ClassDB::bind_method(D_METHOD("drop", "actor"), &Interactable::drop);
}

Interactable::Interactable() {
	// Default to static/frozen behavior (e.g. for Stations)
	set_freeze_enabled(true);
	set_freeze_mode(FREEZE_MODE_STATIC);
}

Interactable::~Interactable() {}

void Interactable::interact(Node3D *p_actor) {
	// Virtual method for subclasses to override
}

void Interactable::pickup(Node3D *p_actor) {
	is_picked_up = true;
	set_freeze_enabled(true); // Disable physics when held
}

void Interactable::drop(Node3D *p_actor) {
	is_picked_up = false;
	// Subclasses will decide if they should unfreeze
}

void Interactable::set_is_interactable(bool p_interactable) { is_interactable = p_interactable; }
bool Interactable::get_is_interactable() const { return is_interactable; }

void Interactable::set_is_picked_up(bool p_picked_up) { is_picked_up = p_picked_up; }
bool Interactable::get_is_picked_up() const { return is_picked_up; }

void Interactable::set_current_owner(Node3D *p_owner) { current_owner = p_owner; }
Node3D *Interactable::get_current_owner() const { return current_owner; }

} // namespace godot
