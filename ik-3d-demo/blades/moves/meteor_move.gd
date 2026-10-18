## Meteor (left click, hammers): both heads whirl once round over your head, building speed, then
## rise and come crashing down on the aim side by side. The crash blasts every enemy round it away
## (the closer, the harder) and flattens the ones right under it; the heads stay buried a moment,
## then drag back.
class_name MeteorMove
extends HammerMove

var whirl_radius := 1.6 ## m: the whirl over your head
var whirl_height := 3.1 ## m above the ground
var laps := 1
var drop_height := 4.5 ## m: how high the heads come over before they fall
var whirl_time := 0.6 ## seconds for the whirl …
var crash_time := 0.32 ## … and for the crash down
var bury := 0.35 ## seconds the heads stay in the crater
var blast_radius := 2.8 ## m round the crash
var blast_power := 12.0 ## m/s it throws the nearest enemies

var _aim := Vector3.ZERO


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "meteor"


func tunables() -> Array:
	return [
		["whirl_radius", 0.6, 3.0, 0.05], ["whirl_height", 1.5, 5.0, 0.05], ["laps", 1, 3, 1],
		["drop_height", 2.0, 8.0, 0.1], ["whirl_time", 0.2, 1.5, 0.01], ["crash_time", 0.1, 0.8, 0.01],
		["bury", 0.0, 1.0, 0.01], ["blast_radius", 0.5, 6.0, 0.1], ["blast_power", 2.0, 25.0, 0.5],
	]


func start(aim: Vector3) -> void:
	if not demo.blades_free():
		return
	_aim = Vector3(aim.x, 0.0, aim.z)
	for blade in demo.blades:
		begin(blade)
		var whirl := _whirl(blade)
		var path := lead_in(blade, whirl)
		path.append_array(whirl)
		path.append_array(_crash(blade, whirl[-1], whirl[-1] - whirl[-2]))
		blade.start_even(Blade.OUT, path, whirl_time + crash_time, "accelerate") # faster and faster, into the ground


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		_impact(blade)
		rebound(blade, bury + 0.3)
	else:
		super(blade)


# The whirl: laps round over your head, ending behind you (so the crash comes over the top).
func _whirl(blade: Blade) -> PackedVector3Array:
	var start := -PI * 0.5 - laps * TAU
	var radius := whirl_radius + blade.side * 0.15 # the two heads a little apart
	var height := whirl_height + blade.side * 0.12
	return helix(demo.player, start, -PI * 0.5, radius, radius, height, height)


# The crash: from the end of the whirl (moving along `going`) up over the top and down on the aim.
func _crash(
		blade: Blade,
		from: Vector3,
		going: Vector3
) -> PackedVector3Array:
	var target := _aim + demo.right() * blade.side * 0.45
	var over := target + Vector3.UP * drop_height - demo.facing * 0.5
	return PackedVector3Array([from, from + going.normalized() * 1.2 + Vector3.UP * 1.5, over, target])


# The crash lands: blast the enemies round it, flatten the ones under it.
func _impact(blade: Blade) -> void:
	blast(blade.tip, blast_radius, blast_power, 1.0)
