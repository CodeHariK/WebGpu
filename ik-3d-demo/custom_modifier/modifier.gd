@tool

class_name CustomModifier
extends SkeletonModifier3D

@export var target_coordinate: Vector3 = Vector3(0, 0, 0)
@export_enum(" ") var bone: String

func _validate_property(property: Dictionary) -> void:
	if property.name == "bone":
		var skeleton: Skeleton3D = get_skeleton()
		if skeleton:
			property.hint = PROPERTY_HINT_ENUM
			property.hint_string = skeleton.get_concatenated_bone_names()

func _process_modification() -> void:
	var skeleton: Skeleton3D = get_skeleton()
	if !skeleton:
		return # Never happen, but for the safety.
	var bone_idx: int = skeleton.find_bone(bone)
	var parent_idx: int = skeleton.get_bone_parent(bone_idx)
	var pose: Transform3D = skeleton.global_transform * skeleton.get_bone_global_pose(bone_idx)
	var looked_at: Transform3D = _y_look_at(pose, target_coordinate)
	skeleton.set_bone_global_pose(bone_idx, Transform3D(looked_at.basis.orthonormalized(), skeleton.get_bone_global_pose(bone_idx).origin))

func _y_look_at(from: Transform3D, target: Vector3) -> Transform3D:
	var t_v: Vector3 = target - from.origin
	var v_y: Vector3 = t_v.normalized()
	var v_z: Vector3 = from.basis.x.cross(v_y)
	v_z = v_z.normalized()
	var v_x: Vector3 = v_y.cross(v_z)
	from.basis = Basis(v_x, v_y, v_z)
	return from
