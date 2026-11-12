class_name TerrainHeight
extends RefCounted
## The ground's shape and where things grow, from noise: one answer for any (x, z). The terrain
## chunks and the props both ask here, so every prop sits exactly on the ground.

const SIZE := 512.0 ## metres on a side, centred on the origin
const HILL_HEIGHT := 28.0 ## metres up or down from 0 the hills reach
const DETAIL_HEIGHT := 1.5 ## metres of small bumps on top

static var _hills: FastNoiseLite
static var _detail: FastNoiseLite
static var _forest: FastNoiseLite


## Metres above 0 at (x, z).
static func height(x: float, z: float) -> float:
	_make_noises()
	return _hills.get_noise_2d(x, z) * HILL_HEIGHT + _detail.get_noise_2d(x, z) * DETAIL_HEIGHT


## Which way the ground faces at (x, z) (from the slope between nearby heights).
static func normal(x: float, z: float) -> Vector3:
	var step := 0.5
	var dx := height(x + step, z) - height(x - step, z)
	var dz := height(x, z + step) - height(x, z - step)
	return Vector3(-dx, 2.0 * step, -dz).normalized()


## 0 in a clearing .. 1 deep in a wood: big patches.
static func forest(x: float, z: float) -> float:
	_make_noises()
	return smoothstep(-0.15, 0.25, _forest.get_noise_2d(x, z))


static func _make_noises() -> void:
	if _hills:
		return
	_hills = FastNoiseLite.new()
	_hills.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	_hills.seed = 3
	_hills.frequency = 0.006
	_hills.fractal_octaves = 3
	_detail = FastNoiseLite.new()
	_detail.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	_detail.seed = 5
	_detail.frequency = 0.08
	_detail.fractal_octaves = 1
	_forest = FastNoiseLite.new()
	_forest.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	_forest.seed = 9
	_forest.frequency = 0.012
	_forest.fractal_octaves = 2
