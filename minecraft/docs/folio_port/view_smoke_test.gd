extends SceneTree
var scene_root
var frames := 0
func _initialize():
	scene_root = load("res://scene/folio/core.tscn").instantiate()
	get_root().add_child(scene_root)
func _process(_delta):
	frames += 1
	if frames >= 6:
		var view = scene_root.get_node("View")
		print("view_pos=", view.get_position())
		print("optimal_radius=", view.get_optimal_radius())
		return true
	return false
