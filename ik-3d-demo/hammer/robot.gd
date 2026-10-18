## A boxy robot to smash: a body, a head on top with two glowing eyes. Robots come in different
## heights, so the jump has to find each one's head.
##
## Its reaction to a hit is a pure function of the time since the hit (react), so a paused or
## scrubbed attack shows it exactly: the head flattens (and sinks into the shoulders), then springs
## back with a wobble; the body squashes a little; stars circle the head for a while.
class_name Robot
extends Node3D

const HEAD := 0.62 ## the head's size (a cube)
const BODY := Vector2(0.9, 0.6) ## the body's width and depth
const STARS := 5
const STAR_TIME := 1.6 ## seconds the stars circle

var height := 1.8 ## the top of its head above the ground
var highlight := 0.0 ## 0..1: picked as the target (glows)

var _body: MeshInstance3D
var _head: MeshInstance3D
var _stars: Array[MeshInstance3D] = []
var _body_material: StandardMaterial3D


func setup(
		at: Vector3,
		new_height: float,
		color: Color
) -> void:
	position = at
	height = new_height
	var body_height := height - HEAD
	_body = _box(Vector3(BODY.x, body_height, BODY.y), color)
	_body_material = _body.material_override
	_head = _box(Vector3.ONE * HEAD, color.lightened(0.25))
	for x in [-0.14, 0.14]:
		var eye := MeshInstance3D.new()
		var sphere := SphereMesh.new()
		sphere.radius = 0.07
		sphere.height = 0.14
		eye.mesh = sphere
		eye.material_override = _glow(Color(0.3, 1.0, 0.9))
		eye.position = Vector3(x, 0.05, HEAD * 0.5)
		_head.add_child(eye)
	for i in STARS:
		var star := MeshInstance3D.new()
		var prism := PrismMesh.new()
		prism.size = Vector3(0.16, 0.16, 0.05)
		star.mesh = prism
		star.material_override = _glow(Color(1.0, 0.9, 0.2))
		add_child(star)
		_stars.append(star)
	react(-1.0)


## Turn to face `point` (on the ground plane).
func face(point: Vector3) -> void:
	var flat := Vector3(point.x, position.y, point.z)
	if flat.distance_to(position) > 0.1:
		look_at(flat, Vector3.UP, true)


## The top of its head (where the hammer lands), as it stands (unhit).
func head_top() -> Vector3:
	return global_position + Vector3.UP * height


## Pose it `since` seconds after a hit (negative: not hit).
func react(since: float) -> void:
	var head_squash := 0.0 # 1: flat
	var body_squash := 0.0
	if since >= 0.0:
		head_squash = 0.65 * exp(-since * 5.0) * cos(since * 18.0)
		body_squash = 0.22 * exp(-since * 4.0) * cos(since * 13.0)
	var body_height := (height - HEAD) * (1.0 - body_squash)
	_body.scale = Vector3(1.0 + body_squash * 0.6, 1.0 - body_squash, 1.0 + body_squash * 0.6)
	_body.position = Vector3.UP * body_height * 0.5
	var head_height := HEAD * (1.0 - head_squash)
	_head.scale = Vector3(1.0 + head_squash * 0.7, 1.0 - head_squash, 1.0 + head_squash * 0.7)
	_head.position = Vector3.UP * (body_height + head_height * 0.5)
	var dizzy := since >= 0.0 and since < STAR_TIME
	for i in STARS:
		var star := _stars[i]
		star.visible = dizzy
		if dizzy:
			var a := since * 7.0 + i * TAU / STARS
			star.position = Vector3(cos(a) * 0.5, body_height + head_height + 0.18 + 0.05 * sin(a * 2.0), sin(a) * 0.5)
			star.rotation = Vector3(0.0, -a, 0.0)
	_body_material.emission_enabled = highlight > 0.0
	_body_material.emission = Color(1.0, 0.6, 0.2) * highlight * 0.6


func _box(
		size: Vector3,
		color: Color
) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	instance.material_override = material
	add_child(instance)
	return instance


static func _glow(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = 2.0
	return material
