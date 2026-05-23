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

	// Setup UI
	ui_root = CUI::create_on_new_layer(this);
	ui_helper = new ArcadeVehicleUI();
	ui_helper->setup(this, ui_root);
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
	SphereMesh *sphere = memnew(SphereMesh);
	sphere->set_radius(config->get_chassis_size().x);
	sphere->set_height(config->get_chassis_size().x * 2);
	chassis_mesh->set_mesh(sphere);
	add_child(chassis_mesh);

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

Ref<VehicleConfig> ArcadeVehicle::get_config() const {
	return config;
}

void ArcadeVehicle::set_debug_visuals_enabled(bool p_enabled) {
	debug_visuals_enabled = p_enabled;
	for (CSGSphere3D *visual : wheel_visuals) {
		visual->set_visible(debug_visuals_enabled);
	}
}

bool ArcadeVehicle::get_debug_visuals_enabled() const {
	return debug_visuals_enabled;
}

void ArcadeVehicle::_on_ui_toggle() {
	if (ui_helper) {
		ui_helper->toggle_visibility();
	}
}

void ArcadeVehicle::_on_ui_slider_value_changed(double p_value, String p_property) {
	set_ui_var(p_property, (float)p_value);
}

void ArcadeVehicle::save_settings() {
	if (config.is_null()) {
		return;
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	cfg->set_value("Vehicle", "max_speed", config->get_max_speed());
	cfg->set_value("Vehicle", "max_accel_force", config->get_max_accel_force());
	cfg->set_value("Vehicle", "brake_decel", config->get_brake_decel());
	cfg->set_value("Vehicle", "arcade_assist", config->get_arcade_assist());
	cfg->set_value("Vehicle", "max_steer_angle_deg", config->get_max_steer_angle_deg());
	cfg->set_value("Vehicle", "base_grip", config->get_base_grip());
	cfg->set_value("Vehicle", "drift_grip", config->get_drift_grip());
	cfg->set_value("Vehicle", "downforce", config->get_downforce());
	cfg->set_value("Vehicle", "angular_damping", config->get_angular_damping());
	cfg->set_value("Vehicle", "velocity_alignment", config->get_velocity_alignment());
	cfg->set_value("Vehicle", "nitro_max_fuel", config->get_nitro_max_fuel());
	cfg->set_value("Vehicle", "nitro_refuel_rate", config->get_nitro_refuel_rate());
	cfg->set_value("Vehicle", "nitro_depletion_rate", config->get_nitro_depletion_rate());
	cfg->set_value("Vehicle", "roll_influence", config->get_roll_influence());
	cfg->set_value("Vehicle", "pitch_influence", config->get_pitch_influence());

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

	config->set_max_speed(cfg->get_value("Vehicle", "max_speed", config->get_max_speed()));
	config->set_max_accel_force(cfg->get_value("Vehicle", "max_accel_force", config->get_max_accel_force()));
	config->set_brake_decel(cfg->get_value("Vehicle", "brake_decel", config->get_brake_decel()));
	config->set_arcade_assist(cfg->get_value("Vehicle", "arcade_assist", config->get_arcade_assist()));
	config->set_max_steer_angle_deg(cfg->get_value("Vehicle", "max_steer_angle_deg", config->get_max_steer_angle_deg()));
	config->set_base_grip(cfg->get_value("Vehicle", "base_grip", config->get_base_grip()));
	config->set_drift_grip(cfg->get_value("Vehicle", "drift_grip", config->get_drift_grip()));
	config->set_downforce(cfg->get_value("Vehicle", "downforce", config->get_downforce()));
	config->set_angular_damping(cfg->get_value("Vehicle", "angular_damping", config->get_angular_damping()));
	config->set_velocity_alignment(cfg->get_value("Vehicle", "velocity_alignment", config->get_velocity_alignment()));
	config->set_nitro_max_fuel(cfg->get_value("Vehicle", "nitro_max_fuel", config->get_nitro_max_fuel()));
	config->set_nitro_refuel_rate(cfg->get_value("Vehicle", "nitro_refuel_rate", config->get_nitro_refuel_rate()));
	config->set_nitro_depletion_rate(cfg->get_value("Vehicle", "nitro_depletion_rate", config->get_nitro_depletion_rate()));
	config->set_roll_influence(cfg->get_value("Vehicle", "roll_influence", config->get_roll_influence()));
	config->set_pitch_influence(cfg->get_value("Vehicle", "pitch_influence", config->get_pitch_influence()));

	// Update UI values if UI exists
	if (ui_root) {
		ui_root->set_value("max_speed", config->get_max_speed());
		ui_root->set_value("max_accel_force", config->get_max_accel_force());
		ui_root->set_value("brake_decel", config->get_brake_decel());
		ui_root->set_value("arcade_assist", config->get_arcade_assist());
		ui_root->set_value("max_steer_angle_deg", config->get_max_steer_angle_deg());
		ui_root->set_value("base_grip", config->get_base_grip());
		ui_root->set_value("drift_grip", config->get_drift_grip());
		ui_root->set_value("downforce", config->get_downforce());
		ui_root->set_value("angular_damping", config->get_angular_damping());
		ui_root->set_value("velocity_alignment", config->get_velocity_alignment());
		ui_root->set_value("roll_influence", config->get_roll_influence());
		ui_root->set_value("pitch_influence", config->get_pitch_influence());
	}

	UtilityFunctions::print("ArcadeVehicle: Settings loaded from user://vehicle_settings.cfg");
}

float ArcadeVehicle::get_ui_var(const String &p_name) const {
	if (config.is_null())
		return 0.0f;
	if (p_name == "max_speed")
		return config->get_max_speed();
	if (p_name == "max_accel_force")
		return config->get_max_accel_force();
	if (p_name == "brake_decel")
		return config->get_brake_decel();
	if (p_name == "arcade_assist")
		return config->get_arcade_assist();
	if (p_name == "max_steer_angle_deg")
		return config->get_max_steer_angle_deg();
	if (p_name == "base_grip")
		return config->get_base_grip();
	if (p_name == "drift_grip")
		return config->get_drift_grip();
	if (p_name == "downforce")
		return config->get_downforce();
	if (p_name == "angular_damping")
		return config->get_angular_damping();
	if (p_name == "velocity_alignment")
		return config->get_velocity_alignment();
	if (p_name == "nitro_max_fuel")
		return config->get_nitro_max_fuel();
	if (p_name == "nitro_refuel_rate")
		return config->get_nitro_refuel_rate();
	if (p_name == "nitro_depletion_rate")
		return config->get_nitro_depletion_rate();
	if (p_name == "roll_influence")
		return config->get_roll_influence();
	if (p_name == "pitch_influence")
		return config->get_pitch_influence();
	return 0.0f;
}

void ArcadeVehicle::set_ui_var(const String &p_name, float p_value) {
	if (config.is_null())
		return;
	if (p_name == "max_speed")
		config->set_max_speed(p_value);
	else if (p_name == "max_accel_force")
		config->set_max_accel_force(p_value);
	else if (p_name == "brake_decel")
		config->set_brake_decel(p_value);
	else if (p_name == "arcade_assist")
		config->set_arcade_assist(p_value);
	else if (p_name == "max_steer_angle_deg")
		config->set_max_steer_angle_deg(p_value);
	else if (p_name == "base_grip")
		config->set_base_grip(p_value);
	else if (p_name == "drift_grip")
		config->set_drift_grip(p_value);
	else if (p_name == "downforce")
		config->set_downforce(p_value);
	else if (p_name == "angular_damping")
		config->set_angular_damping(p_value);
	else if (p_name == "velocity_alignment")
		config->set_velocity_alignment(p_value);
	else if (p_name == "nitro_max_fuel")
		config->set_nitro_max_fuel(p_value);
	else if (p_name == "nitro_refuel_rate")
		config->set_nitro_refuel_rate(p_value);
	else if (p_name == "nitro_depletion_rate")
		config->set_nitro_depletion_rate(p_value);
	else if (p_name == "roll_influence")
		config->set_roll_influence(p_value);
	else if (p_name == "pitch_influence")
		config->set_pitch_influence(p_value);
}

void ArcadeVehicle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config", "config"), &ArcadeVehicle::set_config);
	ClassDB::bind_method(D_METHOD("get_config"), &ArcadeVehicle::get_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "config", PROPERTY_HINT_RESOURCE_TYPE, "VehicleConfig"), "set_config", "get_config");

	ClassDB::bind_method(D_METHOD("set_debug_visuals_enabled", "enabled"), &ArcadeVehicle::set_debug_visuals_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_visuals_enabled"), &ArcadeVehicle::get_debug_visuals_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_visuals_enabled"), "set_debug_visuals_enabled", "get_debug_visuals_enabled");

	ClassDB::bind_method(D_METHOD("_on_ui_toggle"), &ArcadeVehicle::_on_ui_toggle);
	ClassDB::bind_method(D_METHOD("save_settings"), &ArcadeVehicle::save_settings);
	ClassDB::bind_method(D_METHOD("load_settings"), &ArcadeVehicle::load_settings);
	ClassDB::bind_method(D_METHOD("_on_ui_slider_value_changed", "value", "property"), &ArcadeVehicle::_on_ui_slider_value_changed);
}

} //namespace godot