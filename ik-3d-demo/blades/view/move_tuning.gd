## Saves and loads the tunable numbers of the moves (and the shared wind-up settings) to one
## readable file, TUNING_PATH (a ConfigFile: a [section] per move key, `name=value` lines).
##
## A "target" is anything with `tunables() -> Array` of [var name, min, max, step] whose vars it
## reads with get() / set(): every BladeMove, and BladesDemo itself for the shared settings (wind-up,
## hits, chain weight: section "shared"). The values the code starts with are kept as the defaults. The curve editor's
## point offsets go in section "curves" ("move/phase/point" = Vector3); the timing editor's speed
## curves in "timing" ("move/phase" = Vector4 handles), its time scales in "timing_scale".
class_name MoveTuning
extends RefCounted

const TUNING_PATH := "res://blades/moves/tuning.cfg"

var targets := {} ## section name → target
var defaults := {} ## section name → {var name: the code's value}
var timing := PhaseTiming.new() ## the timing editor's edits (Blade.timing)
var _curves: CurveEditor


## Every move with tunables, plus the demo's shared settings; remembers their code defaults.
func _init(demo: BladesDemo) -> void:
	_curves = demo.curves
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
	_curves.offsets.clear()
	if file.has_section("curves"):
		for key in file.get_section_keys("curves"):
			_curves.offsets[key] = file.get_value("curves", key)
	timing.curves.clear()
	timing.scales.clear()
	for pair: Array in [["timing", timing.curves], ["timing_scale", timing.scales]]:
		if file.has_section(pair[0]):
			for key in file.get_section_keys(pair[0]):
				pair[1][key] = file.get_value(pair[0], key)
	return true


## Write every target's current values.
func save() -> Error:
	var file := ConfigFile.new()
	for section: String in targets:
		for entry: Array in targets[section].tunables():
			file.set_value(section, entry[0], targets[section].get(entry[0]))
	for key: String in _curves.offsets:
		file.set_value("curves", key, _curves.offsets[key])
	for key: String in timing.curves:
		file.set_value("timing", key, timing.curves[key])
	for key: String in timing.scales:
		file.set_value("timing_scale", key, timing.scales[key])
	return file.save(TUNING_PATH)


## Put one section back to the code's values (a move's page: its curve edits too).
func reset_section(section: String) -> void:
	_curves.clear(section)
	timing.clear(section)
	for name: String in defaults[section]:
		targets[section].set(name, defaults[section][name])
