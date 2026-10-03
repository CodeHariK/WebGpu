#include "ai/vehicle_states.h"
#include "arcade_vehicle.h"
#include "debug_draw/debug_manager.h"
#include "game_manager/game_manager.h"
#include "godot_cpp/classes/capsule_shape3d.hpp"
#include "godot_cpp/classes/csg_cylinder3d.hpp"
#include "godot_cpp/classes/cylinder_shape3d.hpp"
#include "godot_cpp/classes/sphere_mesh.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace godot {


ArcadeVehicle::ArcadeVehicle() {
	is_boosting = false;
	boost_speed_bonus = 0.0f;

	was_on_ramp = false;
	last_roll_tilt = 0.0f;
}

ArcadeVehicle::~ArcadeVehicle() {

	delete grounded_state;
	delete airborne_state;
	delete driving_state;
	delete drifting_state;
	delete gliding_state;
	delete ramp_spin_state;
	delete ramp_roll_state;

	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		dm->clear_text("veh_" + get_name() + "_drift");
		dm->clear_trajectory("traj_" + get_name());
	}
}

void ArcadeVehicle::_resolve_preset() {
	// A `--car=<name>` command-line argument overrides the inspector `preset`.
	String name = preset;
	OS *os = OS::get_singleton();
	if (os) {
		PackedStringArray args = os->get_cmdline_args();
		args.append_array(os->get_cmdline_user_args());
		for (int i = 0; i < args.size(); i++) {
			String a = args[i];
			if (a.begins_with("--car=") && !a.substr(6).is_empty()) {
				name = a.substr(6);
			}
		}
	}
	if (name.is_empty()) {
		return;
	}
	String path = "res://vehicle/presets/" + name + ".tres";
	Ref<VehicleConfig> loaded = ResourceLoader::get_singleton()->load(path);
	if (loaded.is_valid()) {
		config = loaded;
		UtilityFunctions::print("ArcadeVehicle: using preset '", name, "' (", path, ")");
	} else {
		UtilityFunctions::printerr("ArcadeVehicle: preset not found > ", path);
	}
}

void ArcadeVehicle::set_preset(const String &p_preset) { preset = p_preset; }
String ArcadeVehicle::get_preset() const { return preset; }

void ArcadeVehicle::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	// Pick up a preset (inspector `preset` or `--car=<name>`) before we clone config.
	_resolve_preset();

	// Work on a per-instance copy of the config so runtime tuning and loaded
	// settings never mutate the shared VehicleConfig asset on disk.
	if (config.is_valid()) {
		config = Ref<VehicleConfig>(Object::cast_to<VehicleConfig>(config->duplicate().ptr()));
	}

	// 1. Setup HSM
	grounded_state = new GroundedState("grounded", this, nullptr);
	airborne_state = new AirborneState("airborne", this, nullptr);
	driving_state = new DrivingState("driving", this, grounded_state);
	drifting_state = new DriftingState("drifting", this, grounded_state);
	gliding_state = new GlidingState("gliding", this, airborne_state);
	ramp_spin_state = new RampSpinState("ramp_spin", this, airborne_state);
	ramp_roll_state = new RampRollState("ramp_roll", this, airborne_state);

	current_state = driving_state;
	current_state->enter();

	_setup_vehicle();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_vehicle(this);
	}

	_setup_tuning();

	if (config.is_valid()) {
		nitro_fuel = config->get_nitro_max_fuel();
	}
}

void ArcadeVehicle::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_vehicle() == this) {
		gm->register_vehicle(nullptr);
	}
}

void ArcadeVehicle::change_state(VehicleState *new_state) {
	if (current_state == new_state)
		return;

	if (current_state)
		current_state->exit();
	current_state = new_state;
	if (current_state)
		current_state->enter();
}

