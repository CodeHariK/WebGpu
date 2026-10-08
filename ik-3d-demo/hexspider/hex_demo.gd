## Hex spider test ground. The spider walks a circle (radius ~3 m round the middle) over small
## bumps, up onto a step, across a pit (no floor: those hex points get rejected), over a ramp and
## past a slope too steep to stand on. Drive it yourself to go anywhere.
##   W/S A/D  drive (auto walk stops while a key is held)     Space  auto walk on / off
##   1  foot hex: 6 corners → + centre → + second ring        2  pick: front corners ↔ pure random
##   3  legs 4 → 6 → 8      [ / ]  foot hex size      - / =  pole lift
##   R  rays on / off       P  pole hexes on / off     G  debug on / off    C  camera follow / overview
## See hex_debug.gd for the colours.
extends Node3D

const PIT := Rect2(-3.4, -1.6, 0.9, 3.2) ## x, z, width, depth of the hole in the floor
const FLOOR := 9.0 ## half size

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var spider: HexSpider
var debug: HexDebug
var follow := true
var _steps := 0
var _step_rate := 0.0
var _was_stepping: Array[bool] = []


func _ready() -> void:
	_build_ground()
	_spawn(6)
	debug = HexDebug.new()
	debug.name = "HexDebug"
	debug.spider = spider
	add_child(debug)


func _spawn(leg_count: int) -> void:
	var old := spider
	spider = HexSpider.new()
	spider.name = "HexSpider"
	spider.leg_count = leg_count
	if old != null:
		for property: String in ["rings", "ring_radius", "pole_lift", "weighted", "auto_walk"]:
			spider.set(property, old.get(property))
		spider.position = old.position
		spider.rotation = old.rotation
		old.queue_free()
	else:
		spider.position = Vector3(3.0, 0.6, 0.0)
	add_child(spider)
	if debug != null:
		debug.spider = spider
	_was_stepping.clear()


func _process(delta: float) -> void:
	if spider == null or spider.legs.is_empty():
		return
	_count_steps(delta)
	var target := spider.global_position
	var view := target + Vector3(0, 4.5, 5.5) if follow else Vector3(0, 9.5, 9.5)
	camera.global_position = camera.global_position.lerp(view, 1.0 - exp(-3.0 * delta))
	camera.look_at(target if follow else Vector3.ZERO)
	hud.text = "legs %d   foot hex %s (%.2f m)   pick %s   pole lift %.2f m   auto walk %s\nsteps %.1f /s   legs with nowhere to step: %d   speed %.1f m/s\nW/S A/D drive   Space auto   1 foot hex   2 pick   3 legs   [ ] foot hex size   - = pole lift   R rays   P poles   G debug   C camera" % [
		spider.leg_count, HexPicker.Rings.keys()[spider.rings].to_lower().replace("_", " + "), spider.ring_radius,
		"front corners" if spider.weighted else "pure random", spider.pole_lift, "on" if spider.auto_walk else "off",
		_step_rate, spider.stuck_count(), spider.velocity.length()]


func _count_steps(delta: float) -> void:
	if _was_stepping.size() != spider.legs.size():
		_was_stepping.resize(spider.legs.size())
		_was_stepping.fill(false)
	for i in spider.legs.size():
		if spider.legs[i].stepping and not _was_stepping[i]:
			_steps += 1
		_was_stepping[i] = spider.legs[i].stepping
	_step_rate = lerpf(_step_rate, 0.0, 1.0 - exp(-0.5 * delta))
	if _steps > 0:
		_step_rate += _steps * 0.5
		_steps = 0


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_SPACE: spider.auto_walk = not spider.auto_walk
		KEY_1: spider.rings = ((spider.rings + 1) % HexPicker.Rings.size()) as HexPicker.Rings
		KEY_2: spider.weighted = not spider.weighted
		KEY_3: _spawn(4 if spider.leg_count == 8 else spider.leg_count + 2)
		KEY_BRACKETLEFT: spider.ring_radius = maxf(spider.ring_radius - 0.04, 0.08)
		KEY_BRACKETRIGHT: spider.ring_radius = minf(spider.ring_radius + 0.04, 1.0)
		KEY_MINUS: spider.pole_lift = maxf(spider.pole_lift - 0.05, 0.0)
		KEY_EQUAL: spider.pole_lift = minf(spider.pole_lift + 0.05, 1.0)
		KEY_R: debug.show_rays = not debug.show_rays
		KEY_P: debug.show_poles = not debug.show_poles
		KEY_G: debug.visible = not debug.visible
		KEY_C: follow = not follow


# --- the ground ----------------------------------------------------------------------------------
# On the walk circle (radius ~2.9 m round the middle, going anticlockwise from +X when seen from
# above): bumps, a step up, the pit, a ramp, and a steep wedge beside it.

func _build_ground() -> void:
	var grass := Color(0.42, 0.52, 0.42)
	var stone := Color(0.6, 0.57, 0.55)
	# Floor (top at y = 0) in four pieces round the pit.
	var x0 := PIT.position.x
	var x1 := PIT.end.x
	var z0 := PIT.position.y
	var z1 := PIT.end.y
	_slab(-FLOOR, x0, -FLOOR, FLOOR, grass)
	_slab(x1, FLOOR, -FLOOR, FLOOR, grass)
	_slab(x0, x1, -FLOOR, z0, grass)
	_slab(x0, x1, z1, FLOOR, grass)
	_box(Vector3(PIT.get_center().x, -2.5, PIT.get_center().y), Vector3(PIT.size.x, 0.2, PIT.size.y), Color(0.1, 0.1, 0.12), false) # dark bottom, no collider
	for i in 6: # bumps
		var angle := deg_to_rad(15.0 + i * 11.0)
		var at := Vector3(2.9 * cos(angle), 0.0, -2.9 * sin(angle))
		var height := 0.08 + 0.05 * (i % 3)
		_box(at + Vector3(0.3 * (i % 2 - 0.5), height * 0.5, 0.0), Vector3(0.45, height, 0.45), stone)
	_box(Vector3(-0.2, 0.17, -2.9), Vector3(1.8, 0.34, 1.8), stone.darkened(0.1)) # a step up
	_ramp(Vector3(0.5, 0.0, 2.9), 20.0, Vector3(1.4, 0.2, 2.4), stone.lightened(0.1)) # walkable
	_ramp(Vector3(-1.4, 0.0, 2.7), 55.0, Vector3(0.9, 0.2, 1.2), Color(0.55, 0.4, 0.35)) # too steep


func _slab(xa: float, xb: float, za: float, zb: float, color: Color) -> void:
	_box(Vector3((xa + xb) * 0.5, -0.25, (za + zb) * 0.5), Vector3(xb - xa, 0.5, zb - za), color)


func _ramp(at: Vector3, degrees: float, size: Vector3, color: Color) -> void:
	var body := _box(at, size, color)
	body.rotation.x = deg_to_rad(degrees)


func _box(at: Vector3, size: Vector3, color: Color, solid := true) -> Node3D:
	var root: Node3D = StaticBody3D.new() if solid else Node3D.new()
	root.position = at
	if solid:
		var shape := BoxShape3D.new()
		shape.size = size
		var collision := CollisionShape3D.new()
		collision.shape = shape
		root.add_child(collision)
	var mesh := BoxMesh.new()
	mesh.size = size
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	var visual := MeshInstance3D.new()
	visual.mesh = mesh
	visual.material_override = material
	root.add_child(visual)
	add_child(root)
	return root
