#ifndef PROCEDURAL_SPLINE_3D
#define PROCEDURAL_SPLINE_3D

#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot {

class ProceduralSpline3D;

class SplineComponent : public Node3D {
	GDCLASS(SplineComponent, Node3D)

protected:
	static void _bind_methods() {}

public:
	// Virtual function that children will override
	virtual float get_spline_padding() const { return 0.0f; }
};

class ProceduralSpline3D : public Path3D {
	GDCLASS(ProceduralSpline3D, Path3D)

public:
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
	bool is_closed = true;
	InterpolationMode interpolation_mode = INTERP_IDW_LINE;
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

	float cached_max_padding = 0.0f;
	void _update_max_padding();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	ProceduralSpline3D();
	~ProceduralSpline3D();

	void set_curve(const Ref<Curve3D> &p_curve);
	void set_is_closed(bool p_closed);
	bool get_is_closed() const;
	void set_interpolation_mode(InterpolationMode p_mode);
	InterpolationMode get_interpolation_mode() const;
	void set_bake_interval(float p_interval);
	float get_bake_interval() const;

	PackedVector3Array get_baked_points_3d() const;
	PackedVector2Array get_baked_points_2d() const;
	Rect2 get_padded_aabb() const;
	bool is_point_inside(const Vector2 &p, const PackedVector2Array &polygon) const;

	void mark_dirty();
	void _on_curve_changed();
	void _cache_curve_state();

	bool get_is_dirty() const { return is_dirty; }
	Rect2 consume_dirty_rect();

	void ensure_baked_cache();
	SplineEval evaluate_spline_point_segmented(const Vector2 &p, const std::vector<int> &p_segment_indices) const;

	bool has_baked_cache = false;
	PackedVector3Array baked_poly3d;
	PackedVector2Array baked_poly2d;
	std::vector<BakedSegment> baked_segments;
};

} // namespace godot

VARIANT_ENUM_CAST(godot::ProceduralSpline3D::InterpolationMode);

#endif // PROCEDURAL_SPLINE_3D
