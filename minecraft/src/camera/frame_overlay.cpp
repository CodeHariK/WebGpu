#include "frame_overlay.h"

namespace godot {

static const Color BAND_COLOR(1.0f, 0.85f, 0.2f, 0.9f); // enforced edges
static const Color REF_COLOR(1.0f, 1.0f, 1.0f, 0.45f); // reference-only edges
static const Color SOFT_COLOR(1.0f, 0.85f, 0.2f, 0.4f); // soft zone inner lines
static const Color IN_COLOR(0.3f, 1.0f, 0.4f, 1.0f);
static const Color OUT_COLOR(1.0f, 0.25f, 0.2f, 1.0f);

void CameraFrameOverlay::set_frame(
		float p_left,
		float p_top,
		float p_right,
		float p_bottom,
		float p_soft
) {
	left = p_left;
	top = p_top;
	right = p_right;
	bottom = p_bottom;
	soft = p_soft;
	queue_redraw();
}

void CameraFrameOverlay::set_target(
		const Vector2 &p_px,
		bool p_valid
) {
	target_px = p_px;
	has_target = p_valid;
	queue_redraw();
}

void CameraFrameOverlay::_draw() {
	Vector2 size = get_size();
	float y_top = top * size.y;
	float y_bottom = bottom * size.y;
	float x_left = left * size.x;
	float x_right = right * size.x;

	// Enforced band: full-width solid lines.
	draw_line(Vector2(0, y_top), Vector2(size.x, y_top), BAND_COLOR, 3.0f);
	draw_line(Vector2(0, y_bottom), Vector2(size.x, y_bottom), BAND_COLOR, 3.0f);
	// Soft zone: thinner, fainter lines just inside the band.
	if (soft > 0.0f) {
		float ys_top = (top + soft) * size.y;
		float ys_bottom = (bottom - soft) * size.y;
		draw_dashed_line(Vector2(0, ys_top), Vector2(size.x, ys_top), SOFT_COLOR, 1.5f, 6.0f);
		draw_dashed_line(Vector2(0, ys_bottom), Vector2(size.x, ys_bottom), SOFT_COLOR, 1.5f, 6.0f);
	}
	// Reference box sides: dashed, between the band lines.
	draw_dashed_line(Vector2(x_left, y_top), Vector2(x_left, y_bottom), REF_COLOR, 2.0f, 10.0f);
	draw_dashed_line(Vector2(x_right, y_top), Vector2(x_right, y_bottom), REF_COLOR, 2.0f, 10.0f);

	if (has_target) {
		bool inside = target_px.y >= y_top && target_px.y <= y_bottom;
		Color c = inside ? IN_COLOR : OUT_COLOR;
		draw_circle(target_px, 9.0f, Color(0, 0, 0, 0.6f));
		draw_circle(target_px, 7.0f, c);
	}
}

} // namespace godot
