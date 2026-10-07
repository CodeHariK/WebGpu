@tool

extends Label3D

# Plain initializer (not @onready): the Skeleton3D signals can fire before this node's _ready,
# and in the editor (@tool) an @onready Dictionary is still empty then.
var poses: Dictionary = { "animated_pose": "", "modified_pose": "" }

func _update_text() -> void:
	text = "animated_pose:" + str(poses.get("animated_pose", "")) + "\n" + "modified_pose:" + str(poses.get("modified_pose", ""))

func _on_animation_player_mixer_applied() -> void:
	poses["animated_pose"] = $"../Armature/Skeleton3D".get_bone_pose(1)
	_update_text()

func _on_skeleton_3d_skeleton_updated() -> void:
	poses["modified_pose"] = $"../Armature/Skeleton3D".get_bone_pose(1)
	_update_text()
