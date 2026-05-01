#include "oc_ingredient.h"
#include "../../debug_draw/debug_manager.h"
#include "oc_manager.h"
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCIngredient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ingredient_type", "type"), &OCIngredient::set_ingredient_type);
	ClassDB::bind_method(D_METHOD("get_ingredient_type"), &OCIngredient::get_ingredient_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ingredient_type"), "set_ingredient_type", "get_ingredient_type");

	ClassDB::bind_method(D_METHOD("set_state", "state"), &OCIngredient::set_state);
	ClassDB::bind_method(D_METHOD("get_state"), &OCIngredient::get_state);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_state", PROPERTY_HINT_ENUM, "Raw,Chopped,Cooked,Blended,Frozen,Burnt"), "set_state", "get_state");
}

OCIngredient::OCIngredient() {}
OCIngredient::~OCIngredient() {}

void OCIngredient::_process(double delta) {
	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		String state_name = "RAW";
		switch (current_state) {
			case INGREDIENT_STATE_CHOPPED:
				state_name = "CHOPPED";
				break;
			case INGREDIENT_STATE_COOKED:
				state_name = "COOKED";
				break;
			case INGREDIENT_STATE_BLENDED:
				state_name = "BLENDED";
				break;
			case INGREDIENT_STATE_FROZEN:
				state_name = "FROZEN";
				break;
			case INGREDIENT_STATE_BURNT:
				state_name = "BURNT";
				break;
			default:
				break;
		}

		String progress_text = "";
		if (process_progress > 0.0f && process_progress < 1.0f) {
			progress_text = UtilityFunctions::str(" (", (int)(process_progress * 100), "%)");
		}

		dm->draw_text("ing_" + get_name(), ingredient_type + ": " + state_name + progress_text, get_global_position() + Vector3(0, 1.6f, 0), 0.001f, Color(1, 1, 1));
	}
}

void OCIngredient::_ready() {
	Interactable::_ready();

	om = OvercookedManager::get_singleton();
	if (om) {
		om->register_ingredient(this);
	}

	// Enable physics
	set_freeze_enabled(false);
	update_visuals();
}

void OCIngredient::drop(Node3D *p_actor) {
	Interactable::drop(p_actor);
	set_freeze_enabled(false);
	set_sleeping(false);
	set_linear_velocity(Vector3(0, 0, 0));
	set_angular_velocity(Vector3(0, 0, 0));
}

void OCIngredient::_exit_tree() {
	if (om) {
		om->unregister_ingredient(this);
	}
}

void OCIngredient::set_state(IngredientState p_state) {
	current_state = p_state;
	update_visuals();
}

OCIngredient::IngredientState OCIngredient::get_state() const {
	return current_state;
}

void OCIngredient::set_process_progress(float p_progress) {
	process_progress = p_progress;
	update_visuals();
}

float OCIngredient::get_process_progress() const {
	return process_progress;
}

void OCIngredient::set_ingredient_type(const String &p_type) { ingredient_type = p_type; }
String OCIngredient::get_ingredient_type() const { return ingredient_type; }

void OCIngredient::update_visuals() {
	Node *raw = find_child("Raw", true, false);
	Node *chopped = find_child("Chopped", true, false);
	Node *cooked = find_child("Cooked", true, false);
	Node *burnt = find_child("Burnt", true, false);
	Node *blended = find_child("Blended", true, false);
	Node *frozen = find_child("Frozen", true, false);

	if (raw && Object::cast_to<Node3D>(raw))
		Object::cast_to<Node3D>(raw)->set_visible(current_state == INGREDIENT_STATE_RAW);
	if (chopped && Object::cast_to<Node3D>(chopped))
		Object::cast_to<Node3D>(chopped)->set_visible(current_state == INGREDIENT_STATE_CHOPPED);
	if (cooked && Object::cast_to<Node3D>(cooked))
		Object::cast_to<Node3D>(cooked)->set_visible(current_state == INGREDIENT_STATE_COOKED);
	if (blended && Object::cast_to<Node3D>(blended))
		Object::cast_to<Node3D>(blended)->set_visible(current_state == INGREDIENT_STATE_BLENDED);
	if (frozen && Object::cast_to<Node3D>(frozen))
		Object::cast_to<Node3D>(frozen)->set_visible(current_state == INGREDIENT_STATE_FROZEN);
	if (burnt && Object::cast_to<Node3D>(burnt))
		Object::cast_to<Node3D>(burnt)->set_visible(current_state == INGREDIENT_STATE_BURNT);
}

} // namespace godot
