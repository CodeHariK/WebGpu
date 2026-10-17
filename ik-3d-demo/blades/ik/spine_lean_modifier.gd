## Adds a lean and a twist on top of whatever the animation does to the spine, spread over a few
## spine bones (each turns 1/n of it, and each child carries its parents' turn, so the total adds up
## smoothly along the back). Additive: with lean 0 and twist 0 the clip plays untouched.
##   lean   a horizontal world vector: the direction the upper body tips toward; its length is the
##          angle in radians
##   twist  radians about the up axis (positive turns the chest the way Vector3.rotated(UP, +) does)
class_name SpineLeanModifier
extends SkeletonModifier3D

@export var bones := PackedStringArray(["DEF-spine.002", "DEF-spine.003", "DEF-spine.004"])

var lean := Vector3.ZERO
var twist := 0.0

var _ids := PackedInt32Array()


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null:
		return
	if _ids.is_empty():
		for bone_name in bones:
			_ids.append(skeleton.find_bone(bone_name))
	var to_skeleton := skeleton.global_transform.basis.orthonormalized().inverse()
	var up := (to_skeleton * Vector3.UP).normalized()
	var angle := lean.length()
	var tilt_axis := (to_skeleton * Vector3.UP.cross(lean)).normalized() if angle > 1e-4 else Vector3.ZERO
	var share := 1.0 / maxf(_ids.size(), 1)
	for id in _ids:
		if id < 0:
			continue
		var pose := skeleton.get_bone_global_pose(id)
		var turn := Quaternion(up, twist * share)
		if angle > 1e-4:
			turn = Quaternion(tilt_axis, angle * share) * turn
		pose.basis = Basis(turn) * pose.basis
		skeleton.set_bone_global_pose(id, pose)
