## A heavy slab stuck in the ground that the Rip move (RipMove) tears out: it stands on `base` and
## tilts about a horizontal `axis` through its foot; once ripped it flies (ballistic) and lands on its
## side. Drawn as a stone-coloured box.
class_name RipProp
extends RefCounted

const SIZE := Vector3(1.2, 1.8, 0.7) ## width, height, depth
const GRAVITY := 22.0

var base := Vector3.ZERO ## the foot it stands and tilts on (on the ground)
var axis := Vector3.RIGHT ## horizontal: tilting by +angle tips its top toward whoever set the axis
var tilt := 0.0 ## radians about `axis`
var stuck := true ## still in the ground
var flying := false
var velocity := Vector3.ZERO
var mesh: MeshInstance3D


func _init(
		parent: Node3D,
		ground_position: Vector3
) -> void:
	base = ground_position
	var box := BoxMesh.new()
	box.size = SIZE
	mesh = DemoProps.mesh_instance(parent, box, Color(0.42, 0.4, 0.38))


## A point on the slab: `up` m up its height from the foot, `out` m along the axis (sideways).
func point(
		up: float,
		out: float
) -> Vector3:
	return base + Basis(axis, tilt) * (Vector3.UP * up) + axis * out


## Where a chain grips it: near the top, out to that blade's side.
func grip(side: float) -> Vector3:
	return point(SIZE.y * 0.85, side * 0.3)


## Ballistic flight; true on the frame it lands.
func update(delta: float) -> bool:
	if not flying:
		return false
	velocity.y -= GRAVITY * delta
	base += velocity * delta
	if base.y <= 0.0 and velocity.y < 0.0:
		base.y = 0.0
		flying = false
		velocity = Vector3.ZERO
		return true
	return false


func draw() -> void:
	mesh.global_transform = Transform3D(Basis(axis, tilt), base) * Transform3D(Basis.IDENTITY, Vector3.UP * SIZE.y * 0.5)
