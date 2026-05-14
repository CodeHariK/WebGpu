#include "oc_plate.h"
#include "../../debug_draw/debug_manager.h"
#include "oc_types.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCPlate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_ingredient", "ingredient"), &OCPlate::add_ingredient);
	ClassDB::bind_method(D_METHOD("clear_contents"), &OCPlate::clear_contents);
}

OCPlate::OCPlate() {
}

OCPlate::~OCPlate() {}

void OCPlate::_ready() {
	set_ingredient_type(INGREDIENT_PLATE);
	OCIngredient::_ready();

	// Create or find a parent for held ingredients
	content_parent = Object::cast_to<Node3D>(find_child("ContentParent"));
	if (!content_parent) {
		Marker3D *marker = memnew(Marker3D);
		marker->set_name("ContentParent");
		marker->set_position(Vector3(0, 0.1f, 0));
		add_child(marker);
		content_parent = marker;
	}
}

void OCPlate::_process(double delta) {
	OCIngredient::_process(delta);

	DebugManager *dm = DebugManager::get_singleton();
	if (dm && !get_is_picked_up() && !contents.empty()) {
		String list = "Contents:\n";
		for (auto ing : contents) {
			if (ing) {
				list += String(" - ") + toString(ing->get_ingredient_type()) + " (" + toString(ing->get_state()) + ")\n";
			}
		}
		dm->draw_text("plate_contents_" + get_name(), list, get_global_position() + Vector3(0, 2.2f, 0), 0.001f, Color(0.8, 0.8, 1.0));
	} else if (dm) {
		dm->clear_text("plate_contents_" + get_name());
	}
}

bool OCPlate::add_ingredient(OCIngredient *p_ing) {
	if (!p_ing || p_ing == this)
		return false;

	// Don't add plates to plates for now
	if (Object::cast_to<OCPlate>(p_ing))
		return false;

	// Reparent to the plate
	p_ing->reparent(content_parent);

	// Stack them slightly
	float offset = contents.size() * 0.05f;
	p_ing->set_position(Vector3(0, offset, 0));
	p_ing->set_rotation(Vector3(0, 0, 0));

	// Freeze and disable interaction on the sub-ingredient
	p_ing->set_is_picked_up(true);
	p_ing->set_freeze_enabled(true);
	p_ing->set_is_interactable(false);

	// NEW: Disable collision shapes to prevent physics glitches with the parent plate
	TypedArray<Node> shapes = p_ing->find_children("*", "CollisionShape3D", true, false);
	for (int i = 0; i < shapes.size(); i++) {
		CollisionShape3D *col = Object::cast_to<CollisionShape3D>(shapes[i]);
		if (col) col->set_disabled(true);
	}

	contents.push_back(p_ing);
	UtilityFunctions::print("Plate: Added ", toString(p_ing->get_ingredient_type()));

	return true;
}

void OCPlate::clear_contents() {
	for (auto ing : contents) {
		if (ing && ing->is_inside_tree()) {
			ing->queue_free();
		}
	}
	contents.clear();
}

} // namespace godot
