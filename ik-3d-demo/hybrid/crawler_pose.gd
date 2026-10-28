## Where the crawler's body is and how it's tilted, given where its hands and feet are. The
## spider's body rules plus its springs; the cultist's step dip.
##
##   fit     (walking) each planted limb says how high the body should be over it; a plane fitted
##           through those heights (least squares over the rest spots) gives the body's height,
##           pitch and roll on the surface frame. Uneven ground, a step, a limb up on a box: the
##           body tilts by itself. (Spider._fit_body_plane.)
##   dip     a little lower for each limb mid-step, so the walk bobs (cultist torso dip)
##   tumble  (physics) the body sits on the ball, upright, facing the way it rolls
##   springs the posed body follows its target through second-order springs (t3ssel8r, the
##           spider's SecondOrder): position lags and settles with a little weight; facing
##           (forward and up separately) overshoots a touch on turns and surface changes, which
##           also hides the walker's snaps from floor to wall. kick() shoves it (a flinch).
class_name CrawlerPose
extends RefCounted

var dip_per_step := 0.035 ## m lower per limb mid-step
var max_tilt := 0.6 ## radians, pitch and roll each

var transform := Transform3D.IDENTITY ## the posed body (chest centre)

var _position: SecondOrder
var _up: SecondOrder
var _forward: SecondOrder


func _init(start: Transform3D) -> void:
	_position = SecondOrder.new(2.4, 0.6, 0.4, start.origin)
	_up = SecondOrder.new(2.6, 0.75, 0.0, start.basis.y)
	_forward = SecondOrder.new(2.2, 0.6, -0.15, -start.basis.z)
	transform = start


## Jump straight to `to` (spawning, teleports).
func reset(to: Transform3D) -> void:
	_position.reset(to.origin)
	_up.reset(to.basis.y)
	_forward.reset(-to.basis.z)
	transform = to


## A shove: the body flinches by `velocity` (m/s) and springs back.
func kick(velocity: Vector3) -> void:
	_position.kick(velocity)


func update(delta: float, target: Transform3D) -> void:
	var at := _position.update(delta, target.origin)
	var up := _up.update(delta, target.basis.y)
	var forward := _forward.update(delta, -target.basis.z)
	if up.length() < 1e-3:
		up = target.basis.y
	transform = Transform3D(SurfaceWalker.surface_basis(up.normalized(), forward), at)


## Walking: the body over `frame` (the walker's surface frame), fitted to the planted limbs.
func walk_target(frame: Transform3D, steps: Array[CrawlerStep], stepping: int) -> Transform3D:
	var fit := _fit(frame, steps)
	var height := CrawlerLayout.RIDE_HEIGHT + fit.x - dip_per_step * stepping
	var tilt := Basis.from_euler(Vector3(clampf(fit.y, -max_tilt, max_tilt), 0.0, clampf(fit.z, -max_tilt, max_tilt)))
	return Transform3D(frame.basis * tilt, frame * Vector3(0.0, height, 0.0))


## Tumbling: upright on the ball (or along the surface it's rolling on), facing `heading`.
func tumble_target(centre: Vector3, up: Vector3, heading: Vector3) -> Transform3D:
	return Transform3D(SurfaceWalker.surface_basis(up, heading), centre)


## Least-squares plane h = a + b·x + c·z through each grounded limb's height above the surface
## frame, at its rest spot (x, z). Returns (height, pitch, roll). Forward is -Z: a higher front
## (h falls as z grows) is nose up.
func _fit(frame: Transform3D, steps: Array[CrawlerStep]) -> Vector3:
	var inverse := frame.affine_inverse()
	var samples: Array[Vector3] = []
	for limb in steps.size():
		if steps[limb].airborne:
			continue
		var local := inverse * steps[limb].position
		var rest := CrawlerLayout.rest(limb)
		samples.append(Vector3(rest.x, rest.z, local.y))
	if samples.is_empty():
		return Vector3.ZERO
	var mean := Vector3.ZERO
	for sample in samples:
		mean += sample / float(samples.size())
	var xx := 0.0
	var zz := 0.0
	var xh := 0.0
	var zh := 0.0
	for sample in samples:
		var d := sample - mean
		xx += d.x * d.x
		zz += d.y * d.y
		xh += d.x * d.z
		zh += d.y * d.z
	var slope_x := xh / xx if xx > 0.0001 else 0.0
	var slope_z := zh / zz if zz > 0.0001 else 0.0
	return Vector3(mean.z, atan(-slope_z), atan(slope_x))
