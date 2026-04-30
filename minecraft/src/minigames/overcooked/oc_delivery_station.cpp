#include "oc_delivery_station.h"
#include "oc_manager.h"
#include "oc_ingredient.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void OCDeliveryStation::_bind_methods() {}

OCDeliveryStation::OCDeliveryStation() {}
OCDeliveryStation::~OCDeliveryStation() {}

bool OCDeliveryStation::can_place_item(Interactable *p_item) {
	// We can always "place" an item here to submit it, 
	// as long as we don't already have one (though we consume it instantly)
	return p_item != nullptr && held_item == nullptr;
}

void OCDeliveryStation::place_item(Interactable *p_item) {
	OCIngredient *ing = Object::cast_to<OCIngredient>(p_item);
	if (!ing) return;

	OvercookedManager *om = OvercookedManager::get_singleton();
	if (om) {
		bool success = om->submit_ingredient(ing);
		if (success) {
			UtilityFunctions::print("OCDeliveryStation: Successful delivery!");
			// The item is consumed
			p_item->queue_free();
		} else {
			UtilityFunctions::print("OCDeliveryStation: Item does not match any active orders.");
			// For now, let's just leave it on the station? 
			// Or drop it? Let's leave it so player can pick it back up.
			held_item = p_item;
			update_held_item_position();
		}
	}
}

} // namespace godot
