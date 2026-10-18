## The Orb Warden's weak spots: the parts that can be shot off — power cores, missile pods, the
## gun turret and the laser emitter (the eye's lens). Legs never break (see OrbLimbs: stagger).
##
## Each part is a node riding the warden's body plus a hit sphere (centre offset + radius), a
## health, and the glowing materials that show its state: they flash white on a hit, shift toward
## orange and flicker as it weakens, and on breaking go dark and charred while the wreck smoulders
## (sparks and drips). A part can be invulnerable at times (`vulnerable`, e.g. the laser emitter
## only while it charges or fires): shots then glance off with a few blue sparks.
## OrbWarden reacts to `part_broken` (a weapon stops working, power drops …).
class_name OrbParts
extends Node3D

signal part_hit(part: Part)
signal part_broken(part: Part)

const HURT := Color(1.0, 0.45, 0.05)
const CHAR := Color(0.06, 0.05, 0.05)
const EMBER := Color(1.0, 0.25, 0.04)
const FLASH_TIME := 0.12
const SMOULDER_GAP := 0.15 ## seconds between drips from a broken part
const GLANCE := Color(0.5, 0.8, 1.0) ## sparks when a shot hits a protected part

class Part:
	var name := ""
	var kind := "" ## "core", "pod", "gun", "laser"
	var node: Node3D ## the part's visual root (rides the body)
	var center := Vector3.ZERO ## hit sphere centre in node space
	var radius := 0.3
	var health := 1.0
	var max_health := 1.0
	var glows: Array[StandardMaterial3D] = [] ## lit materials: show its state
	var color := Color.WHITE ## their healthy colour
	var energy := 3.0 ## their healthy glow
	var vulnerable := Callable() ## () -> bool; unset = always
	var alive := true
	var flash := 0.0
	var dead_time := 0.0

	func position() -> Vector3:
		return node.global_transform * center

	func open() -> bool:
		return alive and (not vulnerable.is_valid() or vulnerable.call())

var sparks: OrbSparks
var parts: Array[Part] = []
var floor_y := 0.0 ## ground height under the warden: where sparks land

var _time := 0.0
var _smoulder := 0.0


func clear() -> void:
	parts.clear()


## Register a part. `glows` are the materials to light / char; `color` and `energy` their
## healthy look.
func add(part_name: String, kind: String, node: Node3D, center: Vector3, radius: float, health: float, glows: Array[StandardMaterial3D], color: Color, energy := 3.0, vulnerable := Callable()) -> Part:
	var part := Part.new()
	part.name = part_name
	part.kind = kind
	part.node = node
	part.center = center
	part.radius = radius
	part.health = health
	part.max_health = health
	part.glows = glows
	part.color = color
	part.energy = energy
	part.vulnerable = vulnerable
	parts.append(part)
	return part


## How many parts of `kind` are still whole.
func alive(kind: String) -> int:
	var count := 0
	for part in parts:
		if part.kind == kind and part.alive:
			count += 1
	return count


func total(kind: String) -> int:
	var count := 0
	for part in parts:
		if part.kind == kind:
			count += 1
	return count


## The first whole part a shot along `direction` from `from` passes through: {part, along}, or {}.
func pick(from: Vector3, direction: Vector3) -> Dictionary:
	var best := {}
	var nearest := INF
	var dir := direction.normalized()
	for part in parts:
		if not part.alive or not is_instance_valid(part.node):
			continue
		var offset := part.position() - from
		var along := offset.dot(dir)
		if along <= 0.0 or along >= nearest:
			continue
		if (offset - dir * along).length() < part.radius:
			best = {"part": part, "along": along}
			nearest = along
	return best


## Shoot a part: damage if it's open, else the shot glances off. Returns true if it broke.
func hit(part: Part, damage: float, direction: Vector3) -> bool:
	if not part.alive:
		return false
	var at := part.position()
	if not part.open():
		if sparks != null:
			sparks.burst(at, -direction, 6, 5.0, 0.6, 0.25, 0.025, floor_y, GLANCE)
		return false
	part.health -= damage
	part.flash = FLASH_TIME
	if sparks != null:
		sparks.burst(at, -direction, 10, 6.0, 0.8, 0.35, 0.03, floor_y)
	part_hit.emit(part)
	if part.health <= 0.0:
		destroy(part)
		return true
	return false


## Blow a part off: it goes dark and charred, tips askew, and throws sparks and molten drips.
func destroy(part: Part) -> void:
	if not part.alive:
		return
	part.alive = false
	part.health = 0.0
	if is_instance_valid(part.node):
		part.node.rotation += Vector3(randf_range(-0.4, 0.4), randf_range(-0.3, 0.3), randf_range(-0.4, 0.4))
		part.node.scale *= 0.9
	if sparks != null:
		var at := part.position()
		sparks.burst(at, Vector3.UP, 50, 9.0, 1.0, 0.7, 0.045, floor_y)
		sparks.embers(at, 12, 3.5, floor_y)
	part_broken.emit(part)


func _process(delta: float) -> void:
	_time += delta
	_smoulder -= delta
	var drip := _smoulder <= 0.0
	if drip:
		_smoulder = SMOULDER_GAP
	for part in parts:
		if not is_instance_valid(part.node):
			continue
		part.flash = maxf(part.flash - delta, 0.0)
		if not part.alive:
			part.dead_time += delta
			var ember := clampf(1.0 - part.dead_time / 4.0, 0.0, 1.0) # cools over 4 s
			_light(part, CHAR.lerp(EMBER, ember * 0.6), 2.0 * ember * (0.6 + 0.4 * sin(_time * 11.0)))
			if drip and sparks != null and randf() < 0.4:
				sparks.burst(part.position(), Vector3.UP, 1, 1.5, 0.8, 0.8, 0.035, floor_y, Color(1.0, 0.6, 0.2))
			continue
		var health := part.health / part.max_health
		var color := part.color.lerp(HURT, (1.0 - health) * 0.8)
		var energy := part.energy
		if health < 0.35 and fmod(_time * 12.0 + part.radius * 7.0, 1.0) < 0.35: # about to go
			energy *= 0.3
		if not part.open(): # shielded right now: dimmer
			energy *= 0.5
		color = color.lerp(Color.WHITE, part.flash / FLASH_TIME)
		_light(part, color, energy + 6.0 * part.flash / FLASH_TIME)


func _light(part: Part, color: Color, energy: float) -> void:
	for material in part.glows:
		material.albedo_color = color
		material.emission = color
		material.emission_energy_multiplier = energy
