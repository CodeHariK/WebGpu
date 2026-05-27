#include "terraspline.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TerrainHeightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "width", "height", "default_value"), &TerrainHeightmap::initialize, DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_width"), &TerrainHeightmap::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &TerrainHeightmap::get_height);
	ClassDB::bind_method(D_METHOD("get_height_at", "x", "z"), &TerrainHeightmap::get_height_at);
	ClassDB::bind_method(D_METHOD("set_height_at", "x", "z", "value"), &TerrainHeightmap::set_height_at);
	ClassDB::bind_method(D_METHOD("get_data"), &TerrainHeightmap::get_data);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &TerrainHeightmap::set_data);
	ClassDB::bind_method(D_METHOD("import_from_image", "image"), &TerrainHeightmap::import_from_image);
	ClassDB::bind_method(D_METHOD("export_to_image"), &TerrainHeightmap::export_to_image);
	ClassDB::bind_method(D_METHOD("generate_mesh", "lod_level"), &TerrainHeightmap::generate_mesh, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("generate_collision_shape"), &TerrainHeightmap::generate_collision_shape);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "width"), "", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "height"), "", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "data"), "set_data", "get_data");
}

TerrainHeightmap::TerrainHeightmap() {
	width = 0;
	height = 0;
}

TerrainHeightmap::~TerrainHeightmap() {
}

void TerrainHeightmap::initialize(int p_width, int p_height, float p_default_value) {
	if (p_width <= 0 || p_height <= 0) {
		width = 0;
		height = 0;
		data.clear();
		return;
	}
	width = p_width;
	height = p_height;
	data.resize(width * height);
	for (int i = 0; i < width * height; ++i) {
		data.set(i, p_default_value);
	}
}

float TerrainHeightmap::get_height_at(int x, int z) const {
	if (x < 0 || x >= width || z < 0 || z >= height) {
		return 0.0f;
	}
	return data[z * width + x];
}

void TerrainHeightmap::set_height_at(int x, int z, float val) {
	if (x < 0 || x >= width || z < 0 || z >= height) {
		return;
	}
	data.set(z * width + x, val);
}

void TerrainHeightmap::set_data(const PackedFloat32Array &p_data) {
	data = p_data;
	// Recalculate size from data if possible, assuming a square heightmap if width/height are uninitialized
	if (width * height != data.size()) {
		int side = (int)Math::sqrt((double)data.size());
		if (side * side == data.size()) {
			width = side;
			height = side;
		}
	}
}

void TerrainHeightmap::import_from_image(const Ref<Image> &p_image) {
	if (p_image.is_null() || p_image->is_empty()) {
		return;
	}
	width = p_image->get_width();
	height = p_image->get_height();
	data.resize(width * height);
	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width; ++x) {
			data.set(z * width + x, p_image->get_pixel(x, z).r);
		}
	}
}

Ref<Image> TerrainHeightmap::export_to_image() const {
	Ref<Image> img;
	img.instantiate();
	if (width <= 0 || height <= 0) {
		return img;
	}
	img->create_empty(width, height, false, Image::FORMAT_RF);
	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width; ++x) {
			float val = data[z * width + x];
			img->set_pixel(x, z, Color(val, 0.0f, 0.0f, 1.0f));
		}
	}
	return img;
}

