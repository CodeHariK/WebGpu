## Puts our mobile materials (materials/*.tres) on the original models. Their glTF files carry no
## materials, so every mesh is matched by name. Used by the viewer now and by the player and enemy
## scenes later.
##
## Mobile choices (vs the original):
##   - no subsurface scattering (the Mobile renderer doesn't have it): a little rim light instead
##   - the cultist's custom dissolve/SSS shader is replaced by a StandardMaterial3D; its glowing
##     marks stay as an emission mask (cheap)
##   - the eyes' fresnel shader is replaced by emission + rim
extends RefCounted

const CULTIST_BODY := preload("res://materials/cultist_body.tres")
const CULTIST_EYES := preload("res://materials/cultist_eyes.tres")
const ARMS_SKIN := preload("res://materials/arms_skin.tres")
const REVOLVER := preload("res://materials/revolver.tres")
const FLASHLIGHT := preload("res://materials/flashlight.tres")
const FLASHLIGHT_LENS := preload("res://materials/flashlight_lens.tres")
const BRASS := preload("res://materials/brass.tres")


## Cultist.gltf: CultistMesh (body) and CultistEyesMesh.
static func dress_cultist(root: Node) -> void:
	for mesh in _meshes(root):
		mesh.material_override = CULTIST_EYES if mesh.name.contains("Eyes") else CULTIST_BODY


## ArmsMesh.gltf / ArmsAnimations.gltf: Arms + Hands (skin), Revolver* parts, FlashlightPlaceholder.
static func dress_arms(root: Node) -> void:
	for mesh in _meshes(root):
		if mesh.name.begins_with("Revolver"):
			mesh.material_override = REVOLVER
		elif mesh.name.begins_with("Flashlight"):
			mesh.material_override = FLASHLIGHT
		else:
			mesh.material_override = ARMS_SKIN


## FlashlightMesh.gltf: the body, the button and the lens.
static func dress_flashlight(root: Node) -> void:
	for mesh in _meshes(root):
		mesh.material_override = FLASHLIGHT_LENS if mesh.name.contains("Lens") else FLASHLIGHT


## Bullet, EmptyShell: brass. SpeedLoader: gun metal.
static func dress_ammo(root: Node, gun_metal := false) -> void:
	for mesh in _meshes(root):
		mesh.material_override = REVOLVER if gun_metal else BRASS


## Triangles, mesh surfaces (≈ draw calls) and bones in a model, for the viewer's cost readout.
static func stats(root: Node) -> Dictionary:
	var tris := 0
	var surfaces := 0
	for mesh in _meshes(root):
		for s in mesh.mesh.get_surface_count():
			var arrays := mesh.mesh.surface_get_arrays(s)
			var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
			tris += indices.size() / 3 if indices.size() > 0 else arrays[Mesh.ARRAY_VERTEX].size() / 3
			surfaces += 1
	var bones := 0
	for skeleton in root.find_children("*", "Skeleton3D", true, false):
		bones += skeleton.get_bone_count()
	return {"tris": tris, "surfaces": surfaces, "bones": bones}


static func _meshes(root: Node) -> Array[MeshInstance3D]:
	var out: Array[MeshInstance3D] = []
	if root is MeshInstance3D:
		out.append(root)
	for node in root.find_children("*", "MeshInstance3D", true, false):
		out.append(node)
	return out
