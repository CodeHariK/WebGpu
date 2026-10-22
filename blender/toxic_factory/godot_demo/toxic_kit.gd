## The Blender kit (toxic_kit.glb): every piece is a mesh at the origin, base on the ground, unit
## size. Looks pieces up by name ("Truck_tanker") or by family ("Barrel_", "Jersey_").
extends RefCounted

var _meshes := {}


func _init(path := "res://toxic_kit.glb") -> void:
	var scene: Node = load(path).instantiate()
	for node in scene.find_children("*", "MeshInstance3D", true, false):
		_meshes[String(node.name)] = node.mesh
	scene.free()


func mesh(piece: String) -> Mesh:
	assert(_meshes.has(piece), "toxic_kit.glb has no piece named " + piece)
	return _meshes[piece]


## All piece names starting with `prefix`, sorted (so a seed always picks the same piece).
func family(prefix: String) -> Array[String]:
	var out: Array[String] = []
	for piece: String in _meshes:
		if piece.begins_with(prefix):
			out.append(piece)
	out.sort()
	return out
