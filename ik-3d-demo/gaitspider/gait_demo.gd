## Gait spider test ground: an 8-legged spider stepped by a clock (GaitPattern) walks a circle over
## bumps, a step, a pit and a ramp. The footfall chart (bottom left) shows the pattern.
##   W/S A/D  drive        Space  auto walk on / off      1  gait: spider → tetrapod → ripple → insect
##   [ / ]  time on the ground (duty)    - / =  wave       , / .  speed      G  debug   H  chart   C  camera
extends Node3D

const PIT := Rect2(-3.4, -1.6, 0.9, 3.2) ## x, z, width, depth of the hole in the floor
const FLOOR := 9.0

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var spider: GaitSpider
var debug: GaitDebug
var chart: GaitChart
var follow := true


func _ready() -> void:
	_build_ground()
	spider = GaitSpider.new()
	spider.name = "GaitSpider"
	spider.position = Vector3(3.0, 0.4, 0.0)
	add_child(spider)
	debug = GaitDebug.new()
	debug.name = "GaitDebug"
	debug.spider = spider
	add_child(debug)
	chart = GaitChart.new()
	chart.name = "GaitChart"
	chart.spider = spider
	$Hud.add_child(chart)
	chart.anchor_top = 1.0
	chart.anchor_bottom = 1.0
	chart.offset_left = 12.0
	chart.offset_top = -150.0


func _process(delta: float) -> void:
	if spider == null or spider.legs.is_empty():
		return
	var target := spider.global_position
	var view := target + Vector3(2.5, 2.6, 3.8) if follow else Vector3(0, 9.5, 9.5)
	camera.global_position = camera.global_position.lerp(view, 1.0 - exp(-3.0 * delta))
	camera.look_at(target if follow else Vector3.ZERO)
	var p := spider.pattern
	hud.text = "gait %s   legs %d   on the ground %.0f%%   wave %.2f   cycle %.2f s   speed %.1f m/s   auto %s\nW/S A/D drive   Space auto   1 gait   [ ] duty   - = wave   , . speed   G debug   H chart   C camera" % [
		GaitPattern.Preset.keys()[spider.preset].to_lower(), spider.legs.size(), p.duty * 100.0, p.wave, spider.period,
		spider.velocity.length(), "on" if spider.auto_walk else "off"]


func _unhandled_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	var p := spider.pattern
	match key.keycode:
		KEY_SPACE: spider.auto_walk = not spider.auto_walk
		KEY_1: spider.set_preset(((spider.preset + 1) % GaitPattern.Preset.size()) as GaitPattern.Preset)
		KEY_BRACKETLEFT: p.duty = maxf(p.duty - 0.05, 0.3)
		KEY_BRACKETRIGHT: p.duty = minf(p.duty + 0.05, 0.9)
		KEY_MINUS: p.wave = maxf(p.wave - 0.02, -0.25)
		KEY_EQUAL: p.wave = minf(p.wave + 0.02, 0.25)
		KEY_COMMA: spider.move_speed = maxf(spider.move_speed - 0.2, 0.2)
		KEY_PERIOD: spider.move_speed = minf(spider.move_speed + 0.2, 4.0)
		KEY_G: debug.visible = not debug.visible
		KEY_H: chart.visible = not chart.visible
		KEY_C: follow = not follow


# --- the ground (a circle of radius ~2.9 m round the middle: bumps, a step, the pit, a ramp) ------

func _build_ground() -> void:
	var grass := Color(0.72, 0.67, 0.58) # the reference's warm paper floor
	var stone := Color(0.66, 0.62, 0.58)
	var x0 := PIT.position.x
	var x1 := PIT.end.x
	_slab(-FLOOR, x0, -FLOOR, FLOOR, grass)
	_slab(x1, FLOOR, -FLOOR, FLOOR, grass)
	_slab(x0, x1, -FLOOR, PIT.position.y, grass)
	_slab(x0, x1, PIT.end.y, FLOOR, grass)
	for i in 6:
		var angle := deg_to_rad(15.0 + i * 11.0)
		var at := Vector3(2.9 * cos(angle), 0.0, -2.9 * sin(angle))
		var height := 0.08 + 0.05 * (i % 3)
		_box(at + Vector3(0.3 * (i % 2 - 0.5), height * 0.5, 0.0), Vector3(0.45, height, 0.45), stone)
	_box(Vector3(-0.2, 0.17, -2.9), Vector3(1.8, 0.34, 1.8), stone.darkened(0.1))
	var ramp := _box(Vector3(0.5, 0.0, 2.9), Vector3(1.4, 0.2, 2.4), stone.lightened(0.1))
	ramp.rotation.x = deg_to_rad(20.0)


func _slab(xa: float, xb: float, za: float, zb: float, color: Color) -> void:
	_box(Vector3((xa + xb) * 0.5, -0.25, (za + zb) * 0.5), Vector3(xb - xa, 0.5, zb - za), color)


func _box(at: Vector3, size: Vector3, color: Color) -> Node3D:
	var root := StaticBody3D.new()
	root.position = at
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
