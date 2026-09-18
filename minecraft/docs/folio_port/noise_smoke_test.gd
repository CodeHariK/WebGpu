extends SceneTree
var scene_root
var frames := 0
func _initialize():
	scene_root = load("res://scene/folio/game.tscn").instantiate()
	get_root().add_child(scene_root)
func _process(_delta):
	frames += 1
	if frames >= 3:
		var n = scene_root.get_node("Noises")
		var v = n.get_voronoi()
		var p = n.get_perlin()
		print("voronoi=", v.get_width(), "x", v.get_height(), " valid=", v != null)
		print("perlin_valid=", p != null, " res=", n.get_resolution())
		return true
	return false
