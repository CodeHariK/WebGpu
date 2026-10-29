## The crawler's body plan: where its four limbs attach, how long they are, where each foot
## rests, which way elbows and knees bend, and which limbs step together. Everything else
## (gait, pose, rig) reads these numbers, so the creature's shape lives in this one file.
##
## Frames: positions are in the BODY frame (X right, Y up away from the surface, -Z forward;
## the origin is the chest centre, ride_height above the surface). Rest spots are on the
## SURFACE frame's plane (y = 0).
##
## Limb order everywhere: left hand, right hand, left foot, right foot (like the cultist).
## Step groups are diagonal pairs (left hand + right foot, right hand + left foot), like a
## walking cat or the spider's 4-leg layout: one pair always holds the body up.
class_name CrawlerLayout
extends RefCounted

enum { LEFT_HAND, RIGHT_HAND, LEFT_FOOT, RIGHT_FOOT }
const COUNT := 4
const NAMES := ["left hand", "right hand", "left foot", "right foot"]
const GROUPS := [[LEFT_HAND, RIGHT_FOOT], [RIGHT_HAND, LEFT_FOOT]]

const RIDE_HEIGHT := 0.42 ## chest centre above the surface when relaxed (m)

## Shoulders (on the chest) and hips (on the pelvis), body frame.
const ROOTS := [Vector3(-0.17, 0.0, -0.3), Vector3(0.17, 0.0, -0.3), Vector3(-0.15, -0.02, 0.3), Vector3(0.15, -0.02, 0.3)]
## Upper and lower segment lengths (upper arm + forearm, thigh + shin).
const LENGTHS := [Vector2(0.42, 0.42), Vector2(0.42, 0.42), Vector2(0.45, 0.42), Vector2(0.45, 0.42)]
## Where each hand/foot rests, surface frame (x, z): hands wide and ahead, feet wide and behind.
const REST := [Vector2(-0.55, -0.6), Vector2(0.55, -0.6), Vector2(-0.5, 0.6), Vector2(0.5, 0.6)]
## Where each elbow/knee points, body frame, from the middle of the limb: the cultist's magnet
## offsets scaled down (elbows out, up and back; knees out, up and forward).
const POLES := [Vector3(-0.6, 0.6, 0.3), Vector3(0.6, 0.6, 0.3), Vector3(-0.6, 0.4, -0.4), Vector3(0.6, 0.4, -0.4)]
## Where a hand/foot hangs when it has nothing to hold (in the air, or no foothold in reach):
## body frame, about half its reach from the shoulder/hip, bent, a little forward/back. Limbs ease
## toward these instead of stretching straight at a spot they can't reach.
const RELAXED := [Vector3(-0.36, -0.3, -0.45), Vector3(0.36, -0.3, -0.45), Vector3(-0.33, -0.32, 0.42), Vector3(0.33, -0.32, 0.42)]
## IK target this far off the surface (palm / sole thickness).
const LIFT := [0.035, 0.035, 0.045, 0.045]


static func is_foot(limb: int) -> bool:
	return limb >= LEFT_FOOT


static func is_left(limb: int) -> bool:
	return limb == LEFT_HAND or limb == LEFT_FOOT


static func reach(limb: int) -> float:
	return LENGTHS[limb].x + LENGTHS[limb].y


## Rest spot as a point on the surface frame's plane.
static func rest(limb: int) -> Vector3:
	return Vector3(REST[limb].x, 0.0, REST[limb].y)


## The group a limb steps with.
static func group_of(limb: int) -> int:
	return 0 if limb in GROUPS[0] else 1
