#ifndef GAME_SPEED_LINES_H
#define GAME_SPEED_LINES_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

class ColorRect;
class Node3D;

/**
 * SpeedLines — fullscreen stylized "rushing" speed streaks.
 * ---------------------------------------------------------
 * A CanvasLayer holding a full-rect ColorRect that runs `speed_lines.gdshader`
 * (radial streaks masked to the screen edges). The streak intensity fades in
 * with speed, sold as a cheap, on-brand sense-of-speed effect for the arcade
 * car camera.
 *
 * Two ways to drive it:
 *   - Set `target_path` to a RigidBody3D and the node reads its velocity itself,
 *     mapping [speed_min, speed_max] -> [0, intensity_max].
 *   - Or leave the target empty and push `set_speed_ratio(0..1)` each frame from
 *     gameplay / the camera.
 * Either way the intensity is smoothed (frame-rate independent) before it hits
 * the shader, so it ramps in and out cleanly.
 */
class SpeedLines : public CanvasLayer {
	GDCLASS(SpeedLines,
			CanvasLayer)

private:
	NodePath target_path; ///< Optional RigidBody3D to read speed from.
	Node3D *target_node = nullptr; ///< Resolved target (cached).

	Ref<ShaderMaterial> material; ///< Owns the speed_lines shader instance.
	ColorRect *rect = nullptr; ///< The full-rect overlay the shader draws on.

	float speed_min = 8.0f; ///< Speed (m/s) at which streaks start.
	float speed_max = 30.0f; ///< Speed (m/s) at which streaks are fully on.
	float intensity_max = 1.0f; ///< Streak alpha cap at max speed.
	float smooth_rate = 4.0f; ///< Intensity easing rate (e-folds/sec).
	Color line_color = Color(1, 1, 1, 1); ///< Streak colour.
	float vignette_strength = 0.55f; ///< Max edge darkening at full speed (0 disables).
	int layer_index = 95; ///< CanvasLayer order (below a 100+ post/UI layer).

	float external_ratio = 0.0f; ///< Speed ratio pushed in when there is no target.
	float current_intensity = 0.0f; ///< Smoothed intensity sent to the shader.

	bool debug_force = false; ///< `--fxshot`: force the effect on for a screenshot.

	void _resolve_target();
	void _build_overlay();
	void _fx_capture(); ///< Debug: save a viewport PNG and quit (screenshot flow).

protected:
	static void _bind_methods();

public:
	SpeedLines();
	~SpeedLines();

	void _ready() override;
	void _process(double p_delta) override;

	/// RigidBody3D whose speed drives the effect (optional).
	void set_target_path(const NodePath &p_path);
	NodePath get_target_path() const { return target_path; }

	/// External drive in [0, 1] when no target is set (e.g. from the camera).
	void set_speed_ratio(float p_ratio);

	void set_speed_min(float p_v) { speed_min = p_v; }
	float get_speed_min() const { return speed_min; }
	void set_speed_max(float p_v) { speed_max = p_v; }
	float get_speed_max() const { return speed_max; }
	void set_intensity_max(float p_v) { intensity_max = p_v; }
	float get_intensity_max() const { return intensity_max; }
	void set_line_color(const Color &p_c) { line_color = p_c; }
	Color get_line_color() const { return line_color; }
	void set_vignette_strength(float p_v) { vignette_strength = p_v; }
	float get_vignette_strength() const { return vignette_strength; }
};

} // namespace godot

#endif // GAME_SPEED_LINES_H
