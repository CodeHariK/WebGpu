## Muzzle flash at the end of the barrel, shown for a few frames per shot:
##   flames  two crossed cards along the barrel, using a random frame of their MuzzleFlash.png
##           (a sheet of 6 flames, each pointing up); from behind they read as a star of fire
##   glow    a round soft billboard at the tip
##   light   a short warm OmniLight (no shadow, 6 m range)
## Lives on a BoneAttachment3D following the revolver's "RevolverFront" bone, so it stays on the
## barrel through every animation.
##
## Mobile: always in the scene (nothing instanced per shot), additive unshaded materials, hidden
## between shots.
extends Node3D

const SHEET := preload("res://assets/effects/textures/MuzzleFlash.png")
const FRAMES := 6
const TIP := Vector3(0.0, 0.025, -0.19)  ## barrel tip in the RevolverFront bone's space
const FLAME_LENGTH := 0.26
const FLAME_WIDTH := 0.09

@export var duration := 0.05

var _flame_mat := StandardMaterial3D.new()
var _parts: Array[Node3D] = []
var _light := OmniLight3D.new()
var _left := 0.0


func _ready() -> void:
	position = TIP
	_flame_mat.albedo_texture = SHEET
	_flame_mat.albedo_color = Color(1.0, 0.75, 0.45)
	_flame_mat.uv1_scale = Vector3(1.0 / FRAMES, 1.0, 1.0)
	_additive(_flame_mat)
	for roll in [0.0, PI / 2]:  # two cards crossed along the barrel (-Z)
		var card := MeshInstance3D.new()
		var quad := QuadMesh.new()
		quad.size = Vector2(FLAME_WIDTH, FLAME_LENGTH)
		quad.center_offset = Vector3(0.0, FLAME_LENGTH * 0.5, 0.0)  # base at the tip, flame forward
		card.mesh = quad
		card.material_override = _flame_mat
		card.rotation = Vector3(-PI / 2, 0.0, 0.0)
		card.rotate_object_local(Vector3.UP, roll)
		_add(card)
	var glow := MeshInstance3D.new()
	var glow_quad := QuadMesh.new()
	glow_quad.size = Vector2(0.16, 0.16)
	glow.mesh = glow_quad
	glow.material_override = _glow_material()
	_add(glow)
	_light.light_color = Color(1.0, 0.75, 0.45)
	_light.omni_range = 6.0
	_light.shadow_enabled = false
	add_child(_light)
	_parts.append(_light)
	_show(false)


func flash() -> void:
	_left = duration
	_flame_mat.uv1_offset = Vector3(float(randi() % FRAMES) / FRAMES, 0.0, 0.0)
	scale = Vector3.ONE * randf_range(0.85, 1.15)
	rotation.z = randf() * TAU
	_show(true)


func _process(delta: float) -> void:
	if _left <= 0.0:
		return
	_left -= delta
	_light.light_energy = 4.0 * maxf(_left / duration, 0.0)
	if _left <= 0.0:
		_show(false)


func _add(part: GeometryInstance3D) -> void:
	part.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(part)
	_parts.append(part)


func _show(on: bool) -> void:
	for part in _parts:
		part.visible = on


func _glow_material() -> StandardMaterial3D:
	var gradient := Gradient.new()
	gradient.set_color(0, Color(1.0, 0.9, 0.7, 1.0))
	gradient.set_color(1, Color(1.0, 0.5, 0.2, 0.0))
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	texture.width = 64
	texture.height = 64
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = texture
	mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	_additive(mat)
	return mat


static func _additive(mat: StandardMaterial3D) -> void:
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	mat.no_depth_test = false
