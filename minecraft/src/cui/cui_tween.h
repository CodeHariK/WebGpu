#ifndef CUI_TWEEN_H
#define CUI_TWEEN_H

#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace godot {

class Node;

/**
 * Small helper for the UI's recurring tween patterns. Plain C++ (no GDCLASS) —
 * static factories that build a Tween on a node and hand back the Ref so the
 * caller can hold it and kill() a previous one before starting a new fade
 * (prevents overlapping open/close fades from fighting on the same property).
 */
class CUITween {
public:
	// Fade a node's modulate alpha to p_to over p_duration, optional on-finish.
	// Returns the Tween so the caller can store + kill it.
	static Ref<Tween> fade_alpha(
			Node *p_node,
			double p_to,
			double p_duration,
			const Callable &p_on_finished = Callable()
	);

	// Fade in -> hold -> fade out -> queue_free (transient toast lifecycle).
	static Ref<Tween> toast(
			Node *p_node,
			double p_fade_in,
			double p_hold,
			double p_fade_out
	);
};

} // namespace godot

#endif // CUI_TWEEN_H
