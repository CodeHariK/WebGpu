extends Node3D
##
## Lightweight port of folio World/Scenery.js: instance the authored scenery.glb
## (track curbs, rocks, fences, bridge, road) and give every solid object a
## trimesh collider so the car can drive the track and bump into things.
##
## Folio naming convention in the glb: names carry Physical / Static / Fixed for
## colliders and a `ref` prefix for reference-only nodes (no render / no physics).
## We collide everything that isn't a `ref` and skip nothing else for now.
##
## Materials are still the glb's imported ones (flat palette look) — swapping them
## to the folio MeshDefaultMaterial shader is a later refinement.
##

@export var scenery_path := "res://assets/folio/scenery/scenery.glb"

func _ready() -> void:
	var scn: PackedScene = load(scenery_path)
	if scn == null:
		push_warning("FolioScenery: could not load " + scenery_path)
		return
	var root := scn.instantiate()
	add_child(root)
	_add_colliders(root)

func _add_colliders(n: Node) -> void:
	for c in n.get_children():
		if c is MeshInstance3D and not str(c.name).to_lower().begins_with("ref"):
			# create_trimesh_collision() adds a StaticBody3D + CollisionShape child.
			(c as MeshInstance3D).create_trimesh_collision()
		_add_colliders(c)
