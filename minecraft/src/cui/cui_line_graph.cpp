#include "cui_line_graph.h"

#include <godot_cpp/core/math.hpp>

namespace godot {

void CUILineGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_value", "value"), &CUILineGraph::add_value);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &CUILineGraph::set_data);
	ClassDB::bind_method(D_METHOD("set_range", "min", "max"), &CUILineGraph::set_range);
	ClassDB::bind_method(D_METHOD("set_line_color", "color"), &CUILineGraph::set_line_color);
	ClassDB::bind_method(D_METHOD("set_max_points", "count"), &CUILineGraph::set_max_points);
	ClassDB::bind_method(D_METHOD("get_max_points"), &CUILineGraph::get_max_points);
	ClassDB::bind_method(D_METHOD("clear"), &CUILineGraph::clear);
}

CUILineGraph::CUILineGraph() {
	set_custom_minimum_size(Vector2(0, 100));
}

CUILineGraph::~CUILineGraph() {}

void CUILineGraph::add_value(float p_value) {
	data_points.push_back(p_value);
	while (data_points.size() > max_points) {
		data_points.remove_at(0);
	}
	queue_redraw();
}

void CUILineGraph::set_data(const PackedFloat32Array &p_data) {
	data_points = p_data;
	while (data_points.size() > max_points) {
		data_points.remove_at(0);
	}
	queue_redraw();
}

void CUILineGraph::set_range(
		float p_min,
		float p_max
) {
	min_value = p_min;
	max_value = p_max;
	queue_redraw();
}

void CUILineGraph::set_line_color(const Color &p_color) {
	line_color = p_color;
	queue_redraw();
}

void CUILineGraph::set_max_points(int p_count) {
	max_points = p_count < 2 ? 2 : p_count; // need >=2 for a segment / safe step
	while (data_points.size() > max_points) {
		data_points.remove_at(0);
	}
	queue_redraw();
}

void CUILineGraph::clear() {
	data_points.clear();
	queue_redraw();
}

void CUILineGraph::_draw() {
	const Vector2 size = get_size();
	draw_rect(Rect2(Vector2(), size), bg_color);

	if (data_points.size() < 2) {
		return;
	}

	const float step_x = size.x / (float)(max_points - 1); // max_points >= 2 guaranteed
	float range = max_value - min_value;
	if (range <= 0.0f) {
		range = 1.0f;
	}

	// Right-align the samples when fewer than max_points are present.
	const float x_offset = (max_points - data_points.size()) * step_x;

	PackedVector2Array points;
	points.resize(data_points.size());
	for (int i = 0; i < data_points.size(); ++i) {
		const float px = x_offset + (i * step_x);
		const float norm = Math::clamp((data_points[i] - min_value) / range, 0.0f, 1.0f);
		const float py = size.y - (norm * size.y);
		points[i] = Vector2(px, py);
	}

	draw_polyline(points, line_color, 1.5f, true);
}

} // namespace godot