void ArcadeVehicle::_setup_vehicle() {
	if (config.is_null()) {
		UtilityFunctions::printerr("ArcadeVehicle has no config assigned.");
		return;
	}

	// 1. Setup physics properties
	set_mass(config->get_mass());
	set_center_of_mass_mode(RigidBody3D::CENTER_OF_MASS_MODE_CUSTOM);
	current_com_offset = config->get_center_of_mass_offset();
	set_center_of_mass(current_com_offset);

	// 2. Clear old children if any
	if (chassis_collider) {
		chassis_collider->queue_free();
		chassis_collider = nullptr;
	}
	if (chassis_mesh) {
		chassis_mesh->queue_free();
		chassis_mesh = nullptr;
	}
	for (CSGSphere3D *visual : wheel_visuals) {
		visual->queue_free();
	}
	wheel_visuals.clear();

	// 3. Create Chassis Collider
	chassis_collider = memnew(CollisionShape3D);
	chassis_shape = memnew(SphereShape3D);
	chassis_shape->set_radius(config->get_chassis_size().x);
	chassis_collider->set_shape(chassis_shape);
	add_child(chassis_collider);

	// 4. Create Chassis Mesh (Visual)
	chassis_mesh = memnew(MeshInstance3D);
	Ref<SphereMesh> sphere = memnew(SphereMesh);
	sphere->set_radius(config->get_chassis_size().x);
	sphere->set_height(config->get_chassis_size().x * 2);
	chassis_mesh->set_mesh(sphere);
	add_child(chassis_mesh);
	chassis_mesh->set_visible(debug_visuals_enabled); // debug-only; hide so the real car mesh shows

	// 5. Create wheel debug visuals
	TypedArray<WheelConfig> wconfigs = config->get_wheel_configs();
	for (int i = 0; i < wconfigs.size(); i++) {
		Ref<WheelConfig> wc = wconfigs[i];
		if (wc.is_null())
			continue;

		CSGSphere3D *visual = memnew(CSGSphere3D);
		visual->set_radius(wc->get_radius());
		visual->set_radial_segments(12);
		visual->set_rings(6);
		// Color it so it's a visible tire
		visual->set_use_collision(false);

		if (!debug_visuals_enabled) {
			visual->hide();
		}

		add_child(visual);
		wheel_visuals.push_back(visual);
	}
}

void ArcadeVehicle::set_config(const Ref<VehicleConfig> &p_config) {
	config = p_config;
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		_setup_vehicle();
	}
}

Ref<VehicleConfig> ArcadeVehicle::get_config() const { return config; }

void ArcadeVehicle::set_debug_visuals_enabled(bool p_enabled) {
	debug_visuals_enabled = p_enabled;
	for (CSGSphere3D *visual : wheel_visuals) {
		visual->set_visible(debug_visuals_enabled);
	}
	if (chassis_mesh) {
		chassis_mesh->set_visible(debug_visuals_enabled);
	}
}

bool ArcadeVehicle::get_debug_visuals_enabled() const { return debug_visuals_enabled; }

/**
 * @brief Describe the VehicleConfig tunables for the shared TuningPanel and apply saved
 * values (user://vehicle_settings.cfg). The tab only exists with debug visuals on and is
 * shown only while this vehicle is the one being driven (see _physics_process).
 */
