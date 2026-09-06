/**
 * @file tr_heightmap.h
 * @brief TerrainHeightmap: a flat float grid of elevations for one chunk.
 */
#ifndef TR_HEIGHTMAP_H
#define TR_HEIGHTMAP_H

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineDeformer;

/**
 * @class TerrainHeightmap
 * @brief Contiguous float grid of heights for a single terrain chunk.
 *
 * Workers write into the buffer through get_data_ptrw() with no locking: every tile of a chunk is
 * owned by exactly one task, so writes never overlap. get_image() packs the floats into an
 * Image::FORMAT_RF image for Terrain3D.
 */
class TerrainHeightmap : public RefCounted {
	GDCLASS(TerrainHeightmap,
			RefCounted)

private:
	int width = 0;
	int height = 0;
	float base_elevation = 0.0f; // Value the grid was last cleared to (the flat ground level)
	Ref<Noise> base_noise; // Optional: the base terrain is base_elevation + noise * amplitude
	float base_noise_amplitude = 0.0f;

public:
	/// A spline-height deformer that shapes this terrain; terrain-following profiles sample through them.
	struct BaseDeformer {
		ProceduralSpline3D *spline = nullptr;
		TerrainSplineDeformer *deformer = nullptr;
	};

private:
	std::vector<BaseDeformer> base_deformers; // In application order
	PackedFloat32Array data;

protected:
	static void _bind_methods();

public:
	TerrainHeightmap();
	~TerrainHeightmap();

	/// Allocates width*height floats and fills them with p_default_value.
	void initialize(
			int p_width,
			int p_height,
			float p_default_value = 0.0f
	);
	/// Resets every height to p_default_value (size unchanged).
	void clear(float p_default_value = 0.0f);

	int get_width() const { return width; }
	int get_height() const { return height; }
	/// Ground level the grid was cleared to; relative blend modes measure spline heights against it.
	float get_base_elevation() const { return base_elevation; }
	/// Describes the undeformed terrain so deformers can query it anywhere (also outside this grid).
	void set_base_terrain(
			const Ref<Noise> &p_noise,
			float p_amplitude
	) {
		base_noise = p_noise;
		base_noise_amplitude = p_amplitude;
	}
	/// The spline-height deformers applied to this terrain, in order (for HEIGHT_TERRAIN profiles).
	void set_base_deformers(const std::vector<BaseDeformer> &p_deformers) { base_deformers = p_deformers; }
	const std::vector<BaseDeformer> &get_base_deformers() const { return base_deformers; }
	/// Undeformed terrain height at a world XZ position (base_elevation + noise). Thread-safe.
	float base_height_at(
			float p_x,
			float p_z
	) const {
		if (base_noise.is_valid()) {
			return base_elevation + (float)base_noise->get_noise_2d(p_x, p_z) * base_noise_amplitude;
		}
		return base_elevation;
	}

	/// Raw writable pointer; valid until the next initialize().
	float *get_data_ptrw() { return data.ptrw(); }
	/// Copies the grid into a new FORMAT_RF image.
	Ref<Image> get_image() const;
};

} // namespace godot

#endif // TR_HEIGHTMAP_H
