/**
 * @file tr_heightmap.h
 * @brief TerrainHeightmap: a flat float grid of elevations for one chunk.
 */
#ifndef TR_HEIGHTMAP_H
#define TR_HEIGHTMAP_H

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>

namespace godot {

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

	/// Raw writable pointer; valid until the next initialize().
	float *get_data_ptrw() { return data.ptrw(); }
	/// Copies the grid into a new FORMAT_RF image.
	Ref<Image> get_image() const;
};

} // namespace godot

#endif // TR_HEIGHTMAP_H
