#include "celeste_controller.h"
#include "../camera/camera.h"
#include "../debug_draw/debug_manager.h"
#include "../enemy/enemy_manager.h"
#include "../game_manager/game_constants.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../interaction/environment/moving_platform.h"
#include "../utils/raycast/mc_raycast.h"
#include "celeste_state.h"
#include "celeste_ui.h"
#include "cui/cui.h"
#include "states/airborne_states.h"
#include "states/combat_states.h"
#include "states/dash_states.h"
#include "states/grounded_states.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_shape_query_parameters3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CelesteController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_ui_toggle"), &CelesteController::_on_ui_toggle);
	ClassDB::bind_method(D_METHOD("save_settings"), &CelesteController::save_settings);
	ClassDB::bind_method(D_METHOD("load_settings"), &CelesteController::load_settings);
	ClassDB::bind_method(
			D_METHOD("_on_ui_slider_value_changed", "value", "property"),
			&CelesteController::_on_ui_slider_value_changed
	);
	ClassDB::bind_method(D_METHOD("get_speed_percent"), &CelesteController::get_speed_percent);
	ClassDB::bind_method(D_METHOD("set_control_scheme", "scheme"), &CelesteController::set_control_scheme);
	ClassDB::bind_method(D_METHOD("get_control_scheme"), &CelesteController::get_control_scheme);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "control_scheme", PROPERTY_HINT_ENUM, "Steer,Camera Relative,Legacy"),
			"set_control_scheme", "get_control_scheme"
	);
}

CelesteController::CelesteController() {
	max_speed = 10.0f;
	acceleration = 80.0f;
	friction = 60.0f;

	jump_height = 4.5f; // Higher jump
	jump_time_to_peak = 0.6f; // Slower ascent
	jump_time_to_descent = 0.55f; // Slower descent

	_update_jump_math();
}

CelesteController::~CelesteController() {
	if (ui_helper) {
		delete ui_helper;
		ui_helper = nullptr;
	}
	delete idle_state;
	delete move_state;
	delete jump_state;
	delete fall_state;
	delete grounded_state;
	delete airborne_state;
	delete double_jump_state;
	delete dash_state;
	delete jumpkick_state;
}

void CelesteController::_update_jump_math() {
	_jump_velocity0 = (2.0f * jump_height) / jump_time_to_peak;
	_jump_gravity = (2.0f * jump_height) / (jump_time_to_peak * jump_time_to_peak);
	_fall_gravity = (2.0f * jump_height) / (jump_time_to_descent * jump_time_to_descent);
}

void CelesteController::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	_update_jump_math();

	applyCollisionLayerMaskPlayer(this);

	// Initialize HSM States
	grounded_state = new CelesteGroundedState(this, nullptr);
	airborne_state = new CelesteAirborneState(this, nullptr);
	idle_state = new CelesteIdleState(this, grounded_state);
	move_state = new CelesteMoveState(this, grounded_state);
	jump_state = new CelesteJumpState(this, airborne_state);
	fall_state = new CelesteFallState(this, airborne_state);
	double_jump_state = new CelesteDoubleJumpState(this, airborne_state);
	dash_state = new CelesteDashState(this, airborne_state);
	jumpkick_state = new CelesteJumpKickState(this, nullptr);

	current_state = fall_state;
	current_state->enter();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_celeste_controller(this);
	}

	// Setup UI Vars Map
	ui_vars["max_speed"] = &max_speed;
	ui_vars["acceleration"] = &acceleration;
	ui_vars["friction"] = &friction;
	ui_vars["sprint_multiplier"] = &sprint_multiplier;
	ui_vars["air_resistance"] = &air_resistance;
	ui_vars["jump_height"] = &jump_height;
	ui_vars["jump_time_to_peak"] = &jump_time_to_peak;
	ui_vars["jump_time_to_descent"] = &jump_time_to_descent;
	ui_vars["max_fall_velocity"] = &max_fall_velocity;
	ui_vars["coyote_time"] = &coyote_time_max;
	ui_vars["jump_buffer"] = &jump_buffer_max;
	ui_vars["double_jump_mult"] = &double_jump_multiplier;
	ui_vars["dash_speed"] = &dash_speed;
	ui_vars["dash_duration"] = &dash_duration;
	ui_vars["melee_range"] = &melee_range;
	ui_vars["melee_speed"] = &melee_lunge_speed;

	ui_vars["spring_stiffness"] = &spring_stiffness;
	ui_vars["spring_damping"] = &spring_damping;

	// Setup UI
	ui_root = CUI::create_on_new_layer(this);
	ui_helper = new CelesteUI();
	ui_helper->setup(this, ui_root);
	load_settings();

	// Apply floor snapping defaults
	set_floor_snap_length(0.0f);
	set_floor_constant_speed_enabled(true);

	controls.seed(get_rotation().y);
	set_control_scheme(control_scheme);
	_prev_yaw = get_rotation().y;
	_find_skin();
	_audio.setup(this, "res://assets/character/sounds/");
}

