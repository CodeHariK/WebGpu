/**
 * @file tr_compositor_ui.h
 * @brief TerrainSplineCompositorUI: a TextureRect preview of the combined spline heightmap.
 */
#ifndef TR_COMPOSITOR_UI_H
#define TR_COMPOSITOR_UI_H

#include "tr_heightmap.h"
#include <cstdint>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;

/**
 * @class GrayscaleJob
 * @brief Slice descriptor for normalizing a float heightmap into 8-bit grayscale in parallel.
 */
class GrayscaleJob : public RefCounted {
	GDCLASS(GrayscaleJob,
			RefCounted)

public:
	const float *ptr = nullptr;
	uint8_t *byte_ptr = nullptr;
	float min_h = 0.0f;
	float range = 1.0f;
	int size = 0;
	int chunk_size = 0; // Elements per task

	GrayscaleJob() {}
	~GrayscaleJob() {}

protected:
	static void _bind_methods() {}
};

/**
 * @class TerrainSplineCompositorUI
 * @brief Debug preview: deforms one unified heightmap over a set of splines and shows it as a
 * normalized grayscale texture. Handy while authoring splines - it runs in the editor as well.
 *
 * The splines are the ProceduralSpline3D children of `splines_root` (default: this node), so a
 * TerrainSplineCompositorDemo scene can point it at its TerrainSplineCompositor and preview exactly
 * the splines the terrain uses. Edits to any of them (spline_changed) refresh the preview when
 * auto_apply is on. `toggle_key` shows/hides the preview at runtime (KEY_NONE disables it).
 */
class TerrainSplineCompositorUI : public TextureRect {
	GDCLASS(TerrainSplineCompositorUI,
			TextureRect)

private:
	float default_elevation = 0.0f;
	bool auto_apply = true;
	NodePath splines_root; // Empty = this node
	Key toggle_key = KEY_NONE;
	bool _rebuild_queued = false;
	Node *_watched_root = nullptr; // Node whose children we are connected to

	Node *_resolve_splines_root() const;
	void _watch_root(Node *p_root);
	void _unwatch_root();
	std::vector<ProceduralSpline3D *> _gather_splines(Rect2 &r_bounds) const;
	Ref<TerrainHeightmap> _deform_unified_heightmap(
			const std::vector<ProceduralSpline3D *> &p_splines,
			const Rect2 &p_bounds,
			int &r_w,
			int &r_h
	);
	void _show_as_grayscale(
			const Ref<TerrainHeightmap> &p_buffer,
			int p_w,
			int p_h
	);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	/// The preview texture is derived data: show it, never save it with the scene.
	void _validate_property(PropertyInfo &p_property) const;

public:
	TerrainSplineCompositorUI();
	~TerrainSplineCompositorUI();

	void set_default_elevation(float p_elev);
	float get_default_elevation() const;
	void set_auto_apply(bool p_auto);
	bool get_auto_apply() const;
	void set_apply_now(bool p_apply);
	bool get_apply_now() const;
	void set_splines_root(const NodePath &p_path);
	NodePath get_splines_root() const;
	void set_toggle_key(Key p_key);
	Key get_toggle_key() const;

	void _unhandled_key_input(const Ref<InputEvent> &p_event) override;

	void queue_rebuild();
	void _execute_rebuild();
	void apply_all_splines();
	void _connect_spline(Node *p_node);
	void _disconnect_spline(Node *p_node);
	void _on_spline_changed();
	void _normalize_grayscale_task(
			int p_task_idx,
			Ref<GrayscaleJob> p_job
	);
};

} // namespace godot

#endif // TR_COMPOSITOR_UI_H
