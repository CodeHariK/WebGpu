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

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/world3d.hpp>

namespace godot {

class TerrainSpline2D;
class TerrainSplineScatter;

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

class TerrainChunk : public RefCounted {
	GDCLASS(TerrainChunk, RefCounted)

public:
	enum ChunkState {
		STATE_UNLOADED = 0,
		STATE_GENERATING = 1,
		STATE_VISUAL_ONLY = 2,
		STATE_VISUAL_AND_PHYSICS = 3,
		STATE_MARKED_FOR_EVICTION = 4
	};

	struct PhysicsCache {
		RID shape_rid;
		std::vector<Transform3D> transforms;
	};

private:
	Vector2i chunk_coords;
	ChunkState current_state = STATE_UNLOADED;
	Ref<TerrainHeightmap> heightmap;

	std::vector<uint64_t> visual_nodes;
	RID physics_body_rid;
	std::vector<PhysicsCache> physics_caches;

protected:
	static void _bind_methods();

public:
	TerrainChunk();
	~TerrainChunk();

	Vector2i get_chunk_coords() const { return chunk_coords; }
	void set_chunk_coords(const Vector2i &p_coords) { chunk_coords = p_coords; }

	ChunkState get_state() const { return current_state; }
	void set_state(ChunkState p_state) { current_state = p_state; }

	Ref<TerrainHeightmap> get_heightmap() const { return heightmap; }
	void set_heightmap(const Ref<TerrainHeightmap> &p_heightmap) { heightmap = p_heightmap; }

	std::vector<uint64_t> &get_visual_nodes() { return visual_nodes; }
	const std::vector<uint64_t> &get_visual_nodes() const { return visual_nodes; }

	RID get_physics_body_rid() const { return physics_body_rid; }
	void set_physics_body_rid(const RID &p_rid) { physics_body_rid = p_rid; }

	std::vector<PhysicsCache> &get_physics_caches() { return physics_caches; }
	const std::vector<PhysicsCache> &get_physics_caches() const { return physics_caches; }
};

class ScatterJob : public RefCounted {
	GDCLASS(ScatterJob, RefCounted)

public:
	Ref<TerrainChunk> chunk;
	TerrainSpline2D *spline = nullptr;
	TerrainSplineScatter *scatterer = nullptr;
	Vector2 offset;
	std::vector<Transform3D> transforms;

	// METRICS
	int debug_total_cells = 0;
	int debug_density_skipped = 0;
	int debug_noise_skipped = 0;
	int debug_spline_skipped = 0;
	int debug_slope_skipped = 0;
	uint64_t debug_time_spent_usec = 0;

	ScatterJob();
	~ScatterJob();

protected:
	static void _bind_methods();
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

	struct BakedSegment {
		Vector2 a;
		Vector2 b;
		Vector2 ab;
		float l2;
		float y_start;
		float y_diff;
		float min_x, max_x, min_z, max_z;
	};

private:
	float max_height = 0.0f;
	float spline_width = 2.0f;
	float falloff_distance = 5.0f;
	bool is_closed = true;
	BlendMode blend_mode = BLEND_ADD;
	InterpolationMode interpolation_mode = INTERP_IDW_LINE;
	Ref<Curve> falloff_curve;

	bool use_tile_culling = true;
	int tile_size = 32;
	float bake_interval = 2.0f;

	// DIRTY RECT STATE TRACKING
	struct CurveState {
		Vector3 pos;
		Vector3 in;
		Vector3 out;
	};
	std::vector<CurveState> cached_curve_state;
	Rect2 last_padded_aabb;
	Rect2 accumulated_dirty_rect;
	bool is_dirty = true;
	bool is_full_rebuild = true;

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
	void set_use_tile_culling(bool p_use);
	bool get_use_tile_culling() const;
	void set_tile_size(int p_size);
	int get_tile_size() const;
	void set_bake_interval(float p_interval);
	float get_bake_interval() const;

	PackedVector3Array get_baked_points_3d() const;
	PackedVector2Array get_baked_points_2d() const;
	Rect2 get_padded_aabb() const;
	bool is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const;

	void deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, const Vector2 &p_offset);
	void _deform_heightmap_task(int p_task_idx);