void CelesteController::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_celeste_controller() == this) {
		gm->register_celeste_controller(nullptr);
	}
}

void CelesteController::change_state(CelesteState *p_new_state) {
	if (current_state == p_new_state)
		return;
	if (current_state)
		current_state->exit();
	current_state = p_new_state;
	if (p_new_state == (CelesteState *)jump_state || p_new_state == (CelesteState *)double_jump_state) {
		_jump_event = true; // ground, coyote, wall, dash and double jumps all land here
	}
	if (current_state)
		current_state->enter();
}

// ---------------------------------------------------------------------------
// Input gating + control schemes
// ---------------------------------------------------------------------------

const ActionState &CelesteController::input_state() const {
	static const ActionState empty{};
	PlayerInput *input = PlayerInput::get_singleton();
	return (_active && input) ? input->get_state() : empty;
}

float CelesteController::movement_strength() const {
	PlayerInput *input = PlayerInput::get_singleton();
	return (_active && input) ? input->get_movement_strength(sprint_multiplier) : 0.0f;
}

bool CelesteController::has_move_intent() const {
	return uses_scheme() ? controls.has_move() : input_state().character.move_axis.length() > 0.1f;
}

void CelesteController::set_control_scheme(int p_scheme) {
	control_scheme = CLAMP(p_scheme, 0, 2);
	if (uses_scheme()) {
		controls.set_scheme((CharacterControls::Scheme)control_scheme);
	}
}

void CelesteController::_update_controls(float p_delta) {
	if (!uses_scheme()) {
		return; // Legacy: the states drive rotation from the TPS / Fixed camera as before
	}
	GameManager *gm = GameManager::get_singleton();
	GameCamera *cam = (_active && gm) ? gm->get_camera() : nullptr;
	if (_active) {
		bool v_down = Input::get_singleton()->is_physical_key_pressed(KEY_V);
		if (v_down && !_scheme_key_was_down) {
			controls.toggle_scheme();
			control_scheme = (int)controls.get_scheme();
			UtilityFunctions::print(
					String("CelesteController: control scheme -> ") +
					(control_scheme == 0 ? "STEER (heading cam)" : "CAMERA-RELATIVE (platformer cam)")
			);
		}
		_scheme_key_was_down = v_down;
		const ActionState &st = input_state();
		controls.update(st.character.move_axis, st.camera.look_delta.x, cam, p_delta);
		controls.apply_camera(cam);
	} else {
		controls.idle();
	}
	// The body faces the heading (kinematic body + symmetric capsule: rotating it is free).
	set_rotation(Vector3(0.0f, controls.get_face_yaw(), 0.0f));
}

