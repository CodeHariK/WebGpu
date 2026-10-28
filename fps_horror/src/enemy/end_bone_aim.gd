## Turns chosen bones to given world rotations, after the IK has placed them. TwoBoneIK3D only
## moves a limb so its end reaches the target; it leaves the hand or foot at whatever angle the
## forearm or shin gives it. The original's SkeletonIK3D also copied the target's rotation onto
## the end bone (override_tip_basis), which is how its palms and soles lay flat on the surface
## and the head faced you. This modifier does that copy.
##
##   bones  bone indices to turn
##   aims   one world-space Basis per bone (cultist_rig.gd fills these each physics tick)
## Only the rotation changes; each bone stays where the IK put it. `influence` blends it in
## (the head fades out when you're behind it).
##
## Mobile: one get/set of a bone pose per bone per frame.
extends SkeletonModifier3D

var bones: PackedInt32Array = []
var aims: Array[Basis] = []


func _process_modification_with_delta(_delta: float) -> void:
	var skeleton := get_skeleton()
	if skeleton == null:
		return
	var to_skeleton := skeleton.global_basis.inverse()
	for i in bones.size():
		var pose := skeleton.get_bone_global_pose(bones[i])
		pose.basis = (to_skeleton * aims[i]).orthonormalized()
		skeleton.set_bone_global_pose(bones[i], pose)
