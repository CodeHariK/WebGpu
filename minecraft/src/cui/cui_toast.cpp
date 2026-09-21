#include "cui_toast.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "cui.h"
#include "cui_tween.h"

namespace godot {

CUIToast::CUIToast() {}
CUIToast::~CUIToast() {}

void CUIToast::_ready() {
	set_anchors_preset(Control::PRESET_FULL_RECT);
	set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	if (builder) {
		stack = builder->add_vbox(this, "", 8, Control::PRESET_TOP_WIDE);
		stack->set_offset(Side::SIDE_TOP, 24.0);
		stack->set_alignment(BoxContainer::ALIGNMENT_BEGIN);
		stack->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	}
}

void CUIToast::notify(const String &p_text, double p_duration) {
	if (!stack || !builder) {
		return;
	}
	// Ensure this Control spans its parent so the TOP_WIDE stack is full width
	// (a free-standing Control can come up size 0 at boot -> toasts hug the left).
	Control *parent = Object::cast_to<Control>(get_parent());
	if (parent) {
		set_position(Vector2(0, 0));
		set_size(parent->get_size());
	}
	const double duration = p_duration > 0.0 ? p_duration : default_duration;

	PanelContainer *toast = builder->add_panel_container(stack);
	toast->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	toast->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	Label *label = builder->add_label(toast, p_text, "", 0, HORIZONTAL_ALIGNMENT_CENTER);
	label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	toast->set_modulate(Color(1, 1, 1, 0));
	CUITween::toast(toast, 0.18, duration, 0.3);
}

void CUIToast::_bind_methods() {
	ClassDB::bind_method(D_METHOD("notify", "text", "duration"), &CUIToast::notify, DEFVAL(-1.0));
}

} // namespace godot
