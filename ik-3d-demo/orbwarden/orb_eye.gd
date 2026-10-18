## The Orb Warden's face: a dark visor panel with one glowing lens that slides over the sphere's
## surface to look at things. The panel is not fixed to the body — a pivot at the sphere's centre
## turns it, and the turn follows its target through a second-order spring (spider/second_order.gd)
## with negative r, so every glance winds up the wrong way first, then whips across and settles.
##
## Mood (menace 0 → 1):  cute    cyan, a big round lens, slow lazy blinks
##                       menace  red, the lids squint the lens to a slit, quicker and twitchier
## charge (0 → 1) brightens the lens and its light for the laser (OrbLaser sets it).
class_name OrbEye
extends Node3D

const CUTE := Color(0.25, 0.9, 1.0)
const MENACE := Color(1.0, 0.08, 0.03)
const PANEL := Color(0.05, 0.05, 0.06)
const LOOK_SPRING := Vector3(1.6, 0.55, -0.8) ## (f, ζ, r): r < 0 = anticipation
const MENACE_SPRING := Vector3(3.2, 0.45, -1.4) ## snappier, more wind-up when angry
const LOWEST := -0.45 ## the face never slides further under the body than this (dir.y)
const BLINK_GAP := Vector2(2.5, 6.0) ## seconds between blinks (random in this range)
const BLINK_TIME := 0.16

var radius := 1.5 ## sphere radius: the panel sits on its surface
var menace := 0.0
var charge := 0.0
var power := 1.0 ## 1 = alive … 0 = dead dark lens (the warden collapsed)
var broken := false ## the laser emitter was shot out: a dark, sputtering lens (it still looks around)
var hurt := 0.0 ## seconds of white flash left (a hit on the lens)
var look_target := Vector3.ZERO ## world point to look at
var direction := Vector3.FORWARD ## where the face points now (body space, unit)

var _spring: SecondOrder
var _panel: Node3D
var _lens: MeshInstance3D
var _lens_material: StandardMaterial3D
var _lids: Array[MeshInstance3D] = []
var _light: OmniLight3D
var _blink_wait := 3.0
var _blink := 0.0 ## seconds left in the current blink


func _ready() -> void:
	_spring = SecondOrder.new(LOOK_SPRING.x, LOOK_SPRING.y, LOOK_SPRING.z, Vector3.FORWARD)
	_build()


func _process(delta: float) -> void:
	var spring := LOOK_SPRING.lerp(MENACE_SPRING, menace)
	_spring.set_parameters(spring.x, spring.y, spring.z)
	var want := _target_direction()
	direction = _spring.update(delta, want)
	if direction.length() < 0.01:
		direction = want
	direction = direction.normalized()
	var up := Vector3.UP if absf(direction.y) < 0.98 else Vector3.BACK
	_panel.basis = Basis.looking_at(direction, up)
	_blink_wait -= delta
	if _blink_wait <= 0.0 and charge <= 0.0:
		_blink = BLINK_TIME
		_blink_wait = randf_range(BLINK_GAP.x, BLINK_GAP.y) * lerpf(1.0, 0.5, menace)
	_blink = maxf(_blink - delta, 0.0)
	_apply_look()


## The lens node (the laser emitter: OrbParts aims at it).
func lens() -> Node3D:
	return _lens


## The lens centre in world space (where the laser starts).
func lens_position() -> Vector3:
	return _lens.global_position


## Kick the face (a hit, a stomp): it jolts off and springs back.
func jolt(impulse: Vector3) -> void:
	_spring.kick(impulse)


func _target_direction() -> Vector3:
	var local := global_transform.affine_inverse() * look_target
	if local.length() < 0.01:
		return Vector3.FORWARD
	local = local.normalized()
	local.y = maxf(local.y, LOWEST)
	return local.normalized()


func _apply_look() -> void:
	var color := CUTE.lerp(MENACE, smoothstep(0.2, 0.8, menace))
	var open := lerpf(1.0, 0.32, menace) # menace squints the lens to a slit
	if _blink > 0.0:
		open *= absf(_blink / BLINK_TIME * 2.0 - 1.0) # shut and open again
	open = maxf(open, 0.04)
	_lens.scale = Vector3(lerpf(1.0, 1.15, menace), open, 1.0)
	for i in 2: # lids slide in from above and below as the lens closes
		var side := 1.0 if i == 0 else -1.0
		_lids[i].position.y = side * (0.23 + 0.2 * open)
	if broken:
		color = Color(0.35, 0.05, 0.02)
		charge = 0.0
	var glow := (2.5 + 6.0 * charge + 1.5 * menace) * power
	if broken: # sputters: mostly dark, the odd spark of light
		glow = 0.3 + (2.0 if randf() < 0.06 else 0.0)
	if hurt > 0.0:
		color = color.lerp(Color.WHITE, clampf(hurt / 0.12, 0.0, 1.0))
		glow += 6.0
		hurt = maxf(hurt - get_process_delta_time(), 0.0)
	_lens_material.albedo_color = color
	_lens_material.emission = color
	_lens_material.emission_energy_multiplier = glow
	_light.light_color = color
	_light.light_energy = 0.0 if broken else (1.5 + 6.0 * charge) * power


func _build() -> void:
	_panel = Node3D.new()
	_panel.name = "Panel"
	add_child(_panel)
	var face := Node3D.new() # on the sphere's surface, facing out along -Z
	face.position = Vector3(0, 0, -radius * 0.97)
	_panel.add_child(face)
	var plate := CylinderMesh.new() # the dark visor disc
	plate.top_radius = 0.62
	plate.bottom_radius = 0.7
	plate.height = 0.16
	face.add_child(_mesh(plate, Transform3D(Basis(Vector3.RIGHT, PI * 0.5), Vector3.ZERO), _material(PANEL, 0.35, 0.8)))
	var rim := TorusMesh.new()
	rim.inner_radius = 0.6
	rim.outer_radius = 0.72
	face.add_child(_mesh(rim, Transform3D(Basis(Vector3.RIGHT, PI * 0.5), Vector3(0, 0, -0.06)), _material(Color(0.3, 0.31, 0.34), 0.3, 0.9)))
	var lens := SphereMesh.new()
	lens.radius = 0.3
	lens.height = 0.24
	_lens_material = _material(CUTE, 0.2, 0.0)
	_lens_material.emission_enabled = true
	_lens = _mesh(lens, Transform3D(Basis.IDENTITY, Vector3(0, 0, -0.1)), _lens_material)
	face.add_child(_lens)
	for i in 2:
		var lid := BoxMesh.new()
		lid.size = Vector3(0.7, 0.3, 0.12)
		var instance := _mesh(lid, Transform3D(Basis.IDENTITY, Vector3(0, 0, -0.2)), _material(PANEL, 0.4, 0.7))
		face.add_child(instance)
		_lids.append(instance)
	_light = OmniLight3D.new()
	_light.position = Vector3(0, 0, -0.6)
	_light.omni_range = 6.0
	_light.shadow_enabled = false
	face.add_child(_light)


static func _mesh(mesh: Mesh, xform: Transform3D, material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.transform = xform
	instance.material_override = material
	return instance


static func _material(color: Color, roughness: float, metallic: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metallic
	return material
