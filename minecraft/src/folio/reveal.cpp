#include "reveal.h"

#include "ticker.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

static const char *SG_CENTER = "folio_reveal_center";
static const char *SG_DISTANCE = "folio_reveal_distance";
static const char *SG_THICKNESS = "folio_reveal_thickness";
static const char *SG_COLOR = "folio_reveal_color";
static const char *SG_INTENSITY = "folio_reveal_intensity";

FolioReveal::FolioReveal() {}

FolioReveal::~FolioReveal() {}

void FolioReveal::_ready() {
	_register_globals();
	if (!_subscribe()) {
		call_deferred("_subscribe");
	}
	update();
}

bool FolioReveal::_subscribe() {
	if (subscribed) {
		return true;
	}
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (!ticker) {
		return false;
	}
	ticker->connect_tick(callable_mp(this, &FolioReveal::update), 10);
	subscribed = true;
	return true;
}

void FolioReveal::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	// Register once per process (add is runtime-safe; never use the editor-only get).
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_CENTER, RenderingServer::GLOBAL_VAR_TYPE_VEC2, Vector2());
		rs->global_shader_parameter_add(SG_DISTANCE, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_THICKNESS, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_COLOR, RenderingServer::GLOBAL_VAR_TYPE_COLOR, Color());
		rs->global_shader_parameter_add(SG_INTENSITY, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		s_added = true;
	}
	globals_registered = true;
}

void FolioReveal::_update_globals() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_CENTER, center);
	rs->global_shader_parameter_set(SG_DISTANCE, (float)distance);
	rs->global_shader_parameter_set(SG_THICKNESS, (float)thickness);
	rs->global_shader_parameter_set(SG_COLOR, color);
	rs->global_shader_parameter_set(SG_INTENSITY, (float)(intensity * intensity_multiplier));
}

void FolioReveal::update() { _update_globals(); }

void FolioReveal::set_center(const Vector3 &p_pos) { center = Vector2(p_pos.x, p_pos.z); }

void FolioReveal::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioReveal::update);
	ClassDB::bind_method(D_METHOD("_subscribe"), &FolioReveal::_subscribe);
	ClassDB::bind_method(D_METHOD("set_center", "pos"), &FolioReveal::set_center);
	ClassDB::bind_method(D_METHOD("set_distance", "distance"), &FolioReveal::set_distance);
	ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &FolioReveal::set_thickness);
	ClassDB::bind_method(D_METHOD("set_intensity_multiplier", "mul"), &FolioReveal::set_intensity_multiplier);
	ClassDB::bind_method(D_METHOD("set_day_reveal_color", "color"), &FolioReveal::set_day_reveal_color);
	ClassDB::bind_method(D_METHOD("set_day_reveal_intensity", "intensity"), &FolioReveal::set_day_reveal_intensity);
	ClassDB::bind_method(D_METHOD("get_distance"), &FolioReveal::get_distance);
}

} // namespace godot
