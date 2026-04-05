#ifndef CUI_H
#define CUI_H

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

class CUI : public Control {
	GDCLASS(CUI, Control);

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
	AcceptDialog *add_dialog(Node *p_parent, const String &p_name, const String &p_text);
};

} // namespace godot

#endif // CUI_H
