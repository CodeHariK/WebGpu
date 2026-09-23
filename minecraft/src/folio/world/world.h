#ifndef FOLIO_WORLD_H
#define FOLIO_WORLD_H

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class FolioFloor;
class FolioWaterSurface;
class FolioGrass;
class FolioFoliage;
class FolioTrees;
class FolioFlowers;
class FolioWindLines;
class FolioRainLines;
class FolioLeaves;
class FolioSnow;

/**
 * Folio port — FolioWorld  (folio `World/World.js`, environment slice)
 * -----------------------------------------------
 * Composition root for the environment: builds Floor + WaterSurface + Grass and
 * scatters Bushes (FolioFoliage) + Trees (FolioTrees) across the grassy terrain,
 * reading the shared terrain data map (G = grass coverage) on the CPU. Add one
 * FolioWorld to a scene alongside FolioGame and the full island renders.
 *
 * The build is deferred one frame so FolioGame has finished booting (globals,
 * terrain data, view) before the environment reads them.
 *
 * Deferred vs folio: physics colliders, the portfolio-area scenery, weather FX.
 */
class FolioWorld : public Node3D {
	GDCLASS(FolioWorld,
			Node3D)

private:
	int bush_count = 350;
	int tree_count = 60;
	int flower_count = 1200;
	double bush_min_grass = 0.6;
	double tree_min_grass = 0.7;
	double tree_min_spacing = 7.0;

	FolioFloor *floor = nullptr;
	FolioWaterSurface *water = nullptr;
	FolioGrass *grass = nullptr;
	FolioFoliage *bushes = nullptr;
	FolioTrees *trees = nullptr;
	FolioFlowers *flowers = nullptr;
	FolioWindLines *wind_lines = nullptr;
	FolioRainLines *rain_lines = nullptr;
	FolioLeaves *leaves = nullptr;
	FolioSnow *snow = nullptr;

	bool built = false;
	void _build();

protected:
	static void _bind_methods();

public:
	FolioWorld();
	~FolioWorld();

	void _ready() override;
};

} // namespace godot

#endif // FOLIO_WORLD_H
