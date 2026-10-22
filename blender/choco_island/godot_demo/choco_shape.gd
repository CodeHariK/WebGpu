## The island's outline: a circle of radius RADIUS bent by two slow sine waves (random phases), so
## every seed gets a different but always round-ish, convex-looking cake.
extends RefCounted

const RADIUS := 16.0
const SIDES := 28

var _waves: Array[Vector3] = []  # (frequency, amplitude, phase)


func _init(rng: RandomNumberGenerator) -> void:
	_waves.append(Vector3(2, rng.randf_range(0.03, 0.07), rng.randf_range(0, TAU)))
	_waves.append(Vector3(3, rng.randf_range(0.02, 0.05), rng.randf_range(0, TAU)))


## Distance from the centre to the frosting edge in direction `angle` (angle 0 = +X, PI/2 = +Z).
func radius_at(angle: float) -> float:
	var k := 1.0
	for w in _waves:
		k += w.y * sin(w.x * angle + w.z)
	return RADIUS * k


## The edge as SIDES points (x, z).
func outline() -> PackedVector2Array:
	var ring := PackedVector2Array()
	for i in SIDES:
		var a := TAU * i / SIDES
		ring.append(Vector2(cos(a), sin(a)) * radius_at(a))
	return ring


## True when (x, z) is on the island at least `margin` metres in from the edge.
func contains(p: Vector2, margin := 0.0) -> bool:
	return p.length() < radius_at(p.angle()) * 0.985 - margin
