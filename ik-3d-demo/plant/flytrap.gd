## A dancing Venus flytrap: a terracotta pot with leaves round the rim and `heads` stalks (one big
## main head in the middle, smaller ones leaning out around it). All of them dance to one shared
## beat (`tempo` beats per second), each side stalk a little out of step, and each snaps on its
## own when the target comes within reach (see FlytrapStalk).
class_name Flytrap
extends Node3D

const POT_HEIGHT := 0.45
const POT_COLOR := Color(0.78, 0.42, 0.26)
const SOIL_COLOR := Color(0.25, 0.17, 0.11)
const LEAF_COLOR := Color(0.32, 0.58, 0.22)

@export var target: Node3D ## the prey it watches and snaps at
@export_range(1, 5) var heads := 3:
	set(value):
		heads = value
		if is_inside_tree():
			_build_stalks()
@export var dance := true
@export var tempo := 1.1 ## beats per second: one sway per 2 beats, a chomp on every beat
@export var sway := 0.3 ## radians per bone at the top of a stem
@export var snap_range := 2.1 ## metres from a head that sets it snapping

var beat := 0.0 ## the shared dance clock, in beats
var stalks: Array[FlytrapStalk] = []


func _ready() -> void:
	_build_pot()
	_build_stalks()


func _process(delta: float) -> void:
	beat += delta * tempo
	for stalk in stalks:
		stalk.target = target
		stalk.dance = dance
		stalk.sway = sway
		stalk.snap_range = snap_range * stalk.head_scale
		stalk.animate(delta, beat)


## Every head snaps now.
func snap_all() -> void:
	for stalk in stalks:
		stalk.snap()


# The main stalk straight up from the middle; the others lean out evenly around it, smaller,
# each a little further out of step with the dance.
func _build_stalks() -> void:
	for stalk in stalks:
		stalk.queue_free()
	stalks.clear()
	stalks.append(_add_stalk(Vector3(0, POT_HEIGHT, 0), Vector3.FORWARD, 0.0, 7, 0.17, 0.06, 1.0, 0.0))
	var sides := heads - 1
	for i in sides:
		var angle := TAU * i / sides + 0.4
		var outward := Vector3(sin(angle), 0.0, -cos(angle))
		stalks.append(_add_stalk(Vector3(0, POT_HEIGHT, 0) + outward * 0.18, outward, 0.55, 5, 0.14, 0.04, 0.65, 0.3 * (i + 1) / sides))


func _add_stalk(at: Vector3, outward: Vector3, tilt: float, segments: int, length: float, thickness: float, head_scale: float, phase: float) -> FlytrapStalk:
	var stalk := FlytrapStalk.new()
	stalk.name = "Stalk"
	stalk.position = at
	stalk.outward = outward
	stalk.tilt = tilt
	stalk.segments = segments
	stalk.segment_length = length
	stalk.thickness = thickness
	stalk.head_scale = head_scale
	stalk.phase = phase
	add_child(stalk)
	return stalk


# A tapered terracotta pot with a rim, dark soil, and leaves fanned out over the rim.
func _build_pot() -> void:
	var pot := CylinderMesh.new()
	pot.top_radius = 0.42
	pot.bottom_radius = 0.32
	pot.height = POT_HEIGHT
	add_child(_mesh(pot, Transform3D(Basis.IDENTITY, Vector3(0, POT_HEIGHT * 0.5, 0)), POT_COLOR))
	var rim := TorusMesh.new()
	rim.inner_radius = 0.38
	rim.outer_radius = 0.47
	add_child(_mesh(rim, Transform3D(Basis.IDENTITY, Vector3(0, POT_HEIGHT - 0.01, 0)), POT_COLOR.darkened(0.1)))
	var soil := CylinderMesh.new()
	soil.top_radius = 0.4
	soil.bottom_radius = 0.4
	soil.height = 0.03
	add_child(_mesh(soil, Transform3D(Basis.IDENTITY, Vector3(0, POT_HEIGHT - 0.02, 0)), SOIL_COLOR))
	for i in 7:
		var angle := TAU * i / 7.0
		var leaf := SphereMesh.new()
		leaf.radius = 1.0
		leaf.height = 2.0
		var basis := Basis(Vector3.UP, -angle) * Basis(Vector3.RIGHT, -0.35) * Basis.from_scale(Vector3(0.09, 0.025, 0.28))
		var spot := Vector3(sin(angle), 0.0, -cos(angle)) * 0.36 + Vector3(0, POT_HEIGHT + 0.02, 0)
		add_child(_mesh(leaf, Transform3D(basis, spot), LEAF_COLOR))


static func _mesh(mesh: Mesh, xform: Transform3D, color: Color) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.8
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.transform = xform
	return instance
