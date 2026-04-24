#ifndef CELESTE_UI_H
#define CELESTE_UI_H

#include <godot_cpp/variant/string.hpp>

namespace godot {

class CelesteController;
class CUI;
class Node;
class Panel;

class CelesteUI {
private:
	CUI *ui_root = nullptr;
	CelesteController *controller = nullptr;
	Panel *main_panel = nullptr;

	void _add_variable_slider(Node *p_parent, const String &p_label, const String &p_property, float p_min, float p_max, float p_step);

public:
	CelesteUI();
	~CelesteUI();

	void setup(CelesteController *p_controller, CUI *p_ui_root);
	void toggle_visibility();
	bool is_visible() const;
};

} // namespace godot

#endif // CELESTE_UI_H
