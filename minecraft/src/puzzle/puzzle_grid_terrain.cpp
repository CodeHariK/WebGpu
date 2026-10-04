// PuzzleGrid — terrain: floor tiles, walls, bridges, liquids and their collision.
#include "puzzle_grid.h"

#include "../game_manager/game_constants.h"
#include "../utils/fx/toy_mesh.h"
#include "puzzle_cells.h"
#include "puzzle_liquid.h"

#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {

using namespace PuzzleCell;

static const Color FLOOR_A = Color(0.55f, 0.78f, 0.4f);
static const Color FLOOR_B = Color(0.5f, 0.72f, 0.36f);
static const Color ICE_COLOR = Color(0.78f, 0.92f, 1.0f);
static const Color PLANK_COLOR = Color(0.72f, 0.5f, 0.3f);
static const Color WALL_COLOR = Color(0.66f, 0.62f, 0.7f);
static const Color WALL_TOP = Color(0.5f, 0.82f, 0.45f); // grassy cap
static const float FLOOR_BOTTOM = -0.1f; // island slabs reach just below the ground
static const float LIQUID_DEPTH = 0.3f; // liquid surface sits this far below the floor top
static const float LIQUID_BLOCK_HEIGHT = 1.4f; // invisible fence over liquids (no walking in)

static MultiMeshInstance3D *_multimesh(
		Node3D *p_parent,
		const Ref<Mesh> &p_mesh,
		const Ref<Material> &p_material,
		int p_count
) {
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_use_colors(true);
	mm->set_mesh(p_mesh);
	mm->set_instance_count(p_count);
	MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
	mmi->set_multimesh(mm);
	mmi->set_material_override(p_material);
	p_parent->add_child(mmi);
	return mmi;
}

// Toon material tinted per instance (multimesh colours).
static Ref<StandardMaterial3D> _instance_tinted() {
	Ref<StandardMaterial3D> mat = ToyMesh::toon(Color(1, 1, 1));
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	return mat;
}

int PuzzleGrid::_count_cells(bool (*p_pred)(char)) const {
	int n = 0;
	for (char c : cells) {
		n += p_pred(c) ? 1 : 0;
	}
	return n;
}

static bool _is_tile(char p_k) { return p_k == FLOOR || p_k == PROP || p_k == ICE; }
static bool _is_bridge(char p_k) { return p_k == BRIDGE; }
static bool _is_wall(char p_k) { return p_k == WALL; }
static bool _is_water(char p_k) { return p_k == WATER; }
static bool _is_lava(char p_k) { return p_k == LAVA; }

// Raised island slabs (checker grass, pale ice), with a thin grout gap that reads as "grid".
void PuzzleGrid::_build_floor_tiles() {
	const float gap = cell_size * 0.04f;
	const float h = FLOOR_TOP - FLOOR_BOTTOM;
	MultiMeshInstance3D *mm = _multimesh(content, ToyMesh::box(Vector3(cell_size - gap, h, cell_size - gap)),
			_instance_tinted(), _count_cells(_is_tile));
	int i = 0;
	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			const char c = _cell(x, z);
			if (!_is_tile(c)) {
				continue;
			}
			mm->get_multimesh()->set_instance_transform(
					i, Transform3D(Basis(), _cell_center(x, z) + Vector3(0, FLOOR_BOTTOM + h * 0.5f, 0)));
			mm->get_multimesh()->set_instance_color(i, c == ICE ? ICE_COLOR : (((x + z) & 1) ? FLOOR_A : FLOOR_B));
			++i;
		}
	}
}

// Bridges: three planks across the cell at floor height, over the liquid below.
void PuzzleGrid::_build_bridges() {
	const int planks = 3;
	MultiMeshInstance3D *mm = _multimesh(content,
			ToyMesh::box(Vector3(cell_size * 0.98f, 0.12f, cell_size / planks * 0.86f)), _instance_tinted(),
			_count_cells(_is_bridge) * planks);
	int i = 0;
	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			if (!_is_bridge(_cell(x, z))) {
				continue;
			}
			// Planks run across the crossing direction: along X when the bridge links rows.
			const bool along_x = is_ground(_cell(x, z - 1)) || is_ground(_cell(x, z + 1));
			const Basis b = along_x ? Basis() : Basis(Vector3(0, 1, 0), Math::PI * 0.5f);
			for (int k = 0; k < planks; ++k) {
				const float off = ((k + 0.5f) / planks - 0.5f) * cell_size;
				const Vector3 o = along_x ? Vector3(0, 0, off) : Vector3(off, 0, 0);
				mm->get_multimesh()->set_instance_transform(
						i, Transform3D(b, _cell_center(x, z) + o + Vector3(0, FLOOR_TOP - 0.06f, 0)));
				mm->get_multimesh()->set_instance_color(i, PLANK_COLOR * (0.9f + 0.1f * ((x + k) % 2)));
				++i;
			}
		}
	}
}

