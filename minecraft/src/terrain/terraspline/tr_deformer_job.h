/**
 * @file tr_deformer_job.h
 * @brief DeformerJob: per-(deformer, chunk) inputs and precomputed data for heightmap deformation.
 */
#ifndef TR_DEFORMER_JOB_H
#define TR_DEFORMER_JOB_H

#include "tr_heightmap.h"
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineDeformer;

/**
 * @class DeformerJob
 * @brief Everything a deformation pass needs, prepared once so the per-pixel work is thread-safe.
 *
 * Built on the calling thread by TerrainSplineDeformer::_create_deformer_job, then filled by either
 * the distance-field pass (tr_deformer_field.cpp) or the legacy tile culler
 * (tr_deformer_legacy.cpp). Per-pixel tasks (tr_deformer_pixel.cpp) only read from it and write
 * into disjoint tiles of `data_ptr`.
 */
class DeformerJob : public RefCounted {
	GDCLASS(DeformerJob,
			RefCounted)

public:
	// ---- Inputs ----
	Ref<TerrainHeightmap> heightmap;
	ProceduralSpline3D *spline = nullptr;
	TerrainSplineDeformer *deformer = nullptr;
	Vector2 offset; // World XZ of the chunk's pixel (0,0)
	float *data_ptr = nullptr; // heightmap->get_data_ptrw(), cached

	// ---- Falloff curves, pre-sampled to 256 entries ----
	std::vector<float> baked_curve;
	std::vector<float> baked_inner_curve;
	bool has_curve = false;
	bool has_inner_curve = false;
	bool fill_interior = true;

	// ---- Work decomposition ----
	std::vector<Rect2i> active_tiles; // Pixel rectangles that may receive weight
	std::vector<std::vector<int>> tile_segments; // Legacy path only: candidate segments per tile

	// ---- Spline geometry as structure-of-arrays (tight inner loops) ----
	std::vector<float> seg_ax, seg_az, seg_abx, seg_abz, seg_l2, seg_y0, seg_dy;
	std::vector<float> vert_x, vert_z, vert_y;
	int interpolation_mode = 0; // ProceduralSpline3D::InterpolationMode
	float ridge_steepness = 0.0f;
	bool spline_closed = false;

	// ---- Distance field (Terraspline.md step 4) ----
	// Grid padded by `field_margin` pixels on every side so segments just outside the chunk still
	// influence it. Per pixel: nearest segment index and the nearest point on it, from which distance
	// and spline_y are derived exactly. `field_inside` is the even-odd fill of a closed spline.
	// When `field_valid` is false the per-pixel task falls back to the legacy evaluation.
	bool field_valid = false;
	int field_w = 0;
	int field_h = 0;
	int field_margin = 0;
	std::vector<int32_t> field_seg; // -1 = no segment within range
	std::vector<float> field_nx; // Nearest point X (world)
	std::vector<float> field_nz; // Nearest point Z (world)
	std::vector<uint8_t> field_inside;

	/// Index into the field arrays for chunk-local pixel (p_x, p_z).
	inline size_t field_index(
			int p_x,
			int p_z
	) const {
		return (size_t)(p_z + field_margin) * field_w + (size_t)(p_x + field_margin);
	}

	DeformerJob() {}
	~DeformerJob() {}

protected:
	static void _bind_methods() {}
};

} // namespace godot

#endif // TR_DEFORMER_JOB_H
