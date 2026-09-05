/**
 * @file tr_stream_map.cpp
 * @brief TerrainSplineStreamMap: snapshot the compositor on a timer, draw it top-down around the player.
 */
#include "tr_stream_map.h"
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Palette
static const Color
		COL_BG(0.05f,
			   0.05f,
			   0.07f,
			   0.85f);
static const Color COL_GRID(
		1.0f,
		1.0f,
		1.0f,
		0.08f
);
static const Color COL_VISUAL(
		0.35f,
		0.9f,
		0.35f,
		0.9f
); // Resident, visuals only
static const Color COL_PHYSICS(
		0.2f,
		0.9f,
		1.0f,
		1.0f
); // Resident, physics live
static const Color COL_GENERATING(
		1.0f,
		0.6f,
		0.15f,
		1.0f
); // Job in flight
static const Color COL_QUEUED(
		1.0f,
		0.9f,
		0.2f,
		0.9f
); // Waiting in the generation queue
static const Color COL_EVICTED(
		1.0f,
		0.25f,
		0.25f,
		1.0f
); // Recently dropped
static const Color COL_RENDER_R(
		0.35f,
		0.9f,
		0.35f,
		0.6f
);
static const Color COL_PHYSICS_R(
		0.2f,
		0.9f,
		1.0f,
		0.6f
);
static const Color COL_SPLINE(
		1.0f,
		0.4f,
		1.0f,
		0.8f
);
static const Color COL_PLAYER(
		1.0f,
		1.0f,
		1.0f,
		1.0f
);
static const Color COL_CAMERA(
		1.0f,
		0.85f,
		0.3f,
		1.0f
);
static const Color COL_TEXT(
		1.0f,
		1.0f,
		1.0f,
		0.9f
);

void TerrainSplineStreamMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_compositor", "path"), &TerrainSplineStreamMap::set_compositor);
	ClassDB::bind_method(D_METHOD("get_compositor"), &TerrainSplineStreamMap::get_compositor);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::NODE_PATH, "compositor", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "TerrainSplineCompositor"
			),
			"set_compositor", "get_compositor"
	);
	ClassDB::bind_method(D_METHOD("set_view_radius", "metres"), &TerrainSplineStreamMap::set_view_radius);
	ClassDB::bind_method(D_METHOD("get_view_radius"), &TerrainSplineStreamMap::get_view_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "view_radius"), "set_view_radius", "get_view_radius");
	ClassDB::bind_method(D_METHOD("set_refresh_interval", "seconds"), &TerrainSplineStreamMap::set_refresh_interval);
	ClassDB::bind_method(D_METHOD("get_refresh_interval"), &TerrainSplineStreamMap::get_refresh_interval);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "refresh_interval"), "set_refresh_interval", "get_refresh_interval");
	ClassDB::bind_method(D_METHOD("set_thumbnail_size", "size"), &TerrainSplineStreamMap::set_thumbnail_size);
	ClassDB::bind_method(D_METHOD("get_thumbnail_size"), &TerrainSplineStreamMap::get_thumbnail_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "thumbnail_size"), "set_thumbnail_size", "get_thumbnail_size");
	ClassDB::bind_method(D_METHOD("set_show_heights", "show"), &TerrainSplineStreamMap::set_show_heights);
	ClassDB::bind_method(D_METHOD("get_show_heights"), &TerrainSplineStreamMap::get_show_heights);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_heights"), "set_show_heights", "get_show_heights");
	ClassDB::bind_method(D_METHOD("set_show_splines", "show"), &TerrainSplineStreamMap::set_show_splines);
	ClassDB::bind_method(D_METHOD("get_show_splines"), &TerrainSplineStreamMap::get_show_splines);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_splines"), "set_show_splines", "get_show_splines");
	ClassDB::bind_method(
			D_METHOD("set_evicted_fade_seconds", "seconds"), &TerrainSplineStreamMap::set_evicted_fade_seconds
	);
	ClassDB::bind_method(D_METHOD("get_evicted_fade_seconds"), &TerrainSplineStreamMap::get_evicted_fade_seconds);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "evicted_fade_seconds"), "set_evicted_fade_seconds", "get_evicted_fade_seconds"
	);
	ClassDB::bind_method(D_METHOD("set_toggle_key", "key"), &TerrainSplineStreamMap::set_toggle_key);
	ClassDB::bind_method(D_METHOD("get_toggle_key"), &TerrainSplineStreamMap::get_toggle_key);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "toggle_key"), "set_toggle_key", "get_toggle_key");
}

