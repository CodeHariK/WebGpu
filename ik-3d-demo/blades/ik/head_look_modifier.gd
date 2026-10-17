## Turns the neck and head so the face points along `look` (a world direction), on top of whatever
## the clip and the spine lean did. The neck takes half the turn and the head the rest, and the
## whole turn is limited to MAX_TURN from where the clip had the face (no owl heads).
##
## The face direction: the head bone's own axis that pointed along the character's front (+Z in
## skeleton space) in the first pose it saw (her idle), so it works whatever the rig's bone axes.
class_name HeadLookModifier
extends SkeletonModifier3D

const MAX_TURN := 1.2 ## radians: the most the face turns away from the clip's

@export var neck_bone := "DEF-spine.005"
@export var head_bone := "DEF-spine.006"

var look := Vector3.FORWARD ## world direction to face
var face := Vector3.ZERO ## world, for the debug view: where the face points after the turn

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
	var angle := current.angle_to(wanted)
	if angle > MAX_TURN:
		wanted = current.slerp(wanted, MAX_TURN / angle)
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


func _find_bones(skeleton: Skeleton3D) -> bool:
	if _ids.x < 0:
		_ids = Vector2i(skeleton.find_bone(neck_bone), skeleton.find_bone(head_bone))
	return _ids.x >= 0 and _ids.y >= 0
