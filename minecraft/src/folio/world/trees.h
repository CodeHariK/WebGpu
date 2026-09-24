#ifndef FOLIO_TREES_H
#define FOLIO_TREES_H

#include "foliage.h"
#include "instanced_group.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

/**
 * Folio port — FolioTrees  (folio `World/Trees.js`, stylized)
 * -----------------------------------------------
 * A tree = an instanced trunk body + a FolioFoliage crown on top. Composes the
 * two scatter helpers: FolioInstancedGroup for the trunks (a tapered cylinder,
 * folio-lit) and FolioFoliage for the leaf crowns (offset up + scaled). folio
 * loads a tree GLB (treeBody + treeLeaves); we build a stylized trunk instead so
 * no content model is needed. `scatter(transforms)` plants a tree at each.
 */
class FolioTrees : public Node3D {
	GDCLASS(FolioTrees,
			Node3D)

private:
	Color trunk_color = Color(0.30f, 0.20f, 0.12f);
	Color crown_color_a = Color(0.42f, 0.50f, 0.22f); // lightened shadow side
	Color crown_color_b = Color(0.62f, 0.70f, 0.30f);
	double trunk_height = 2.6;
	double trunk_radius = 0.32;
	double crown_scale = 2.4;
	bool collide = true; // add static trunk colliders

	Ref<Mesh> trunk_mesh;
	FolioInstancedGroup *trunks = nullptr;
	FolioFoliage *crowns = nullptr;

	void _build_trunk_mesh();

protected:
	static void _bind_methods();

public:
	FolioTrees();
	~FolioTrees();

	// Set the crown (leaf) colours before scatter() so per-type trees (birch/oak/
	// cherry) get folio's distinct palettes.
	void set_crown_colors(const Color &p_a, const Color &p_b);

	// Plant a tree (trunk + crown) at each transform.
	void scatter(const TypedArray<Transform3D> &p_transforms);
};

} // namespace godot

#endif // FOLIO_TREES_H
