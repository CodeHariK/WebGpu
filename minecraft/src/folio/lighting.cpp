#include "lighting.h"

#include "game.h"
#include "ticker.h"
#include "view/spherical.h"
#include "view/view.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// Global shader uniform names (consumed by the base material).
static const char *SG_DIR = "folio_light_direction";
static const char *SG_COLOR = "folio_light_color";
static const char *SG_INTENSITY = "folio_light_intensity";
static const char *SG_BOUNCE_LOW = "folio_light_bounce_edge_low";
static const char *SG_BOUNCE_HIGH = "folio_light_bounce_edge_high";
static const char *SG_BOUNCE_DIST = "folio_light_bounce_distance";
static const char *SG_BOUNCE_MUL = "folio_light_bounce_multiplier";
static const char *SG_BOUNCE_COLOR = "folio_bounce_color";
static const char *SG_CORE_LOW = "folio_core_shadow_edge_low";
static const char *SG_CORE_HIGH = "folio_core_shadow_edge_high";
static const char *SG_SHADOW_COLOR = "folio_shadow_color";

FolioLighting::FolioLighting() {}

FolioLighting::~FolioLighting() {}

void FolioLighting::_ready() {
	_refresh_from_view();
	depth = radius * 2.0;

	direction = FolioViewSpherical::from_spherical(1.0, phi, theta).normalized();

	// Real directional light (drop shadows).
	light = memnew(DirectionalLight3D);
	light->set_shadow(true);
	add_child(light);
	_update_shadow();

	_register_globals();

	if (!_subscribe()) {
		call_deferred("_subscribe");
	}
	update();
}

void FolioLighting::_refresh_from_view() {
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_view()) {
		const double r = game->get_view()->get_optimal_radius();
		if (r > 0.0) {
			radius = r;
		}
	}
}

bool FolioLighting::_subscribe() {
	if (subscribed) {
		return true;
	}
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (!ticker) {
		return false;
	}
	ticker->connect_tick(callable_mp(this, &FolioLighting::update), 9);
	subscribed = true;
	return true;
}

void FolioLighting::_update_shadow() {
	if (!light) {
		return;
	}
	light->set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, near_plane + depth);
	light->set_param(Light3D::PARAM_SHADOW_BIAS, shadow_bias);
	light->set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, shadow_normal_bias);
	light->set_param(Light3D::PARAM_SHADOW_BLUR, shadow_blur);
	light->set_param(Light3D::PARAM_ENERGY, light_intensity);
}

void FolioLighting::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	struct G {
		const char *name;
		RenderingServer::GlobalShaderParameterType type;
	};
	const G gs[] = {
		{ SG_DIR, RenderingServer::GLOBAL_VAR_TYPE_VEC3 },
		{ SG_COLOR, RenderingServer::GLOBAL_VAR_TYPE_COLOR },
		{ SG_INTENSITY, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_BOUNCE_LOW, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_BOUNCE_HIGH, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_BOUNCE_DIST, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_BOUNCE_MUL, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_BOUNCE_COLOR, RenderingServer::GLOBAL_VAR_TYPE_COLOR },
		{ SG_CORE_LOW, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_CORE_HIGH, RenderingServer::GLOBAL_VAR_TYPE_FLOAT },
		{ SG_SHADOW_COLOR, RenderingServer::GLOBAL_VAR_TYPE_COLOR },
	};
	static bool s_added = false;
	if (!s_added) {
		// global_shader_parameter_get is EDITOR-ONLY (errors at runtime); never check
		// existence here. Adding is runtime-safe; the static guard avoids re-adding.
		for (const G &g : gs) {
			Variant def = (g.type == RenderingServer::GLOBAL_VAR_TYPE_FLOAT) ? Variant(0.0f)
					: (g.type == RenderingServer::GLOBAL_VAR_TYPE_VEC3)		 ? Variant(Vector3())
																			 : Variant(Color());
			rs->global_shader_parameter_add(g.name, g.type, def);
		}
		s_added = true;
	}
	globals_registered = true;
}

void FolioLighting::_update_globals() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	rs->global_shader_parameter_set(SG_DIR, direction);
	rs->global_shader_parameter_set(SG_COLOR, light_color);
	rs->global_shader_parameter_set(SG_INTENSITY, (float)light_intensity);
	rs->global_shader_parameter_set(SG_BOUNCE_LOW, (float)bounce_edge_low);
	rs->global_shader_parameter_set(SG_BOUNCE_HIGH, (float)bounce_edge_high);
	rs->global_shader_parameter_set(SG_BOUNCE_DIST, (float)bounce_distance);
	rs->global_shader_parameter_set(SG_BOUNCE_MUL, (float)bounce_multiplier);
	rs->global_shader_parameter_set(SG_BOUNCE_COLOR, bounce_color);
	rs->global_shader_parameter_set(SG_CORE_LOW, (float)core_shadow_edge_low);
	rs->global_shader_parameter_set(SG_CORE_HIGH, (float)core_shadow_edge_high);
	rs->global_shader_parameter_set(SG_SHADOW_COLOR, shadow_color);
}

void FolioLighting::update() {
	// Spherical direction, optionally swayed by the day cycle.
	double t = theta;
	double p = phi;
	if (use_day_cycles) {
		const double progress_offset = 9.0 / 16.0;
		const double a = -(day_progress + progress_offset) * Math::TAU;
		t = theta + Math::sin(a) * theta_amplitude;
		p = phi + (Math::cos(a) * 0.5) * phi_amplitude;
	}
	direction = FolioViewSpherical::from_spherical(1.0, p, t).normalized();

	// Aim the light from the sun position at the framed ground centre.
	Vector3 target(0, 0, 0);
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_view()) {
		target = game->get_view()->get_optimal_area_position();
	}
	if (light) {
		const Vector3 sun_pos = target + direction * radius;
		light->look_at_from_position(sun_pos, target, Vector3(0, 1, 0));
		light->set_param(Light3D::PARAM_ENERGY, light_intensity);
	}

	_update_globals();
}

void FolioLighting::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioLighting::update);
	ClassDB::bind_method(D_METHOD("_subscribe"), &FolioLighting::_subscribe);
	ClassDB::bind_method(D_METHOD("set_use_day_cycles", "on"), &FolioLighting::set_use_day_cycles);
	ClassDB::bind_method(D_METHOD("set_day_progress", "progress"), &FolioLighting::set_day_progress);
	ClassDB::bind_method(D_METHOD("set_day_light_color", "color"), &FolioLighting::set_day_light_color);
	ClassDB::bind_method(D_METHOD("set_day_light_intensity", "intensity"), &FolioLighting::set_day_light_intensity);
	ClassDB::bind_method(D_METHOD("set_day_shadow_color", "color"), &FolioLighting::set_day_shadow_color);
	ClassDB::bind_method(D_METHOD("get_direction"), &FolioLighting::get_direction);
}

} // namespace godot
