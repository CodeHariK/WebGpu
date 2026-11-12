class_name GrassMaps
extends RefCounted
## The terrain as two textures the grass shaders read, so every blade finds its ground, its
## density and its colour on the GPU: the CPU never touches a blade.
##   height   metres, one texel a metre over the whole terrain (filtered: between texels it
##            blends the way the terrain mesh does)
##   mask     r: how densely grass grows (thin on steep slopes, thinner in patches)
##            g: how dry it is (orange where TerrainHeight says woods, green in the clearings)
## Texel i is at world -SIZE / 2 + i metres; a shader reads world (x, z) at
## (x, z + SIZE / 2 + 0.5) / TEXELS.

const TEXELS := 513 ## TerrainHeight.SIZE metres, a texel a metre, both edges included


## Every texel's height, row by row (z).
static func heights() -> PackedFloat32Array:
	var half := TerrainHeight.SIZE * 0.5
	var data := PackedFloat32Array()
	data.resize(TEXELS * TEXELS)
	for z in TEXELS:
		for x in TEXELS:
			data[z * TEXELS + x] = TerrainHeight.height(x - half, z - half)
	return data


static func height_texture(p_heights: PackedFloat32Array) -> ImageTexture:
	var image := Image.create_from_data(TEXELS, TEXELS, false, Image.FORMAT_RF, p_heights.to_byte_array())
	return ImageTexture.create_from_image(image)


## Density and dryness; the slopes come from the heights (no more noise to work out).
static func mask_texture(p_heights: PackedFloat32Array) -> ImageTexture:
	var patches := FastNoiseLite.new()
	patches.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	patches.seed = 17
	patches.frequency = 0.05
	var half := TerrainHeight.SIZE * 0.5
	var bytes := PackedByteArray()
	bytes.resize(TEXELS * TEXELS * 4)
	for z in TEXELS:
		for x in TEXELS:
			var i := (z * TEXELS + x) * 4
			var flat := smoothstep(0.55, 0.8, _up(p_heights, x, z))
			var patchy := 0.55 + 0.45 * smoothstep(-0.4, 0.3, patches.get_noise_2d(x, z))
			bytes[i] = int(flat * patchy * 255.0)
			bytes[i + 1] = int(TerrainHeight.forest(x - half, z - half) * 255.0)
			bytes[i + 3] = 255
	return ImageTexture.create_from_image(Image.create_from_data(TEXELS, TEXELS, false, Image.FORMAT_RGBA8, bytes))


## How much the ground at texel (x, z) faces up: 1 flat .. 0 a wall.
static func _up(p_heights: PackedFloat32Array, p_x: int, p_z: int) -> float:
	var left := p_heights[p_z * TEXELS + maxi(p_x - 1, 0)]
	var right := p_heights[p_z * TEXELS + mini(p_x + 1, TEXELS - 1)]
	var back := p_heights[maxi(p_z - 1, 0) * TEXELS + p_x]
	var front := p_heights[mini(p_z + 1, TEXELS - 1) * TEXELS + p_x]
	return Vector3(left - right, 2.0, back - front).normalized().y
