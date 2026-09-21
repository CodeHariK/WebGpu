#ifndef CUI_LINE_GRAPH_H
#define CUI_LINE_GRAPH_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

namespace godot {

/**
 * A lightweight scrolling line graph (e.g. FPS / value-over-time readouts).
 * Keeps the last `max_points` samples and repaints on change.
 */
class CUILineGraph : public Control {
	GDCLASS(CUILineGraph,
			Control)

private:
	PackedFloat32Array data_points;
	int max_points = 100;
	float min_value = 0.0f;
	float max_value = 1.0f;
	Color line_color = Color(0.0f, 1.0f, 0.0f); // green
	Color bg_color = Color(0.0f, 0.0f, 0.0f, 0.3f); // translucent black

protected:
	static void _bind_methods();

public:
	CUILineGraph();
	~CUILineGraph();

	void add_value(float p_value);
	void set_data(const PackedFloat32Array &p_data);
	void set_range(
			float p_min,
			float p_max
	);
	void set_line_color(const Color &p_color);
	void set_max_points(int p_count);
	int get_max_points() const { return max_points; }
	void clear();

	void _draw() override;
};

} // namespace godot

#endif // CUI_LINE_GRAPH_H
