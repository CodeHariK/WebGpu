// MCGrid: the water debug stages (Water tab, "Debug stage" slider; the Water debug overlay).
//
// How the water's shape is built, one layer more per stage (the mesh is hidden for 1 .. 3):
//   1 columns    the wet columns (blue held, green stream), the column spill's columns in orange
//                with the step each was spilled at (1: the landing past the low corner, then 2, 3 ...
//                as it spreads)
//   2 cells      + the cell grid of every cell with water, and a dot in each corner of each water
//                body for its class there: bank (orange), wet (blue), lower (purple), dry (yellow).
//                A cell's water edge is where the corners' rounded blend crosses half way, so these
//                four dots decide the curve in that cell
//   3 edges      + every cell's edge pieces as the tiles baked them (bank green, fall orange) and
//                the wet sample points: the curves of step 2, cell by cell
//   4 mesh       the water mesh with its triangles drawn over it
#include "marching_cubes/grid/mc_debug_mesh.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/3_bodies/mc_water_shape.h"
#include "marching_cubes/water/4_tiles/mc_water_tile.h"
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <algorithm>

namespace godot {

using namespace mc_water;

namespace {
const float LIFT = 0.06f; // m the stage layers float above the water
const Color SPILLED(1.0f, 0.55f, 0.1f, 0.9f); // a column spill's column
const Color GRID(0.85f, 0.85f, 0.85f, 0.6f); // the cell grid
const Color CLASS_COLOR[4] = {
	Color(1.0f, 0.5f, 0.15f), // bank
	Color(0.2f, 0.55f, 1.0f), // wet (one of the body's own corners)
	Color(0.7f, 0.3f, 1.0f), // lower
	Color(1.0f, 0.95f, 0.25f), // dry
};
const Color BANK_EDGE(0.2f, 1.0f, 0.3f);
const Color FALL_EDGE(1.0f, 0.55f, 0.1f);
const Vector2i CELL_AT[4] = { Vector2i(0, 1), Vector2i(1, 1), Vector2i(1, 0), Vector2i(0, 0) }; // Body corner order

} // namespace

String MCGrid::water_debug_stage_text(int p_stage) {
	switch (p_stage) {
		case 1:
			return "1 columns: wet (blue held, green stream), column spill (orange, step number)";
		case 2:
			return "2 cells: + cell grid, corner classes: bank orange, wet blue, lower purple, dry yellow";
		case 3:
			return "3 edges: + each cell's edge pieces (bank green, fall orange) and wet samples";
		case 4:
			return "4 mesh: the water mesh's triangles";
		default:
			return "0: off (the normal water debug overlay)";
	}
}

void MCGrid::set_water_debug_stage(int p_stage) {
	water_debug_stage = std::clamp(p_stage, 0, WATER_DEBUG_STAGES);
	if (water_debug_stage > 0 && !debug_water) {
		set_debug_water(true); // rebuilds
	} else {
		_rebuild_water();
	}
}

void MCGrid::_add_water_stage_debug(
		MCDebugMesh &r_debug,
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow
) {
	// the step numbers: rebuilt every time
	if (!water_debug_labels) {
		water_debug_labels = memnew(Node3D);
		water_debug_labels->set_name("StageLabels");
		water_debug_node->add_child(water_debug_labels);
	}
	for (int i = water_debug_labels->get_child_count() - 1; i >= 0; i--) {
		Node *child = water_debug_labels->get_child(i);
		water_debug_labels->remove_child(child);
		memdelete(child);
	}
	if (water_debug_stage <= 0) {
		return;
	}
	Vector2i columns = _column_counts();
	// 1. the spilled columns
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			size_t i = static_cast<size_t>(_column_index(x, z));
			int step = i < water_spill_steps.size() ? water_spill_steps[i] : 0;
			if (step == 0 || !_is_wet(static_cast<int>(i))) {
				continue;
			}
			Vector3 centre(static_cast<float>(x), p_surface[i] + LIFT, static_cast<float>(z));
			r_debug.square(centre, 0.3f, SPILLED);
			Label3D *label = memnew(Label3D);
			label->set_text(String::num_int64(step));
			label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
			label->set_pixel_size(0.006f);
			label->set_font_size(48);
			label->set_outline_size(12);
			label->set_position(centre + Vector3(0, 0.25f, 0));
			water_debug_labels->add_child(label);
		}
	}
	// 2. the cell grid and each body's corner classes
	if (water_debug_stage >= 2) {
		std::vector<Body> bodies;
		std::vector<float> bottoms;
		for (int cz = 0; cz + 1 < columns.y; cz++) {
			for (int cx = 0; cx + 1 < columns.x; cx++) {
				_water_bodies(cx, cz, p_surface, p_held, p_flow, bodies, bottoms);
				float top = -1e6f;
				for (const Body &body : bodies) {
					top = std::max(top, body.top);
				}
				if (bodies.empty()) {
					continue;
				}
				float y = top + LIFT;
				Vector3 c00(static_cast<float>(cx), y, static_cast<float>(cz));
				Vector3 c10 = c00 + Vector3(1, 0, 0);
				Vector3 c11 = c00 + Vector3(1, 0, 1);
				Vector3 c01 = c00 + Vector3(0, 0, 1);
				r_debug.line(c00, c10, GRID);
				r_debug.line(c10, c11, GRID);
				r_debug.line(c11, c01, GRID);
				r_debug.line(c01, c00, GRID);
				Vector3 centre = c00 + Vector3(0.5f, 0, 0.5f);
				for (const Body &body : bodies) {
					for (int k = 0; k < 4; k++) {
						Vector3 corner(static_cast<float>(cx + CELL_AT[k].x), body.top + LIFT * 1.5f, static_cast<float>(cz + CELL_AT[k].y));
						Vector3 at = corner + (Vector3(centre.x, corner.y, centre.z) - corner) * 0.22f; // just inside the cell
						r_debug.square(at, 0.05f, CLASS_COLOR[corner_class(body, k)]);
					}
				}
			}
		}
	}
	// 3. every cell's edge pieces
	if (water_debug_stage == 3) {
		for (const WaterEdge &edge : water_debug_edges) {
			Vector3 lift(0, LIFT * 2.0f, 0);
			r_debug.line(edge.a + lift, edge.b + lift, edge.kind == WaterEdge::FALL ? FALL_EDGE : BANK_EDGE);
		}
	}
}

} // namespace godot