Ref<ArrayMesh> TerrainHeightmap::generate_mesh(int p_lod_level) const {
	if (width <= 1 || height <= 1) {
		return Ref<ArrayMesh>();
	}

	int stride = 1 << p_lod_level;
	if (stride >= width || stride >= height) {
		stride = 1;
	}

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	std::vector<int> index_map(width * height, -1);
	int vertex_count = 0;

	for (int z = 0; z < height; z += stride) {
		for (int x = 0; x < width; x += stride) {
			float h = get_height_at(x, z);
			Vector3 vertex((float)x, h, (float)z);
			Vector2 uv((float)x / (width - 1), (float)z / (height - 1));

			// Normal estimation
			float hl = get_height_at(x - 1, z);
			float hr = get_height_at(x + 1, z);
			float hd = get_height_at(x, z - 1);
			float hu = get_height_at(x, z + 1);
			Vector3 normal(hl - hr, 2.0f, hd - hu);
			normal = normal.normalized();

			st->set_normal(normal);
			st->set_uv(uv);
			st->add_vertex(vertex);

			index_map[z * width + x] = vertex_count++;
		}
	}

	for (int z = 0; z < height - stride; z += stride) {
		for (int x = 0; x < width - stride; x += stride) {
			int i0 = index_map[z * width + x];
			int i1 = index_map[z * width + (x + stride)];
			int i2 = index_map[(z + stride) * width + x];
			int i3 = index_map[(z + stride) * width + (x + stride)];

			if (i0 == -1 || i1 == -1 || i2 == -1 || i3 == -1) {
				continue;
			}

			// First triangle (CCW)
			st->add_index(i0);
			st->add_index(i1);
			st->add_index(i3);

			// Second triangle (CCW)
			st->add_index(i0);
			st->add_index(i3);
			st->add_index(i2);
		}
	}

	// Add Skirts along the 4 borders
	const float skirt_depth = 10.0f;

	auto add_skirt_segment = [&](int x0, int z0, int x1, int z1, bool reverse_winding) {
		int idx0 = index_map[z0 * width + x0];
		int idx1 = index_map[z1 * width + x1];
		if (idx0 == -1 || idx1 == -1) {
			return;
		}

		float h0 = get_height_at(x0, z0);
		float h1 = get_height_at(x1, z1);

		Vector3 v0_skirt((float)x0, h0 - skirt_depth, (float)z0);
		Vector3 v1_skirt((float)x1, h1 - skirt_depth, (float)z1);

		Vector2 uv0((float)x0 / (width - 1), (float)z0 / (height - 1));
		Vector2 uv1((float)x1 / (width - 1), (float)z1 / (height - 1));

		Vector3 normal0(0, 1, 0);

		st->set_normal(normal0);
		st->set_uv(uv0);
		st->add_vertex(v0_skirt);
		int idx0_skirt = vertex_count++;

		st->set_normal(normal0);
		st->set_uv(uv1);
		st->add_vertex(v1_skirt);
		int idx1_skirt = vertex_count++;

		if (reverse_winding) {
			// Triangle 1
			st->add_index(idx0);
			st->add_index(idx0_skirt);
			st->add_index(idx1);

			// Triangle 2
			st->add_index(idx1);
			st->add_index(idx0_skirt);
			st->add_index(idx1_skirt);
		} else {
			// Triangle 1
			st->add_index(idx0);
			st->add_index(idx1);
			st->add_index(idx0_skirt);

			// Triangle 2
			st->add_index(idx1);
			st->add_index(idx1_skirt);
			st->add_index(idx0_skirt);
		}
	};

	int max_valid_x = ((width - 1) / stride) * stride;
	int max_valid_z = ((height - 1) / stride) * stride;

	// North edge
	for (int x = 0; x < max_valid_x; x += stride) {
		add_skirt_segment(x, 0, x + stride, 0, false);
	}

	// South edge
	for (int x = 0; x < max_valid_x; x += stride) {
		add_skirt_segment(x, max_valid_z, x + stride, max_valid_z, true);
	}

	// West edge
	for (int z = 0; z < max_valid_z; z += stride) {
		add_skirt_segment(0, z, 0, z + stride, true);
	}

	// East edge
	for (int z = 0; z < max_valid_z; z += stride) {
		add_skirt_segment(max_valid_x, z, max_valid_x, z + stride, false);
	}

	return st->commit();
}

Ref<HeightMapShape3D> TerrainHeightmap::generate_collision_shape() const {
	Ref<HeightMapShape3D> shape;
	shape.instantiate();
	shape->set_map_width(width);
	shape->set_map_depth(height);
	shape->set_map_data(data);
	return shape;
}

} // namespace godot
