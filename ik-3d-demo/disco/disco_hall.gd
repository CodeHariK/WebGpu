## The Boogey-Pod's dance hall: the main light-up floor (where the boss stands, centre), two raised
## side stages with stairs, a DJ stage at the back between speaker stacks, a back wall with neon
## strips and a sign, pillars and an overhead truss for the moving lights (DiscoLights).
##
##   top view (z+ toward the camera)        main floor   9 × 9 tiles at ground level
##     ┌──────── wall + sign ────────┐      side stages  2 × 5 tiles, 1.8 m up, stairs at the front
##     │ spk   [  DJ stage  ]   spk  │      DJ stage     5 × 2 tiles, 2.6 m up
##     │ ┌──┐ ┌───────────┐ ┌──┐     │
##     │ │  │ │   main    │ │  │     │      everything else is dark: the floors and neon carry the light
##     │ └╥─┘ └───────────┘ └─╥┘     │
##     └──────────────────────────────┘
class_name DiscoHall
extends Node3D

const P := preload("res://disco/disco_palette.gd")
const GROUND_Y := -0.7
const STAGE_HEIGHT := 1.8
const DJ_HEIGHT := 2.6
const WALL_Z := -18.0
const TRUSS_Y := 12.5
const TRUSS_HALF := 15.0

var beat: DiscoBeat
var floors: Array[DiscoFloor] = [] ## [main, left stage, right stage, DJ stage]
var main_floor: DiscoFloor

var _speakers: Array[Node3D] = [] ## cones that thump on the beat
var _dark := P.surface(Color(0.09, 0.06, 0.14), 0.35, 0.3)
var _metal := P.surface(Color(0.25, 0.22, 0.3), 0.3, 0.8)


func _ready() -> void:
	_add_ground()
	main_floor = _add_floor(Vector3.ZERO, 9, 9, 0, P.MAGENTA)
	var side_x := 9.9 + 1.0 + 2.2
	for side: float in [-1.0, 1.0]:
		var stage := _add_floor(Vector3(side_x * side, STAGE_HEIGHT, 0.0), 2, 5, 1 if side < 0.0 else 3, P.CYAN)
		stage.pattern = "sweep"
		_add_stairs(Vector3(side_x * side, STAGE_HEIGHT, 5.5))
	var dj := _add_floor(Vector3(0.0, DJ_HEIGHT, -13.1), 5, 2, 2, P.PURPLE)
	dj.pattern = "checker"
	for side: float in [-1.0, 1.0]:
		_add_speaker_stack(Vector3(8.6 * side, GROUND_Y, -13.6))
	_add_wall()
	_add_truss()


func _process(_delta: float) -> void:
	if beat == null:
		return
	var thump := beat.pulse(9.0)
	for cone in _speakers:
		cone.scale = Vector3(1.0, 1.0, 1.0 + thump * 0.8)


# --- floors ----------------------------------------------------------------------------------

# A tile grid at `at` (top of its base), on a dark base block with a neon edge.
func _add_floor(at: Vector3, columns: int, rows: int, shift: int, trim: Color) -> DiscoFloor:
	var grid := DiscoFloor.new()
	grid.beat = beat
	grid.columns = columns
	grid.rows = rows
	grid.colour_shift = shift
	grid.position = at
	add_child(grid)
	floors.append(grid)
	var half := grid.half_extent() + Vector2(0.35, 0.35)
	var base_top := at.y - DiscoFloor.THICKNESS * 0.5
	var height := base_top - GROUND_Y
	_box(Vector3(half.x * 2.0, height, half.y * 2.0), Vector3(at.x, GROUND_Y + height * 0.5, at.z), _dark)
	_add_edge(Vector3(at.x, base_top, at.z), half, trim)
	return grid


# A thin neon line around a rectangle (half size `half`) at height `at.y`.
func _add_edge(at: Vector3, half: Vector2, color: Color) -> void:
	var glow := P.neon(color, 4.0)
	var t := 0.09
	_box(Vector3(half.x * 2.0 + t, t, t), at + Vector3(0, 0, half.y), glow)
	_box(Vector3(half.x * 2.0 + t, t, t), at + Vector3(0, 0, -half.y), glow)
	_box(Vector3(t, t, half.y * 2.0), at + Vector3(half.x, 0, 0), glow)
	_box(Vector3(t, t, half.y * 2.0), at + Vector3(-half.x, 0, 0), glow)


# A neon outline of a rectangle facing +z (half size `half`), centred on `at`.
func _add_frame(at: Vector3, half: Vector2, color: Color) -> void:
	var glow := P.neon(color, 3.5)
	var t := 0.07
	_box(Vector3(half.x * 2.0, t, t), at + Vector3(0, half.y, 0), glow)
	_box(Vector3(half.x * 2.0, t, t), at + Vector3(0, -half.y, 0), glow)
	_box(Vector3(t, half.y * 2.0, t), at + Vector3(half.x, 0, 0), glow)
	_box(Vector3(t, half.y * 2.0, t), at + Vector3(-half.x, 0, 0), glow)


