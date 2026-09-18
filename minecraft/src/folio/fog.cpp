#include "fog.h"

#include "game.h"
#include "ticker.h"
#include "view/view.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

static const char *SG_COLOR_A = "folio_fog_color_a";
static const char *SG_COLOR_B = "folio_fog_color_b";
static const char *SG_CENTER = "folio_fog_radial_center";
static const char *SG_START = "folio_fog_radial_start";
static const char *SG_END = "folio_fog_radial_end";
static const char *SG_NEAR = "folio_fog_near";
static const char *SG_FAR = "folio_fog_far";

FolioFog::FolioFog() {}

FolioFog::~FolioFog() {}

void FolioFog::_ready() {
	_register_globals();
	if (!_subscribe()) {
		call_deferred("_subscribe");
	}
	update();
}

bool FolioFog::_subscribe() {
	if (subscribed) {
		return true;
	}
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (!ticker) {
		return false;
	}
	ticker->connect_tick(callable_mp(this, &FolioFog::update), 10);
	subscribed = true;
	return true;
}

void FolioFog::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	// Register once per process (add is runtime-safe; never use the editor-only get).
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_COLOR_A, RenderingServer::GLOBAL_VAR_TYPE_COLOR, Color());
		rs->global_shader_parameter_add(SG_COLOR_B, RenderingServer::GLOBAL_VAR_TYPE_COLOR, Color());
		rs->global_shader_parameter_add(SG_CENTER, RenderingServer::GLOBAL_VAR_TYPE_VEC2, Vector2());
		rs->global_shader_parameter_add(SG_START, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_END, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 1.0f);
		rs->global_shader_parameter_add(SG_NEAR, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 0.0f);
		rs->global_shader_parameter_add(SG_FAR, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 1.0f);
		s_added = true;
	}
	globals_registered = true;
}

void FolioFog::_update_globals() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_COLOR_A, color_a);
	rs->global_shader_parameter_set(SG_COLOR_B, color_b);
	rs->global_shader_parameter_set(SG_CENTER, radial_center);
	rs->global_shader_parameter_set(SG_START, (float)radial_start);
	rs->global_shader_parameter_set(SG_END, (float)radial_end);
	rs->global_shader_parameter_set(SG_NEAR, (float)near_distance);
	rs->global_shader_parameter_set(SG_FAR, (float)far_distance);
}

void FolioFog::update() {
	// Pull the framed near/far ground distances from the View, then compress by the
	// day-cycle ratios (folio: near = nd + nearRatio*amp, far = nd + farRatio*amp).
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_view()) {
		const double nd = game->get_view()->get_optimal_near_distance();
		const double fd = game->get_view()->get_optimal_far_distance();
		const double amp = fd - nd;
		near_distance = nd + fog_near_ratio * amp;
		far_distance = nd + fog_far_ratio * amp;
	}
	_update_globals();
}

void FolioFog::set_day_fog_colors(
		const Color &p_a,
		const Color &p_b
) {
	color_a = p_a;
	color_b = p_b;
}

void FolioFog::set_day_fog_ratios(
		double p_near_ratio,
		double p_far_ratio
) {
	fog_near_ratio = p_near_ratio;
	fog_far_ratio = p_far_ratio;
}

void FolioFog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioFog::update);
	ClassDB::bind_method(D_METHOD("_subscribe"), &FolioFog::_subscribe);
	ClassDB::bind_method(D_METHOD("set_day_fog_colors", "a", "b"), &FolioFog::set_day_fog_colors);
	ClassDB::bind_method(D_METHOD("set_day_fog_ratios", "near_ratio", "far_ratio"), &FolioFog::set_day_fog_ratios);
}

} // namespace godot
