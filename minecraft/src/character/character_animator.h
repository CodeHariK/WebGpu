#ifndef CHARACTER_ANIMATOR_H
#define CHARACTER_ANIMATOR_H

#include <godot_cpp/classes/node.hpp>

namespace godot {

/**
 * CharacterAnimator — maps the controller's locomotion facts onto a skin's intent API.
 * ----------------------------------------------------------------------------------
 * Modularity contract: the SKIN owns its AnimationTree and ALL transition / crossfade
 * logic (which clip, how long the blend is, which edges are legal). This class never
 * names an animation. It only decides which high-level locomotion state the body is in
 * and tells the skin ONCE when that changes, through four duck-typed intent methods:
 *
 *     idle()   move()   jump()   fall()        (+ optional `run_tilt` lean property)
 *
 * Any scene exposing those methods is a valid skin (GDQuest's SophiaSkin, a future
 * custom rig, a placeholder) — swap it in the scene and nothing here changes.
 */
class CharacterAnimator {
public:
	enum Locomotion {
		LOCO_NONE, ///< No skin / not yet decided.
		LOCO_IDLE, ///< Grounded, (near) still.
		LOCO_MOVE, ///< Grounded, travelling.
		LOCO_JUMP, ///< Airborne and rising.
		LOCO_FALL ///< Airborne and descending.
	};

private:
	Node *skin = nullptr; ///< The skin node (not owned). Must have idle/move/jump/fall.
	Locomotion current = LOCO_NONE; ///< Last state we told the skin about.
	bool has_tilt = false; ///< Skin exposes a `run_tilt` lean property.

	void _travel(Locomotion p_state);

public:
	/// Bind a skin. Returns false (and binds nothing) if it lacks the intent API.
	bool set_skin(Node *p_skin);
	bool has_skin() const { return skin != nullptr; }

	/// Call once per physics frame with the body's current facts.
	///   p_grounded : on the ground (ride servo engaged)
	///   p_vy       : vertical velocity (m/s), + = up
	///   p_h_speed  : horizontal speed (m/s)
	///   p_lean     : -1..1 lean into a turn (right = +), fed to `run_tilt` if present
	void update(
			bool p_grounded,
			float p_vy,
			float p_h_speed,
			float p_lean
	);
};

} // namespace godot

#endif // CHARACTER_ANIMATOR_H