// Two-ray step test, measured from the real floor (the sole hovers above it):
//   - a LOW ray just above the floor catches a step riser (a steep face right ahead),
//   - a HIGH ray at max_ride_height above the floor; clear = short enough to float over,
//     blocked too = a wall (left to the wall jump).
// The previous single ray treated vertical risers as walls (normal.y < 0.25) and never lifted.
void CelesteController::_update_ride_height(
		const Vector3 &p_bottom,
		float p_delta
) {
	float target = min_ride_height;
	Vector3 forward = -get_global_transform().basis.get_column(2).normalized();
	Vector3 vel = get_velocity();
	float forward_speed = Vector3(vel.x, 0.0f, vel.z).dot(forward);

	if (_ride_probe_ground && forward_speed > 0.5f) {
		Vector3 up(0.0f, 1.0f, 0.0f);
		Vector3 ground = p_bottom - up * _last_ground_dist;
		Vector3 low = ground + up * 0.1f;
		Vector3 high = ground + up * (max_ride_height + 0.1f);
		TypedArray<RID> exclude;
		exclude.append(get_rid());
		MCRaycastHit low_hit = raycast_3d(this, low, low + forward * 1.0f, 1, exclude);
		bool high_clear = !raycast_3d(this, high, high + forward * 1.0f, 1, exclude).is_hit;
		bool riser = low_hit.is_hit && low_hit.normal.dot(up) < 0.5f;
		if (riser && high_clear) {
			target = max_ride_height;
		}
#if DEBUG
		DebugManager::get_singleton()->clear_line("forward_ray");
		if (riser && high_clear) {
			DebugManager::get_singleton()->draw_line("forward_ray", low, low_hit.position, 0.1f, Color(1, 1, 0), 0.1f);
		}
#endif
	}
	ride_height = Math::move_toward(ride_height, target, p_delta * ride_height_speed);
}

// A skin is any Node3D child exposing the intent API (idle/move/jump/fall). When one is
// present, the placeholder meshes (direct MeshInstance3D children) are hidden.
void CelesteController::_find_skin() {
	_skin = nullptr;
	for (int i = 0; i < get_child_count(); i++) {
		Node3D *c = Object::cast_to<Node3D>(get_child(i));
		if (c && !Object::cast_to<MeshInstance3D>(c) && _animator.set_skin(c)) {
			_skin = c;
			break;
		}
	}
	if (_skin) {
		for (int i = 0; i < get_child_count(); i++) {
			if (MeshInstance3D *m = Object::cast_to<MeshInstance3D>(get_child(i))) {
				m->set_visible(false);
			}
		}
	}
	UtilityFunctions::print(
			_skin ? String("CelesteController: skin bound -> ") + _skin->get_name()
				  : String("CelesteController: no skin child, using placeholder mesh")
	);
}

void CelesteController::_update_skin_and_audio(float p_delta) {
	Vector3 vel = get_velocity();
	float h_speed = Vector3(vel.x, 0.0f, vel.z).length();
	float yaw = get_rotation().y;
	if (_skin) {
		// Lean into turns: normalised yaw rate (right turn = +).
		float yaw_rate =
				(p_delta > 0.0f) ? UtilityFunctions::wrapf(yaw - _prev_yaw, -Math::PI, Math::PI) / p_delta : 0.0f;
		float lean = (controls.steer_rate > 0.001f) ? CLAMP(-yaw_rate / controls.steer_rate, -1.0f, 1.0f) : 0.0f;
		_animator.update(is_hovering, vel.y, h_speed, lean);
	}
	_prev_yaw = yaw;
	_audio.update(is_hovering, vel.y, h_speed, _jump_event, p_delta);
	_jump_event = false;
}

void CelesteController::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	float f_delta = (float)delta;
	PlayerInput *input = PlayerInput::get_singleton();
	if (!input)
		return;

	GameManager *gm_active = GameManager::get_singleton();
	_active = gm_active && gm_active->is_active(this);
	const ActionState &state = input_state();

	_update_controls(f_delta);

	// Update Coyote Timer
	if (is_on_floor()) {
		coyote_timer = coyote_time_max;
	} else {
		coyote_timer -= f_delta;
	}

	// Update Jump Buffer Timer
	if (state.character.jump_just_pressed) {
		jump_buffer_timer = jump_buffer_max;
	} else {
		jump_buffer_timer -= f_delta;
	}

	if (is_hovering) {
		is_jumping = false;
		can_dash = true;
		can_double_jump = true;
	}

	// Update Timers
	dash_cooldown_timer -= f_delta;

	// 1. Execute State Logic
	if (current_state) {
		current_state->physics_update(f_delta);
	}

	// 2. Hover Spring Logic (PD Controller)
	is_hovering = false;
	last_spring_error = 0.0f;
	bool had_ground = _has_last_ground;
	_has_last_ground = false; // re-set below if the hover ray hits this frame
	platform_velocity = Vector3(0, 0, 0);

