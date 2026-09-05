/**
 * @file tr_heightmap.cpp
 * @brief TerrainHeightmap implementation.
 */
#include "tr_heightmap.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TerrainHeightmap::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("initialize", "width", "height", "default_value"), &TerrainHeightmap::initialize, DEFVAL(0.0f)
	);
	ClassDB::bind_method(D_METHOD("clear", "default_value"), &TerrainHeightmap::clear, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_width"), &TerrainHeightmap::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &TerrainHeightmap::get_height);
	ClassDB::bind_method(D_METHOD("get_image"), &TerrainHeightmap::get_image);
}

TerrainHeightmap::TerrainHeightmap() {}

TerrainHeightmap::~TerrainHeightmap() {}

void TerrainHeightmap::initialize(
		int p_width,
		int p_height,
		float p_default_value
) {
	width = p_width;
	height = p_height;
	data.resize(width * height);
	clear(p_default_value);
}

void TerrainHeightmap::clear(float p_default_value) {
	int sz = width * height;
	float *ptr = data.ptrw();
	for (int i = 0; i < sz; ++i) {
		ptr[i] = p_default_value;
	}
}

Ref<Image> TerrainHeightmap::get_image() const {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();

	Ref<Image> img = Image::create_empty(width, height, false, Image::FORMAT_RF);
	if (width > 0 && height > 0) {
		PackedByteArray byte_data;
		byte_data.resize(data.size() * sizeof(float));
		memcpy(byte_data.ptrw(), data.ptr(), byte_data.size());
		img->set_data(width, height, false, Image::FORMAT_RF, byte_data);
	}

#if DEBUG
	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("    [TerrainHeightmap] get_image() created in: ", (t_end - t_start) / 1000.0, " ms.");
#endif
	return img;
}

} // namespace godot