TerrainSplineStreamMap::TerrainSplineStreamMap() {
	set_clip_contents(true); // World-space drawing extends past the control; keep it inside.
}
TerrainSplineStreamMap::~TerrainSplineStreamMap() {}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void TerrainSplineStreamMap::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_process(true);
		set_process_unhandled_key_input(toggle_key != KEY_NONE);
		_resolve_compositor();
		_refresh();
	} else if (p_what == NOTIFICATION_PROCESS) {
		if (!is_visible_in_tree()) {
			return;
		}
		_time_since_refresh += (float)get_process_delta_time();
		if (_time_since_refresh >= refresh_interval) {
			_time_since_refresh = 0.0f;
			_refresh();
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (_compositor) {
			_compositor->set_thumbnail_size(0);
			_compositor = nullptr;
		}
	}
}

void TerrainSplineStreamMap::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == toggle_key) {
		set_visible(!is_visible());
		if (is_visible()) {
			_refresh();
		}
	}
}

void TerrainSplineStreamMap::set_compositor(const NodePath &p_path) {
	compositor_path = p_path;
	if (is_inside_tree()) {
		_compositor = nullptr;
		_resolve_compositor();
		_refresh();
	}
}

void TerrainSplineStreamMap::set_thumbnail_size(int p_size) {
	thumbnail_size = MAX(0, p_size);
	if (_compositor) {
		_compositor->set_thumbnail_size(show_heights ? thumbnail_size : 0);
	}
}

/// Finds the compositor and asks it to start building height thumbnails.
TerrainSplineCompositor *TerrainSplineStreamMap::_resolve_compositor() {
	if (_compositor) {
		return _compositor;
	}
	if (compositor_path.is_empty()) {
		return nullptr;
	}
	_compositor = Object::cast_to<TerrainSplineCompositor>(get_node_or_null(compositor_path));
	if (!_compositor) {
		UtilityFunctions::push_warning(
				"[StreamMap] compositor '", compositor_path, "' is not a TerrainSplineCompositor."
		);
		return nullptr;
	}
	_compositor->set_thumbnail_size(show_heights ? thumbnail_size : 0);
	return _compositor;
}

/// Takes a fresh snapshot, rebuilds the height texture and schedules a redraw.
void TerrainSplineStreamMap::_refresh() {
	TerrainSplineCompositor *c = _resolve_compositor();
	if (!c) {
		_has_snapshot = false;
		queue_redraw();
		return;
	}
	c->get_stream_snapshot(_snap);
	_has_snapshot = true;
	if (show_heights) {
		_rebuild_heights_texture();
	} else {
		_heights_texture.unref();
	}
	queue_redraw();
}

/**
 * @brief Packs every resident chunk's thumbnail into one L8 image covering their union, normalized
 * to the min/max of all resident heights so neighbouring chunks shade consistently.
 */
void TerrainSplineStreamMap::_rebuild_heights_texture() {
	_heights_texture.unref();
	const int ts = _snap.thumbnail_size;
	if (ts <= 0 || _snap.resident.empty()) {
		return;
	}
	bool any = false;
	Rect2 bounds;
	float min_h = 1e20f, max_h = -1e20f;
	for (const StreamSnapshot::Chunk &c : _snap.resident) {
		if (!c.thumbnail) {
			continue;
		}
		bounds = any ? bounds.merge(c.rect) : c.rect;
		any = true;
		for (float h : *c.thumbnail) {
			min_h = Math::min(min_h, h);
			max_h = Math::max(max_h, h);
		}
	}
	if (!any) {
		return;
	}
	const float inv_range = 1.0f / Math::max(max_h - min_h, 0.001f);
	const int cols = (int)Math::round(bounds.size.x / _snap.chunk_size);
	const int rows = (int)Math::round(bounds.size.y / _snap.chunk_size);
	const int w = cols * ts, h = rows * ts;
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
		return;
	}
	PackedByteArray px;
	px.resize((int64_t)w * h);
	uint8_t *dst = px.ptrw();
	memset(dst, 0, (size_t)w * h);
	for (const StreamSnapshot::Chunk &c : _snap.resident) {
		if (!c.thumbnail) {
			continue;
		}
		const int cx = (int)Math::round((c.rect.position.x - bounds.position.x) / _snap.chunk_size);
		const int cz = (int)Math::round((c.rect.position.y - bounds.position.y) / _snap.chunk_size);
		for (int tz = 0; tz < ts; ++tz) {
			for (int tx = 0; tx < ts; ++tx) {
				const float n = ((*c.thumbnail)[(size_t)tz * ts + tx] - min_h) * inv_range;
				dst[(cz * ts + tz) * w + (cx * ts + tx)] = (uint8_t)(Math::clamp(n, 0.0f, 1.0f) * 255.0f);
			}
		}
	}
	Ref<Image> img = Image::create_from_data(w, h, false, Image::FORMAT_L8, px);
	_heights_texture = ImageTexture::create_from_image(img);
	_heights_rect = bounds;
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

