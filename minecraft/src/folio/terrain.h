#ifndef FOLIO_TERRAIN_H
#define FOLIO_TERRAIN_H

#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {

/**
 * Folio port — Terrain (data)  (folio `Game/Terrain.js`, registered `FolioTerrain`)
 * --------------------------------------------------------------------------------
 * Provides the ground-bounce COLOUR the base material samples (its `terrainNode` +
 * `colorNode`). Not the geometry — a small data map + a height gradient that give
 * each world XZ a biome colour used for ambient light bounce.
 *
 * Shader logic the base .gdshader reproduces:
 *   uv   = worldXZ / subdivision / 1.5 + 0.5
 *   data = texture(terrain_data, uv)            // G = grass mask, B = height
 *   base = texture(gradient, vec2(0, 1 - data.b))  // orange→teal→deep-blue
 *   bounceColor = mix(base, grass_color, data.g)
 *
 * Published globals:
 *   folio_terrain_gradient  (SAMPLER2D)  1x16 height ramp (source_color)
 *   folio_terrain_data      (SAMPLER2D)  biome data map (default: all-grass)
 *   folio_terrain_grass_color (COLOR)
 *   folio_terrain_subdivision / _size (FLOAT)
 *
 * Deferred: the real data map is folio CONTENT (terrain/terrain.png) and wheel
 * `tracks` erase grass — both un-ported. Default data = 1x1 grass (neutral bounce);
 * `set_terrain_data()` plugs the game's own biome map when ready.
 */
class FolioTerrain : public Node {
	GDCLASS(FolioTerrain,
			Node)

private:
	int subdivision = 128;
	double size = 192.0;
	Color grass_color = Color(0.72f, 0.71f, 0.18f); // #b8b62e

	Ref<ImageTexture> gradient;
	Ref<Texture2D> data_texture; // default all-grass, overridable

	bool globals_registered = false;

	void _generate_gradient();
	void _generate_default_data();
	void _register_globals();
	void _push();

protected:
	static void _bind_methods();

public:
	FolioTerrain();
	~FolioTerrain();

	void _ready() override;

	// Plug the game's own biome data map (G = grass, B = height).
	void set_terrain_data(const Ref<Texture2D> &p_texture);
	void set_grass_color(const Color &p_color);

	Ref<ImageTexture> get_gradient() const { return gradient; }
	Ref<Texture2D> get_terrain_data() const { return data_texture; }
	Color get_grass_color() const { return grass_color; }
};

} // namespace godot

#endif // FOLIO_TERRAIN_H
