#ifndef CAMERA_FRAME_OVERLAY_H
#define CAMERA_FRAME_OVERLAY_H

#include <godot_cpp/classes/control.hpp>

namespace godot {

/**
 * CameraFrameOverlay — debug drawing of the follow camera's framing box (GameCamera.show_frame_bounds).
 * ------------------------------------------------------------------------------------------------
 *   - Top / bottom lines (solid): the vertical band the camera enforces (frame_top / frame_bottom).
 *   - Fainter inner lines: the soft zone (frame_soft_zone) where the camera starts easing.
 *   - Left / right lines (dashed): reference only; horizontal follow doesn't use them.
 *   - Dot: the follow target's centre on screen; green inside the band, red outside.
 * Fractions are of the viewport (0 = top / left).
 */
class CameraFrameOverlay : public Control {
	GDCLASS(CameraFrameOverlay,
			Control)

private:
	float left = 0.35f;
	float top = 0.45f;
	float right = 0.65f;
	float bottom = 0.78f;
	float soft = 0.05f;
	Vector2 target_px;
	bool has_target = false;

protected:
	static void _bind_methods() {}

public:
	void set_frame(
			float p_left,
			float p_top,
			float p_right,
			float p_bottom,
			float p_soft
	);
	void set_target(
			const Vector2 &p_px,
			bool p_valid
	);
	void _draw() override;
};

} // namespace godot

#endif // CAMERA_FRAME_OVERLAY_H
