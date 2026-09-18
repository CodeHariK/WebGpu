#include "wind.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

#include "game.h"
#include "ticker.h"

using namespace godot;

static const char *SG_DIR = "folio_wind_direction";
static const char *SG_POS_FREQ = "folio_wind_position_frequency";
static const char *SG_STRENGTH = "folio_wind_strength";
static const char *SG_TIME = "folio_wind_time";

FolioWind::FolioWind() {
	angle = Math::PI * 0.6;
	direction = Vector2(Math::sin(angle), Math::cos(angle));
}

FolioWind::~FolioWind() {}

void FolioWind::_ready() {
	_register_globals();
	_push();

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioWind::update), 9);
	}
}

void FolioWind::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_DIR, RenderingServer::GLOBAL_VAR_TYPE_VEC2, Vector2());
		rs->global_shader_parameter_add(SG_POS_FREQ, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.5f);
		rs->global_shader_parameter_add(SG_STRENGTH, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.5f);
		rs->global_shader_parameter_add(SG_TIME, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		s_added = true;
	}
	globals_registered = true;
}

void FolioWind::_push() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_DIR, direction);
	rs->global_shader_parameter_set(SG_POS_FREQ, (float)position_frequency);
	rs->global_shader_parameter_set(SG_STRENGTH, (float)strength);
	rs->global_shader_parameter_set(SG_TIME, (float)local_time);
}

void FolioWind::update() {
	FolioTicker *ticker = FolioTicker::get_singleton();
	const double delta_scaled = ticker ? ticker->get_delta_scaled() : (1.0 / 30.0);
	// folio: localTime += deltaScaled * timeFrequency * strength
	local_time += delta_scaled * time_frequency * strength;
	_push();
}

void FolioWind::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioWind::update);
	ClassDB::bind_method(D_METHOD("set_strength", "v"), &FolioWind::set_strength);
	ClassDB::bind_method(D_METHOD("get_strength"), &FolioWind::get_strength);
}
