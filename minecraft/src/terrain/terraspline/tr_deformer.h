/**
 * @file tr_deformer.h
 * @brief TerrainSplineDeformer: raises or lowers a chunk heightmap along a parent spline.
 */
#ifndef TR_DEFORMER_H
#define TR_DEFORMER_H

#include "tr_deformer_job.h"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

/**
 * @class TerrainSplineDeformer
 * @brief SplineComponent that deforms terrain heights within a corridor around its parent spline.
 *
 * Every pixel within `spline_width` of the spline receives the target height at full weight; from
 * there the weight ramps to zero over `falloff_distance` (outside a closed loop) or
 * `inner_falloff_distance` (inside one, unless `fill_interior`). Weight and height are combined
 * with the current terrain according to `blend_mode`.
 *
 * Implementation is split across:
 *  - tr_deformer.cpp        bindings, entry points, task dispatch
 *  - tr_deformer_field.cpp  distance-field pass (default; O(pixels) per chunk, exact)
 *  - tr_deformer_legacy.cpp job creation and the tile-culling fallback
 *  - tr_deformer_pixel.cpp  per-pixel weight, spline height and blend
 */
class TerrainSplineDeformer : public SplineComponent {
	GDCLASS(TerrainSplineDeformer,
			SplineComponent)

public:
	/*
	 * The target height is `spline_y + max_height`, where spline_y is the spline's world-space Y at
	 * the nearest point. ADD/SUBTRACT apply the target's offset from the chunk's base elevation
	 * (TerrainHeightmap::get_base_elevation, i.e. the compositor's default_elevation) scaled by the
	 * falloff weight, so on flat ground the surface meets the spline exactly and base noise is kept:
	 * a spline at y=25 over 40 m ground gives 25 m at full weight, 30 m with max_height=5.
	 * MAX/MIN/REPLACE move the terrain toward the absolute target.
	 */
	enum BlendMode { BLEND_ADD = 0, BLEND_SUBTRACT = 1, BLEND_MAX = 2, BLEND_MIN = 3, BLEND_REPLACE = 4 };

	/*
	 * Where the spline's height profile comes from.
	 *  SPLINE  - the control points' Y (mountains, carving: the terrain follows the spline).
	 *  TERRAIN - the undeformed terrain sampled along the spline, smoothed over `profile_smoothing`
	 *            metres and limited to `max_grade`; the spline's Y is ignored. With BLEND_REPLACE this
	 *            is a road: flat across its width, gently following the ground along its length, with
	 *            cut-and-fill shoulders over falloff_distance. max_height is the offset above the profile.
	 *            The profile is derived from the base noise, never from a chunk buffer, so it is identical
	 *            in every chunk and in the editor preview.
	 *            Blend modes in TERRAIN mode: ADD and REPLACE both blend to profile + max_height (ADD would
	 *            otherwise be a no-op, since the target is the ground itself); SUBTRACT blends to
	 *            profile - max_height (sunken road); MAX = fill only (embankments, never cuts);
	 *            MIN = cut only (trenches, never fills).
	 */
	enum HeightSource { HEIGHT_SPLINE = 0, HEIGHT_TERRAIN = 1 };

	/*
	 * TERRAIN only: which earthwork the profile may do relative to the ground it follows.
	 *  CUT_AND_FILL - smoothed, grade-limited profile; lower than the ground where it cuts through a
	 *                 rise, higher where it bridges a dip or climbs on an embankment (default).
	 *  CUT_ONLY     - the profile never rises above the ground: bumps are shaved, dips are followed, a
	 *                 climb that exceeds max_grade is cut INTO the slope as a ramped trench. No embankments.
	 *  FILL_ONLY    - the mirror: never below the ground; dips are bridged, rises are climbed as they are.
	 */
	enum Earthwork { EARTHWORK_CUT_AND_FILL = 0, EARTHWORK_CUT_ONLY = 1, EARTHWORK_FILL_ONLY = 2 };

