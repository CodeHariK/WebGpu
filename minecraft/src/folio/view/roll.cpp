#include "roll.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ViewRoll::kick(double p_strength) {
	// Random left/right direction, scaled by kick_strength (folio: Math.random()<0.5).
	const double dir = (UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0;
	speed = p_strength * kick_strength * dir;
}

void ViewRoll::update(double p_delta_scaled) {
	velocity = -value * pull_strength * p_delta_scaled;
	speed += velocity;
	value += speed * p_delta_scaled;
	speed *= 1.0 - damping * p_delta_scaled;
}

} // namespace godot
