## One leg of the gait spider: its place on the body, its rest spot, its size, and where its foot,
## knee and swing are now. The spider (GaitSpider) decides when it swings and where it lands.
class_name GaitLeg
extends RefCounted

var pair := 0 ## 0 = front … 3 = back
var side := 1 ## +1 right, −1 left
var name := "" ## L1 … R4 (for the chart)

var hip_local := Vector3.ZERO ## on the body (body space)
var rest_local := Vector3.ZERO ## rest spot on the ground (body space, y = 0)
var upper := 0.9 ## femur (hip → knee)
var lower := 1.1 ## tibia + tarsus (knee → foot)
var lift := 0.25 ## how high the foot rises in a swing

var foot := Vector3.ZERO ## world
var knee := Vector3.ZERO ## world
var swinging := false
var lift_off := Vector3.ZERO ## where the foot left the ground
var target := Vector3.ZERO ## where it will land (re-aimed during the first half of the swing)
var progress := 0.0 ## 0..1 through the swing
var landed_ok := true ## false: no floor under the target, it landed on the nearest spot it found


## Foot position at `progress` along the swing: across with ease in / out, up and down an arc
## (higher when stepping up onto something).
func swing_position() -> Vector3:
	var t := progress * progress * (3.0 - 2.0 * progress)
	var up := lift + maxf(target.y - lift_off.y, 0.0)
	return lift_off.lerp(target, t) + Vector3.UP * sin(PI * progress) * up
