## Walks a point over any surface: floors, walls, ceilings, round outside corners and up into
## inside corners. No physics body: a few rays per frame, so it never slides, bounces or gets
## knocked about (crawler_ball.gd handles those times). This is the cultist's "up is the surface
## you're on" trick, done kinematically like the spider's body instead of with a rolling ball.
##
##   frame     origin on the surface under the body, Y = the surface normal, -Z = heading
##   move()    forward speed and turn rate in, each frame:
##               1. wall ahead (a ray from just above the surface, along the motion)?
##                  → climb onto it: inside corner (floor → wall → ceiling)
##               2. ground under the next spot (a ray down along -Y)?  → follow it
##               3. no ground: a ray from just past the edge back under it?
##                  → wrap round the outside corner (top of a box → its side)
##               4. nothing at all → returns false: the crawler falls (its ball takes over)
##   heading   moving onto a new surface turns the whole frame (heading AND velocity) by the
##             rotation that takes the old up to the new one, as if it rolled over the corner:
##             walking forwards into a wall, "forward" becomes "up the wall"; over an edge,
##             "down the face". Backwards it works the same way round: backing over an edge it
##             ends up facing up the face and keeps backing down it (it used to flip round and
##             get stuck going back and forth over the edge)
##
## Mobile: 1–3 rays per physics tick.
class_name SurfaceWalker
extends RefCounted

var speed := 1.6 ## m/s at full forward input
var accel := 6.0 ## how fast the speed follows the input (1/s)
var turn_rate := 3.0 ## rad/s at full turn input
var probe_height := 0.25 ## the wall ray runs this far above the surface
var wall_reach := 0.45 ## how far ahead a wall makes it climb
var steep := 0.6 ## a hit whose normal is less aligned than this with "up" counts as a wall
var world_mask := 1

var frame := Transform3D.IDENTITY
var velocity := Vector3.ZERO ## along the surface
var rays: CrawlerRays ## every ray goes through this log (debug drawing, counts)


## Put it on a surface: `normal` up, facing `forward` (projected onto the surface).
func reset(at: Vector3, normal: Vector3, forward: Vector3) -> void:
	frame = Transform3D(surface_basis(normal, forward), at)
	velocity = Vector3.ZERO


func up() -> Vector3:
	return frame.basis.y


func forward() -> Vector3:
	return -frame.basis.z


## One frame. `ahead` -1..1 (back..forward), `turn` -1..1 (right..left, about the surface normal).
## Returns false when there's no surface left under it.
func move(space: PhysicsDirectSpaceState3D, delta: float, ahead: float, turn: float) -> bool:
	if turn != 0.0:
		frame.basis = frame.basis.rotated(up(), turn * turn_rate * delta).orthonormalized()
	var wish := forward() * clampf(ahead, -1.0, 1.0) * speed
	velocity = velocity.lerp(wish, 1.0 - exp(-accel * delta))
	velocity -= up() * velocity.dot(up())
	var step := velocity * delta
	var along := step.normalized() if step.length() > 1e-5 else forward() * signf(ahead if ahead != 0.0 else 1.0)
	var eye := frame.origin + up() * probe_height
	var wall := _ray(space, eye, eye + along * (wall_reach + step.length()))
	if not wall.is_empty() and wall.normal.dot(up()) < steep:
		_change_surface(wall.position, wall.normal, along)
		return true
	var next := frame.origin + step
	var ground := _ray(space, next + up() * 0.4, next - up() * 0.6)
	if not ground.is_empty():
		frame = Transform3D(surface_basis(ground.normal, forward()), ground.position)
		return true
	for depth in [0.12, 0.3]:  # thin slabs (a roof) need the shallow one
		var under: Vector3 = next - up() * depth + along * 0.2
		var face := _ray(space, under, under - along * 0.7)
		if not face.is_empty():
			_change_surface(face.position, face.normal, along)
			return true
	return false


## Onto a new surface: turn heading and velocity by the shortest rotation from the old up to the
## new normal (rolling over the corner), whichever way it was moving.
func _change_surface(at: Vector3, normal: Vector3, _along: Vector3) -> void:
	var old_up := up()
	var turn_over := Quaternion(old_up, normal.normalized()) if old_up.dot(normal) > -0.999 else Quaternion(frame.basis.x, PI)
	frame = Transform3D(surface_basis(normal, turn_over * forward()), at)
	velocity = turn_over * velocity
	velocity -= normal * velocity.dot(normal)


## X right, Y = normal, -Z = forward (made square to the normal).
static func surface_basis(normal: Vector3, forward_hint: Vector3) -> Basis:
	var y := normal.normalized()
	var f := forward_hint - y * forward_hint.dot(y)
	if f.length() < 1e-4:
		f = y.cross(Vector3.RIGHT if absf(y.x) < 0.9 else Vector3.FORWARD)
	var back := -f.normalized()
	return Basis(y.cross(back).normalized(), y, back)


func _ray(space: PhysicsDirectSpaceState3D, from: Vector3, to: Vector3) -> Dictionary:
	return rays.cast(space, from, to, world_mask, CrawlerRays.Kind.WALKER)
