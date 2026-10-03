#ifndef BUBBLE_H
#define BUBBLE_H

#include <godot_cpp/classes/animatable_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * Bubble — the Bubble Wand's projectile: a floating, wobbling, standable soap bubble.
 * ---------------------------------------------------------------------------------
 * Lifecycle: launch() fires it forward from the wand; it decelerates to a stop, then
 * floats upward slowly with a few overlapping sine waves (bob + sway) so its path never
 * visibly repeats. After `lifetime` seconds (or pop()) it swells, fades and frees itself.
 *
 *   - Look: one sphere + a small spatial shader (see bubble.cpp): transparent centre, bright
 *     rim (fresnel), drifting rainbow thin-film tint, one fixed highlight. One draw call.
 *   - Wobble: squash-and-stretch on the MESH only (never scale a physics body), from a
 *     breathing sine plus a spring "dip" when something stands on it.
 *   - Platform: an AnimatableBody3D moved kinematically, so the character's ground ray lands
 *     on it; it reports its frame velocity like MovingPlatform so riders are carried.
 *   - Collision layer 2 (not 1) so the follow cameras' wall rays ignore it.
 */
class Bubble : public AnimatableBody3D {
	GDCLASS(Bubble,
			AnimatableBody3D)

private:
	// --- Tunables ---
	float radius = 0.8f; ///< Sphere radius (metres).
	float launch_speed = 14.0f; ///< Speed leaving the wand (travels ~speed*time/3 before floating).
	float launch_time = 0.7f; ///< Seconds to decelerate to a float.
	float rise_speed = 0.35f; ///< Slow upward drift while floating (m/s).
	float lifetime = 6.0f; ///< Seconds before it pops on its own.
	float bob_amp = 0.25f; ///< Main vertical bob (metres).
	float sway_amp = 0.15f; ///< Horizontal sway (metres).
	float wobble_amp = 0.07f; ///< Breathing squash-and-stretch fraction.
	float dip_depth = 0.25f; ///< How far it sinks when stood on (metres).

	// --- Runtime ---
	MeshInstance3D *mesh = nullptr;
	CollisionShape3D *shape = nullptr;
	Ref<ShaderMaterial> material;
	Vector3 base; ///< Floating anchor (launch + rise), before bob/dip offsets.
	Vector3 launch_velocity;
	Vector3 velocity; ///< Frame velocity, for carrying riders.
	Vector3 prev_position;
	float age = 0.0f;
	float phase = 0.0f; ///< Random per bubble so neighbours don't bob in sync.
	float dip = 0.0f; ///< Spring offset (negative = pushed down).
	float dip_velocity = 0.0f;
	bool stood_on = false; ///< Set by a rider this frame, consumed next tick.
	float pop_progress = -1.0f; ///< <0 alive; 0..1 popping.
	bool launched = false;

	static Ref<Shader> shared_shader; ///< Compiled once, shared by every bubble.
	static Ref<Shader> _shared_shader();
	void _build();
	void _tick_float(float p_dt);
	void _tick_pop(float p_dt);
	void _apply_wobble();

protected:
	static void _bind_methods();

public:
	Bubble();

	void _ready() override;
	void _physics_process(double p_delta) override;

	/// Fire from `p_origin` along `p_dir` (flattened to the ground plane).
	void launch(
			const Vector3 &p_origin,
			const Vector3 &p_dir
	);
	/// A rider stands on it this frame (it dips under the weight).
	void notify_stood_on() { stood_on = true; }
	/// Burst now: swell, fade, free.
	void pop();
	bool is_popping() const { return pop_progress >= 0.0f; }
	float get_age() const { return age; }
	Vector3 get_velocity() const { return velocity; }

	/// Release the shared shader. Called from the module's uninitialize, while the
	/// rendering server is still alive (a static Ref outliving it crashes at exit).
	static void clear_shader_cache() { shared_shader.unref(); }
};

} // namespace godot

#endif // BUBBLE_H
