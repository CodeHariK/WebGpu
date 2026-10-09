## A closed, wobbly outline around a centre: point(angle) → position on the ground plane (x, z).
## The wobble is noise sampled on a circle, so angle −π and +π give the same point (no seam).
class_name IslandOutline
extends RefCounted

var centre := Vector2.ZERO
var radius := 10.0
var wobble := 0.1          # ± fraction of the radius
var _noise := FastNoiseLite.new()
var _offset := 0.0


func _init(p_centre: Vector2, p_radius: float, p_wobble: float, noise_seed: int, offset: float) -> void:
	centre = p_centre
	radius = p_radius
	wobble = p_wobble
	_noise.seed = noise_seed
	_noise.frequency = 1.0
	_noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	_offset = offset


## Distance from the centre in direction `angle`.
func reach(angle: float) -> float:
	var on_circle := Vector2(cos(angle), sin(angle)) * 1.6
	return radius * (1.0 + wobble * _noise.get_noise_2d(on_circle.x + _offset, on_circle.y))


## The outline point in direction `angle`, pushed out (or in) by `extra` metres.
func point(angle: float, extra: float = 0.0) -> Vector2:
	return centre + Vector2(cos(angle), sin(angle)) * (reach(angle) + extra)
