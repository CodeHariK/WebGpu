#ifndef MC_WIRE_H
#define MC_WIRE_H

// Wireframe pieces for the editor's hover previews and debug views: line meshes and a line material
// that stays readable behind terrain (solid where visible, faint where hidden).
//
//   mc_wire_box        the 12 edges of a box centred on the origin (scale the node to resize it)
//   mc_wire_cube       a box's thick edge bars and faces as two surfaces (outlined, filled cube)
//   mc_wire_face       a square in the XY plane facing +Z: an outline plus a faint fill (two surfaces)
//   mc_wire_material   unshaded line colour; a next pass draws the hidden parts at a quarter of it

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

// The 12 edges of a box of p_size centred on the origin (PRIMITIVE_LINES).
Ref<ArrayMesh> mc_wire_box(const Vector3 &p_size);

// A p_size square in the XY plane centred on the origin, facing +Z: surface 0 its outline (lines),
// surface 1 its fill (two triangles). Give each surface its own material.
Ref<ArrayMesh> mc_wire_face(float p_size);

// A box of p_size centred on the origin: surface 0 its 12 edges as thick bars (p_thickness metres
// square, so they read at any distance, unlike 1-pixel lines), surface 1 its 6 faces (facing out).
// Give each surface its own material.
Ref<ArrayMesh> mc_wire_cube(
		const Vector3 &p_size,
		float p_thickness
);

// Unshaded, see-through line colour p_color. p_show_hidden: a next pass without depth test draws the
// parts behind other geometry at a quarter of the alpha, so the outline is never lost.
Ref<StandardMaterial3D> mc_wire_material(
		const Color &p_color,
		bool p_show_hidden = true
);

} // namespace godot

#endif // MC_WIRE_H
