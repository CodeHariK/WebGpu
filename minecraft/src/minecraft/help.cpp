#include "minecraft.h"

#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"

#include "godot_cpp/core/math.hpp"

#include <godot_cpp/classes/fast_noise_lite.hpp>

PackedInt32Array MinecraftNode::generate_terrain_heights(int width, int depth, float scale, float height_scale) {
	PackedInt32Array heights;
	heights.resize(width * depth);

	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	noise->set_seed(1337);
	noise->set_frequency(scale);

	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			float h = noise->get_noise_2d((float)x + 0.5f, (float)z + 0.5f) * height_scale;
			int height = (int)Math::floor(h);
			heights.set(x + z * width, height);
		}
	}

	return heights;
}
