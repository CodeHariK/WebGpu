## Spent brass falling out of the revolver during a reload (a revolver doesn't throw a shell per
## shot). When a reload starts, after `delay` seconds the cylinder dumps one casing per round fired.
##
## Pooled: POOL small RigidBodies with their EmptyShell mesh, made once. They bounce, settle, and
## after `linger` seconds freeze and hide until reused. Collision is a capsule a bit fatter than the
## shell (tiny shapes tunnel through floors) with continuous collision on.
extends Node3D

const SHELL := preload("res://assets/player/EmptyShell.gltf")
const Look := preload("res://src/look/character_look.gd")
const POOL := 12

@export var delay := 1.1  ## seconds into the reload clip when the cylinder is tipped out
@export var linger := 30.0

var _shells: Array[RigidBody3D] = []
var _ages: Array[float] = []
var _next := 0
var _skeleton: Skeleton3D
var _bone := -1


## `player`: the casings never bump the player's own body (they fall out right next to it).
func setup(skeleton: Skeleton3D, player: PhysicsBody3D) -> void:
	_skeleton = skeleton
	_bone = skeleton.find_bone("RevolverBase")
	for body in _shells:
		body.add_collision_exception_with(player)


func _ready() -> void:
	top_level = true
	global_transform = Transform3D.IDENTITY
	var mesh := _shell_mesh()
	for i in POOL:
		var body := RigidBody3D.new()
		body.mass = 0.01
		body.continuous_cd = true
		body.freeze = true
		body.visible = false
		var shape := CollisionShape3D.new()
		var capsule := CapsuleShape3D.new()
		capsule.radius = 0.012
		capsule.height = 0.05
		shape.shape = capsule
		shape.rotation.x = PI / 2
		body.add_child(shape)
		var view := MeshInstance3D.new()
		view.mesh = mesh
		view.material_override = Look.BRASS
		view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		body.add_child(view)
		add_child(body)
		_shells.append(body)
		_ages.append(-1.0)


## Called when a reload starts: drop `count` spent casings a moment later.
func drop(count: int) -> void:
	if count <= 0 or _skeleton == null:
		return
	await get_tree().create_timer(delay).timeout
	var at := _skeleton.global_transform * _skeleton.get_bone_global_pose(_bone)
	for i in count:
		var body := _shells[_next]
		_ages[_next] = 0.0
		_next = (_next + 1) % POOL
		body.freeze = false
		body.visible = true
		body.global_transform = Transform3D(Basis.from_euler(Vector3(randf() * TAU, randf() * TAU, 0)),
			at.origin + Vector3(randf_range(-0.02, 0.02), -0.03, randf_range(-0.02, 0.02)))
		body.linear_velocity = Vector3(randf_range(-0.4, 0.4), randf_range(-0.5, 0.2), randf_range(-0.4, 0.4))
		body.angular_velocity = Vector3(randf_range(-8, 8), randf_range(-8, 8), randf_range(-8, 8))


func _process(delta: float) -> void:
	for i in POOL:
		if _ages[i] < 0.0:
			continue
		_ages[i] += delta
		if _ages[i] > linger:
			_ages[i] = -1.0
			_shells[i].freeze = true
			_shells[i].visible = false


## The EmptyShell glTF's mesh (its only MeshInstance3D).
func _shell_mesh() -> Mesh:
	var scene := SHELL.instantiate()
	var mesh: Mesh = scene.find_children("*", "MeshInstance3D", true, false)[0].mesh
	scene.free()
	return mesh
