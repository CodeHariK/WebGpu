## Tornado (Q, hammers): both heads whirl round you, opposite each other, in a widening helix that
## starts high over your head and comes down to waist height — every turn they can hit each enemy
## again, batting it outward.
class_name TornadoMove
extends HammerMove

var turns := 3
var top_radius := 2.0 ## m: the helix starts this wide …
var bottom_radius := 3.8 ## … and ends this wide
var top_height := 3.0 ## m above the ground, at the start …
var bottom_height := 0.9 ## … and at the end
var spin_time := 1.5 ## seconds for all the turns


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "tornado"


func tunables() -> Array:
	return [
		["turns", 1, 6, 1], ["top_radius", 0.8, 4.0, 0.05], ["bottom_radius", 1.0, 6.0, 0.05],
		["top_height", 1.0, 5.0, 0.05], ["bottom_height", 0.3, 3.0, 0.05], ["spin_time", 0.5, 4.0, 0.05],
	]


func start() -> void:
	if not demo.blades_free():
		return
	for blade in demo.blades:
		begin(blade)
		blade.delay = 0.0 # opposite each other from the start
		var helix_path := _helix(blade)
		var path := lead_in(blade, helix_path)
		path.append_array(helix_path)
		blade.start_even(Blade.SPIN, path, spin_time, "smooth")


func hits_during(phase: int) -> bool:
	return phase == Blade.SPIN


## A new turn: every enemy can be hit again.
func after_tip(blade: Blade) -> bool:
	var turn := int(blade.eased * turns)
	if turn != blade.turn:
		blade.turn = turn
		blade.hit.clear()
	return false


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.SPIN:
		back(blade)
	else:
		super(blade)


# The right head starts on your right, the left one opposite; both go the same way round.
func _helix(blade: Blade) -> PackedVector3Array:
	var a0 := 0.0 if blade.side > 0.0 else PI
	return helix(demo.player, a0, a0 + turns * TAU, top_radius, bottom_radius, top_height, bottom_height)
