#include "../cui/cui.h"
#include "ai/vehicle_states.h"
#include "arcade_vehicle.h"
#include "debug_draw/debug_manager.h"
#include "ui/arcade_vehicle_ui.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>

namespace godot {

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

} //namespace godot