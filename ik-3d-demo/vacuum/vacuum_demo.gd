## Ghost-vacuum prototype: a tug-of-war capture in the style of Luigi's Mansion, all procedural.
## Hold suck with the ghost in front of the nozzle to grab it; it bolts and drags you about, so
## push the stick AGAINST its pull to fill the power meter (it drains the ghost faster); when the
## meter is full, slam it over your head. At zero health it's sucked in.
##   WASD / arrows  walk        Space (hold)  suck        E  slam (power full)      R  new ghost
## Files: vacuum_tug.gd (the mechanic), vacuum_hunter.gd (arms + rope hose), vacuum_ghost.gd
## (stretchy ghost + rope tail), vacuum_beam.gd (the bending stream); chains from limbs/.
## Order every frame: tug moves things → hunter, ghost, stream pose → renderers draw.
extends Node3D

const CAMERA_OFFSET := Vector3(0, 4.5, 5.5)

@onready var camera: Camera3D = $Camera3D
@onready var hud: Label = $Hud/Label

var hunter: VacuumHunter
var ghost: VacuumGhost
var beam: VacuumBeam
var tug: VacuumTug
var limbs: LimbRenderer
## Tests fill this to drive the demo: {"move": Vector3, "suck": bool, "slam": bool}. Empty = keys.
var scripted := {}

var _slam_was_down := false
var _time := 0.0


func _ready() -> void:
	_add_room()
	limbs = LimbRenderer.new()
	limbs.name = "Limbs"
	limbs.roughness = 0.6
	add_child(limbs)
	hunter = VacuumHunter.new()
	hunter.name = "Hunter"
	add_child(hunter)
	hunter.setup(limbs)
	ghost = VacuumGhost.new()
	ghost.name = "Ghost"
	add_child(ghost)
	ghost.position = Vector3(0, VacuumTug.HOVER, -4.0)
	ghost.setup(limbs)
	beam = VacuumBeam.new()
	beam.name = "Beam"
	add_child(beam)
	tug = VacuumTug.new(hunter, ghost)
	camera.position = hunter.position + CAMERA_OFFSET
	camera.look_at(hunter.position + Vector3(0, 0.8, 0))


func _process(delta: float) -> void:
	_time += delta
	var controls := _controls()
	hunter.input = controls.move
	tug.update(delta, controls.suck, controls.slam)
	hunter.pose(delta)
	ghost.pose(delta)
	beam.active = tug.streaming()
	beam.update(delta, tug.mouth(), tug.mouth_direction(), ghost.global_position)
	limbs.draw()
	_follow(delta)
	_update_hud()


func _controls() -> Dictionary:
	if not scripted.is_empty():
		return {"move": scripted.get("move", Vector3.ZERO), "suck": scripted.get("suck", false), "slam": scripted.get("slam", false)}
	var x := float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT)) - float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT))
	var z := float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN)) - float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP))
	var move := Vector3(x, 0, z)
	var slam_down := Input.is_key_pressed(KEY_E)
	var slam := slam_down and not _slam_was_down
	_slam_was_down = slam_down
	return {"move": move.normalized() if move.length() > 1.0 else move, "suck": Input.is_key_pressed(KEY_SPACE), "slam": slam}


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key != null and key.pressed and not key.echo and key.keycode == KEY_R:
		tug.respawn()


func _follow(delta: float) -> void:
	var focus := hunter.global_position.lerp(ghost.global_position, 0.3 if tug.streaming() else 0.0)
	camera.position = camera.position.lerp(focus + CAMERA_OFFSET, 1.0 - exp(-4.0 * delta))
	var jolt := Vector3(sin(_time * 70.0), sin(_time * 83.0), 0.0) * 0.15 * tug.shake * tug.shake
	camera.look_at(focus + Vector3(0, 0.8, 0) + jolt)


func _update_hud() -> void:
	var tip := {
		"free": "walk up to the ghost, face it and hold Space",
		"captured": "pull AGAINST the ghost to fill power%s" % ("   —   POWER FULL: press E to slam!" if tug.power >= 1.0 else ""),
		"slam": "SLAM!",
		"suck_in": "gotcha…",
		"caught": "caught! another one is coming",
	}
	hud.text = "%s   ghost %s   power %s   caught %d\n%s\nWASD walk   Space suck   E slam   R new ghost" % [
		tug.state.to_upper(), _bar(ghost.health / VacuumGhost.MAX_HEALTH), _bar(tug.power), tug.caught, tip[tug.state],
	]


static func _bar(fraction: float) -> String:
	var filled := roundi(clampf(fraction, 0.0, 1.0) * 12.0)
	return "[" + "■".repeat(filled) + "·".repeat(12 - filled) + "]"


# A haunted room: floor, low walls, a few crates and a rug.
func _add_room() -> void:
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(VacuumTug.ARENA * 2.0 + 2.0, VacuumTug.ARENA * 2.0 + 2.0)
	add_child(_mesh(floor_mesh, Transform3D.IDENTITY, Color(0.42, 0.33, 0.45)))
	var rug := PlaneMesh.new()
	rug.size = Vector2(5, 3.5)
	add_child(_mesh(rug, Transform3D(Basis.IDENTITY, Vector3(0, 0.01, 0)), Color(0.65, 0.25, 0.3)))
	var wall := BoxMesh.new()
	var span := VacuumTug.ARENA * 2.0 + 2.0
	for side in 4:
		var along_x := side < 2
		wall.size = Vector3(span, 1.2, 0.3) if along_x else Vector3(0.3, 1.2, span)
		var offset := (VacuumTug.ARENA + 1.0) * (1.0 if side % 2 == 0 else -1.0)
		var at := Vector3(0, 0.6, offset) if along_x else Vector3(offset, 0.6, 0)
		add_child(_mesh(wall.duplicate(), Transform3D(Basis.IDENTITY, at), Color(0.3, 0.24, 0.35)))
	var crate := BoxMesh.new()
	crate.size = Vector3(0.8, 0.8, 0.8)
	for spot: Vector3 in [Vector3(-5, 0.4, -5), Vector3(5.5, 0.4, -4), Vector3(-5.5, 0.4, 4.5), Vector3(4.5, 0.4, 5.5)]:
		add_child(_mesh(crate, Transform3D(Basis(Vector3.UP, spot.x * 0.3), spot), Color(0.6, 0.45, 0.3)))


static func _mesh(mesh: Mesh, xform: Transform3D, color: Color) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.transform = xform
	return instance
