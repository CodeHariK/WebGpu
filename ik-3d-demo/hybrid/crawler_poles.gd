## Where each elbow and knee points: the pole each limb's two-bone IK bends toward. With every
## option off it is exactly the old rule: halfway between shoulder/hip and hand/foot, plus the
## limb's fixed magnet offset in the body frame (CrawlerLayout.POLES, the cultist's). Each option
## below adds one behaviour on top and has its own switch, so they can be tried one at a time.
##
##   lift     while a limb steps, its pole rises `lift_amount` (a half sine over the step): the
##            elbow/knee visibly lifts and comes down with the hand/foot (hex spider's pole_lift)
##   sweep    the pole swings round the body's up axis by how far the hand/foot is ahead of or
##            behind its shoulder/hip, so the bend plane follows the stride instead of always
##            facing one way (the spider's knee yaw, "follow" style)
##   tuck     in the air (falling, tumbling, a gallop leap) or hanging, the pole blends toward a
##            folded direction (TUCK: elbows back and up, knees forward and up), so the limbs
##            fold like a jumping animal's instead of keeping their walking bend
##   surface  keeps each pole at least `clearance` off the surface its hand/foot is on (on its
##            own side), so elbows and knees don't fold into the wall in corners
##   hex      each new step picks a random corner of a hexagon (`hex_radius`) round the usual
##            pole and eases to it over the step: every step bends a little differently (hex
##            spider's pole hex)
##   noise    a slow smooth wobble (`noise_amount` m at `noise_rate` Hz): a living, twitchy bend
##   smooth   the final pole follows through a spring (SecondOrder), so switches between all of
##            the above never snap
##
## Called by crawler_rig.gd once per limb per frame. Cheap: no rays, a few vector ops.
class_name CrawlerPoles
extends RefCounted

## Folded pole directions (body frame, from the middle of the limb): elbows back and up, knees
## forward and up.
const TUCK := [Vector3(-0.35, 0.55, 0.6), Vector3(0.35, 0.55, 0.6), Vector3(-0.35, 0.55, -0.7), Vector3(0.35, 0.55, -0.7)]

var lift_on := false
var lift_amount := 0.35 ## m

var sweep_on := false
var sweep_amount := 1.2 ## radians of swing for a hand/foot a full reach ahead or behind

var tuck_on := false
var tuck_amount := 1.0 ## 0..1 how far toward TUCK

var surface_on := false
var clearance := 0.3 ## m

var hex_on := false
var hex_radius := 0.35 ## m

var noise_on := false
var noise_amount := 0.15 ## m
var noise_rate := 1.5 ## Hz

var smooth_on := false
var smooth_frequency := 4.0 ## Hz (SecondOrder f; damping 0.6)

var _hex_from: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO, Vector3.ZERO, Vector3.ZERO]
var _hex_to: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO, Vector3.ZERO, Vector3.ZERO]
var _was_stepping: Array[bool] = [false, false, false, false]
var _springs: Array = [null, null, null, null]
var _noise := FastNoiseLite.new()
var _time := 0.0


func _init() -> void:
	_noise.frequency = 1.0


## The pole for `limb` this frame. `root`/`target`: the limb's shoulder/hip and IK target;
## `body`: the posed body; `step`: its CrawlerStep; `leaping`: the gallop is mid-leap.
func pole(limb: int, root: Vector3, target: Vector3, body: Transform3D, step: CrawlerStep, delta: float, leaping: bool) -> Vector3:
	if limb == 0:
		_time += delta
	var offset: Vector3 = CrawlerLayout.POLES[limb]
	if hex_on:
		offset += _hex_offset(limb, step)
	if sweep_on:
		offset = offset.rotated(Vector3.UP, _sweep_angle(limb, target, body))
	var free := step.airborne or step.hanging or leaping
	if tuck_on and free:
		offset = offset.lerp(TUCK[limb], tuck_amount)
	var result := (root + target) * 0.5 + body.basis * offset
	if lift_on and step.stepping:
		result += body.basis.y * lift_amount * sin(PI * step.progress())
	if noise_on:
		result += body.basis * _wobble(limb)
	if surface_on and not free:
		var gap := (result - target).dot(step.normal)
		if gap < clearance:
			result += step.normal * (clearance - gap)
	if smooth_on:
		result = _smooth(limb, result, delta)
	else:
		_springs[limb] = null
	return result


# A new step picks a fresh hexagon corner; the offset eases from the old corner over the step.
func _hex_offset(limb: int, step: CrawlerStep) -> Vector3:
	if step.stepping and not _was_stepping[limb]:
		_hex_from[limb] = _hex_to[limb]
		var angle := randi_range(0, 5) * PI / 3.0
		_hex_to[limb] = Vector3(cos(angle), sin(angle), 0.0) * hex_radius
	_was_stepping[limb] = step.stepping
	var t := smoothstep(0.0, 1.0, step.progress()) if step.stepping else 1.0
	return _hex_from[limb].lerp(_hex_to[limb], t)


# How far the target is ahead of (−) or behind (+) the root along the body, as an angle.
func _sweep_angle(limb: int, target: Vector3, body: Transform3D) -> float:
	var local := body.affine_inverse() * target - (CrawlerLayout.ROOTS[limb] as Vector3)
	var share := clampf(local.z / CrawlerLayout.reach(limb), -1.0, 1.0)
	var side := -1.0 if CrawlerLayout.is_left(limb) else 1.0
	return share * sweep_amount * side


func _wobble(limb: int) -> Vector3:
	var t := _time * noise_rate
	return Vector3(_noise.get_noise_2d(t, limb * 31.0), _noise.get_noise_2d(t, limb * 31.0 + 11.0), _noise.get_noise_2d(t, limb * 31.0 + 23.0)) * noise_amount


func _smooth(limb: int, to: Vector3, delta: float) -> Vector3:
	if _springs[limb] == null:
		_springs[limb] = SecondOrder.new(smooth_frequency, 0.6, 0.0, to)
	var spring: SecondOrder = _springs[limb]
	spring.set_parameters(smooth_frequency, 0.6, 0.0)
	return spring.update(delta, to)