void ArcadeVehicle::_setup_tuning() {
	if (config.is_null()) {
		return;
	}
	Object *cfg = config.ptr();
	tuning.begin("Car: " + String(get_name()), "user://vehicle_settings.cfg", "Vehicle");
	tuning.header("Driving");
	tuning.slider("Max Speed", "max_speed", cfg, 5.0f, 100.0f, 0.5f);
	tuning.slider("Max Accel Force", "max_accel_force", cfg, 1000.0f, 25000.0f, 100.0f);
	tuning.slider("Brake Decel", "brake_decel", cfg, 1000.0f, 30000.0f, 100.0f);
	tuning.slider("Arcade Assist", "arcade_assist", cfg, 0.0f, 10.0f, 0.1f);
	tuning.header("Steering");
	tuning.slider("Max Steer Angle", "max_steer_angle_deg", cfg, 5.0f, 60.0f, 0.5f);
	tuning.slider("Turn Radius", "turn_radius", cfg, 2.0f, 25.0f, 0.5f);
	tuning.slider("Turn Speed", "turn_speed", cfg, 0.5f, 12.0f, 0.1f);
	tuning.slider("Max Yaw Rate", "max_yaw_rate", cfg, 0.3f, 4.0f, 0.05f);
	tuning.header("Grip / Drift");
	tuning.slider("Base Grip", "base_grip", cfg, 0.0f, 2.0f, 0.05f);
	tuning.slider("Drift Grip", "drift_grip", cfg, 0.0f, 2.0f, 0.05f);
	tuning.slider("Grip Limit", "grip_lateral_accel", cfg, 5.0f, 60.0f, 0.5f);
	tuning.slider("Drift Grip Limit", "drift_lateral_accel", cfg, 2.0f, 40.0f, 0.5f);
	tuning.slider("Mini-Turbo Boost", "mini_turbo_boost", cfg, 0.0f, 20.0f, 0.5f);
	tuning.header("Body");
	tuning.slider("Downforce", "downforce", cfg, 0.0f, 10000.0f, 50.0f);
	tuning.slider("Yaw Damping", "angular_damping", cfg, 0.0f, 20.0f, 0.1f);
	tuning.slider("Vel Alignment", "velocity_alignment", cfg, 0.0f, 10.0f, 0.1f);
	tuning.slider("Roll Influence", "roll_influence", cfg, 0.0f, 1.0f, 0.05f);
	tuning.slider("Pitch Influence", "pitch_influence", cfg, 0.0f, 1.0f, 0.05f);
	tuning.header("Nitro");
	tuning.slider("Max Fuel", "nitro_max_fuel", cfg, 10.0f, 300.0f, 5.0f);
	tuning.slider("Refuel Rate", "nitro_refuel_rate", cfg, 0.0f, 100.0f, 1.0f);
	tuning.slider("Depletion Rate", "nitro_depletion_rate", cfg, 0.0f, 100.0f, 0.5f);
	tuning.graph("Speed", 0.0f, config->get_max_speed() + config->get_drift_boost_max_speed_bonus());
	tuning.load();
	if (debug_visuals_enabled) {
		tuning.attach();
		tuning.set_shown(false); // revealed while driven
	}
}

void ArcadeVehicle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config", "config"), &ArcadeVehicle::set_config);
	ClassDB::bind_method(D_METHOD("get_config"), &ArcadeVehicle::get_config);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "config", PROPERTY_HINT_RESOURCE_TYPE, "VehicleConfig"), "set_config",
			"get_config"
	);

	ClassDB::bind_method(D_METHOD("set_preset", "preset"), &ArcadeVehicle::set_preset);
	ClassDB::bind_method(D_METHOD("get_preset"), &ArcadeVehicle::get_preset);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preset"), "set_preset", "get_preset");

	ClassDB::bind_method(D_METHOD("set_debug_visuals_enabled", "enabled"), &ArcadeVehicle::set_debug_visuals_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_visuals_enabled"), &ArcadeVehicle::get_debug_visuals_enabled);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "debug_visuals_enabled"), "set_debug_visuals_enabled",
			"get_debug_visuals_enabled"
	);

	// GDScript-facing accessors for the folio car visual (planted wheels + drift FX).
	ClassDB::bind_method(D_METHOD("get_is_drifting"), &ArcadeVehicle::get_is_drifting);
	ClassDB::bind_method(D_METHOD("get_wheel_count"), &ArcadeVehicle::get_wheel_count);
	ClassDB::bind_method(D_METHOD("get_wheel_displacement", "index"), &ArcadeVehicle::get_wheel_displacement);

}

} //namespace godot