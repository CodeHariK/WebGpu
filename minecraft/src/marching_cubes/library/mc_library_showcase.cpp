// MCTileLibrary::build_showcase(): every loaded mesh where it was in Blender (the add-on's Arrange,
// then Export), labelled, each clickable (LAYER_TILE_LIBRARY) so MCManager can pin it in the grid.
#include "game_manager/game_constants.h"
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

namespace {

// A 1 m click box around the tile's cell, carrying what MCManager needs to pin it.
StaticBody3D *make_click_box(const MCShowcaseTile &p_tile) {
	StaticBody3D *body = memnew(StaticBody3D);
	body->set_collision_layer(toLayer(LAYER_TILE_LIBRARY));
	body->set_collision_mask(0);
	body->set_position(p_tile.transform.origin);
	body->set_meta("mc_hash", p_tile.hash);
	body->set_meta("mc_level", String::chr(p_tile.height));
	body->set_meta("mc_name", p_tile.name);
	body->set_meta("mc_mesh", p_tile.mesh);
	Ref<BoxShape3D> box;
	box.instantiate();
	box->set_size(Vector3(1, 1, 1));
	CollisionShape3D *shape = memnew(CollisionShape3D);
	shape->set_shape(box);
	body->add_child(shape);
	return body;
}

} // namespace

// Replace the children with every loaded mesh at its Blender position, a label and a click box.
void MCTileLibrary::build_showcase() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		remove_child(child);
		child->queue_free();
	}
	for (const MCShowcaseTile &tile : showcase_tiles) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(tile.mesh);
		mi->set_name(tile.name);
		mi->set_transform(tile.transform);
		add_child(mi);

		Label3D *label = memnew(Label3D);
		label->set_text(String::chr(tile.height).to_upper() + " " + tile.name);
		label->set_position(tile.transform.origin + Vector3(0, 0.9f, 0));
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		label->set_font_size(24);
		add_child(label);

		add_child(make_click_box(tile));
	}
}

// World-space bounds of the shown tiles (each a 1 m cell around its position).
AABB MCTileLibrary::get_showcase_bounds() const {
	AABB bounds;
	bool first = true;
	Transform3D to_world = is_inside_tree() ? get_global_transform() : get_transform();
	for (const MCShowcaseTile &tile : showcase_tiles) {
		AABB cell(to_world.xform(tile.transform.origin) - Vector3(0.5f, 0.5f, 0.5f), Vector3(1, 1, 1));
		bounds = first ? cell : bounds.merge(cell);
		first = false;
	}
	return bounds;
}

} // namespace godot
