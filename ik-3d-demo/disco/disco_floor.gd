## A grid of light-up dance tiles: one MultiMesh (one draw call) with the disco_tile shader; every
## frame each tile gets a colour and brightness from the current pattern, driven by DiscoBeat.
##
## Patterns (cycle with next_pattern(), or `auto` changes every AUTO_BARS bars):
##   pulse     every tile thumps on the beat, colour changes each bar
##   checker   two colours swap on every beat
##   ripple    rings spread from the centre, one per beat
##   sweep     a bright row runs across the floor with a fading trail
##   sparkle   random tiles flash
##   spiral    a lit arm turns around the centre
##
## warn(cell) overrides a tile with a fast orange flash: the "something is about to hit here"
## telegraph the boss will use.
class_name DiscoFloor
extends Node3D

const P := preload("res://disco/disco_palette.gd")
const TILE := 2.2 ## metres between tile centres
const GAP := 0.1 ## gap between tiles
const THICKNESS := 0.24
const PATTERNS := ["pulse", "checker", "ripple", "sweep", "sparkle", "spiral"]
const AUTO_BARS := 4

var beat: DiscoBeat
var columns := 9
var rows := 9
var pattern := "ripple"
var auto := true
var brightness := 1.0
var colour_shift := 0 ## offsets which palette colours this floor uses (so floors differ)

var _multimesh := MultiMesh.new()
var _warn := {} ## Vector2i cell -> seconds left
var _material := ShaderMaterial.new()


func _ready() -> void:
	var box := BoxMesh.new()
	box.size = Vector3(TILE - GAP, THICKNESS, TILE - GAP)
	_material.shader = preload("res://disco/disco_tile.gdshader")
	_material.set_shader_parameter("half_size", box.size * 0.5)
	box.material = _material
	_multimesh.transform_format = MultiMesh.TRANSFORM_3D
	_multimesh.use_custom_data = true
	_multimesh.mesh = box
	_multimesh.instance_count = columns * rows
	for cell_index in columns * rows:
		var cell := Vector2i(cell_index % columns, cell_index / columns)
		_multimesh.set_instance_transform(cell_index, Transform3D(Basis.IDENTITY, _local_center(cell)))
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = _multimesh
	add_child(instance)
	if beat:
		beat.bar_hit.connect(_on_bar)


func _process(delta: float) -> void:
	for cell: Vector2i in _warn.keys():
		_warn[cell] -= delta
		if _warn[cell] <= 0.0:
			_warn.erase(cell)
	for cell_index in columns * rows:
		var cell := Vector2i(cell_index % columns, cell_index / columns)
		_multimesh.set_instance_custom_data(cell_index, _tile(cell))


## Flash `cell` orange for `seconds` (a telegraph).
func warn(cell: Vector2i, seconds := 1.0) -> void:
	_warn[cell] = seconds


## The next pattern in PATTERNS.
func next_pattern() -> void:
	pattern = PATTERNS[(PATTERNS.find(pattern) + 1) % PATTERNS.size()]


## World position of the top centre of `cell`.
func cell_center(cell: Vector2i) -> Vector3:
	return global_transform * (_local_center(cell) + Vector3.UP * THICKNESS * 0.5)


## The cell under a world position (may be outside the grid: check has_cell()).
func cell_at(world: Vector3) -> Vector2i:
	var local := global_transform.affine_inverse() * world
	return Vector2i(roundi(local.x / TILE + (columns - 1) * 0.5), roundi(local.z / TILE + (rows - 1) * 0.5))


func has_cell(cell: Vector2i) -> bool:
	return cell.x >= 0 and cell.y >= 0 and cell.x < columns and cell.y < rows


## Half the floor's size (x, z), metres.
func half_extent() -> Vector2:
	return Vector2(columns, rows) * TILE * 0.5


func _local_center(cell: Vector2i) -> Vector3:
	return Vector3((cell.x - (columns - 1) * 0.5) * TILE, 0.0, (cell.y - (rows - 1) * 0.5) * TILE)


func _on_bar(bar: int) -> void:
	if auto and bar % AUTO_BARS == 0:
		next_pattern()


# Colour (rgb) + brightness (a) of one tile right now.
func _tile(cell: Vector2i) -> Color:
	if _warn.has(cell):
		var flash := 0.5 + 0.5 * sin(_warn[cell] * 40.0)
		return Color(P.WARN.r, P.WARN.g, P.WARN.b, 0.6 + flash * 1.2)
	if beat == null:
		return Color(P.MAGENTA.r, P.MAGENTA.g, P.MAGENTA.b, 0.3)
	var lit := _pattern(cell)
	var colour: Color = lit[0]
	return Color(colour.r, colour.g, colour.b, float(lit[1]) * brightness)


# [Color, brightness] for `cell` under the current pattern.
func _pattern(cell: Vector2i) -> Array:
	var centre := Vector2(columns - 1, rows - 1) * 0.5
	var offset := Vector2(cell) - centre
	var b := beat.beat + colour_shift
	var thump := beat.pulse()
	match pattern:
		"pulse":
			return [P.pick(beat.bar() + colour_shift), 0.2 + 0.8 * thump]
		"checker":
			var on := (cell.x + cell.y + b) % 2 == 0
			return [P.pick(b if on else b + 2), 0.85 * (0.4 + 0.6 * thump) if on else 0.12]
		"ripple":
			var radius := (beat.beat_in_bar() + beat.phase) * 1.6
			var ring := clampf(1.0 - absf(offset.length() - radius) / 1.3, 0.0, 1.0)
			return [P.pick(b), 0.03 + ring * ring * 1.3]
		"sweep":
			var head := fposmod((beat.beat + beat.phase) * 2.0, float(columns + 4)) - 2.0
			var behind := head - cell.x
			var trail := clampf(1.0 - behind / 3.0, 0.0, 1.0) if behind >= 0.0 else 0.0
			return [P.pick(cell.y + beat.bar()), 0.08 + trail * 1.1]
		"sparkle":
			var roll := hash(Vector3i(cell.x, cell.y, beat.beat))
			var on := roll % 5 == 0
			return [P.pick(roll % 5), 0.08 + (1.2 * thump if on else 0.0)]
		"spiral":
			var angle := fposmod(atan2(offset.y, offset.x) - (beat.beat + beat.phase) * PI * 0.5, TAU)
			var arm := clampf(1.0 - angle / 1.4, 0.0, 1.0)
			return [P.pick(b + int(offset.length())), 0.1 + arm * (0.6 + 0.5 * thump)]
	return [P.MAGENTA, 0.3]
