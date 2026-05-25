#include "surface_nets/sdf_helpers.h"
#include "surface_nets/sn_grid.h"

namespace godot {

void SNGrid::generate_test_sdf() {
	int max_x = grid_size.x * chunk_size.x;
	int max_y = grid_size.y * chunk_size.y;
	int max_z = grid_size.z * chunk_size.z;

	// Center coordinates for sphere (left half) and box (right half)
	Vector3 sphere_center(static_cast<float>(max_x) * 0.3f, static_cast<float>(max_y) * 0.5f, static_cast<float>(max_z) * 0.5f);
	float sphere_radius = static_cast<float>(max_y) * 0.35f;

	Vector3 box_center(static_cast<float>(max_x) * 0.7f, static_cast<float>(max_y) * 0.5f, static_cast<float>(max_z) * 0.5f);
	Vector3 box_half_size(static_cast<float>(max_y) * 0.25f, static_cast<float>(max_y) * 0.25f, static_cast<float>(max_y) * 0.25f);

	SDFSphere sphere(sphere_center, sphere_radius);
	SDFBox box(box_center, box_half_size);

	// Increase this scale multiplier to give the 8-bit integer more fractional precision.
	// 120.0f is a good sweet spot for 8-bit ints (since the max is 127).
	const float SDF_SCALE = 120.0f;

	for (SNChunk &chunk : chunks) {
		int nx = chunk.size_x + 1;
		int ny = chunk.size_y + 1;
		int nz = chunk.size_z + 1;

		for (int ly = 0; ly < ny; ly++) {
			for (int lz = 0; lz < nz; lz++) {
				for (int lx = 0; lx < nx; lx++) {
					int gx = (chunk.loc_x * chunk.size_x) + lx;
					int gy = (chunk.loc_y * chunk.size_y) + ly;
					int gz = (chunk.loc_z * chunk.size_z) + lz;

					// Check boundary first
					int8_t req_density = 127;
					if (_is_boundary_corner(gx, gy, gz, req_density)) {
						chunk.set_corner(lx, ly, lz, req_density);
						continue;
					}

					Vector3 p(static_cast<float>(gx), static_cast<float>(gy), static_cast<float>(gz));

					// Sphere SDF
					float d_sphere = sphere.sample(p);

					float d_box = box.sample_approx(p);

					// Union
					float d = sdf_union(d_sphere, d_box);

					// Scale the float heavily so the decimals become whole numbers
					float scaled = d * SDF_SCALE;

					// Clamp it strictly within the int8_t bounds
					int8_t density = static_cast<int8_t>(std::clamp(scaled, -128.0f, 127.0f));

					chunk.set_corner(lx, ly, lz, density);
				}
			}
		}
	}

	refresh_grid();
}

void SNGrid::generate_noise_sdf() {
	if (terrain_noise.is_null()) {
		return;
	}

	int max_x = grid_size.x * chunk_size.x;
	int max_y = grid_size.y * chunk_size.y;
	int max_z = grid_size.z * chunk_size.z;

	const float SDF_SCALE = 120.0f;

	SDFHeightmap heightmap(terrain_noise, static_cast<float>(max_y));

	for (SNChunk &chunk : chunks) {
		int nx = chunk.size_x + 1;
		int ny = chunk.size_y + 1;
		int nz = chunk.size_z + 1;

		for (int ly = 0; ly < ny; ly++) {
			for (int lz = 0; lz < nz; lz++) {
				for (int lx = 0; lx < nx; lx++) {
					int gx = (chunk.loc_x * chunk.size_x) + lx;
					int gy = (chunk.loc_y * chunk.size_y) + ly;
					int gz = (chunk.loc_z * chunk.size_z) + lz;

					// Check boundary first
					int8_t req_density = 127;
					if (_is_boundary_corner(gx, gy, gz, req_density)) {
						chunk.set_corner(lx, ly, lz, req_density);
						continue;
					}

					Vector3 p(static_cast<float>(gx), static_cast<float>(gy), static_cast<float>(gz));
					float d = heightmap.sample(p);

					// Scale the SDF value for 8-bit precision
					float scaled = d * SDF_SCALE;

					// Clamp strictly within int8_t range
					int8_t density = static_cast<int8_t>(std::clamp(scaled, -128.0f, 127.0f));

					chunk.set_corner(lx, ly, lz, density);
				}
			}
		}
	}

	refresh_grid();
}

} //namespace godot
