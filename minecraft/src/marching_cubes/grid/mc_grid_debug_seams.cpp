// MCGrid: the tile-seams overlay (Debug tab, "Tile seams"; on while inspecting).
//
// The terrain is one mesh per chunk, merged from one tile per cell, every cell a 1 m cube on the
// grid (cell (x, y, z) spans x .. x + 1 and so on in grid space; chunk meshes are in grid space).
// So two tiles meet exactly where the surface crosses a whole number in x, y or z. The overlay
// shader draws a dark line there (about line_px pixels wide, using how fast the position changes
// across a pixel), and tints each tile faintly with a colour from its cell (the cell just behind
// the surface), so neighbouring tiles tell apart. A surface lying IN such a plane (a flat wall on a
// cell side) would be all line, so a plane only counts where the surface actually crosses it.
// It replaces the wireframe overlay while on; with both on, the wireframe is its next pass.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

namespace {
const char *SEAMS_SHADER = R"(shader_type spatial;
render_mode unshaded, cull_back, depth_draw_never;
uniform float line_px = 1.6; // line width in pixels
uniform float tint = 0.16; // how strongly each tile is tinted
varying vec3 grid_pos;
varying vec3 grid_normal;
void vertex() {
	grid_pos = VERTEX; // chunk meshes are in grid space
	grid_normal = NORMAL;
}
float hash3(vec3 p) {
	return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}
void fragment() {
	vec3 change = fwidth(grid_pos); // metres per pixel along x, y, z
	vec3 away = abs(fract(grid_pos + 0.5) - 0.5) / max(change, vec3(1e-6)); // pixels to the nearest plane
	float px = 1e6;
	if (change.x > 1e-4) { px = min(px, away.x); } // only planes the surface crosses
	if (change.y > 1e-4) { px = min(px, away.y); }
	if (change.z > 1e-4) { px = min(px, away.z); }
	float line = 1.0 - clamp(px / line_px, 0.0, 1.0);
	vec3 cell = floor(grid_pos - normalize(grid_normal) * 0.02); // the cell this tile belongs to
	vec3 hue = 0.5 + 0.5 * cos(6.2831853 * (hash3(cell) + vec3(0.0, 0.33, 0.67)));
	ALBEDO = mix(hue, vec3(0.03), line);
	ALPHA = max(line * 0.9, tint);
}
)";
} // namespace

void MCGrid::set_debug_seams(bool p_on) {
	debug_seams = p_on;
	for (Chunk &chunk : chunks) {
		if (chunk.mesh_node) {
			_apply_wireframe(chunk.mesh_node);
		}
	}
}

Ref<Material> MCGrid::_seams_overlay() {
	if (seams_material.is_null()) {
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(SEAMS_SHADER);
		seams_material.instantiate();
		seams_material->set_shader(shader);
	}
	seams_material->set_next_pass(debug_wireframe ? _wireframe_material : Ref<Material>());
	return seams_material;
}

} // namespace godot
