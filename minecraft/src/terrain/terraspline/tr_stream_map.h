/**
 * @file tr_stream_map.h
 * @brief TerrainSplineStreamMap: a live top-down map of what the compositor has streamed in.
 */
#ifndef TR_STREAM_MAP_H
#define TR_STREAM_MAP_H

#include "tr_compositor.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/node_path.hpp>

namespace godot {

/**
 * @class TerrainSplineStreamMap
 * @brief Debug overlay that follows the player and shows, every `refresh_interval` seconds, which
 * chunks are resident (with their real heights as grayscale), which are queued or being generated,
 * which were just evicted, the render/physics radii, spline bounds, and the player + camera headings.
 *
 * Streaming is distance-based: a chunk is generated when its centre comes within `max_render_radius`
 * of the player and evicted when it leaves, regardless of view direction - the map makes that
 * visible. Point `compositor` at the TerrainSplineCompositor; `toggle_key` shows/hides at runtime.
 */
class TerrainSplineStreamMap : public Control {
	GDCLASS(TerrainSplineStreamMap,
			Control)

private:
	NodePath compositor_path;
	float view_radius = 0.0f; // Metres from the player to the map edge; 0 = 1.25 × render radius
	float refresh_interval = 0.1f; // Seconds between snapshots
	int thumbnail_size = 16; // Height samples per chunk edge
	bool show_heights = true;
	bool show_splines = true;
	float evicted_fade_seconds = 4.0f;
	Key toggle_key = KEY_NONE;

	TerrainSplineCompositor *_compositor = nullptr;
	StreamSnapshot _snap;
	bool _has_snapshot = false;
	float _time_since_refresh = 0.0f;
	Ref<ImageTexture> _heights_texture;
	Rect2 _heights_rect; // Logical world rect the texture covers

	TerrainSplineCompositor *_resolve_compositor();
	void _refresh();
	void _rebuild_heights_texture();

	// ---- Drawing helpers (world = logical metres, map = local pixels) ----
	float _scale() const; // Pixels per metre
	Vector2 _to_map(const Vector2 &p_world) const;
	Rect2 _to_map(const Rect2 &p_world) const;
	void _draw_heights();
	void _draw_chunks();
	void _draw_radii();
	void _draw_splines();
	void _draw_player();
	void _draw_legend();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineStreamMap();
	~TerrainSplineStreamMap();

	void _draw() override;
	void _unhandled_key_input(const Ref<InputEvent> &p_event) override;

	void set_compositor(const NodePath &p_path);
	NodePath get_compositor() const { return compositor_path; }
	void set_view_radius(float p_radius) { view_radius = MAX(0.0f, p_radius); }
	float get_view_radius() const { return view_radius; }
	void set_refresh_interval(float p_seconds) { refresh_interval = MAX(0.0f, p_seconds); }
	float get_refresh_interval() const { return refresh_interval; }
	void set_thumbnail_size(int p_size);
	int get_thumbnail_size() const { return thumbnail_size; }
	void set_show_heights(bool p_show) { show_heights = p_show; }
	bool get_show_heights() const { return show_heights; }
	void set_show_splines(bool p_show) { show_splines = p_show; }
	bool get_show_splines() const { return show_splines; }
	void set_evicted_fade_seconds(float p_seconds) { evicted_fade_seconds = MAX(0.0f, p_seconds); }
	float get_evicted_fade_seconds() const { return evicted_fade_seconds; }
	void set_toggle_key(Key p_key) { toggle_key = p_key; }
	Key get_toggle_key() const { return toggle_key; }
};

} // namespace godot

#endif // TR_STREAM_MAP_H
