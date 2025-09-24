#include "minecraft.h"

#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/math.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector2i.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/classes/fast_noise_lite.hpp>

PackedInt32Array MinecraftNode::generate_terrain_heights(Vector2i indexPos, int dim, float freq, bool height_curve_sampling) {
	Vector3 offset = Vector3(indexPos.x * part_size, 0, indexPos.y * part_size);

	PackedInt32Array heights;

	heights.resize(dim * dim);

	if (height_curve != nullptr & height_curve_sampling) {
		for (int z = 0; z < dim; ++z) {
			for (int x = 0; x < dim; ++x) {
				float h = noise->get_noise_2d(offset.x + x, offset.z + z) + .5;

				h = height_curve->sample(h);

				int height = (int)Math::floor(h * part_size);

				heights.set(x + z * dim, height);
			}
		}
	} else {
		for (int z = 0; z < dim; ++z) {
			for (int x = 0; x < dim; ++x) {
				float h = noise->get_noise_2d(offset.x + x, offset.z + z) + .5;

				int height = (int)Math::floor(h * part_size);

				heights.set(x + z * dim, height);
			}
		}
	}
	return heights;
}
