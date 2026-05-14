#ifndef OC_TYPES_H
#define OC_TYPES_H

#include <cstdint>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

enum IngredientState : uint8_t {
	INGREDIENT_STATE_RAW = 0,
	INGREDIENT_STATE_CHOPPED,
	INGREDIENT_STATE_COOKED,
	INGREDIENT_STATE_BLENDED,
	INGREDIENT_STATE_FROZEN,
	INGREDIENT_STATE_BURNT,
	INGREDIENT_STATE_MAX
};

enum IngredientType : uint8_t {
	INGREDIENT_GENERIC = 0,
	INGREDIENT_TOMATO,
	INGREDIENT_LETTUCE,
	INGREDIENT_ONION,
	INGREDIENT_BUN,
	INGREDIENT_CHEESE,
	INGREDIENT_PLATE,
	INGREDIENT_MAX
};

inline const char *toString(IngredientState p_state) {
	static const char *names[] = { "Raw", "Chopped", "Cooked", "Blended", "Frozen", "Burnt" };
	if ((int)p_state >= 0 && (int)p_state < 6)
		return names[(int)p_state];
	return "Unknown";
}

inline const char *toString(IngredientType p_type) {
	static const char *names[] = { "Generic", "Tomato", "Lettuce", "Onion", "Bun", "Cheese", "Plate" };
	if ((int)p_type >= 0 && (int)p_type < (int)INGREDIENT_MAX)
		return names[(int)p_type];
	return "Unknown";
}

struct OCRecipeRequirement {
	IngredientType type;
	IngredientState state;
};

enum ProcessOperation : uint8_t {
	PROCESS_NONE = 0,
	PROCESS_CUT,
	PROCESS_COOK,
	PROCESS_BLEND
};

struct OCProcessStep {
	IngredientState input_state;
	IngredientState output_state;
	float speed;
	bool automatic;
	ProcessOperation operation;
};

enum StationType : uint8_t {
	TYPE_COUNTER = 0,
	TYPE_CUTTING,
	TYPE_COOKING,
	TYPE_BLENDER,
	TYPE_CRATE,
	TYPE_DELIVERY,
	TYPE_TRASH
};

inline const char *get_station_operation_name(StationType p_type) {
	switch (p_type) {
		case TYPE_CUTTING:
			return "CHOP";
		case TYPE_COOKING:
			return "COOK";
		case TYPE_BLENDER:
			return "BLEND";
		default:
			return "Process";
	}
}

} // namespace godot

#endif // OC_TYPES_H
