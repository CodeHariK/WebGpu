#ifndef CHARACTER_UI_H
#define CHARACTER_UI_H

#include <godot_cpp/variant/string.hpp>

namespace godot {

class SpringCharacter;
class CUI;
class CUILineGraph;
class Node;
class Panel;

/**
 * Live tuning UI for SpringCharacter (ported from CelesteUI): a Settings button
 * that opens a tabbed panel of sliders bound to the controller's tunables, plus
 * Save / Load buttons and a real-time speed graph.
 */
class CharacterUI {
private:
	CUI *ui_root = nullptr;
	SpringCharacter *controller = nullptr;
	Panel *main_panel = nullptr;
	CUILineGraph *speed_graph = nullptr;

	void _slider(
			Node *p_parent,
			const String &p_label,
			const String &p_property,
			float p_min,
			float p_max,
			float p_step
	);

public:
	CharacterUI();
	~CharacterUI();

	void setup(
			SpringCharacter *p_controller,
			CUI *p_ui_root
	);
	void toggle_visibility();
	void update_graph(float p_val);
	bool is_visible() const;
};

} // namespace godot

#endif // CHARACTER_UI_H
