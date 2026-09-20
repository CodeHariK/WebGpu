#include "world.h"

#include "floor.h"
#include "flowers.h"
#include "foliage.h"
#include "grass.h"
#include "trees.h"
#include "rain_lines.h"
#include "wind_lines.h"
#include "water_surface.h"

#include "../game.h"
#include "../quality.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

FolioWorld::FolioWorld() {}

FolioWorld::~FolioWorld() {}

void FolioWorld::_ready() {
	// Defer one frame so FolioGame has booted (globals / terrain data / view).
	callable_mp(this, &FolioWorld::_build).call_deferred();
}

// Sample the terrain data map's G (grass coverage) at a world XZ (folio uv).
static float grass_at(
		const Ref<Image> &img,
		double x,
		double z
) {
	if (img.is_null()) {
		return 0.0f;
	}
	const double u = x / 128.0 / 1.5 + 0.5;
	const double v = z / 128.0 / 1.5 + 0.5;
	if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) {
		return 0.0f;
	}
	const int px = (int)CLAMP(u * (img->get_width() - 1), 0, img->get_width() - 1);
	const int py = (int)CLAMP(v * (img->get_height() - 1), 0, img->get_height() - 1);
	return img->get_pixel(px, py).g;
}

void FolioWorld::_build() {
	if (built) {
		return;
	}
	built = true;

	// Per-quality scatter counts (low tier -> fewer instances).
	int bushes_n = bush_count;
	int trees_n = tree_count;
	int flowers_n = flower_count;
	{
		FolioGame *g = FolioGame::get_singleton();
		if (g && g->get_quality() && g->get_quality()->get_level() >= 1) {
			bushes_n = (int)(bush_count * 0.5);
			trees_n = (int)(tree_count * 0.5);
			flowers_n = (int)(flower_count * 0.5);
		}
	}

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

	trees = memnew(FolioTrees);
	trees->set_name("Trees");
	add_child(trees);

	// Atmospheric wind streaks (pool of gust ribbons over the world).
	wind_lines = memnew(FolioWindLines);
	wind_lines->set_name("WindLines");
	add_child(wind_lines);

	// Rain / snow line-quads (weather-driven; snow when cold).
	rain_lines = memnew(FolioRainLines);
	rain_lines->set_name("RainLines");
	add_child(rain_lines);

	// Biome scatter from the terrain data map.
	Ref<Image> data = Image::load_from_file("res://material/textures/folio/terrain_data.png");

	Ref<RandomNumberGenerator> rng;
	rng.instantiate();
	rng->set_seed(20250919);

	// Bushes.
	TypedArray<Transform3D> bush_t;
	int tries = 0;
	while (bush_t.size() < bushes_n && tries < bushes_n * 20) {
		tries++;
		const double x = rng->randf_range(-88.0, 88.0);
		const double z = rng->randf_range(-88.0, 88.0);
		if (grass_at(data, x, z) < bush_min_grass) {
			continue;
		}
		const double s = rng->randf_range(0.7, 1.5);
		Basis b(Vector3(0, 1, 0), rng->randf() * (float)Math::TAU);
		b = b.scaled(Vector3(s, s * rng->randf_range(0.8, 1.3), s));
		bush_t.push_back(Transform3D(b, Vector3(x, 0.35 * s, z)));
	}
	bushes->scatter(bush_t);

	// Trees (min-spaced).
	TypedArray<Transform3D> tree_t;
	PackedVector2Array placed;
	tries = 0;
	while (tree_t.size() < trees_n && tries < trees_n * 100) {
		tries++;
		const double x = rng->randf_range(-85.0, 85.0);
		const double z = rng->randf_range(-85.0, 85.0);
		if (grass_at(data, x, z) < tree_min_grass) {
			continue;
		}
		bool too_close = false;
		for (int i = 0; i < placed.size(); i++) {
			if (placed[i].distance_to(Vector2(x, z)) < tree_min_spacing) {
				too_close = true;
				break;
			}
		}
		if (too_close) {
			continue;
		}
		placed.push_back(Vector2(x, z));
		const double s = rng->randf_range(0.9, 1.4);
		Basis b(Vector3(0, 1, 0), rng->randf() * (float)Math::TAU);
		b = b.scaled(Vector3(s, s, s));
		tree_t.push_back(Transform3D(b, Vector3(x, 0.0, z)));
	}
	trees->scatter(tree_t);

	// Flowers (dense tiny tufts on grass).
	flowers = memnew(FolioFlowers);
	flowers->set_name("Flowers");
	add_child(flowers);
	TypedArray<Transform3D> flower_t;
	tries = 0;
	while (flower_t.size() < flowers_n && tries < flowers_n * 12) {
		tries++;
		const double x = rng->randf_range(-88.0, 88.0);
		const double z = rng->randf_range(-88.0, 88.0);
		if (grass_at(data, x, z) < bush_min_grass) {
			continue;
		}
		const double s = rng->randf_range(0.4, 0.85);
		Basis b(Vector3(0, 1, 0), rng->randf() * (float)Math::TAU);
		b = b.scaled(Vector3(s, s, s));
		flower_t.push_back(Transform3D(b, Vector3(x, 0.0, z)));
	}
	flowers->scatter(flower_t);
}

void FolioWorld::_bind_methods() {}
