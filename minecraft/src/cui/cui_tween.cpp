#include "cui_tween.h"

#include <godot_cpp/classes/node.hpp>

namespace godot {

Ref<Tween> CUITween::fade_alpha(
		Node *p_node,
		double p_to,
		double p_duration,
		const Callable &p_on_finished
) {
	if (!p_node) {
		return Ref<Tween>();
	}
	Ref<Tween> tw = p_node->create_tween();
	tw->tween_property(p_node, "modulate:a", p_to, p_duration);
	if (p_on_finished.is_valid()) {
		tw->tween_callback(p_on_finished);
	}
	return tw;
}

Ref<Tween> CUITween::toast(
		Node *p_node,
		double p_fade_in,
		double p_hold,
		double p_fade_out
) {
	if (!p_node) {
		return Ref<Tween>();
	}
	Ref<Tween> tw = p_node->create_tween();
	tw->tween_property(p_node, "modulate:a", 1.0, p_fade_in);
	tw->tween_interval(p_hold);
	tw->tween_property(p_node, "modulate:a", 0.0, p_fade_out);
	tw->tween_callback(callable_mp((Node *)p_node, &Node::queue_free));
	return tw;
}

} // namespace godot
