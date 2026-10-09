## A lofted procedural island node: terrain + water meshes from IslandLoft, sand shader on the
## ground, any water shader on the sea. regenerate(seed, style) builds a new one.
class_name LoftIsland
extends Node3D

var loft: IslandLoft
var terrain := MeshInstance3D.new()
var water := MeshInstance3D.new()
var layers: Array[IslandLayer] = []
var layer_material: Material
var _layer_meshes: Array[MeshInstance3D] = []


func _init(sand_material: Material, water_material: Material) -> void:
	terrain.name = "Terrain"
	water.name = "Water"
	terrain.material_override = sand_material
	water.material_override = water_material
	add_child(terrain)
	add_child(water)


func regenerate(island_seed: int, style: BankProfile.Style) -> void:
	var started := Time.get_ticks_msec()
	loft = IslandLoft.new(island_seed, style)
	terrain.mesh = loft.terrain_mesh()
	water.mesh = loft.water_mesh()
	rebuild_layers()
	var tris: int = terrain.mesh.surface_get_array_index_len(0) / 3
	print("loft island %d (%s): %d terrain triangles, built in %d ms"
		% [island_seed, BankProfile.NAMES[style], tris, Time.get_ticks_msec() - started])


## Swap the material on the layer sheets (the snow tutorial puts its snow steps on them).
func set_layer_material(material: Material) -> void:
	layer_material = material
	for m in _layer_meshes:
		m.material_override = material


## (Re)build the layer meshes on the current island — cheap, so the demo can do it on every tweak.
func rebuild_layers() -> void:
	for m in _layer_meshes:
		m.queue_free()
	_layer_meshes.clear()
	for layer in layers:
		var m := MeshInstance3D.new()
		m.mesh = LayerMesh.build(loft, layer)
		m.material_override = layer_material
		add_child(m)
		_layer_meshes.append(m)
