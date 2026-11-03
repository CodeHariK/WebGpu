// MCManager: record an inspected spot so it can be looked at again later (or by a script).
//
// Every inspect click (mc_manager_inspect.cpp) and the Debug tab's "Record view" button write:
//   user://inspect/<time>.json   the camera (position, basis, fov), the cell (inspected, or the
//                                hovered one for Record view) and its info text, the mouse, the
//                                scene, the water settings, and the 4 x 4 columns around the cell
//                                (ground height + water level)
//   user://inspect/<time>.mct    the whole map (MCGrid::save_grid)
//   user://inspect/<time>.png    the screen as it was
// and the same three again as user://inspect/last.* (the newest record).
#include "marching_cubes/editor/mc_manager.h"
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {
const char *RECORD_DIR = "user://inspect";

Array vec3(const Vector3 &p_v) {
	Array a;
	a.push_back(p_v.x);
	a.push_back(p_v.y);
	a.push_back(p_v.z);
	return a;
}

Array vec3i(const Vector3i &p_v) {
	Array a;
	a.push_back(p_v.x);
	a.push_back(p_v.y);
	a.push_back(p_v.z);
	return a;
}
} // namespace

// Debug tab "Record view": the camera and the hovered cell, without inspecting.
void MCManager::_on_record_view() {
	MCGrid *grid = Object::cast_to<MCGrid>(get_node_or_null(grid_stats.path));
	if (grid) {
		_record_inspect(grid, locked_grid_pos, "hovered");
	}
}

// Write the record (see the top of this file) for p_cell; p_how says how the cell was picked.
void MCManager::_record_inspect(
		MCGrid *p_grid,
		const Vector3i &p_cell,
		const String &p_how
) {
	Viewport *viewport = get_viewport();
	Camera3D *camera = viewport ? viewport->get_camera_3d() : nullptr;
	if (!p_grid || !camera) {
		return;
	}
	DirAccess::make_dir_recursive_absolute(RECORD_DIR);
	String stamp = Time::get_singleton()->get_datetime_string_from_system().replace(":", "-");

	Dictionary record;
	record["time"] = stamp;
	Node *scene_root = get_owner() ? get_owner() : get_tree()->get_current_scene(); // the scene this manager is in
	record["scene"] = scene_root ? scene_root->get_scene_file_path() : String();
	Transform3D view = camera->get_global_transform();
	Dictionary cam;
	cam["origin"] = vec3(view.origin);
	cam["x"] = vec3(view.basis.get_column(0));
	cam["y"] = vec3(view.basis.get_column(1));
	cam["z"] = vec3(view.basis.get_column(2));
	cam["fov"] = camera->get_fov();
	record["camera"] = cam;
	record["mouse"] = Array::make(viewport->get_mouse_position().x, viewport->get_mouse_position().y);
	record["cell_picked"] = p_how;
	record["cell"] = vec3i(p_cell);
	record["cell_info"] = p_grid->get_cell_info(p_cell);
	record["grid_origin"] = vec3(p_grid->get_global_position());

	Dictionary water;
	water["expand"] = p_grid->get_water_expand();
	water["drop"] = p_grid->get_water_drop();
	water["sample_rate"] = p_grid->get_water_sample_rate();
	water["sampled"] = p_grid->get_water_sampled();
	water["square"] = p_grid->get_water_square();
	record["water"] = water;

	// the columns around the cell: rows of "ground~level" (level only when wet)
	Array columns;
	for (int z = p_cell.z - 1; z <= p_cell.z + 2; z++) {
		Array row;
		for (int x = p_cell.x - 1; x <= p_cell.x + 2; x++) {
			Vector2i column(x, z);
			int ground = p_grid->get_column_height(column);
			int level = p_grid->get_water_level(column);
			row.push_back(vformat("(%d,%d) %d", x, z, ground) + (level > ground ? vformat("~%d", level) : String()));
		}
		columns.push_back(row);
	}
	record["columns"] = columns;

	String text = JSON::stringify(record, "  ");
	Ref<Image> screen = viewport->get_texture()->get_image();
	for (const String &name : { stamp, String("last") }) {
		String base = String(RECORD_DIR) + "/" + name;
		Ref<FileAccess> file = FileAccess::open(base + ".json", FileAccess::WRITE);
		if (file.is_valid()) {
			file->store_string(text);
		}
		p_grid->save_grid(base + ".mct");
		if (screen.is_valid()) {
			screen->save_png(base + ".png");
		}
	}
	String summary = vformat("Recorded %s cell (%d, %d, %d)\n%s", p_how, p_cell.x, p_cell.y, p_cell.z, stamp);
	if (record_label) {
		record_label->set_text(summary);
	}
	UtilityFunctions::print("MCManager: ", summary.replace("\n", " "), " -> ", RECORD_DIR, "/last.*");
}

} // namespace godot
