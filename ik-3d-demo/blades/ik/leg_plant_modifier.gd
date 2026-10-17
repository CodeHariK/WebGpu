## A leg on ArmChainModifier's two-bone solve (thigh → shin → foot): the ankle goes to `target`
## (a planted or stepping foot, from FootPlanter), the knee bends toward `pole`, and then the foot
## gets its clip orientation back (so it stays flat however the shin turned) plus `foot_twist`
## about the up axis (so a planted foot keeps pointing where it was put down while the hips turn).
class_name LegPlantModifier
extends ArmChainModifier

var foot_twist := 0.0 ## radians about up: the foot's heading minus the body's heading


func _process_modification() -> void:
	var skeleton := get_skeleton()
	if skeleton == null or not _find_bones(skeleton):
		return
	var animated_foot := skeleton.get_bone_global_pose(_bones.z).basis
	super()
	var foot := skeleton.get_bone_global_pose(_bones.z)
	foot.basis = Basis(Vector3.UP, foot_twist) * animated_foot
	skeleton.set_bone_global_pose(_bones.z, foot)