# Steps from a stage's front edge (`top` = top-centre of that edge) down to the ground, toward +z.
func _add_stairs(top: Vector3) -> void:
	var drop := top.y - DiscoFloor.THICKNESS * 0.5 - GROUND_Y
	var steps := 6
	var rise := drop / steps
	var depth := 0.55
	var width := 3.4
	var glow := P.neon(P.CYAN, 3.0)
	for i in steps:
		var step_top := top.y - DiscoFloor.THICKNESS * 0.5 - rise * (i + 1)
		var z := top.z + 0.35 + depth * (i + 0.5)
		var height := step_top - GROUND_Y
		_box(Vector3(width, height, depth), Vector3(top.x, GROUND_Y + height * 0.5, z), _dark)
		_box(Vector3(width, 0.05, 0.06), Vector3(top.x, step_top, z + depth * 0.5), glow) # nosing


# --- set dressing ----------------------------------------------------------------------------

func _add_ground() -> void:
	var plane := PlaneMesh.new()
	plane.size = Vector2(160, 160)
	var instance := MeshInstance3D.new()
	instance.mesh = plane
	instance.position.y = GROUND_Y
	var material := ShaderMaterial.new()
	material.shader = preload("res://disco/disco_ground.gdshader")
	instance.material_override = material # glossy black + a fading neon grid
	add_child(instance)


# A tower of speaker boxes; each cone thumps on the beat.
func _add_speaker_stack(at: Vector3) -> void:
	var sizes := [Vector3(3.2, 2.6, 2.4), Vector3(2.8, 2.2, 2.2), Vector3(2.2, 1.6, 2.0)]
	var y := at.y
	for i in sizes.size():
		var size: Vector3 = sizes[i]
		_box(size, Vector3(at.x, y + size.y * 0.5, at.z), _dark)
		var front := at.z + size.z * 0.5
		var cone_radius := size.y * 0.33
		_add_cone(Vector3(at.x, y + size.y * 0.5, front), cone_radius, P.pick(i + 1))
		_add_frame(Vector3(at.x, y + size.y * 0.5, front + 0.03), Vector2(size.x, size.y) * 0.5, P.MAGENTA)
		y += size.y


func _add_cone(at: Vector3, radius: float, rim_color: Color) -> void:
	var pivot := Node3D.new()
	pivot.position = at
	add_child(pivot)
	var rim := TorusMesh.new()
	rim.inner_radius = radius * 0.86
	rim.outer_radius = radius
	var ring := MeshInstance3D.new()
	ring.mesh = rim
	ring.rotation.x = PI * 0.5
	ring.material_override = P.neon(rim_color, 4.0)
	pivot.add_child(ring)
	var cone := CylinderMesh.new()
	cone.top_radius = radius * 0.15
	cone.bottom_radius = radius * 0.85
	cone.height = 0.25
	var body := MeshInstance3D.new()
	body.mesh = cone
	body.rotation.x = PI * 0.5
	body.position.z = 0.02
	body.material_override = _metal
	pivot.add_child(body)
	_speakers.append(pivot)


func _add_wall() -> void:
	_box(Vector3(70.0, 22.0, 1.0), Vector3(0.0, GROUND_Y + 11.0, WALL_Z), _dark)
	for i in range(-5, 6):
		var x := i * 5.5
		if absf(x) < 7.0:
			continue # leave the middle for the sign
		_box(Vector3(0.14, 16.0, 0.1), Vector3(x, GROUND_Y + 9.0, WALL_Z + 0.55), P.neon(P.pick(i), 3.5))
	var neon_sign := Label3D.new()
	neon_sign.text = "BOOGEY NIGHT"
	neon_sign.font_size = 220
	neon_sign.pixel_size = 0.012
	neon_sign.outline_size = 36
	neon_sign.modulate = Color(3.0, 0.5, 2.2)
	neon_sign.outline_modulate = Color(0.2, 1.6, 2.4)
	neon_sign.position = Vector3(0.0, GROUND_Y + 12.5, WALL_Z + 0.6)
	add_child(neon_sign)


# Four pillars and a square truss overhead, with two cross bars the moving lights hang from.
func _add_truss() -> void:
	var h := TRUSS_HALF
	for corner: Vector2 in [Vector2(-h, -h), Vector2(h, -h), Vector2(-h, h), Vector2(h, h)]:
		var height := TRUSS_Y - GROUND_Y
		_box(Vector3(0.9, height, 0.9), Vector3(corner.x, GROUND_Y + height * 0.5, corner.y), _metal)
		for band: float in [0.2, 0.45]:
			_box(Vector3(1.0, 0.12, 1.0), Vector3(corner.x, GROUND_Y + height * band, corner.y), P.neon(P.CYAN, 3.0))
	var bar := 0.45
	for z: float in [-h, h, -h * 0.5, h * 0.5]:
		_box(Vector3(h * 2.0, bar, bar), Vector3(0.0, TRUSS_Y, z), _metal)
	for x: float in [-h, h]:
		_box(Vector3(bar, bar, h * 2.0), Vector3(x, TRUSS_Y, 0.0), _metal)


func _box(size: Vector3, at: Vector3, material: Material) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = at
	instance.material_override = material
	add_child(instance)
	return instance
