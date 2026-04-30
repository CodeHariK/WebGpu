#include "oc_plate.h"
#include <godot_cpp/classes/marker3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCPlate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_ingredient", "ingredient"), &OCPlate::add_ingredient);
	ClassDB::bind_method(D_METHOD("clear_contents"), &OCPlate::clear_contents);
}

OCPlate::OCPlate() {
	set_ingredient_type("Plate");
}

OCPlate::~OCPlate() {}

void OCPlate::_ready() {
	OCIngredient::_ready();

	// Create or find a parent for held ingredients
	content_parent = Object::cast_to<Node3D>(find_child("ContentParent"));
	if (!content_parent) {
		Marker3D* marker = memnew(Marker3D);
		marker->set_name("ContentParent");
		marker->set_position(Vector3(0, 0.1f, 0));
		add_child(marker);
		content_parent = marker;
	}
}

bool OCPlate::add_ingredient(OCIngredient* p_ing) {
	if (!p_ing || p_ing == this) return false;
	
	// Don't add plates to plates for now
	if (Object::cast_to<OCPlate>(p_ing)) return false;

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

	contents.push_back(p_ing);
	UtilityFunctions::print("Plate: Added ", p_ing->get_ingredient_type());
	
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