	/**
	 * @brief Combines a target height into a current height according to a blend mode.
	 * `p_target_h` is the absolute height the spline asks for (spline Y + offset). ADD/SUBTRACT apply
	 * its offset from the base elevation so on flat ground the surface meets the spline exactly while
	 * noise underneath is preserved; MAX/MIN/REPLACE move toward the absolute target.
	 */
	static inline float blend_height(
			float p_current_h,
			int p_blend_mode,
			float p_weight,
			float p_target_h,
			float p_base_h
	) {
		const float lerped = p_current_h + p_weight * (p_target_h - p_current_h);
		switch (p_blend_mode) {
			case BLEND_ADD:
				return p_current_h + (p_target_h - p_base_h) * p_weight;
			case BLEND_SUBTRACT:
				return p_current_h - (p_target_h - p_base_h) * p_weight;
			case BLEND_MAX:
				return Math::max(p_current_h, lerped);
			case BLEND_MIN:
				return Math::min(p_current_h, lerped);
			case BLEND_REPLACE:
			default:
				return lerped;
		}
	}

private:
	// ---- Shape ----
	float max_height = 0.0f; // Added to the spline's Y to form the target height (ridge above the spline)
	float spline_width = 2.0f; // Half-width of the full-weight core
	float falloff_distance = 5.0f; // Ramp to zero outside the core (and outside a closed loop)
	float inner_falloff_distance = 5.0f; // Ramp to zero inside a closed loop when !fill_interior
	BlendMode blend_mode = BLEND_ADD;
	HeightSource height_source = HEIGHT_SPLINE;
	float profile_smoothing = 40.0f; // TERRAIN: box-filter window along the spline, metres
	float max_grade = 0.0f; // TERRAIN: max rise per metre in percent (0 = unlimited)
	bool profile_include_splines = true; // TERRAIN: sample the ground after the other splines' deformers
	Earthwork earthwork = EARTHWORK_CUT_AND_FILL; // TERRAIN: may the profile fill, cut, or both
	float max_cut_depth = 0.0f; // TERRAIN: deepest the road may sit below the ground, metres (0 = unlimited)
	float max_fill_height = 0.0f; // TERRAIN: highest the road may sit above the ground, metres (0 = unlimited)
	float road_blur = 0.0f; // TERRAIN: final blur of the finished profile, metres (0 = off); softens the caps
	Ref<Curve> falloff_curve; // Optional remap of the outer ramp (x = 1 at the core, 0 at the edge)
	Ref<Curve> inner_falloff_curve; // Optional remap of the inner ramp
	bool fill_interior = true; // Closed loops: fill the interior at full weight

	// ---- Evaluation strategy ----
	bool use_distance_field = true; // Off = legacy per-pixel spline evaluation (for A/B only)
	bool use_tile_culling = true; // Legacy path: cull segments per tile
	int tile_size = 32; // Pixel tile size for parallel work

	// ---- Job preparation (tr_deformer_legacy.cpp) ----
	void _fill_job_shape(
			Ref<DeformerJob> &p_job,
			ProceduralSpline3D *p_spline
	) const;
	Ref<DeformerJob> _create_deformer_job(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset
	);
	/// Blend mode actually applied: TERRAIN remaps ADD/SUBTRACT to REPLACE (see HeightSource).
	int _effective_blend_mode() const {
		if (height_source == HEIGHT_TERRAIN && (blend_mode == BLEND_ADD || blend_mode == BLEND_SUBTRACT)) {
			return BLEND_REPLACE;
		}
		return blend_mode;
	}
	/// Offset added to spline_y: max_height, negated for a TERRAIN + SUBTRACT (sunken) road.
	float _effective_height_offset() const {
		return (height_source == HEIGHT_TERRAIN && blend_mode == BLEND_SUBTRACT) ? -max_height : max_height;
	}

	// ---- Terrain-following profile (tr_deformer_profile.cpp) ----
	void _bake_terrain_profile(
			const Ref<DeformerJob> &p_job,
			const Ref<TerrainHeightmap> &p_heightmap
	) const;
	float _sample_ground(
			const Ref<TerrainHeightmap> &p_heightmap,
			const std::vector<Ref<DeformerJob>> &p_context,
			float p_x,
			float p_z
	) const;
	void _compute_active_tiles_and_culling(
			Ref<DeformerJob> p_job,
			const Rect2 &p_aabb,
			int p_w,
			int p_h
	);
	void _print_deformer_debug_info(
			ProceduralSpline3D *p_spline,
			const Rect2 &p_aabb
	);