float TerrainSplineStreamMap::_scale() const {
	float radius = view_radius > 0.0f ? view_radius : _snap.render_radius * 1.25f;
	radius = Math::max(radius, 1.0f);
	return Math::min(get_size().x, get_size().y) * 0.5f / radius;
}

Vector2 TerrainSplineStreamMap::_to_map(const Vector2 &p_world) const {
	return get_size() * 0.5f + (p_world - _snap.player) * _scale();
}

Rect2 TerrainSplineStreamMap::_to_map(const Rect2 &p_world) const {
	return Rect2(_to_map(p_world.position), p_world.size * _scale());
}

void TerrainSplineStreamMap::_draw() {
	draw_rect(Rect2(Vector2(), get_size()), COL_BG, true);
	if (!_has_snapshot) {
		Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
		draw_string(font, Vector2(8, 20), "StreamMap: no compositor", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, COL_TEXT);
		return;
	}
	if (show_heights) {
		_draw_heights();
	}
	_draw_chunks();
	if (show_splines) {
		_draw_splines();
	}
	_draw_radii();
	_draw_player();
	_draw_legend();
}

void TerrainSplineStreamMap::_draw_heights() {
	if (_heights_texture.is_valid()) {
		draw_texture_rect(_heights_texture, _to_map(_heights_rect), false, Color(1, 1, 1, 0.9f));
	}
}

/// Resident chunks as outlines (physics chunks tinted), queued/in-flight as dashed-looking outlines,
/// evictions as fading red fills.
void TerrainSplineStreamMap::_draw_chunks() {
	const float now = Time::get_singleton()->get_ticks_msec() / 1000.0f;
	for (const StreamSnapshot::Evicted &e : _snap.evicted) {
		float age = now - e.time_msec / 1000.0f;
		if (evicted_fade_seconds <= 0.0f || age > evicted_fade_seconds) {
			continue;
		}
		float a = 1.0f - age / evicted_fade_seconds;
		Color fill = COL_EVICTED;
		fill.a = 0.35f * a;
		draw_rect(_to_map(e.rect).grow(-1), fill, true);
		Color line = COL_EVICTED;
		line.a = a;
		draw_rect(_to_map(e.rect).grow(-1), line, false, 1.0f);
	}
	for (const Rect2 &r : _snap.queued) {
		draw_rect(_to_map(r).grow(-2), COL_QUEUED, false, 1.0f);
	}
	for (const Rect2 &r : _snap.in_flight) {
		Color fill = COL_GENERATING;
		fill.a = 0.25f;
		draw_rect(_to_map(r).grow(-2), fill, true);
		draw_rect(_to_map(r).grow(-2), COL_GENERATING, false, 2.0f);
	}
	for (const StreamSnapshot::Chunk &c : _snap.resident) {
		Rect2 mr = _to_map(c.rect).grow(-1);
		if (c.state == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
			Color fill = COL_PHYSICS;
			fill.a = 0.18f;
			draw_rect(mr, fill, true);
			draw_rect(mr, COL_PHYSICS, false, 2.0f);
		} else if (c.state == TerrainChunk::STATE_GENERATING) {
			draw_rect(mr, COL_GENERATING, false, 1.0f);
		} else {
			draw_rect(mr, COL_VISUAL, false, 1.0f);
		}
	}
	// Chunk grid over the visible area, for orientation where nothing is resident.
	const float cs = (float)_snap.chunk_size;
	if (cs > 0.0f && cs * _scale() > 6.0f) {
		const float half = Math::max(get_size().x, get_size().y) * 0.5f / _scale();
		const int c0 = (int)Math::floor((_snap.player.x - half) / cs),
				  c1 = (int)Math::ceil((_snap.player.x + half) / cs);
		const int r0 = (int)Math::floor((_snap.player.y - half) / cs),
				  r1 = (int)Math::ceil((_snap.player.y + half) / cs);
		for (int c = c0; c <= c1; ++c) {
			float x = _to_map(Vector2(c * cs, 0)).x;
			draw_line(Vector2(x, 0), Vector2(x, get_size().y), COL_GRID, 1.0f);
		}
		for (int r = r0; r <= r1; ++r) {
			float y = _to_map(Vector2(0, r * cs)).y;
			draw_line(Vector2(0, y), Vector2(get_size().x, y), COL_GRID, 1.0f);
		}
	}
}

