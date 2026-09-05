/**
 * @file tr_compositor_ui.h
 * @brief TerrainSplineCompositorUI: a TextureRect preview of the combined spline heightmap.
 */
#ifndef TR_COMPOSITOR_UI_H
#define TR_COMPOSITOR_UI_H

#include "tr_heightmap.h"
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
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
 * @brief Debug preview: deforms one unified heightmap over all child splines and shows it as a
 * normalized grayscale texture. Not used by the game; handy while authoring splines.
 */
class TerrainSplineCompositorUI : public TextureRect {
	GDCLASS(TerrainSplineCompositorUI,
			TextureRect)

private:
	float default_elevation = 0.0f;
	bool auto_apply = true;
	bool _rebuild_queued = false;

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

public:
	TerrainSplineCompositorUI();
	~TerrainSplineCompositorUI();

	void set_default_elevation(float p_elev);
	float get_default_elevation() const;
	void set_auto_apply(bool p_auto);
	bool get_auto_apply() const;
	void set_apply_now(bool p_apply);
	bool get_apply_now() const;

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
