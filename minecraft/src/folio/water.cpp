#include "water.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

static const char *SG_ELEVATION = "folio_water_surface_elevation";
static const char *SG_THICKNESS = "folio_water_surface_thickness";

FolioWater::FolioWater() {}

FolioWater::~FolioWater() {}

void FolioWater::_ready() {
	_register_globals();
	_push();
}

void FolioWater::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	// Register once per process (add is runtime-safe; never use the editor-only get).
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_ELEVATION, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_THICKNESS, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		s_added = true;
	}
	globals_registered = true;
}

void FolioWater::_push() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_ELEVATION, (float)surface_elevation);
	rs->global_shader_parameter_set(SG_THICKNESS, (float)surface_thickness);
}

void FolioWater::set_surface_elevation(double p_value) {
	surface_elevation = p_value;
	_push();
}

void FolioWater::set_surface_thickness(double p_value) {
	surface_thickness = p_value;
	_push();
}

void FolioWater::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_surface_elevation", "value"), &FolioWater::set_surface_elevation);
	ClassDB::bind_method(D_METHOD("set_surface_thickness", "value"), &FolioWater::set_surface_thickness);
	ClassDB::bind_method(D_METHOD("get_surface_elevation"), &FolioWater::get_surface_elevation);
	ClassDB::bind_method(D_METHOD("get_surface_thickness"), &FolioWater::get_surface_thickness);
	ClassDB::bind_method(D_METHOD("get_depth_elevation"), &FolioWater::get_depth_elevation);
}

} // namespace godot
