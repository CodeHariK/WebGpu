@tool

extends FABRIK3D

@onready var target: Node3D = self.get_node_or_null(self.get_target_node(self.get_setting_count() - 1))
@onready var skel: Skeleton3D = self.get_skeleton()
@onready var mod: CopyTransformModifier3D = $"../CopyTransformModifier3D"

@export_group("Tip Rotaiton")
@export_range(0, 5, 0.001, "or_greater") var threshold: float
@export var curve: Curve

func _on_modification_processed() -> void:
	var distance: float = target.global_transform.origin.distance_to(skel.global_position + skel.get_bone_global_pose(5).origin)
	var amount: float = remap(distance, 0.0, threshold, 0.0, 1.0)
	if curve:
		amount = curve.sample(amount)
	mod.set_amount(0, amount)
