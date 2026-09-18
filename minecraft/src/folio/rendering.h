#ifndef FOLIO_RENDERING_H
#define FOLIO_RENDERING_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/world_environment.hpp>

namespace godot {

/**
 * Folio port — FolioRendering  (folio `Game/Rendering.js`)
 * -----------------------------------------------
 * folio's Rendering owns the WebGPU renderer AND the post chain. Godot owns the
 * renderer itself, so this node ports only the portable part: the post-process
 * composite, quality-switched.
 *
 *   folio outputNode:
 *     level 0 (high): cheapDOF(scenePass) + bloom     -> DOF on,  wide bloom
 *     level 1 (low):  scenePass          + bloom      -> DOF off, narrow bloom
 *   bloom: threshold 1, strength 0.25 (nMips 5 @ lvl0, 2 @ lvl1)
 *   cheapDOF: fake tilt-shift (see cheap_dof.gdshader)
 *
 * Bloom -> a `WorldEnvironment` with Godot's native additive glow (the closest
 * 1:1, and cheap). cheapDOF -> a fullscreen `ColorRect` on a `CanvasLayer` using
 * `cheap_dof.gdshader`, toggled by quality level. Subscribes to FolioQuality's
 * `change` event to re-apply.
 *
 * (Name kept as `FolioRendering` — no core Godot class collides.)
 */
class FolioRendering : public Node {
	GDCLASS(FolioRendering,
			Node)

private:
	// Bloom (folio defaults)
	double bloom_threshold = 1.0;
	double bloom_strength = 0.25;

	// cheapDOF (folio defaults)
	double dof_start = 0.2;
	double dof_end = 0.5;
	int dof_repeats = 25;
	double dof_amount = 0.003;

	int level = 0; // mirrors FolioQuality: 0 = high, 1 = low

	WorldEnvironment *world_env = nullptr;
	Ref<Environment> env;
	CanvasLayer *dof_layer = nullptr;
	ColorRect *dof_rect = nullptr;
	Ref<ShaderMaterial> dof_material;
	MeshInstance3D *background = nullptr;
	Ref<ShaderMaterial> background_material;

	void _build_environment();
	void _build_dof();
	void _build_background();

protected:
	static void _bind_methods();

public:
	FolioRendering();
	~FolioRendering();

	void _ready() override;

	// Re-apply the post chain for a quality tier (bound to FolioQuality `change`).
	void apply_quality(int p_level);

	Ref<Environment> get_environment() const { return env; }
};

} // namespace godot

#endif // FOLIO_RENDERING_H