void PuzzleGrid::_build_walls() {
	const int n = _count_cells(_is_wall);
	MultiMeshInstance3D *wall_mm =
			_multimesh(content, ToyMesh::box(Vector3(cell_size, wall_height, cell_size)), _instance_tinted(), n);
	MultiMeshInstance3D *cap_mm = _multimesh(content,
			ToyMesh::box(Vector3(cell_size * 1.02f, cell_size * 0.12f, cell_size * 1.02f)), _instance_tinted(), n);
	int i = 0;
	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			if (!_is_wall(_cell(x, z))) {
				continue;
			}
			const Vector3 p = _cell_center(x, z);
			// Slight per-block shade variation so walls don't read as one flat slab.
			const float v = 0.92f + 0.08f * (float)(((x * 7 + z * 13) % 5)) / 4.0f;
			wall_mm->get_multimesh()->set_instance_transform(i, Transform3D(Basis(), p + Vector3(0, wall_height * 0.5f, 0)));
			wall_mm->get_multimesh()->set_instance_color(i, WALL_COLOR * Color(v, v, v));
			cap_mm->get_multimesh()->set_instance_transform(i, Transform3D(Basis(), p + Vector3(0, wall_height, 0)));
			cap_mm->get_multimesh()->set_instance_color(i, WALL_TOP);
			++i;
		}
	}
}

// Water and lava surfaces below the island tops (one multimesh each, shader animated).
// Bridges and filled cells keep water under them too, so the island edge reads clearly.
void PuzzleGrid::_build_liquids() {
	water_mat = PuzzleLiquid::water_material();
	lava_mat = PuzzleLiquid::lava_material();
	auto under_water = [](char p_k) { return p_k == WATER || p_k == BRIDGE || p_k == FILLED; };
	int nw = 0;
	int nl = 0;
	for (char c : cells) {
		nw += under_water(c) ? 1 : 0;
		nl += _is_lava(c) ? 1 : 0;
	}
	const Ref<Mesh> sheet = ToyMesh::box(Vector3(cell_size, 0.1f, cell_size));
	MultiMeshInstance3D *water = _multimesh(content, sheet, water_mat, nw);
	MultiMeshInstance3D *lava = _multimesh(content, sheet, lava_mat, nl);
	int iw = 0;
	int il = 0;
	const Vector3 down(0, FLOOR_TOP - LIQUID_DEPTH - 0.05f, 0);
	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			const char c = _cell(x, z);
			const Transform3D t(Basis(), _cell_center(x, z) + down);
			if (under_water(c)) {
				water->get_multimesh()->set_instance_transform(iw++, t);
			} else if (_is_lava(c)) {
				lava->get_multimesh()->set_instance_transform(il++, t);
			}
		}
	}
}

// Static: walls and the exit door. Dynamic (rebuilt when a box fills water): island ground
// and the invisible fences over liquids.
void PuzzleGrid::_build_static_collision() {
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_name("WallCollision");
	body->set_collision_layer(toLayer(LAYER_TERRAIN));
	body->set_collision_mask(0);
	content->add_child(body);
	_build_collision_runs(body, is_solid, wall_height, 0.0f);
}

void PuzzleGrid::_rebuild_dynamic_collision() {
	if (dynamic_body) {
		content->remove_child(dynamic_body);
		dynamic_body->queue_free();
	}
	dynamic_body = memnew(StaticBody3D);
	dynamic_body->set_name("GroundCollision");
	dynamic_body->set_collision_layer(toLayer(LAYER_TERRAIN));
	dynamic_body->set_collision_mask(0);
	content->add_child(dynamic_body);
	_build_collision_runs(dynamic_body, is_ground, FLOOR_TOP - FLOOR_BOTTOM, FLOOR_BOTTOM);
	_build_collision_runs(dynamic_body, is_liquid, LIQUID_BLOCK_HEIGHT, FLOOR_BOTTOM);
}

// One box shape per horizontal run of matching cells (keeps the shape count low).
void PuzzleGrid::_build_collision_runs(
		StaticBody3D *p_body,
		bool (*p_pred)(char),
		float p_height,
		float p_y
) {
	for (int z = 0; z < depth; ++z) {
		int x = 0;
		while (x < width) {
			if (!p_pred(_cell(x, z))) {
				++x;
				continue;
			}
			int end = x;
			while (end + 1 < width && p_pred(_cell(end + 1, z))) {
				++end;
			}
			const int n = end - x + 1;
			Ref<BoxShape3D> shape;
			shape.instantiate();
			shape->set_size(Vector3(n * cell_size, p_height, cell_size));
			CollisionShape3D *cs = memnew(CollisionShape3D);
			cs->set_shape(shape);
			cs->set_position(Vector3((x + n * 0.5f) * cell_size, p_y + p_height * 0.5f, (z + 0.5f) * cell_size));
			p_body->add_child(cs);
			x = end + 1;
		}
	}
}

} // namespace godot
