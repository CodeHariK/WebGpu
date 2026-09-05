/**
 * @file tr_scatter.cpp
 * @brief TerrainSplineScatter: bindings and construction.
 *
 * Per-cell evaluation is in tr_scatter_cell.cpp; job creation/execution/finalization is in
 * tr_scatter_job.cpp.
 */
#include "tr_scatter.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void TerrainSplineScatter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &TerrainSplineScatter::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &TerrainSplineScatter::get_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");

	ClassDB::bind_method(D_METHOD("set_collision_shape", "shape"), &TerrainSplineScatter::set_collision_shape);
	ClassDB::bind_method(D_METHOD("get_collision_shape"), &TerrainSplineScatter::get_collision_shape);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "collision_shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"),
			"set_collision_shape", "get_collision_shape"
	);

	ClassDB::bind_method(D_METHOD("set_density", "density"), &TerrainSplineScatter::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &TerrainSplineScatter::get_density);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density"), "set_density", "get_density");

	ClassDB::bind_method(D_METHOD("set_spacing", "spacing"), &TerrainSplineScatter::set_spacing);
	ClassDB::bind_method(D_METHOD("get_spacing"), &TerrainSplineScatter::get_spacing);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spacing"), "set_spacing", "get_spacing");

	ClassDB::bind_method(D_METHOD("set_scale_min", "min_scale"), &TerrainSplineScatter::set_scale_min);
	ClassDB::bind_method(D_METHOD("get_scale_min"), &TerrainSplineScatter::get_scale_min);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_min"), "set_scale_min", "get_scale_min");

	ClassDB::bind_method(D_METHOD("set_scale_max", "max_scale"), &TerrainSplineScatter::set_scale_max);
	ClassDB::bind_method(D_METHOD("get_scale_max"), &TerrainSplineScatter::get_scale_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_max"), "set_scale_max", "get_scale_max");

	ClassDB::bind_method(D_METHOD("set_min_slope", "min_slope"), &TerrainSplineScatter::set_min_slope);
	ClassDB::bind_method(D_METHOD("get_min_slope"), &TerrainSplineScatter::get_min_slope);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_slope"), "set_min_slope", "get_min_slope");

	ClassDB::bind_method(D_METHOD("set_max_slope", "max_slope"), &TerrainSplineScatter::set_max_slope);
	ClassDB::bind_method(D_METHOD("get_max_slope"), &TerrainSplineScatter::get_max_slope);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_slope"), "set_max_slope", "get_max_slope");

	ClassDB::bind_method(D_METHOD("set_min_spline_dist", "dist"), &TerrainSplineScatter::set_min_spline_dist);
	ClassDB::bind_method(D_METHOD("get_min_spline_dist"), &TerrainSplineScatter::get_min_spline_dist);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_spline_dist"), "set_min_spline_dist", "get_min_spline_dist");

	ClassDB::bind_method(D_METHOD("set_max_spline_dist", "dist"), &TerrainSplineScatter::set_max_spline_dist);
	ClassDB::bind_method(D_METHOD("get_max_spline_dist"), &TerrainSplineScatter::get_max_spline_dist);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_spline_dist"), "set_max_spline_dist", "get_max_spline_dist");

	ClassDB::bind_method(D_METHOD("set_seed_offset", "seed_offset"), &TerrainSplineScatter::set_seed_offset);
	ClassDB::bind_method(D_METHOD("get_seed_offset"), &TerrainSplineScatter::get_seed_offset);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed_offset"), "set_seed_offset", "get_seed_offset");

	ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &TerrainSplineScatter::set_biome_noise);
	ClassDB::bind_method(D_METHOD("get_biome_noise"), &TerrainSplineScatter::get_biome_noise);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "biome_noise", PROPERTY_HINT_RESOURCE_TYPE, "Noise"), "set_biome_noise",
			"get_biome_noise"
	);

	ClassDB::bind_method(
			D_METHOD("set_biome_noise_threshold", "threshold"), &TerrainSplineScatter::set_biome_noise_threshold
	);
	ClassDB::bind_method(D_METHOD("get_biome_noise_threshold"), &TerrainSplineScatter::get_biome_noise_threshold);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "biome_noise_threshold"), "set_biome_noise_threshold",
			"get_biome_noise_threshold"
	);

	ClassDB::bind_method(D_METHOD("run_scatter_job", "job", "chunk_size"), &TerrainSplineScatter::run_scatter_job);
}

TerrainSplineScatter::TerrainSplineScatter() {}

TerrainSplineScatter::~TerrainSplineScatter() {}

} // namespace godot
