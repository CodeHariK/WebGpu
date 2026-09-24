#include "world.h"

#include "floor.h"
#include "flowers.h"
#include "foliage.h"
#include "grass.h"
#include "trees.h"
#include "rain_lines.h"
#include "leaves.h"
#include "snow.h"
#include "wind_lines.h"
#include "water_surface.h"

#include "../game.h"
#include "../quality.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

FolioWorld::FolioWorld() {}

FolioWorld::~FolioWorld() {}

void FolioWorld::_ready() {
	// Defer one frame so FolioGame has booted (globals / terrain data / view).
	callable_mp(this, &FolioWorld::_build).call_deferred();
}

// Read the per-instance transforms authored in a folio *References glb: load the
// imported scene, take each top-level child's transform, and free the copy. These
// are folio's exact placements for bushes / trees / flowers.
static TypedArray<Transform3D> read_ref_transforms(const String &p_path) {
	TypedArray<Transform3D> out;
	Ref<PackedScene> scn = ResourceLoader::get_singleton()->load(p_path);
	if (scn.is_null()) {
		return out;
	}
	Node *inst = scn->instantiate();
	if (!inst) {
		return out;
	}
	const int n = inst->get_child_count();
	for (int i = 0; i < n; i++) {
		Node3D *c = Object::cast_to<Node3D>(inst->get_child(i));
		if (c) {
			out.push_back(c->get_transform());
		}
	}
	memdelete(inst);
	return out;
}

void FolioWorld::_build() {
	if (built) {
		return;
	}
	built = true;

	// Ground + water + grass.
	floor = memnew(FolioFloor);
	floor->set_name("Floor");
	add_child(floor);

	water = memnew(FolioWaterSurface);
	water->set_name("Water");
	add_child(water);

	grass = memnew(FolioGrass);
	grass->set_name("Grass");
	add_child(grass);

	bushes = memnew(FolioFoliage);
	bushes->set_name("Bushes");
	add_child(bushes);

	// Atmospheric wind streaks (pool of gust ribbons over the world).
	wind_lines = memnew(FolioWindLines);
	wind_lines->set_name("WindLines");
	add_child(wind_lines);

	// Rain / snow line-quads (weather-driven; snow when cold).
	rain_lines = memnew(FolioRainLines);
	rain_lines->set_name("RainLines");
	add_child(rain_lines);

	leaves = memnew(FolioLeaves);
	leaves->set_name("Leaves");
	add_child(leaves);

	snow = memnew(FolioSnow);
	snow->set_name("Snow");
	add_child(snow);

	// --- Environment scatter at folio's AUTHORED positions (the *References glbs).
	// Each reference glb holds one empty per instance; we plant our billboard
	// bushes / trunk+crown trees / flower tufts at those exact transforms
	// (replacing the earlier procedural RNG scatter).

	// Bushes: folio Bushes = Foliage at bushesReferences.
	bushes->scatter(read_ref_transforms("res://assets/folio/refs/bushesReferences.glb"));

	// Trees: one FolioTrees per type, each with folio's crown palette
	// (birch autumn-orange, oak yellow-green, cherry pink).
	struct TreeType {
		const char *name;
		const char *path;
		const char *col_a;
		const char *col_b;
	};
	const TreeType tree_types[] = {
		{ "BirchTrees", "res://assets/folio/refs/birchTreesReferences.glb", "ff4f2b", "ff903f" },
		{ "OakTrees", "res://assets/folio/refs/oakTreesReferences.glb", "b4b536", "d8cf3b" },
		{ "CherryTrees", "res://assets/folio/refs/cherryTreesReferences.glb", "ff6d6d", "ff9990" },
	};
	for (const TreeType &tt : tree_types) {
		FolioTrees *t = memnew(FolioTrees);
		t->set_name(tt.name);
		t->set_crown_colors(Color::html(tt.col_a), Color::html(tt.col_b));
		add_child(t);
		t->scatter(read_ref_transforms(tt.path));
	}

	// Flowers: folio Flowers = plane clusters at flowersReferences.
	flowers = memnew(FolioFlowers);
	flowers->set_name("Flowers");
	add_child(flowers);
	flowers->scatter(read_ref_transforms("res://assets/folio/refs/flowersReferences.glb"));
}

void FolioWorld::_bind_methods() {}
