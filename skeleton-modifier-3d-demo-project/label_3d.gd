@tool

extends Label3D

@onready var poses: Dictionary = { "animated_pose": "", "modified_pose": "" }

func _update_text() -> void:
	text = "animated_pose:" + str(poses["animated_pose"]) + "\n" + "modified_pose:" + str(poses["modified_pose"])

func _on_animation_player_mixer_applied() -> void:
	poses["animated_pose"] = $"../Armature/Skeleton3D".get_bone_pose(1)
	_update_text()

func _on_skeleton_3d_skeleton_updated() -> void:
	poses["modified_pose"] = $"../Armature/Skeleton3D".get_bone_pose(1)
	_update_text()
