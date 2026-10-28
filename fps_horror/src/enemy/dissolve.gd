## Burns a dead enemy away instead of a ragdoll: after a short random wait, its meshes switch to
## the dissolve shader (materials/cultist_dissolve.gdshader) and holes eat through the skin from
## glowing blue edges over `duration` seconds; the eyes go out, a puff of embers rises. Then the
## whole enemy (this node's parent) is freed. Ported from CultistCorpseDissolve in the original
## (which dissolved a ragdoll; ours dissolves the body where it fell, so no physics bones).
##
## Mobile: the shader (it discards pixels, which is slower) only runs while a body is dissolving;
## the embers are one CPUParticles3D burst made per death.
extends Node

const SHADER := preload("res://materials/cultist_dissolve.gdshader")
const NOISE := preload("res://materials/dissolve_noise.tres")
const ALBEDO := preload("res://assets/enemy/textures/CultistAlbedo.png")
const NORMAL := preload("res://assets/enemy/textures/CultistNormalMap.png")

@export var wait := Vector2(0.4, 1.2)  ## seconds before it starts, random in this range
@export var duration := 2.5

var meshes: Array[MeshInstance3D] = []

var _wait := 0.0
var _progress := -1.0  # < 0: not started
var _material := ShaderMaterial.new()


func _ready() -> void:
	_wait = randf_range(wait.x, wait.y)


func _process(delta: float) -> void:
	if _progress < 0.0:
		_wait -= delta
		if _wait <= 0.0:
			_start()
		return
	_progress = minf(_progress + delta / duration, 1.0)
	_material.set_shader_parameter("progress", _progress)
	if _progress >= 1.0:
		get_parent().queue_free()


func _start() -> void:
	_progress = 0.0
	_material.shader = SHADER
	_material.set_shader_parameter("albedo_tex", ALBEDO)
	_material.set_shader_parameter("normal_tex", NORMAL)
	_material.set_shader_parameter("noise_tex", NOISE)
	for mesh in meshes:
		if mesh.name.contains("Eyes"):
			mesh.visible = false
		else:
			mesh.material_override = _material
	if not meshes.is_empty():
		_embers(meshes[0].global_position)


## Blue sparks drifting up off the body while it burns (spread over `duration`), left in the
## level so they outlive the body.
func _embers(at: Vector3) -> void:
	var embers := CPUParticles3D.new()
	embers.one_shot = true
	embers.amount = 60
	embers.lifetime = duration
	embers.explosiveness = 0.0
	embers.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	embers.emission_sphere_radius = 0.9
	embers.direction = Vector3.UP
	embers.spread = 30.0
	embers.initial_velocity_min = 0.1
	embers.initial_velocity_max = 0.5
	embers.gravity = Vector3(0, 0.3, 0)
	embers.damping_min = 0.2
	embers.damping_max = 0.5
	var colors := Gradient.new()
	colors.set_color(0, Color(0.5, 0.75, 1.0, 1.0))
	colors.set_color(1, Color(0.2, 0.4, 1.0, 0.0))
	embers.color_ramp = colors
	var quad := QuadMesh.new()
	quad.size = Vector2(0.05, 0.05)
	var soft := Gradient.new()
	soft.set_color(0, Color.WHITE)
	soft.set_color(1, Color(1, 1, 1, 0))
	var dot := GradientTexture2D.new()  # round soft dot, made in code
	dot.gradient = soft
	dot.fill = GradientTexture2D.FILL_RADIAL
	dot.fill_from = Vector2(0.5, 0.5)
	dot.fill_to = Vector2(1.0, 0.5)
	dot.width = 16
	dot.height = 16
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = dot
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.vertex_color_use_as_albedo = true
	mat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	quad.material = mat
	embers.mesh = quad
	var level := get_parent().get_parent()
	level.add_child(embers)
	embers.global_position = at
	embers.emitting = true
	get_tree().create_timer(embers.lifetime * 2.0 + 0.5).timeout.connect(embers.queue_free)