#if DEBUG
	DebugManager::get_singleton()->clear_line("hover_ray");
	DebugManager::get_singleton()->clear_line("forward_ray");
#endif

	if (!is_jumping) {
		Vector3 bottom = get_global_position() - Vector3(0, half_height, 0);

		TypedArray<RID> exclude;
		exclude.append(get_rid());

		// Step detection: rise to max_ride_height for a short step ahead (two-ray test).
		_ride_probe_ground = had_ground;
		_update_ride_height(bottom, f_delta);

		Vector3 ray_origin = bottom + Vector3(0, 0.2f, 0);
		// Increase ray length to catch ground earlier
		Vector3 ray_dir = Vector3(0, -(ride_height + 1.0f), 0);

		MCRaycastHit b_hit = raycast_3d(this, ray_origin, ray_origin + ray_dir, 1, exclude);

		if (b_hit.is_hit) {
			is_hovering = true;
			float dist = (ray_origin - b_hit.position).length() - 0.2f;
			_last_ground_dist = MAX(0.0f, dist);
			_has_last_ground = true;
			float error = ride_height - dist;
			last_spring_error = error;

			Vector3 vel = get_velocity();

			// PD Controller: Force = Stiffness * error - Damping * velocity
			float spring_force = (error * spring_stiffness) - (vel.y * spring_damping);
			vel.y += spring_force * f_delta;

			set_velocity(vel);

			// set_velocity(vel);
			set_floor_snap_length(0.0f);

			// Detect if hovering over a moving platform
			MovingPlatform *moving_platform = Object::cast_to<MovingPlatform>(b_hit.collider);
			if (moving_platform) {
				platform_velocity = moving_platform->get_velocity();
			}

			// // Apply dynamic weight & reaction forces to RigidBody3D grounds (like the car)
			// RigidBody3D *rb = Object::cast_to<RigidBody3D>(b_hit.collider);
			// if (rb) {
			// 	float player_mass = 80.0f; // in kg
			// 	float gravity = 9.8f;
			// 	// Dynamic spring reaction + baseline weight/gravity force
			// 	float reaction_magnitude = (spring_force * player_mass) + (player_mass * gravity);
			// 	Vector3 reaction_force = Vector3(0.0f, -reaction_magnitude, 0.0f);

			// 	// Apply at contact point
			// 	Vector3 relative_hit_pos = b_hit.position - rb->get_global_transform().origin;
			// 	rb->apply_force(reaction_force, relative_hit_pos);
			// }

#if DEBUG
			// DebugManager::get_singleton()->draw_line("hover_ray", ray_origin, b_hit.position, 0.05f, Color(1, 0, 1,
			// 0.8f), 0.1f);

			// // Draw velocity vector
			// Vector3 current_vel = get_velocity();
			// DebugManager::get_singleton()->draw_line("velocity_vec", get_global_position(), get_global_position() +
			// current_vel * 0.5f, 0.1f, Color(0, 1, 0), 0.1f);
#endif
		}
	}

	// Apply platform velocity for move_and_slide
	if (platform_velocity != Vector3(0, 0, 0)) {
		set_velocity(get_velocity() + platform_velocity);
	}

	// 3. Apply Movement
	move_and_slide();

	// Restore relative velocity (subtracting the platform velocity from final velocity)
	if (platform_velocity != Vector3(0, 0, 0)) {
		set_velocity(get_velocity() - platform_velocity);
	}

	_update_skin_and_audio(f_delta);

	if (ui_helper) {
		ui_helper->update_graph(get_velocity().length());
	}
#if DEBUG
	// debug_draw_label();
	// debug_draw_trajectory(delta);
	// debug_draw_bottom();
#endif
}