	// ---- Distance field (tr_deformer_field.cpp) ----
	/// Builds job->field_* and job->active_tiles. Returns false for degenerate splines.
	bool _compute_distance_field(
			Ref<DeformerJob> p_job,
			const Rect2 &p_aabb,
			int p_w,
			int p_h
	);
	void _field_allocate(
			Ref<DeformerJob> p_job,
			int p_w,
			int p_h,
			float p_search_radius
	);
	void _field_seed_segments(Ref<DeformerJob> p_job);
	void _field_sweep_nearest(Ref<DeformerJob> p_job);
	void _field_refine_adjacent(Ref<DeformerJob> p_job);
	void _field_exact_band(
			Ref<DeformerJob> p_job,
			float p_search_radius
	);
	void _field_fill_interior(Ref<DeformerJob> p_job);
	void _field_collect_active_tiles(
			Ref<DeformerJob> p_job,
			const Rect2 &p_aabb,
			int p_w,
			int p_h,
			float p_search_radius
	);

	// ---- Per-pixel work (tr_deformer_pixel.cpp) ----
	void _deform_tile_field(
			Ref<DeformerJob> p_job,
			const Rect2i &p_tile,
			int p_w
	);
	void _deform_tile_fallback(
			Ref<DeformerJob> p_job,
			const Rect2i &p_tile,
			int p_task_idx,
			int p_w
	);
	float _falloff_weight(
			const Ref<DeformerJob> &p_job,
			float p_distance,
			bool p_is_inside
	) const;

	// ---- Dispatch (tr_deformer.cpp) ----
	void _dispatch_deformer_job(
			Ref<DeformerJob> p_job,
			bool p_threaded
	);

protected:
	static void _bind_methods();

public:
	TerrainSplineDeformer();
	~TerrainSplineDeformer();

	/// How far beyond the spline this component affects terrain (used for chunk culling).
	float get_spline_padding() const override { return spline_width + falloff_distance; }

	void set_max_height(float p_height) {
		max_height = p_height;
		mark_dirty();
	}
	float get_max_height() const { return max_height; }

	void set_spline_width(float p_width) {
		spline_width = MAX(0.0f, p_width);
		mark_dirty();
	}
	float get_spline_width() const { return spline_width; }

	void set_falloff_distance(float p_dist) {
		falloff_distance = MAX(0.0f, p_dist);
		mark_dirty();
	}
	float get_falloff_distance() const { return falloff_distance; }

	void set_inner_falloff_distance(float p_dist) {
		inner_falloff_distance = MAX(0.0f, p_dist);
		mark_dirty();
	}
	float get_inner_falloff_distance() const { return inner_falloff_distance; }

	void set_fill_interior(bool p_fill) {
		fill_interior = p_fill;
		mark_dirty();
	}
	bool get_fill_interior() const { return fill_interior; }

	void set_blend_mode(BlendMode p_mode) {
		blend_mode = p_mode;
		mark_dirty();
	}
	BlendMode get_blend_mode() const { return blend_mode; }

	void set_falloff_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_falloff_curve() const { return falloff_curve; }
	void set_inner_falloff_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_inner_falloff_curve() const { return inner_falloff_curve; }

	void set_use_tile_culling(bool p_use) {
		use_tile_culling = p_use;
		mark_dirty();
	}
	bool get_use_tile_culling() const { return use_tile_culling; }

	void set_use_distance_field(bool p_use) {
		use_distance_field = p_use;
		mark_dirty();
	}
	bool get_use_distance_field() const { return use_distance_field; }

