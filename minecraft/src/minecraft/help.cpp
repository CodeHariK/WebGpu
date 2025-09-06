#include "minecraft.h"

#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"

#include "godot_cpp/core/math.hpp"

#include <godot_cpp/classes/fast_noise_lite.hpp>

PackedInt32Array MinecraftNode::generate_terrain_heights(bool height_curve_sampling, int width, int depth, float scale, float height_scale) {
	PackedInt32Array heights;
	heights.resize(width * depth);

	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	noise->set_seed(1337);
	noise->set_frequency(scale);

	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			float n = noise->get_noise_2d((float)x + 0.5f, (float)z + 0.5f);

			UtilityFunctions::print(n);

			float h;
			if (height_curve_sampling) {
				h = (n + 1.0f) / 2.0f; // Normalize to [0, 1]
				// h = height_curve->sample(h); // Apply curve: output should also be [0, 1]
			} else {
				h = n; // Optional: still normalize if you want unsigned height
			}

			int height = (int)Math::floor(h * height_scale);
			heights.set(x + z * width, height);
		}
	}

	UtilityFunctions::print(heights);

	return heights;
}
