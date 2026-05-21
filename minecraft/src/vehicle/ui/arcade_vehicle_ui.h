#ifndef ARCADE_VEHICLE_UI_H
#define ARCADE_VEHICLE_UI_H

#include <godot_cpp/templates/vector.hpp>

namespace godot {

class ArcadeVehicle;
class CUI;
class CUILineGraph;
class Node;
class Panel;

class ArcadeVehicleUI {
private:
	CUI *ui_root = nullptr;
	ArcadeVehicle *vehicle = nullptr;
	Panel *main_panel = nullptr;
	CUILineGraph *velocity_graph = nullptr;

	void _add_variable_slider(Node *p_parent, const String &p_label, const String &p_property, float p_min, float p_max, float p_step);

public:
	ArcadeVehicleUI();
	~ArcadeVehicleUI();

	void setup(ArcadeVehicle *p_vehicle, CUI *p_ui_root);
	void toggle_visibility();
	void update_graph(float p_val);
	bool is_visible() const;
};

} // namespace godot

#endif // ARCADE_VEHICLE_UI_H
