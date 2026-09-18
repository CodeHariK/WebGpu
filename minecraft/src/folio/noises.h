#ifndef FOLIO_NOISES_H
#define FOLIO_NOISES_H

#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/node.hpp>

namespace godot {

/**
 * Folio port — Noises  (folio `Game/Noises.js`, registered `FolioNoises`)
 * ----------------------------------------------------------------------
 * The shared, tileable noise textures every material samples: `voronoi` (25+
 * uses), `perlin`, `hash`. Folio renders them once via TSL into 128x128 render
 * targets at boot.
 *
 * Port: generated on the CPU at _ready from faithful ports of the same hash /
 * voronoi / perlin functions (they are pure, periodic math, so the textures tile
 * seamlessly), stored as float `ImageTexture`s, and published as GLOBAL sampler
 * uniforms the base .gdshader samples:
 *   folio_noise_voronoi  RGB = (minDist, edgeDist, cellHash)
 *   folio_noise_perlin   R   = perlin (remapped 0.1..0.9 -> 0..1)
 *   folio_noise_hash     R   = white-noise hash
 *
 * One-time CPU cost (~16k px x3), deterministic — no GPU render-target plumbing.
 */
class FolioNoises : public Node {
	GDCLASS(FolioNoises,
			Node)

private:
	int resolution = 128;
	Ref<ImageTexture> voronoi;
	Ref<ImageTexture> perlin;
	Ref<ImageTexture> hash_texture;

	bool globals_registered = false;

	void _generate();
	void _register_globals();

protected:
	static void _bind_methods();

public:
	FolioNoises();
	~FolioNoises();

	void _ready() override;

	Ref<ImageTexture> get_voronoi() const { return voronoi; }
	Ref<ImageTexture> get_perlin() const { return perlin; }
	Ref<ImageTexture> get_hash() const { return hash_texture; }
	int get_resolution() const { return resolution; }
};

} // namespace godot

#endif // FOLIO_NOISES_H
