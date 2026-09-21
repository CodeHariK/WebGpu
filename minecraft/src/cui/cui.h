#ifndef CUI_H
#define CUI_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {
class AcceptDialog;
class Button;
class HBoxContainer;
class Label;
class Panel;
class ScrollContainer;
class VBoxContainer;
class PanelContainer;
class CanvasLayer;
class ColorRect;
class CenterContainer;
class HSlider;
class LineEdit;
class OptionButton;
class ProgressBar;
class SpinBox;
class TabContainer;
class Viewport;

class CUILineGraph;
class CUI : public Control {
	GDCLASS(CUI,
			Control)

private:
	// Registry stores instance IDs (not raw pointers) so freed elements can't
	// dangle: get_element resolves + prunes them on access.
	HashMap<String, uint64_t> element_ids;
	HashSet<String> collision_warned; // de-dupe the name-collision warning

	// Attach a freshly-created control to p_parent (or to this if null).
	void _reparent(Node *p_parent, Control *p_child);
	// Register a named control in the registry (warns on collision).
	void _register(const String &p_name, Control *p_control);

protected:
	static void _bind_methods();
	void _on_slider_value_changed(
			double p_value,
			Label *p_label
	);

public:
	CUI();
	~CUI();

	static CUI *create_on_new_layer(Node *p_owner_node);

	void _ready() override;

	// Registry helpers
	Control *get_element(const String &p_name) const;
	void set_text(
			const String &p_name,
			const String &p_text
	);
	void set_value(
			const String &p_name,
			float p_value
	);

	Panel *add_panel(
			Node *p_parent,
			const String &p_name,
			LayoutPreset p_preset,
			const Vector2 &p_min_size
	);
	PanelContainer *add_panel_container(
			Node *p_parent,
			const String &p_name = "",
			LayoutPreset p_preset = PRESET_CENTER
	);
	ColorRect *add_color_rect(
			Node *p_parent,
			const Color &p_color,
			const String &p_name = ""
	);
	CenterContainer *add_center_container(
			Node *p_parent,
			const String &p_name = ""
	);
	Button *add_button(
			Node *p_parent,
			const String &p_text,
			const Callable &p_callback,
			const String &p_name = "",
			const Vector2 &p_min_size = Vector2()
	);
	HBoxContainer *add_hbox(
			Node *p_parent,
			const String &p_name = "",
			int p_separation = 0,
			LayoutPreset p_preset = PRESET_FULL_RECT
	);
	ScrollContainer *add_scroll(
			Node *p_parent,
			const String &p_name
	);
	VBoxContainer *add_vbox(
			Node *p_parent,
			const String &p_name = "",
			int p_separation = 0,
			LayoutPreset p_preset = PRESET_FULL_RECT
	);
	Label *add_label(
			Node *p_parent,
			const String &p_text,
			const String &p_name = "",
			int p_font_size = 0, // 0 = theme default
			int p_align = 0 // HorizontalAlignment; 0 = LEFT
	);
	// A styled section header (uppercased, small, accent colour) for grouping.
	Label *add_header(
			Node *p_parent,
			const String &p_text,
			const String &p_name = ""
	);
	ProgressBar *add_progress_bar(
			Node *p_parent,
			const String &p_name = ""
	);
	LineEdit *add_line_edit(
			Node *p_parent,
			const String &p_placeholder = "",
			const String &p_name = ""
	);
	OptionButton *add_option_button(
			Node *p_parent,
			const String &p_name = ""
	);
	SpinBox *add_spin_box(
			Node *p_parent,
			double p_min,
			double p_max,
			double p_step,
			double p_value,
			const String &p_name = ""
	);
	HSlider *add_hslider(
			Node *p_parent,
			float p_min,
			float p_max,
			float p_step,
			float p_value,
			const Callable &p_callback,
			const String &p_name = ""
	);
	TabContainer *add_tab_container(
			Node *p_parent,
			const String &p_name
	);
	AcceptDialog *add_dialog(
			Node *p_parent,
			const String &p_name,
			const String &p_text
	);
	CUILineGraph *add_graph(
			Node *p_parent,
			const String &p_name
	);
};

} // namespace godot

#endif // CUI_H
