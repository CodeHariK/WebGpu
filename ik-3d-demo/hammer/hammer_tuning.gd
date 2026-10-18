## Saves and loads the hammer demo's sliders (the attack's and each body's) to one readable file,
## PATH: a ConfigFile with an [attack] section and one per body (its key), `name=value` lines.
## The code's values are the defaults: remembered the first time a target is seen.
class_name HammerTuning
extends RefCounted

const PATH := "res://hammer/tuning.cfg"

static var _defaults := {} ## target's instance id → {var name: the code's value}


## Remember the code's values of `target` (before anything is loaded into it).
static func remember(target: Object) -> void:
	var values := {}
	for entry in target.tunables():
		if entry is Array:
			values[entry[0]] = target.get(entry[0])
	_defaults[target.get_instance_id()] = values


## Write every slider of the attack and the bodies.
static func save(
		attack: SmashAttack,
		bodies: Array[HammerBody]
) -> Error:
	var file := ConfigFile.new()
	_write(file, "attack", attack)
	for body in bodies:
		_write(file, body.key, body)
	return file.save(PATH)


## Read the saved values into the attack and the bodies. False: no file yet.
static func load_into(
		attack: SmashAttack,
		bodies: Array[HammerBody]
) -> bool:
	var file := ConfigFile.new()
	if file.load(PATH) != OK:
		return false
	_read(file, "attack", attack)
	for body in bodies:
		_read(file, body.key, body)
	return true


## Put the sliders of one tab (`heading`) of `target` back to the code's values.
static func reset(
		target: Object,
		heading: String
) -> void:
	var values: Dictionary = _defaults.get(target.get_instance_id(), {})
	var inside := false
	for entry in target.tunables():
		if entry is String:
			inside = entry == heading
		elif inside and values.has(entry[0]):
			target.set(entry[0], values[entry[0]])


static func _write(
		file: ConfigFile,
		section: String,
		target: Object
) -> void:
	for entry in target.tunables():
		if entry is Array:
			file.set_value(section, entry[0], target.get(entry[0]))


static func _read(
		file: ConfigFile,
		section: String,
		target: Object
) -> void:
	for entry in target.tunables():
		if entry is Array and file.has_section_key(section, entry[0]):
			target.set(entry[0], file.get_value(section, entry[0]))
