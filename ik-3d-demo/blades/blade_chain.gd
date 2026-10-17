## The chain between a hand and its blade — no physics, no wobble. A metal chain under load is
## taut, so it only ever takes one of two shapes (the demo resamples either into evenly spaced links):
##   on the path  while the blade flies out or reels back, the chain lies ON the blade's path:
##                the travelled part (out) or the part still ahead (back), over every segment —
##                wind-up included. See on_path().
##   taut         when the blade swings round you (sweep, spin, slice) or holds a target, a
##                straight line bent only slightly behind the blade's motion. See taut().
class_name BladeChain
extends RefCounted


## Links along `path` from e0 to e1, starting at the hand. `from_start`: the hand end is the path's
## start (flying out: e0 = 0) or its end (reeling in: e1 = 1). The hand may have moved since the
## path was made: that offset fades from the hand to the blade, so the blade end stays put.
static func on_path(
		hand: Vector3,
		path: PackedVector3Array,
		e0: float,
		e1: float,
		from_start: bool,
		count: int
) -> PackedVector3Array:
	var points := Bezier.path_even_points(path, e0, e1, count)
	if not from_start:
		points.reverse()
	var drift := hand - points[0]
	for k in count:
		points[k] += drift * (1.0 - float(k) / maxf(count - 1, 1))
	return points


## The 4 control points of a taut chain (the debug view draws them).
static func taut(
		hand: Vector3,
		blade: Vector3,
		trail: Vector3
) -> PackedVector3Array:
	var third := (blade - hand) / 3.0
	var bend := trail * hand.distance_to(blade) * 0.12 # a small lag, never a loop
	return PackedVector3Array([hand, hand + third + bend * 0.5, blade - third + bend, blade])
