#include "cui_line_graph.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CUILineGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_value", "value"), &CUILineGraph::add_value);
	ClassDB::bind_method(D_METHOD("clear"), &CUILineGraph::clear);
}

CUILineGraph::CUILineGraph() {
	set_custom_minimum_size(Vector2(0, 100));
}

CUILineGraph::~CUILineGraph() {}

void CUILineGraph::add_value(float p_value) {
	data_points.push_back(p_value);
	if (data_points.size() > (size_t)max_points) {
		data_points.erase(data_points.begin());
	}
	queue_redraw();
}

void CUILineGraph::set_data(const std::vector<float> &p_data) {
	data_points = p_data;
	queue_redraw();
}

void CUILineGraph::set_range(float p_min, float p_max) {
	min_value = p_min;
	max_value = p_max;
	queue_redraw();
}

void CUILineGraph::clear() {
	data_points.clear();
	queue_redraw();
}

void CUILineGraph::_draw() {
	Vector2 size = get_size();
	
	// Draw Background
	draw_rect(Rect2(Vector2(), size), bg_color);

	if (data_points.size() < 2) {
		return;
	}

	PackedVector2Array points;
	float step_x = size.x / (max_points - 1);
	float range = max_value - min_value;
	if (range <= 0.0f) range = 1.0f;

	// Start drawing from the right if we have fewer than max_points
	float x_offset = (max_points - (int)data_points.size()) * step_x;

	for (size_t i = 0; i < data_points.size(); ++i) {
		float px = x_offset + (i * step_x);
		
		// Normalize value to 0-1 range based on min/max
		float norm_val = (data_points[i] - min_value) / range;
		norm_val = UtilityFunctions::clamp(norm_val, 0.0f, 1.0f);
		
		float py = size.y - (norm_val * size.y);
		points.push_back(Vector2(px, py));
	}

	draw_polyline(points, line_color, 1.5f, true);
}

} // namespace godot
