#ifndef CUI_LINE_GRAPH_H
#define CUI_LINE_GRAPH_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/variant/color.hpp>
#include <vector>

namespace godot {

class CUILineGraph : public Control {
	GDCLASS(CUILineGraph, Control)

private:
	std::vector<float> data_points;
	int max_points = 100;
	float min_value = 0.0f;
	float max_value = 1.0f;
	Color line_color = Color(0.0f, 1.0f, 0.0f); // Green
	Color bg_color = Color(0.0f, 0.0f, 0.0f, 0.3f); // Transparent black

protected:
	static void _bind_methods();

public:
	CUILineGraph();
	~CUILineGraph();

	void add_value(float p_value);
	void set_data(const std::vector<float> &p_data);
	void set_range(float p_min, float p_max);
	void set_line_color(const Color &p_color) { line_color = p_color; queue_redraw(); }
	void set_max_points(int p_count) { max_points = p_count; queue_redraw(); }
	void clear();

	void _draw() override;
};

} // namespace godot

#endif // CUI_LINE_GRAPH_H