void CelesteController::_on_ui_slider_value_changed(
		double p_value,
		String p_property
) {
	if (ui_vars.count(p_property)) {
		*ui_vars[p_property] = (float)p_value;

		// Re-calculate math if jump parameters changed
		if (p_property == "jump_height" || p_property == "jump_time_to_peak" || p_property == "jump_time_to_descent") {
			_update_jump_math();
		}
	}
}

float CelesteController::get_speed_percent() const {
	if (max_speed <= 0.001f)
		return 0.0f;
	Vector3 h_vel = get_velocity();
	h_vel.y = 0.0f;
	return h_vel.length() / max_speed;
}

void CelesteController::_on_ui_toggle() {
	if (ui_helper) {
		ui_helper->toggle_visibility();
	}
}

void CelesteController::save_settings() {
	Ref<ConfigFile> config;
	config.instantiate();

	config->set_value("Celeste", "max_speed", max_speed);
	config->set_value("Celeste", "acceleration", acceleration);
	config->set_value("Celeste", "friction", friction);
	config->set_value("Celeste", "sprint_multiplier", sprint_multiplier);
	config->set_value("Celeste", "air_resistance", air_resistance);
	config->set_value("Celeste", "jump_height", jump_height);
	config->set_value("Celeste", "jump_time_to_peak", jump_time_to_peak);
	config->set_value("celeste_physics", "jump_time_to_descent", jump_time_to_descent);
	config->set_value("celeste_physics", "max_fall_velocity", max_fall_velocity);
	config->save("user://celeste_settings.cfg");
	UtilityFunctions::print("CelesteController: Settings saved to user://celeste_settings.cfg");
}

void CelesteController::load_settings() {
	Ref<ConfigFile> config;
	config.instantiate();

	Error err = config->load("user://celeste_settings.cfg");
	if (err != OK)
		return;

	max_speed = config->get_value("Celeste", "max_speed", 10.0f);
	acceleration = config->get_value("Celeste", "acceleration", 80.0f);
	friction = config->get_value("Celeste", "friction", 60.0f);
	sprint_multiplier = config->get_value("Celeste", "sprint_multiplier", 1.5f);
	air_resistance = config->get_value("Celeste", "air_resistance", 20.0f);
	jump_height = config->get_value("Celeste", "jump_height", 2.5f);
	jump_time_to_peak = config->get_value("Celeste", "jump_time_to_peak", 0.4f);
	jump_time_to_descent = config->get_value("celeste_physics", "jump_time_to_descent", 0.2f);
	max_fall_velocity = config->get_value("celeste_physics", "max_fall_velocity", 20.0f);
	_update_jump_math();
	set_floor_snap_length(0.0f);
	set_floor_constant_speed_enabled(true);

	if (ui_root) {
		for (auto const &[name, ptr] : ui_vars) {
			ui_root->set_value(name, *ptr);
		}
	}

	UtilityFunctions::print("CelesteController: Settings loaded from user://celeste_settings.cfg");
}

Node3D *CelesteController::_find_melee_target() {
	EnemyManager *em = EnemyManager::get_singleton();
	if (!em)
		return nullptr;

	PlayerInput *input = PlayerInput::get_singleton();
	Vector3 input_dir = Vector3(0, 0, 0);
	if (input) {
		Vector2 axis = input->get_move_axis();
		if (axis.length() > 0.1f) {
			Node3D *cam = GameManager::get_singleton()->get_camera();
			if (cam) {
				Vector3 fwd = cam->get_global_transform().basis.get_column(2).normalized();
				fwd.y = 0;
				fwd.normalize();
				Vector3 right = cam->get_global_transform().basis.get_column(0).normalized();
				right.y = 0;
				right.normalize();
				input_dir = (fwd * axis.y + right * axis.x).normalized();
			}
		}
	}

	return em->get_best_target(get_global_position(), input_dir, melee_range);
}

Vector3 CelesteController::_collide_and_slide(
		const Vector3 &p_velocity,
		const Vector3 &p_normal
) {
	float dot = p_velocity.dot(p_normal);
	if (dot < 0.0f) {
		// Project velocity onto the plane perpendicular to the normal
		return p_velocity - p_normal * dot;
	}
	return p_velocity;
}

} // namespace godot
