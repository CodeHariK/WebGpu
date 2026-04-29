#include "ingredient.h"
#include "../../debug_draw/debug_manager.h"
#include "overcooked_manager.h"
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void Ingredient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_state", "state"), &Ingredient::set_state);
	ClassDB::bind_method(D_METHOD("get_state"), &Ingredient::get_state);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_state", PROPERTY_HINT_ENUM, "Raw,Chopped,Cooked,Burnt"), "set_state", "get_state");

	ClassDB::bind_method(D_METHOD("set_process_progress", "progress"), &Ingredient::set_process_progress);
	ClassDB::bind_method(D_METHOD("get_process_progress"), &Ingredient::get_process_progress);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "process_progress", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_process_progress", "get_process_progress");

	BIND_ENUM_CONSTANT(STATE_RAW);
	BIND_ENUM_CONSTANT(STATE_CHOPPED);
	BIND_ENUM_CONSTANT(STATE_COOKED);
	BIND_ENUM_CONSTANT(STATE_BURNT);
}

Ingredient::Ingredient() {}
Ingredient::~Ingredient() {}

void Ingredient::_process(double delta) {
	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		String state_name = "RAW";
		switch (current_state) {
			case STATE_CHOPPED:
				state_name = "CHOPPED";
				break;
			case STATE_COOKED:
				state_name = "COOKED";
				break;
			case STATE_BURNT:
				state_name = "BURNT";
				break;
			default:
				break;
		}

		String progress_text = "";
		if (process_progress > 0.0f && process_progress < 1.0f) {
			progress_text = UtilityFunctions::str(" (", (int)(process_progress * 100), "%)");
		}

		dm->draw_text("ing_" + get_name(), state_name + progress_text, get_global_position() + Vector3(0, 1.6f, 0), 0.001f, Color(1, 1, 1));
	}
}

void Ingredient::_ready() {
	Interactable::_ready();

	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->register_ingredient(this);
	}

	update_visuals();
}

void Ingredient::_exit_tree() {
	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		om->unregister_ingredient(this);
	}
}

void Ingredient::set_state(State p_state) {
	current_state = p_state;
	update_visuals();
}

Ingredient::State Ingredient::get_state() const {
	return current_state;
}

void Ingredient::set_process_progress(float p_progress) {
	process_progress = p_progress;
	update_visuals();
}

float Ingredient::get_process_progress() const {
	return process_progress;
}

void Ingredient::update_visuals() {
	// 1. Debug Label support
	Label3D *label = Object::cast_to<Label3D>(find_child("DebugLabel", true, false));
	if (label) {
		String state_name = "RAW";
		switch (current_state) {
			case STATE_CHOPPED:
				state_name = "CHOPPED";
				break;
			case STATE_COOKED:
				state_name = "COOKED";
				break;
			case STATE_BURNT:
				state_name = "BURNT";
				break;
			default:
				break;
		}

		String progress_text = "";
		if (process_progress > 0.0f && process_progress < 1.0f) {
			progress_text = UtilityFunctions::str(" (", (int)(process_progress * 100), "%)");
		}

		label->set_text(state_name + progress_text);
	}

	// 2. Mesh swapping (still works if you have them)
	Node *raw = find_child("Raw", true, false);
	Node *chopped = find_child("Chopped", true, false);
	Node *cooked = find_child("Cooked", true, false);
	Node *burnt = find_child("Burnt", true, false);

	if (raw && Object::cast_to<Node3D>(raw))
		Object::cast_to<Node3D>(raw)->set_visible(current_state == STATE_RAW);
	if (chopped && Object::cast_to<Node3D>(chopped))
		Object::cast_to<Node3D>(chopped)->set_visible(current_state == STATE_CHOPPED);
	if (cooked && Object::cast_to<Node3D>(cooked))
		Object::cast_to<Node3D>(cooked)->set_visible(current_state == STATE_COOKED);
	if (burnt && Object::cast_to<Node3D>(burnt))
		Object::cast_to<Node3D>(burnt)->set_visible(current_state == STATE_BURNT);
}

} // namespace godot
