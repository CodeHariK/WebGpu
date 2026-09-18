#include "terrain.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

static const char *SG_GRADIENT = "folio_terrain_gradient";
static const char *SG_DATA = "folio_terrain_data";
static const char *SG_GRASS = "folio_terrain_grass_color";
static const char *SG_SUBDIV = "folio_terrain_subdivision";
static const char *SG_SIZE = "folio_terrain_size";

FolioTerrain::FolioTerrain() {}

FolioTerrain::~FolioTerrain() {}

void FolioTerrain::_ready() {
	_generate_gradient();
	_generate_default_data();
	_register_globals();
	_push();
}

void FolioTerrain::_generate_gradient() {
	// 1x16 vertical ramp matching folio's stops (sRGB): orange, teal, deep blue.
	const int h = 16;
	Ref<Image> img = Image::create_empty(1, h, false, Image::FORMAT_RGBA8);

	struct Stop {
		double at;
		Color color;
	};
	const Stop stops[] = {
		{ 0.1, Color::html("#ffa94e") },
		{ 0.3, Color::html("#5bc2b9") },
		{ 0.9, Color::html("#13375f") },
	};
	const int n = 3;

	for (int y = 0; y < h; y++) {
		const double t = (double)y / (double)(h - 1);
		Color c;
		if (t <= stops[0].at) {
			c = stops[0].color;
		} else if (t >= stops[n - 1].at) {
			c = stops[n - 1].color;
		} else {
			c = stops[n - 1].color;
			for (int i = 0; i < n - 1; i++) {
				if (t >= stops[i].at && t <= stops[i + 1].at) {
					const double f = (t - stops[i].at) / (stops[i + 1].at - stops[i].at);
					c = stops[i].color.lerp(stops[i + 1].color, f);
					break;
				}
			}
		}
		img->set_pixel(0, y, c);
	}
	gradient = ImageTexture::create_from_image(img);
}

void FolioTerrain::_generate_default_data() {
	// Neutral biome data: all grass (G=1), height 0 (B=0) — bounce ≈ grass color.
	Ref<Image> img = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	img->set_pixel(0, 0, Color(0, 1, 0, 1));
	data_texture = ImageTexture::create_from_image(img);
}

void FolioTerrain::_register_globals() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	// Register once per process (add is runtime-safe; never use the editor-only get).
	static bool s_added = false;
	if (!s_added) {
		rs->global_shader_parameter_add(SG_GRADIENT, RenderingServer::GLOBAL_VAR_TYPE_SAMPLER2D, Variant());
		rs->global_shader_parameter_add(SG_DATA, RenderingServer::GLOBAL_VAR_TYPE_SAMPLER2D, Variant());
		rs->global_shader_parameter_add(SG_GRASS, RenderingServer::GLOBAL_VAR_TYPE_COLOR, Color());
		rs->global_shader_parameter_add(SG_SUBDIV, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 128.0f);
		rs->global_shader_parameter_add(SG_SIZE, RenderingServer::GLOBAL_VAR_TYPE_FLOAT, 192.0f);
		s_added = true;
	}
	globals_registered = true;
}

void FolioTerrain::_push() {
	if (!globals_registered) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs) {
		return;
	}
	if (gradient.is_valid()) {
		rs->global_shader_parameter_set(SG_GRADIENT, gradient);
	}
	if (data_texture.is_valid()) {
		rs->global_shader_parameter_set(SG_DATA, data_texture);
	}
	rs->global_shader_parameter_set(SG_GRASS, grass_color);
	rs->global_shader_parameter_set(SG_SUBDIV, (float)subdivision);
	rs->global_shader_parameter_set(SG_SIZE, (float)size);
}

void FolioTerrain::set_terrain_data(const Ref<Texture2D> &p_texture) {
	if (p_texture.is_valid()) {
		data_texture = p_texture;
		_push();
	}
}

void FolioTerrain::set_grass_color(const Color &p_color) {
	grass_color = p_color;
	_push();
}

void FolioTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain_data", "texture"), &FolioTerrain::set_terrain_data);
	ClassDB::bind_method(D_METHOD("set_grass_color", "color"), &FolioTerrain::set_grass_color);
	ClassDB::bind_method(D_METHOD("get_gradient"), &FolioTerrain::get_gradient);
	ClassDB::bind_method(D_METHOD("get_terrain_data"), &FolioTerrain::get_terrain_data);
	ClassDB::bind_method(D_METHOD("get_grass_color"), &FolioTerrain::get_grass_color);
}

} // namespace godot
