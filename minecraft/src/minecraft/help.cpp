#include "minecraft.h"

#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/math.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/classes/fast_noise_lite.hpp>

PackedInt32Array MinecraftNode::generate_terrain_heights(Vector3 pos, bool height_curve_sampling) {
	PackedInt32Array heights;
	heights.resize(part_size * part_size);

	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	noise->set_seed(1337);
	noise->set_frequency(terrain_freq);

	if (height_curve != nullptr & height_curve_sampling) {
		for (int z = 0; z < part_size; ++z) {
			for (int x = 0; x < part_size; ++x) {
				float h = noise->get_noise_2d(pos.x + x, pos.z + z) + .5;

				h = height_curve->sample(h);

				int height = (int)Math::floor(h * terrain_height);

				heights.set(x + z * part_size, height);
			}
		}
	} else {
		for (int z = 0; z < part_size; ++z) {
			for (int x = 0; x < part_size; ++x) {
				float h = noise->get_noise_2d(pos.x + x, pos.z + z) + .5;

				int height = (int)Math::floor(h * terrain_height);

				heights.set(x + z * part_size, height);
			}
		}
	}
	return heights;
}
