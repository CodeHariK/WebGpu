## Saves and loads the tunable numbers of the moves (and the shared wind-up settings) to one
## readable file, TUNING_PATH (a ConfigFile: a [section] per move key, `name=value` lines).
##
## A "target" is anything with `tunables() -> Array` of [var name, min, max, step] whose vars it
## reads with get() / set(): every BladeMove, and BladesDemo itself for the shared wind-up settings
## (section "shared"). The values the code starts with are kept as the defaults.
class_name MoveTuning
extends RefCounted

const TUNING_PATH := "res://blades/moves/tuning.cfg"

var targets := {} ## section name → target
var defaults := {} ## section name → {var name: the code's value}


## Every move with tunables, plus the demo's shared settings; remembers their code defaults.
func _init(demo: BladesDemo) -> void:
	targets["shared"] = demo
	for move: BladeMove in demo.moves.values():
		if not move.tunables().is_empty():
			targets[move.key] = move
	for section: String in targets:
		var values := {}
		for entry: Array in targets[section].tunables():
			values[entry[0]] = targets[section].get(entry[0])
		defaults[section] = values


## Set `name` on a section's target, keeping ints ints.
func apply(
		section: String,
		name: String,
		value: float
) -> void:
	var target: Object = targets[section]
	target.set(name, int(round(value)) if typeof(target.get(name)) == TYPE_INT else value)


## Load the saved values (the ones the file has; the rest stay as they are). False: no file yet.
func load_saved() -> bool:
	var file := ConfigFile.new()
	if file.load(TUNING_PATH) != OK:
		return false
	for section: String in targets:
		for entry: Array in targets[section].tunables():
			if file.has_section_key(section, entry[0]):
				apply(section, entry[0], float(file.get_value(section, entry[0])))
	return true


## Write every target's current values.
func save() -> Error:
	var file := ConfigFile.new()
	for section: String in targets:
		for entry: Array in targets[section].tunables():
			file.set_value(section, entry[0], targets[section].get(entry[0]))
	return file.save(TUNING_PATH)


## Put one section back to the code's values.
func reset_section(section: String) -> void:
	for name: String in defaults[section]:
		targets[section].set(name, defaults[section][name])
