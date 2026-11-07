// The water rebuild (_rebuild_water): runs the steps in order, then makes the one water mesh.
//
//   shared   1 flood (stored levels) -> 2a spill -> 2b held -> 2c flow -> 3 bodies
//   tiles    4 tiles: each body's baked tile, stamped and welded (+ the fall strips)
//   build    the mesh (_water_mesh); debug overlays when on
// One water mesh for the whole grid, rebuilt at the end of a frame in which water or ground changed
// (flush_dirty_chunks) and on refresh_grid. water_stage_usec holds each step's time.
#include "marching_cubes/terrain/mc_grid.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <vector>

namespace godot {

namespace {
const char *WATER_SHADER = "res://material/shaders/terrain/water.gdshader"; // surfaces + falls

// A material from a shader path (null if the shader isn't there).
Ref<Material> shader_material(const char *p_path) {
	Ref<Shader> shader = ResourceLoader::get_singleton()->load(p_path);
	if (shader.is_null()) {
		return Ref<Material>();
	}
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}
} // namespace

// Rebuild the water mesh: surfaces and falls cell by cell with the cliff tiles' shape
// (mc_water_bodies.cpp), the flow in the vertex colour.
void MCGrid::_rebuild_water() {
	uint64_t started = Time::get_singleton()->get_ticks_usec();
	water_dirty = false;
	MCWaterArrays arrays_out;
	if (heightmap_mode && water_levels.size() == column_heights.size()) {
		// diagonal spills, for this build only (mc_water_2a_spill.cpp): swapped back below
		Time *time = Time::get_singleton();
		uint64_t s0 = time->get_ticks_usec();
		std::vector<uint8_t> stored = _spilled_levels(&water_spill_steps);
		water_levels.swap(stored);
		uint64_t s1 = time->get_ticks_usec();
		std::vector<float> surface;
		std::vector<uint8_t> held;
		_water_surfaces(surface, held);
		uint64_t s2 = time->get_ticks_usec();
		std::vector<Color> flow = _water_flow(surface);
		uint64_t s3 = time->get_ticks_usec();
		water_bodies_nsec = 0;
		int winding_before = slice_winding;
		_emit_water_tiles(surface, held, flow, arrays_out);
		if (winding_before == 0 && slice_winding < 0) {
			// this build settled the bank slices' winding and flipped the ones made before: the cells
			// already placed used tiles from the unflipped ones, so place everything again
			arrays_out = MCWaterArrays();
			water_bodies_nsec = 0;
			_emit_water_tiles(surface, held, flow, arrays_out);
		}
		water_debug_samples = arrays_out.samples;
		uint64_t t0 = Time::get_singleton()->get_ticks_usec();
		uint64_t bodies_usec = water_bodies_nsec / 1000;
		water_stage_usec[0] = s1 - s0; // 2a spill (spilled levels)
		water_stage_usec[1] = s2 - s1; // 2b held / stream surfaces
		water_stage_usec[2] = s3 - s2; // 2c flow
		water_stage_usec[3] = bodies_usec; // 3 bodies (inside the tiles pass)
		water_stage_usec[4] = (t0 - s3) - std::min(bodies_usec, t0 - s3); // 4 tiles
		water_debug_edges = arrays_out.edges; // (only collected for debug stage 3)
		if (debug_water) {
			_rebuild_water_debug(surface, held, flow);
		}
		water_levels.swap(stored); // the stored levels back
	}
	water_rebuild_usec = Time::get_singleton()->get_ticks_usec() - started; // (mesh upload below not included)
	uint64_t timed = 0;
	for (int i = 0; i < 5; i++) {
		timed += water_stage_usec[i];
	}
	water_stage_usec[5] = water_rebuild_usec - std::min(timed, water_rebuild_usec); // the rest: debug overlay
	water_stage_usec[6] = 0; // mesh upload, set below
	if (arrays_out.vertices.is_empty()) {
		if (water_node) {
			water_node->set_mesh(Ref<Mesh>());
		}
		if (water_debug_node) {
			water_debug_node->hide();
		}
		return;
	}
	if (!water_node) {
		water_node = memnew(MeshInstance3D);
		water_node->set_name("Water");
		water_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(water_node);
	}
	if (water_material.is_null()) {
		water_material = shader_material(WATER_SHADER);
	}
	uint64_t upload = Time::get_singleton()->get_ticks_usec();
	water_node->set_mesh(water_view == WATER_VIEW_PIECES ? _water_pieces_mesh(arrays_out) : _water_mesh(arrays_out));
	// how many pieces have triangles: x the flat tiles, y the fall strips ("pieces" view)
	auto non_empty = [](const std::vector<int64_t> &p_starts, int64_t p_end) {
		int count = 0;
		for (size_t k = 0; k < p_starts.size(); k++) {
			count += (k + 1 < p_starts.size() ? p_starts[k + 1] : p_end) > p_starts[k] ? 1 : 0;
		}
		return count;
	};
	water_piece_counts = Vector2i(non_empty(arrays_out.pieces, arrays_out.indices.size()),
			non_empty(arrays_out.first_pieces, arrays_out.first_indices.size()));
	_apply_water_view();
	water_stage_usec[6] = Time::get_singleton()->get_ticks_usec() - upload; // the mesh to the GPU
}

// The water mesh: ONE surface with one material (water.gdshader) for the surfaces and the falls, so
// where a fall's strip leaves the surface the same shader runs on both sides (no seam). The falls'
// triangles come first in the index list: they write depth, so a surface behind a fall is hidden
// and one in front blends over it. UV2 per vertex: over the lip (0 on the surfaces), and metres
// above the landing (falls) or -1 on a stream (see water.gdshader).
Ref<ArrayMesh> MCGrid::_water_mesh(const MCWaterArrays &p_arrays) const {
	PackedVector3Array vertices = p_arrays.vertices; // surfaces and fall strips together
	PackedVector3Array normals = p_arrays.normals;
	PackedInt32Array indices = p_arrays.first_indices; // the falls first (they write depth)
	indices.append_array(p_arrays.indices);
	// every triangle wound to face the way its vertex normals point: the builders put corners in
	// whatever order their shape gave (a fall's ends a / b come either way round), and a triangle
	// seen from its back gets its normal flipped by the renderer (cull_disabled), so it is lit as if
	// it faced away — dark pieces among light ones
	int32_t *ix = indices.ptrw();
	for (int64_t t = 0; t + 2 < indices.size(); t += 3) {
		const Vector3 &a = vertices[ix[t]];
		const Vector3 &b = vertices[ix[t + 1]];
		const Vector3 &c = vertices[ix[t + 2]];
		Vector3 average = normals[ix[t]] + normals[ix[t + 1]] + normals[ix[t + 2]];
		// Godot's front faces are clockwise: their (b - a) x (c - a) points away from the normal
		if ((b - a).cross(c - a).dot(average) > 0.0f) {
			std::swap(ix[t + 1], ix[t + 2]);
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = p_arrays.uvs;
	arrays[Mesh::ARRAY_COLOR] = p_arrays.colors;
	arrays[Mesh::ARRAY_TEX_UV2] = p_arrays.uv2s;
	// CUSTOM0.x: the water's depth, baked (2 m where a vertex has none)
	PackedFloat32Array depths;
	depths.resize(vertices.size());
	depths.fill(2.0f);
	for (int i = 0; i < p_arrays.depths.size() && i < depths.size(); i++) {
		depths.set(i, p_arrays.depths[i]);
	}
	arrays[Mesh::ARRAY_CUSTOM0] = depths;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	uint64_t format = static_cast<uint64_t>(Mesh::ARRAY_CUSTOM_R_FLOAT) << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, TypedArray<Array>(), Dictionary(),
			BitField<Mesh::ArrayFormat>(format));
	mesh->surface_set_material(0, water_material);
	return mesh;
}

} // namespace godot
