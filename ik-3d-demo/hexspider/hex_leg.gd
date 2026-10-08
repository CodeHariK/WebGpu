## One leg of the hex spider: where it hangs on the body, where its foot rests, where the foot is
## now, and the swing of a step in progress. Also keeps the last pick (the hex points it tried and
## why each was rejected) for the debug drawing.
class_name HexLeg
extends RefCounted

var hip_local := Vector3.ZERO ## on the body (body space)
var rest_local := Vector3.ZERO ## the foot's rest spot, body space, on the ground (y = 0)
var side := 1 ## +1 right, −1 left
var group := 0 ## gait group: legs in one group step together, the other group waits

var foot := Vector3.ZERO ## world
var knee := Vector3.ZERO ## world (two-bone solve, for drawing)
var stepping := false
var step_from := Vector3.ZERO
var step_to := Vector3.ZERO
var step_t := 0.0 ## 0..1 through the swing
var planted_for := 0.0 ## seconds since it last landed

var pole := Vector3.ZERO ## world: where the knee points (on the pole hex, lifted while stepping)
var pole_from := Vector3.ZERO ## the pole hex corner before this step (offset, body space) …
var pole_to := Vector3.ZERO ## … and the one picked for it (the knee eases between them)

var centre := Vector3.ZERO ## the last pick's hex centre (world)
var candidates: Array[Dictionary] = [] ## the last pick: {point, top, hit, ok, why}
var chosen := -1


func start_step(to: Vector3, pole_corner: Vector3) -> void:
	pole_from = pole_corner_now()
	pole_to = pole_corner
	stepping = true
	step_from = foot
	step_to = to
	step_t = 0.0


## The pole hex corner the knee is using now (eases from the old corner to the new one mid-step).
func pole_corner_now() -> Vector3:
	return pole_from.lerp(pole_to, step_t) if stepping else pole_to


## How high the pole is lifted now: up while the foot is in the air, down when it lands.
func pole_lift_now(lift: float) -> float:
	return sin(PI * step_t) * lift if stepping else 0.0


## Move the foot along its arc: straight across, up and down by `height` (more when stepping up).
## True when it lands.
func swing(delta: float, duration: float, height: float) -> bool:
	step_t = minf(step_t + delta / maxf(duration, 0.01), 1.0)
	var lift := height + maxf(step_to.y - step_from.y, 0.0)
	var t := step_t * step_t * (3.0 - 2.0 * step_t) # ease in and out across
	foot = step_from.lerp(step_to, t) + Vector3.UP * sin(PI * step_t) * lift
	if step_t >= 1.0:
		stepping = false
		foot = step_to
		planted_for = 0.0
		return true
	return false
