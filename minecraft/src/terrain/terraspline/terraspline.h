#ifndef TERRASPLINE_H
#define TERRASPLINE_H

#include <cstdint>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/height_map_shape3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

namespace godot {

class TerrainManager;

class TerrainHeightmap : public RefCounted {
	GDCLASS(TerrainHeightmap, RefCounted)

private:
	int width = 0;
	int height = 0;
	PackedFloat32Array data;
	TerrainManager *manager = nullptr;
	Vector2i chunk_coords;

protected:
	static void _bind_methods();

public:
	TerrainHeightmap();
	~TerrainHeightmap();

	void initialize(int p_width, int p_height, float p_default_value = 0.0f);
	void set_manager(TerrainManager *p_manager, const Vector2i &p_coords);

	int get_width() const { return width; }
	int get_height() const { return height; }

	float get_height_at(int x, int z) const;
	void set_height_at(int x, int z, float val);

	PackedFloat32Array get_data() const { return data; }
	void set_data(const PackedFloat32Array &p_data);

	void import_from_image(const Ref<Image> &p_image);
	Ref<Image> export_to_image() const;

	Ref<ArrayMesh> generate_mesh(int p_lod_level = 0) const;
	Ref<HeightMapShape3D> generate_collision_shape() const;

	float *get_data_ptrw() { return data.ptrw(); }
	const float *get_data_ptr() const { return data.ptr(); }
};

class TerrainSpline2D : public Path3D {
	GDCLASS(TerrainSpline2D, Path3D)

public:
	enum BlendMode : uint8_t {
		BLEND_ADD = 0,
		BLEND_SUBTRACT = 1,
		BLEND_MAX = 2,
		BLEND_MIN = 3,
		BLEND_REPLACE = 4 // Added for strict 3D road/river carving
	};

	enum InterpolationMode : uint8_t {
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
	float max_height = 0.0f; // Now acts as a global offset to the 3D spline
	float spline_width = 2.0f; // The flat inner radius
	float falloff_distance = 5.0f;
	bool is_closed = true;
	BlendMode blend_mode = BLEND_ADD;
	InterpolationMode interpolation_mode = INTERP_IDW_LINE;
	Ref<Curve> falloff_curve;
	bool is_dirty = true;

protected:
	static void _bind_methods();

public:
	TerrainSpline2D();
	~TerrainSpline2D();

	// Phase 1 Getters / Setters
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

	// Phase 1 Math Methods
	PackedVector3Array get_baked_points_3d() const;
	PackedVector2Array get_baked_points_2d() const;
	Rect2 get_padded_aabb() const;

	// Phase 2 Math Methods
	bool is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const;
	float distance_to_spline(const Vector2 &p, const PackedVector2Array &polygon) const;
	float get_target_height(float x, float z, float current_h) const; // Updated signature

	// Phase 3 Methods
	void deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, const Vector2 &p_offset = Vector2(0, 0));

	// Phase 4 Methods & Overrides
	virtual void _process(double delta) override;
	void _deform_heightmap_row(int p_row_idx);

	// Phase 5 Methods
	Ref<Image> generate_splatmap(const Ref<TerrainHeightmap> &p_heightmap) const;
	TypedArray<Transform3D> generate_scatter_transforms(const Ref<TerrainHeightmap> &p_heightmap, int p_count, float p_min_slope = 0.0f, float p_max_slope = 90.0f) const;

private:
	float get_distance_to_segment(const Vector2 &p, const Vector2 &a, const Vector2 &b) const;
	SplineEval _evaluate_spline_point(const Vector2 &p, const PackedVector3Array &poly3d, const PackedVector2Array &poly2d) const;

	// Phase 4 Private Variables (Thread Caches)
	Ref<Curve3D> last_connected_curve;
	Ref<TerrainHeightmap> temp_heightmap;
	int temp_min_x = 0;
	int temp_max_x = 0;
	int temp_min_z = 0;
	int temp_max_z = 0;
	float *temp_data_ptr = nullptr;
	std::vector<float> temp_baked_curve;
	bool temp_has_curve = false;
	Vector2 temp_offset;

	// Memory optimizations for the thread pool
	PackedVector3Array temp_polygon_3d;
	PackedVector2Array temp_polygon_2d;

public:
	// Helper to track changes
	void mark_dirty();
	void _on_curve_changed();
};

class TerrainManager : public Node3D {
	GDCLASS(TerrainManager, Node3D)

private:
	int chunk_size = 64;
	HashMap<Vector2i, Ref<TerrainHeightmap>> chunks;

protected:
	static void _bind_methods();

public:
	TerrainManager();
	~TerrainManager();

	void set_chunk_size(int p_size);
	int get_chunk_size() const;

	Ref<TerrainHeightmap> get_chunk(const Vector2i &p_coords);
	Ref<TerrainHeightmap> create_chunk(const Vector2i &p_coords);
	void delete_chunk(const Vector2i &p_coords);
	void clear_chunks();
	TypedArray<Vector2i> get_active_chunks() const;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSpline2D::BlendMode);
VARIANT_ENUM_CAST(godot::TerrainSpline2D::InterpolationMode);

#endif // TERRASPLINE_H