	void mark_dirty();
	void _on_curve_changed();
	void _cache_curve_state();

	bool get_is_dirty() const { return is_dirty; }
	Rect2 consume_dirty_rect();

	void ensure_baked_cache();
	SplineEval _evaluate_spline_point(const Vector2 &p) const;

	bool has_baked_cache = false;
	PackedVector3Array baked_poly3d;
	PackedVector2Array baked_poly2d;
	std::vector<BakedSegment> baked_segments;

private:
	Ref<TerrainHeightmap> thread_heightmap;
	Vector2 thread_offset;
	int thread_min_x = 0;
	int thread_max_x = 0;
	int thread_min_z = 0;
	int thread_max_z = 0;
	float *thread_data_ptr = nullptr;

	PackedVector3Array thread_poly3d;
	PackedVector2Array thread_poly2d;
	std::vector<BakedSegment> thread_segments;
	std::vector<float> thread_baked_curve;
	bool thread_has_curve = false;
	std::vector<Rect2i> thread_active_tiles;
};

class TerrainSplineScatter : public Node3D {
	GDCLASS(TerrainSplineScatter, Node3D)

private:
	Ref<Mesh> mesh;
	Ref<Shape3D> collision_shape;
	float density = 0.1f;
	float spacing = 4.0f;
	float scale_min = 0.8f;
	float scale_max = 1.2f;
	float min_slope = 0.0f;
	float max_slope = 35.0f;
	float min_spline_dist = 2.0f;
	float max_spline_dist = 30.0f;
	uint32_t seed_offset = 0;
	Ref<Noise> biome_noise;
	float biome_noise_threshold = 0.0f;

protected:
	static void _bind_methods();

public:
	TerrainSplineScatter();
	~TerrainSplineScatter();

	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;

	void set_collision_shape(const Ref<Shape3D> &p_shape);
	Ref<Shape3D> get_collision_shape() const;

	void set_density(float p_density);
	float get_density() const;

	void set_spacing(float p_spacing);
	float get_spacing() const;

	void set_scale_min(float p_min);
	float get_scale_min() const;

	void set_scale_max(float p_max);
	float get_scale_max() const;

	void set_min_slope(float p_min_slope);
	float get_min_slope() const;

	void set_max_slope(float p_max_slope);
	float get_max_slope() const;

	void set_min_spline_dist(float p_dist);
	float get_min_spline_dist() const;

	void set_max_spline_dist(float p_dist);
	float get_max_spline_dist() const;

	void set_seed_offset(int p_seed_offset);
	int get_seed_offset() const;

	void set_biome_noise(const Ref<Noise> &p_noise);
	Ref<Noise> get_biome_noise() const;

	void set_biome_noise_threshold(float p_threshold);
	float get_biome_noise_threshold() const;
};

class Terrain3DSplineCompositor : public Node {
	GDCLASS(Terrain3DSplineCompositor, Node)
private:
	Node *terrain = nullptr;
	int chunk_size = 256;
	float default_elevation = 0.0f;
	bool auto_apply = true;
	bool _rebuild_queued = false;
	bool compositor_full_rebuild = true;
	HashMap<Vector2i, Ref<TerrainChunk>> chunk_buffers;

	float max_render_radius = 2048.0f;
	float max_physics_radius = 150.0f;
	uint64_t last_eviction_check_time = 0;
	Vector2 global_world_offset = Vector2(0.0f, 0.0f);
	Node3D *scatter_container = nullptr;

	void _check_and_evict_far_chunks();
	void _check_origin_shift();
	void _generate_chunks(const std::vector<Vector2i> &p_chunks, const std::vector<TerrainSpline2D *> &p_splines, Object *p_target_api);

	void _run_scatter_job(const Ref<ScatterJob> &p_job);
	void _update_chunk_physics(const Ref<TerrainChunk>& p_chunk);
	void _check_chunk_physics_culling();

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
	void set_max_render_radius(float p_radius);
	float get_max_render_radius() const;
	void set_max_physics_radius(float p_radius);
	float get_max_physics_radius() const;
	void set_global_world_offset(Vector2 p_offset);
	Vector2 get_global_world_offset() const;
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