void TerrainSplineStreamMap::_draw_radii() {
	Vector2 centre = get_size() * 0.5f;
	draw_arc(centre, _snap.render_radius * _scale(), 0.0f, Math::TAU, 96, COL_RENDER_R, 1.5f, true);
	draw_arc(centre, _snap.physics_radius * _scale(), 0.0f, Math::TAU, 64, COL_PHYSICS_R, 1.5f, true);
}

void TerrainSplineStreamMap::_draw_splines() {
	for (const Rect2 &r : _snap.spline_bounds) {
		draw_rect(_to_map(r), COL_SPLINE, false, 1.0f);
	}
}

/// Player dot with heading, plus the camera's view direction so "looking" vs "loading" is visible.
void TerrainSplineStreamMap::_draw_player() {
	Vector2 centre = get_size() * 0.5f;
	if (_snap.camera_forward != Vector2()) {
		const float len = Math::max(14.0f, _snap.physics_radius * _scale());
		Vector2 f = _snap.camera_forward;
		Vector2 l = f.rotated(-0.5f), r = f.rotated(0.5f);
		Color wedge = COL_CAMERA;
		wedge.a = 0.18f;
		PackedVector2Array tri;
		tri.push_back(centre);
		tri.push_back(centre + l * len);
		tri.push_back(centre + r * len);
		PackedColorArray cols;
		cols.push_back(wedge);
		draw_polygon(tri, cols);
		draw_line(centre, centre + f * len, COL_CAMERA, 1.5f, true);
	}
	if (_snap.player_forward != Vector2()) {
		draw_line(centre, centre + _snap.player_forward * 12.0f, COL_PLAYER, 2.0f, true);
	}
	draw_circle(centre, 3.5f, COL_PLAYER);
}

void TerrainSplineStreamMap::_draw_legend() {
	Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
	int physics = 0;
	for (const StreamSnapshot::Chunk &c : _snap.resident) {
		physics += c.state == TerrainChunk::STATE_VISUAL_AND_PHYSICS ? 1 : 0;
	}
	String line1 =
			vformat("resident %d (physics %d)  queued %d  generating %d", (int)_snap.resident.size(), physics,
					(int)_snap.queued.size(), (int)_snap.in_flight.size());
	String line2 =
			vformat("player %.0f, %.0f   chunk %d m   render r %.0f   physics r %.0f", _snap.player.x, _snap.player.y,
					_snap.chunk_size, _snap.render_radius, _snap.physics_radius);
	draw_string(font, Vector2(6, 14), line1, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, COL_TEXT);
	draw_string(font, Vector2(6, 28), line2, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, COL_TEXT);

	struct Item {
		Color col;
		const char *label;
	};
	const Item items[] = {
		{ COL_PHYSICS, "physics" }, { COL_VISUAL, "visual" },	{ COL_GENERATING, "generating" },
		{ COL_QUEUED, "queued" },	{ COL_EVICTED, "evicted" }, { COL_SPLINE, "spline" },
		{ COL_CAMERA, "camera" },
	};
	float x = 6.0f;
	const float y = get_size().y - 8.0f;
	for (const Item &it : items) {
		draw_rect(Rect2(x, y - 8, 8, 8), it.col, true);
		draw_string(font, Vector2(x + 11, y), it.label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, COL_TEXT);
		x += 11.0f + font->get_string_size(it.label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10).x + 10.0f;
	}
}

} // namespace godot
