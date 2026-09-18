extends SceneTree
var scene_root
var frames := 0
func _initialize():
	scene_root = load("res://scene/folio/game.tscn").instantiate()
	get_root().add_child(scene_root)
func _process(_delta):
	frames += 1
	if frames >= 6:
		var g = scene_root
		print("has_ticker=", g.get_ticker() != null)
		print("has_view=", g.get_view() != null)
		print("view_pos=", g.get_view().get_position())
		print("optimal_radius=", g.get_view().get_optimal_radius())
		return true
	return false
