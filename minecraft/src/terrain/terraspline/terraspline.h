#ifndef TERRASPLINE_NEW_H
#define TERRASPLINE_NEW_H

#include "godot_cpp/classes/texture_rect.hpp"
#include <cstdint>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

namespace godot {

class TerrainHeightmap : public RefCounted {
	GDCLASS(TerrainHeightmap, RefCounted)

private:
	int width = 0;
	int height = 0;
	PackedFloat32Array data;

protected:
	static void _bind_methods();

public:
	TerrainHeightmap();
	~TerrainHeightmap();

	void initialize(int p_width, int p_height, float p_default_value = 0.0f);
	void clear(float p_default_value = 0.0f);

	int get_width() const { return width; }
	int get_height() const { return height; }

	float *get_data_ptrw() { return data.ptrw(); }
	Ref<Image> get_image() const;
};

class TerrainSpline2D : public Path3D {
	GDCLASS(TerrainSpline2D, Path3D)

public:
	enum BlendMode {
		BLEND_ADD = 0,
		BLEND_SUBTRACT = 1,
		BLEND_MAX = 2,
		BLEND_MIN = 3,
		BLEND_REPLACE = 4
	};

	enum InterpolationMode {
		INTERP_NEAREST = 0,
		INTERP_IDW_LINE = 1,
		INTERP_IDW_VERTEX = 2
	};

	struct SplineEval {
		float distance;
		float spline_y;
		bool is_inside;
	};

private:
	float max_height = 0.0f;
	float spline_width = 2.0f;
	float falloff_distance = 5.0f;
	bool is_closed = true;
	BlendMode blend_mode = BLEND_ADD;
	InterpolationMode interpolation_mode = INTERP_IDW_LINE;
	Ref<Curve> falloff_curve;

	// NEW: TILE CULLING PARAMETERS
	bool use_tile_culling = true;
	int tile_size = 32;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSpline2D();
	~TerrainSpline2D();

	void set_max_height(float p_height);
	float get_max_height() const;
	void set_spline_width(float p_width);
	float get_spline_width() const;
	void set_falloff_distance(float p_dist);
	float get_falloff_distance() const;
	void set_is_closed(bool p_closed);
	bool get_is_closed() const;
	void set_blend_mode(BlendMode p_mode);
	BlendMode get_blend_mode() const;
	void set_interpolation_mode(InterpolationMode p_mode);
	InterpolationMode get_interpolation_mode() const;
	void set_falloff_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_falloff_curve() const;

	// NEW: GETTERS AND SETTERS
	void set_use_tile_culling(bool p_use);
	bool get_use_tile_culling() const;
	void set_tile_size(int p_size);
	int get_tile_size() const;

	PackedVector3Array get_baked_points_3d() const;
	PackedVector2Array get_baked_points_2d() const;
	Rect2 get_padded_aabb() const;
	bool is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const;

	void deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, const Vector2 &p_offset);

	// CHANGED: We now thread by "Tasks" (Tiles or Rows) instead of strictly rows
	void _deform_heightmap_task(int p_task_idx);

	void mark_dirty();
	void _on_curve_changed();

private:
	SplineEval _evaluate_spline_point(const Vector2 &p, const PackedVector3Array &poly3d, const PackedVector2Array &poly2d) const;

	Ref<TerrainHeightmap> thread_heightmap;
	Vector2 thread_offset;
	int thread_min_x = 0;
	int thread_max_x = 0;
	int thread_min_z = 0;
	int thread_max_z = 0;
	float *thread_data_ptr = nullptr;
	PackedVector3Array thread_poly3d;
	PackedVector2Array thread_poly2d;
	std::vector<float> thread_baked_curve;
	bool thread_has_curve = false;

	// NEW: THREAD TASK CACHE
	std::vector<Rect2i> thread_active_tiles;
};

class Terrain3DSplineCompositor : public Node {
	GDCLASS(Terrain3DSplineCompositor, Node)

private:
	Node *terrain = nullptr;
	int chunk_size = 256;
	float default_elevation = 0.0f;
	bool auto_apply = true;
	bool _rebuild_queued = false;
	HashMap<Vector2i, Ref<TerrainHeightmap>> chunk_buffers;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Terrain3DSplineCompositor();
	~Terrain3DSplineCompositor();
	void set_terrain(Node *p_terrain);
	Node *get_terrain() const;
	void set_chunk_size(int p_size);
	int get_chunk_size() const;
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
	void _on_spline_changed();
};

class TerrainSplineCompositorUI : public TextureRect {
	GDCLASS(TerrainSplineCompositorUI, TextureRect)

private:
	float default_elevation = 0.0f;
	bool auto_apply = true;
	bool _rebuild_queued = false;

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
	void _on_spline_changed();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSpline2D::BlendMode);
VARIANT_ENUM_CAST(godot::TerrainSpline2D::InterpolationMode);

#endif // TERRASPLINE_NEW_H