	void set_height_source(HeightSource p_source) {
		height_source = p_source;
		mark_dirty();
	}
	HeightSource get_height_source() const { return height_source; }
	void set_profile_smoothing(float p_metres) {
		profile_smoothing = MAX(0.0f, p_metres);
		mark_dirty();
	}
	float get_profile_smoothing() const { return profile_smoothing; }
	void set_max_grade(float p_percent) {
		max_grade = MAX(0.0f, p_percent);
		mark_dirty();
	}
	float get_max_grade() const { return max_grade; }
	void set_profile_include_splines(bool p_include) {
		profile_include_splines = p_include;
		mark_dirty();
	}
	bool get_profile_include_splines() const { return profile_include_splines; }
	void set_earthwork(Earthwork p_mode) {
		earthwork = p_mode;
		mark_dirty();
	}
	Earthwork get_earthwork() const { return earthwork; }
	void set_max_cut_depth(float p_metres) {
		max_cut_depth = MAX(0.0f, p_metres);
		mark_dirty();
	}
	float get_max_cut_depth() const { return max_cut_depth; }
	void set_max_fill_height(float p_metres) {
		max_fill_height = MAX(0.0f, p_metres);
		mark_dirty();
	}
	float get_max_fill_height() const { return max_fill_height; }
	void set_road_blur(float p_metres) {
		road_blur = MAX(0.0f, p_metres);
		mark_dirty();
	}
	float get_road_blur() const { return road_blur; }

	/**
	 * @brief The road profile a HEIGHT_TERRAIN deformer would carve along its spline: world-space
	 * (x, y, z) of every baked vertex with y replaced by the smoothed / graded / capped ground profile,
	 * computed against p_heightmap's base terrain description (see TerrainSplineCompositor::
	 * make_profile_context). Lets a TerrainSplineRoad lie exactly on the roadbed. Main thread.
	 */
	bool bake_road_profile(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			std::vector<Vector3> &r_points
	);

	/**
	 * @brief Height this deformer would leave at world (p_x, p_z) given the current height there.
	 * 1-D evaluation for other deformers' terrain profiles; exact, O(segments).
	 */
	float evaluate_height_at(
			const Ref<DeformerJob> &p_job,
			float p_x,
			float p_z,
			float p_current_h
	) const;

	void set_tile_size(int p_size) {
		tile_size = MAX(8, p_size);
		mark_dirty();
	}
	int get_tile_size() const { return tile_size; }

	/**
	 * @brief The corridor weight of every pixel of a p_size² chunk at p_offset (row-major, z then x):
	 * exactly the weights this deformer blends heights with — 1 on the core, the falloff ramp (and
	 * curve) outside, the interior of a closed loop when fill_interior. Zero elsewhere. Lets other
	 * components (TerrainSplinePainter) follow the earthwork footprint precisely. Any thread once the
	 * spline cache is baked (tr_deformer_weights.cpp). Returns false when the spline misses the chunk;
	 * r_weights is then untouched.
	 */
	bool compute_weight_field(
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset,
			int p_size,
			const Rect2 &p_padded_aabb,
			std::vector<float> &r_weights
	);

	/// Main-thread entry point: bakes the spline cache, then deforms with tile-level threading.
	void deform_heightmap(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset
	);
	/// Any-thread entry point: the spline's padded AABB is precomputed and its cache already baked
	/// (both need the main thread). p_threaded=false runs tiles serially on the calling thread.
	void deform_heightmap_prepared(
			const Ref<TerrainHeightmap> &p_heightmap,
			ProceduralSpline3D *p_spline,
			const Vector2 &p_offset,
			const Rect2 &p_padded_aabb,
			bool p_threaded
	);
	/// WorkerThreadPool group-task entry: deforms active tile p_task_idx.
	void _deform_heightmap_task(
			int p_task_idx,
			Ref<DeformerJob> p_job
	);

	/// Marks the parent spline dirty so the compositor regenerates affected chunks.
	void mark_dirty();
	void _on_curve_changed();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplineDeformer::BlendMode);
VARIANT_ENUM_CAST(godot::TerrainSplineDeformer::HeightSource);
VARIANT_ENUM_CAST(godot::TerrainSplineDeformer::Earthwork);

#endif // TR_DEFORMER_H
