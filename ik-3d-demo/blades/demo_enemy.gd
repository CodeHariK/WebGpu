## A target dummy: a capsule that gets hit, knocked, yanked, bound and thrown around by the blades.
## It falls with gravity and slides to a stop on the ground; moves that carry it (yank, whirl)
## set `yanked` and place it themselves, Chain Bind sets `bound` and it stays put.
class_name DemoEnemy
extends RefCounted

const GRAVITY := 20.0
const STAND_HEIGHT := 1.0 ## its centre when standing
const FRICTION := 8.0 ## how fast a slide stops on the ground

var position := Vector3.ZERO
var velocity := Vector3.ZERO
var flash := 0.0 ## 1 on a hit, fades out (the red glow)
var yanked := false ## a move is carrying it
var bound := 0.0 ## seconds left held by Chain Bind (can't move)
var thrown := false ## flying after a Whirl: knocks over others until it lands
var cooldown := 0.0 ## can't be body-hit again for this long
var squash := 0.0 ## 1 when smashed into the ground (a slam), springs back to 0: flattens the capsule
var mesh: MeshInstance3D
var material: StandardMaterial3D


func _init(
		parent: Node3D,
		ground_position: Vector3
) -> void:
	position = ground_position + Vector3.UP * STAND_HEIGHT
	material = StandardMaterial3D.new()
	material.albedo_color = Color(0.45, 0.55, 0.75)
	mesh = MeshInstance3D.new()
	mesh.mesh = DemoProps.capsule(0.4, 2.0)
	mesh.material_override = material
	parent.add_child(mesh)


func update(delta: float) -> void:
	flash = maxf(flash - delta * 4.0, 0.0)
	squash = maxf(squash - delta * 3.0, 0.0)
	cooldown = maxf(cooldown - delta, 0.0)
	if yanked:
		return
	if bound > 0.0:
		bound -= delta
		velocity = Vector3.ZERO
		return
	velocity.y -= GRAVITY * delta
	position += velocity * delta
	if position.y <= STAND_HEIGHT:
		position.y = STAND_HEIGHT
		thrown = false
		var slow := exp(-FRICTION * delta)
		velocity = Vector3(velocity.x * slow, 0.0, velocity.z * slow)


func draw() -> void:
	var flat := squash * squash # quick to spring back at the end
	mesh.scale = Vector3(1.0 + flat * 0.35, 1.0 - flat * 0.5, 1.0 + flat * 0.35)
	mesh.position = position - Vector3.UP * (STAND_HEIGHT * flat * 0.5) # squashed onto the ground, not into the air
	material.emission_enabled = flash > 0.0
	material.emission = Color(1, 0.3, 0.2) * flash
