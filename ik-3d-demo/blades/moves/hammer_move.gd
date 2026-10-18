## What the hammer moves share (weapon mode "hammers", M): the heads are heavy, so they hit harder
## and freeze the game longer, their chains run taut from the hand to the head (a weight on a
## chain doesn't lie on its path), and their paths are built from arcs round the player:
## helix() — a circle that can grow, rise or sink as it goes round — and their impacts throw the
## enemies round them away: blast(). A head that lands doesn't stop dead: rebound() drives it on
## into what it hit, bounces it back up off it, and lets it drop back down to rest there.
class_name HammerMove
extends BladeMove


const DRIVE := 0.25 ## m a head drives on into what it hit …
const DRIVE_TIME := 0.05 ## … in this long
const REBOUND := 0.9 ## m it bounces back off it …
const REBOUND_TIME := 0.2 ## … in this long …
const FALL_TIME := 0.22 ## … then drops back down (faster and faster) and lies there


func hitstop_scale() -> float:
	return 1.8


func chain_on_path(_blade: Blade) -> bool:
	return false


## Arcs round `center` from angle a0 to a1 (radians, in the frame right / forward: 0 = your right,
## π/2 = in front), the radius going r0 → r1 and the height h0 → h1 on the way, as cubic Beziers of
## at most a quarter turn each (their handles keep each piece round).
func helix(
		center: Vector3,
		a0: float,
		a1: float,
		r0: float,
		r1: float,
		h0: float,
		h1: float
) -> PackedVector3Array:
	var right := demo.right()
	var forward := demo.facing
	var pieces := maxi(ceili(absf(a1 - a0) / (PI * 0.5)), 1)
	var path := PackedVector3Array()
	for k in pieces:
		var u0 := float(k) / pieces
		var u1 := float(k + 1) / pieces
		var b0 := lerpf(a0, a1, u0)
		var b1 := lerpf(a0, a1, u1)
		var p0 := _ring_point(center, right, forward, b0, lerpf(r0, r1, u0), lerpf(h0, h1, u0))
		var p3 := _ring_point(center, right, forward, b1, lerpf(r0, r1, u1), lerpf(h0, h1, u1))
		var handle := 4.0 / 3.0 * tan((b1 - b0) * 0.25) # a round arc's handle length (× its radius)
		var t0 := (-right * sin(b0) + forward * cos(b0)) * handle * lerpf(r0, r1, u0)
		var t1 := (-right * sin(b1) + forward * cos(b1)) * handle * lerpf(r0, r1, u1)
		path.append_array([p0, p0 + t0, p3 - t1, p3])
	return path


## A lead-in from where the head hangs now onto the start of `path`, arriving along its direction.
func lead_in(
		blade: Blade,
		path: PackedVector3Array
) -> PackedVector3Array:
	var from := blade.tip
	var to := path[0]
	var arrive := (path[1] - path[0]).normalized()
	var reach := from.distance_to(to)
	return PackedVector3Array([from, from + Vector3.UP * reach * 0.4, to - arrive * reach * 0.4, to])


## A head has just landed: it drives on along the way it was going, bounces back up off the
## surface (the way it came, mirrored off it: `normal`), and drops back to rest by the dent,
## where it stays until `rest` seconds after it landed — all one HOLD, then the move goes on.
func rebound(
		blade: Blade,
		rest: float,
		normal := Vector3.UP
) -> void:
	var tip := blade.tip
	var going := blade.path[-1] - blade.path[-2] if blade.path.size() >= 2 else Vector3.DOWN
	going = going.normalized() if going.length() > 1e-3 else Vector3.DOWN
	var bounced := going - normal * 2.0 * going.dot(normal) # mirrored off the surface
	var dent := tip + going * DRIVE
	var top := tip + (bounced.normalized() + normal) * 0.5 * REBOUND
	var lie := tip + Vector3(bounced.x, 0.0, bounced.z) * 0.25 # it comes to rest just by the dent
	var path := PackedVector3Array([tip, tip + going * DRIVE * 0.5, dent, dent])
	path.append_array([dent, dent + bounced * REBOUND * 0.6, top + normal * 0.15, top])
	path.append_array([top, top + normal * 0.1, lie + normal * 0.3, lie])
	path.append_array([lie, lie, lie, lie]) # and lies there
	var still := maxf(rest - DRIVE_TIME - REBOUND_TIME - FALL_TIME, 0.05)
	blade.start_segments(Blade.HOLD, path, PackedFloat32Array([DRIVE_TIME, REBOUND_TIME, FALL_TIME, still]), ["out", "out", "accelerate", "linear"])


## An impact at `at`: every enemy within `radius` is thrown away from it (the closer, the harder:
## `power` m/s at the centre) and up, the ones within `crush` flattened; sparks, a big shake, a
## long freeze.
func blast(
		at: Vector3,
		radius: float,
		power: float,
		crush: float
) -> void:
	for enemy in demo.enemies:
		var out := enemy.position - at
		out.y = 0.0
		var distance := out.length()
		if distance > radius or enemy.yanked:
			continue
		var strength := 1.0 - distance / radius
		var away := out.normalized() if distance > 0.1 else demo.facing
		enemy.velocity = away * power * (0.4 + 0.6 * strength) + Vector3.UP * (4.0 + 7.0 * strength)
		enemy.flash = 1.0
		if distance < crush:
			enemy.squash = 1.0
	demo.fire.spark(at)
	demo.impacts.impact(Vector3(at.x, 0.05, at.z), 34, 16, 9.0, Color(0.25, 0.2, 0.15), radius * 0.3) # rubble, sparks, a scorch
	demo.shake = maxf(demo.shake, 0.65)
	demo.hitstop = demo.hitstop_time * 2.5


static func _ring_point(
		center: Vector3,
		right: Vector3,
		forward: Vector3,
		angle: float,
		radius: float,
		height: float
) -> Vector3:
	return Vector3(center.x, height, center.z) + (right * cos(angle) + forward * sin(angle)) * radius
