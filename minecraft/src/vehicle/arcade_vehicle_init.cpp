#include "../cui/cui.h"
#include "ai/vehicle_states.h"
#include "arcade_vehicle.h"
#include "debug_draw/debug_manager.h"
#include "game_manager/game_manager.h"
#include "godot_cpp/classes/capsule_shape3d.hpp"
#include "godot_cpp/classes/csg_cylinder3d.hpp"
#include "godot_cpp/classes/cylinder_shape3d.hpp"
#include "godot_cpp/classes/sphere_mesh.hpp"
#include "ui/arcade_vehicle_ui.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>

namespace godot {

// Config properties persisted to user://vehicle_settings.cfg and mirrored by the
// tuning UI. Single source of truth: save/load and the UI iterate this list, so a
// new tunable only needs adding here (plus the bound property on VehicleConfig).
namespace {
const char *VEHICLE_TUNABLES[] = { "max_speed",
								   "max_accel_force",
								   "brake_decel",
								   "arcade_assist",
								   "max_steer_angle_deg",
								   "base_grip",
								   "drift_grip",
								   "downforce",
								   "angular_damping",
								   "velocity_alignment",
								   "nitro_max_fuel",
								   "nitro_refuel_rate",
								   "nitro_depletion_rate",
								   "roll_influence",
								   "pitch_influence" };
} // namespace

ArcadeVehicle::ArcadeVehicle() {
	is_boosting = false;
	boost_speed_bonus = 0.0f;
	ui_helper = nullptr;
	ui_root = nullptr;

	was_on_ramp = false;
	last_roll_tilt = 0.0f;
}

ArcadeVehicle::~ArcadeVehicle() {
	if (ui_helper) {
		delete ui_helper;
		ui_helper = nullptr;
	}

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

void ArcadeVehicle::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;

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

	// Setup UI (debug tuning HUD; only when debug visuals are enabled)
	if (debug_visuals_enabled) {
		ui_root = CUI::create_on_new_layer(this);
		ui_helper = new ArcadeVehicleUI();
		ui_helper->setup(this, ui_root);
	}
	load_settings();

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

void ArcadeVehicle::_on_ui_toggle() {
	if (ui_helper) {
		ui_helper->toggle_visibility();
	}
}

void ArcadeVehicle::_on_ui_slider_value_changed(
		double p_value,
		String p_property
) {
	set_ui_var(p_property, (float)p_value);
}

void ArcadeVehicle::save_settings() {
	if (config.is_null()) {
		return;
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	for (const char *key : VEHICLE_TUNABLES) {
		cfg->set_value("Vehicle", key, config->get(key));
	}

	cfg->save("user://vehicle_settings.cfg");
	UtilityFunctions::print("ArcadeVehicle: Settings saved to user://vehicle_settings.cfg");
}

void ArcadeVehicle::load_settings() {
	if (config.is_null()) {
		return;
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	Error err = cfg->load("user://vehicle_settings.cfg");
	if (err != OK) {
		return;
	}

	// Load only keys that are present, falling back to the current value, and mirror
	// each into the tuning UI (set_value no-ops for keys without a matching slider).
	for (const char *key : VEHICLE_TUNABLES) {
		if (!cfg->has_section_key("Vehicle", key)) {
			continue;
		}
		config->set(key, cfg->get_value("Vehicle", key, config->get(key)));
		if (ui_root) {
			ui_root->set_value(key, (float)config->get(key));
		}
	}

	UtilityFunctions::print("ArcadeVehicle: Settings loaded from user://vehicle_settings.cfg");
}

float ArcadeVehicle::get_ui_var(const String &p_name) const {
	if (config.is_null())
		return 0.0f;
	// Bound VehicleConfig properties are addressable by name via the object system.
	return (float)config->get(p_name);
}

void ArcadeVehicle::set_ui_var(
		const String &p_name,
		float p_value
) {
	if (config.is_null())
		return;
	// Bound VehicleConfig properties are addressable by name via the object system.
	config->set(p_name, p_value);
}

void ArcadeVehicle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config", "config"), &ArcadeVehicle::set_config);
	ClassDB::bind_method(D_METHOD("get_config"), &ArcadeVehicle::get_config);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "config", PROPERTY_HINT_RESOURCE_TYPE, "VehicleConfig"), "set_config",
			"get_config"
	);

	ClassDB::bind_method(D_METHOD("set_debug_visuals_enabled", "enabled"), &ArcadeVehicle::set_debug_visuals_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_visuals_enabled"), &ArcadeVehicle::get_debug_visuals_enabled);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "debug_visuals_enabled"), "set_debug_visuals_enabled",
			"get_debug_visuals_enabled"
	);

	ClassDB::bind_method(D_METHOD("_on_ui_toggle"), &ArcadeVehicle::_on_ui_toggle);
	ClassDB::bind_method(D_METHOD("save_settings"), &ArcadeVehicle::save_settings);
	ClassDB::bind_method(D_METHOD("load_settings"), &ArcadeVehicle::load_settings);
	ClassDB::bind_method(
			D_METHOD("_on_ui_slider_value_changed", "value", "property"), &ArcadeVehicle::_on_ui_slider_value_changed
	);
}

} //namespace godot