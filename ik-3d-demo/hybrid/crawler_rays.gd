## Every ray the crawler casts goes through here, so the debug view can draw all of them and the
## HUD can count them. One shared log per crawler; each part tags its rays with what it was for:
##   WALKER    surface_walker.gd: wall ahead, ground under the next spot, edge wrap (every frame)
##   HOME      crawler_gait.gd: one per limb, its resting spot (every frame while walking)
##   FOOTHOLD  crawler_foothold.gd: the candidate search (only when a limb starts a step)
##   BALL      crawler_ball.gd: ground under the ball (every frame while tumbling)
##   PLACE     crawler.gd: finding the ground when it's put down (spawn, reset)
## Every-frame rays (walker, home, ball) are only drawn for the frame they're cast in; one-off
## rays (a foothold search, placing) are kept for `linger` seconds so they stay visible long
## enough to see.
class_name CrawlerRays
extends RefCounted

enum Kind { WALKER, HOME, FOOTHOLD, BALL, PLACE }
const NAMES := ["walker", "home", "foothold", "ball", "place"]

var linger := 0.5 ## seconds a one-off ray (FOOTHOLD, PLACE) stays in the log
const EVERY_FRAME := [Kind.WALKER, Kind.HOME, Kind.BALL] ## these last one frame only
var recent: Array = [] ## [from, end (hit point or ray end), hit?, Kind, time (s), physics frame]
var this_frame := PackedInt32Array([0, 0, 0, 0, 0]) ## per kind, the current physics frame
var per_second := PackedFloat32Array([0, 0, 0, 0, 0]) ## per kind, smoothed

var _counting := PackedInt32Array([0, 0, 0, 0, 0])


func cast(space: PhysicsDirectSpaceState3D, from: Vector3, to: Vector3, mask: int, kind: Kind, exclude: Array[RID] = []) -> Dictionary:
	var query := PhysicsRayQueryParameters3D.create(from, to, mask, exclude)
	var hit := space.intersect_ray(query)
	recent.append([from, hit.position if not hit.is_empty() else to, not hit.is_empty(), kind, _now(), Engine.get_physics_frames()])
	_counting[kind] += 1
	return hit


## Drop every-frame rays from earlier frames, and one-off rays older than `linger`.
func prune() -> void:
	var cutoff := _now() - linger
	var frame := Engine.get_physics_frames()
	var kept: Array = []
	for ray: Array in recent:
		var stale: bool = ray[5] < frame if ray[3] in EVERY_FRAME else ray[4] < cutoff
		if not stale:
			kept.append(ray)
	recent = kept


func total_this_frame() -> int:
	var total := 0
	for count in this_frame:
		total += count
	return total


## Call at the start of each physics frame: last frame's counts become this_frame (and feed the
## per-second average).
func begin_frame() -> void:
	var rate := float(Engine.physics_ticks_per_second)
	for kind in _counting.size():
		this_frame[kind] = _counting[kind]
		per_second[kind] = lerpf(per_second[kind], _counting[kind] * rate, 0.05)
		_counting[kind] = 0


static func _now() -> float:
	return Time.get_ticks_msec() / 1000.0
