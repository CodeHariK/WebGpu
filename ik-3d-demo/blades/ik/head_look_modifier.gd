## Turns the neck and head so the face points along `look` (a world direction), on top of whatever
## the clip and the spine lean did. The neck takes half the turn and the head the rest.
##
## The turn is built as yaw then pitch, never as one shortest rotation: a look far round behind
## (the blades winding up behind her) would otherwise swing the face up over the top. Yaw is limited
## to MAX_YAW either side of where the clip had the face (no owl heads); pitch goes straight to the
## look's own pitch, clamped (MAX_UP / MAX_DOWN) — so a torso leaning back still keeps the face level.
##
## The face direction: the head bone's own axis that pointed along the character's front (+Z in
## skeleton space) in the first pose it saw (her idle), so it works whatever the rig's bone axes.
class_name HeadLookModifier
extends SkeletonModifier3D

const MAX_YAW := 1.2 ## radians: the most the face turns left / right of the clip's
const MAX_UP := 0.3 ## radians: the face never points higher than this …
const MAX_DOWN := 0.45 ## … or lower than this

@export var neck_bone := "DEF-spine.005"
@export var head_bone := "DEF-spine.006"

var look := Vector3.FORWARD ## world direction to face
var face := Vector3.ZERO ## world, for the debug view: where the face points after the turn
var clip_face := Vector3.ZERO ## world, for the debug view: where the face pointed before the turn

var _ids := Vector2i(-1, -1) ## neck, head
var _face_axis := Vector3.ZERO ## the head bone's local axis that is "the face"


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or not _find_bones(skeleton):
		return
	var head := skeleton.get_bone_global_pose(_ids.y)
	if _face_axis == Vector3.ZERO:
		_face_axis = (head.basis.inverse() * Vector3.BACK).normalized() # skeleton +Z: her front
	var to_skeleton := skeleton.global_transform.basis.orthonormalized().inverse()
	var wanted := (to_skeleton * look).normalized()
	var current := (head.basis * _face_axis).normalized()
	clip_face = (skeleton.global_transform.basis * current).normalized()
	wanted = _yaw_then_pitch(current, wanted)
	# neck: half the turn
	var turn := ArmChainModifier._arc(current, wanted)
	var neck := skeleton.get_bone_global_pose(_ids.x)
	neck.basis = Basis(Quaternion.IDENTITY.slerp(turn, 0.5)) * neck.basis
	skeleton.set_bone_global_pose(_ids.x, neck)
	# head (moved with the neck): the rest
	head = skeleton.get_bone_global_pose(_ids.y)
	current = (head.basis * _face_axis).normalized()
	head.basis = Basis(ArmChainModifier._arc(current, wanted)) * head.basis
	skeleton.set_bone_global_pose(_ids.y, head)
	face = (skeleton.global_transform.basis * (head.basis * _face_axis)).normalized()


# The face direction `wanted` asks for, reached from `current` by a limited yaw about up, then a
# clamped pitch (skeleton space, whose up is +Y).
static func _yaw_then_pitch(
		current: Vector3,
		wanted: Vector3
) -> Vector3:
	var flat := Vector3(current.x, 0.0, current.z)
	var wanted_flat := Vector3(wanted.x, 0.0, wanted.z)
	if flat.length() < 1e-3:
		return current
	var yaw := 0.0
	if wanted_flat.length() > 1e-3:
		yaw = clampf(flat.signed_angle_to(wanted_flat, Vector3.UP), -MAX_YAW, MAX_YAW)
	var pitch := clampf(asin(clampf(wanted.normalized().y, -1.0, 1.0)), -MAX_DOWN, MAX_UP)
	return flat.normalized().rotated(Vector3.UP, yaw) * cos(pitch) + Vector3.UP * sin(pitch)


func _find_bones(skeleton: Skeleton3D) -> bool:
	if _ids.x < 0:
		_ids = Vector2i(skeleton.find_bone(neck_bone), skeleton.find_bone(head_bone))
	return _ids.x >= 0 and _ids.y >= 0
