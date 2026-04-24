#ifndef CUI_H
#define CUI_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {
class AcceptDialog;
class Button;
class HBoxContainer;
class Label;
class Panel;
class ScrollContainer;
class VBoxContainer;
class TabContainer;
class CanvasLayer;
class HSlider;
class Viewport;

class CUI : public Control {
	GDCLASS(CUI, Control)

protected:
	static void _bind_methods() {}

public:
	CUI();
	~CUI();

	static CUI *create_on_new_layer(Node *p_owner_node);

	void _ready() override;

	Panel *add_panel(Node *p_parent, const String &p_name, LayoutPreset p_preset, const Vector2 &p_min_size);
	Button *add_button(Node *p_parent, const String &p_text, const Callable &p_callback);
	HBoxContainer *add_hbox(Node *p_parent, const String &p_name);
	ScrollContainer *add_scroll(Node *p_parent, const String &p_name);
	VBoxContainer *add_vbox(Node *p_parent, const String &p_name);
	Label *add_label(Node *p_parent, const String &p_text);
	HSlider *add_hslider(Node *p_parent, float p_min, float p_max, float p_step, float p_value, const Callable &p_callback);
	TabContainer *add_tab_container(Node *p_parent, const String &p_name);
	AcceptDialog *add_dialog(Node *p_parent, const String &p_name, const String &p_text);
};

} // namespace godot

#endif // CUI_H
