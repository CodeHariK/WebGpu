## The shape of a procedural island as a height function: height(x, z) in metres, water at y = 0.
## Grass plateau (off-centre) → steep earth bank → wide gentle beach → seabed.
## Every edge wobbles with noise sampled around a circle (so it wraps seamlessly), and the seed
## moves the plateau and changes every wobble. Same seed → same island.
## This is the ONE source of truth: the terrain mesh, the water depth and anything else
## (collision, prop placement, the sand shader's zones) all ask this function.
class_name IslandShape
extends RefCounted

var beach_radius := 10.5
var beach_top := 0.75       # beach height where it meets the bank
var beach_edge := -0.4      # height at the beach's outer edge (already under water)
var seabed := -2.2
var plateau_radius := 5.0
var plateau_height := 1.7
var bank_width := 0.8

var _plateau_centre := Vector2.ZERO
var _noise := FastNoiseLite.new()


func _init(island_seed: int = 1) -> void:
	_noise.seed = island_seed
	_noise.frequency = 1.0
	_noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	var rng := RandomNumberGenerator.new()
	rng.seed = island_seed
	var angle := rng.randf_range(-PI, PI)
	_plateau_centre = Vector2(cos(angle), sin(angle)) * rng.randf_range(1.5, 3.5)
	plateau_radius = rng.randf_range(3.8, 5.5)


## Height of the ground at world (x, z).
func height(x: float, z: float) -> float:
	var beach := _beach(x, z)
	var bank := _bank(x, z)
	var lumps := 0.05 * sin(x * 1.3) * cos(z * 1.1) if bank > 0.99 else 0.0
	return beach + (plateau_height - beach) * bank + lumps  # the bank rises from the beach to the grass


## Water depth at (x, z), 0 on dry land.
func depth(x: float, z: float) -> float:
	return maxf(-height(x, z), 0.0)


## Beach profile: gentle slope down to beach_edge at the (wobbly) beach radius, then down to the seabed.
func _beach(x: float, z: float) -> float:
	var radius := beach_radius * _wobble(atan2(z, x), 0.0, 0.1)
	var t := Vector2(x, z).length() / radius
	if t <= 1.0:
		return beach_top + (beach_edge - beach_top) * pow(t, 1.6)
	return beach_edge + (seabed - beach_edge) * pow(clampf((t - 1.0) / 0.5, 0.0, 1.0), 0.8)


## 0 on the beach, 1 on the plateau, a smooth S-curve across the bank.
func _bank(x: float, z: float) -> float:
	var p := Vector2(x, z) - _plateau_centre
	var edge := plateau_radius * _wobble(atan2(p.y, p.x), 50.0, 0.14)
	var t := clampf(1.0 - (p.length() - edge) / bank_width, 0.0, 1.0)
	return t * t * (3.0 - 2.0 * t)


## 1 ± amount, varying smoothly around the circle. Sampling the noise on a circle (cos, sin)
## makes angle −π and +π give the same value, so the outline closes without a seam.
func _wobble(angle: float, offset: float, amount: float) -> float:
	var on_circle := Vector2(cos(angle), sin(angle)) * 1.6
	return 1.0 + amount * _noise.get_noise_2d(on_circle.x + offset, on_circle.y)
