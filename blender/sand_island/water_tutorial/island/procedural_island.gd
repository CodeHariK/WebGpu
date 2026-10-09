## A procedurally generated island: terrain + water meshes from an IslandShape, with the sand shader
## on the ground and any water shader on the sea. Call regenerate(seed) for a new island.
class_name ProceduralIsland
extends Node3D

var shape: IslandShape
var terrain := MeshInstance3D.new()
var water := MeshInstance3D.new()


func _init(sand_material: Material, water_material: Material) -> void:
	terrain.name = "Terrain"
	water.name = "Water"
	terrain.material_override = sand_material
	water.material_override = water_material
	add_child(terrain)
	add_child(water)


func regenerate(island_seed: int) -> void:
	var started := Time.get_ticks_msec()
	shape = IslandShape.new(island_seed)
	terrain.mesh = IslandMeshBuilder.terrain(shape)
	water.mesh = IslandMeshBuilder.water(shape)
	print("island %d built in %d ms" % [island_seed, Time.get_ticks_msec() - started])
