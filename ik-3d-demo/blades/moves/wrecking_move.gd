## Wrecking swing (right click, hammers): both heads swing out in one huge spiral round you — from
## behind on one side, round through the front, out to behind on the other — growing wider as
## they go. Every swing picks its own plane at random: level, dipping low in front or rising,
## rolled into a diagonal (an uppercut or a down-chop), and either way round. Everything it
## touches is knocked flying.
class_name WreckingMove
extends HammerMove

var start_radius := 1.8 ## m: the spiral starts this close …
var end_radius := 5.0 ## … and ends this wide
var height := 1.0 ## m above the ground
var turn := 300.0 ## degrees it goes round
var swing_time := 0.7 ## seconds for the swing
var dip_min_deg := -10.0 ## degrees the plane dips toward your front, picked at random each swing between these …
var dip_max_deg := 30.0
var roll_max_deg := 40.0 ## … degrees it rolls into a diagonal (either way, at random) …
var reverse_chance := 0.5 ## … and the chance it goes left → right instead of right → left


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "wrecking"


func tunables() -> Array:
	return [
		["start_radius", 0.8, 4.0, 0.05], ["end_radius", 2.0, 8.0, 0.1], ["height", 0.3, 2.5, 0.05],
		["turn", 90.0, 540.0, 5.0], ["swing_time", 0.3, 1.6, 0.01],
		["dip_min_deg", -45.0, 45.0, 1.0], ["dip_max_deg", -45.0, 60.0, 1.0], ["roll_max_deg", 0.0, 80.0, 1.0], ["reverse_chance", 0.0, 1.0, 0.05],
	]


func start() -> void:
	if not demo.blades_free():
		return
	var dip := deg_to_rad(randf_range(dip_min_deg, dip_max_deg)) # this swing's plane
	var roll := deg_to_rad(randf_range(-roll_max_deg, roll_max_deg))
	var reverse := randf() < reverse_chance
	for blade in demo.blades:
		begin(blade)
		blade.delay = 0.0 # together, one heavy swing
		var spiral := _tilt(_spiral(blade, reverse), dip, roll)
		var path := lead_in(blade, spiral)
		path.append_array(spiral)
		blade.start_even(Blade.SPIN, path, swing_time, "accelerate")


func hits_during(phase: int) -> bool:
	return phase == Blade.SPIN


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.SPIN:
		back(blade)
	else:
		super(blade)


# Behind-right (−60° from your right) round through the front — or, reversed, from behind-left the
# other way round: the left head a little higher.
func _spiral(
		blade: Blade,
		reverse: bool
) -> PackedVector3Array:
	var a0 := deg_to_rad(-60.0)
	var a1 := a0 + deg_to_rad(turn)
	if reverse:
		a0 = PI - a0
		a1 = PI - a1
	var lift := height + (0.35 if blade.side < 0.0 else 0.0)
	return helix(demo.player, a0, a1, start_radius, end_radius, lift, lift)


# The swing's plane: the level spiral turned about your shoulders — dipped toward your front by
# `dip`, rolled about your facing by `roll` (control points turn with it: still a smooth curve).
func _tilt(
		path: PackedVector3Array,
		dip: float,
		roll: float
) -> PackedVector3Array:
	var pivot := demo.player + Vector3.UP * height
	var turn_by := Basis(demo.facing, roll) * Basis(demo.right(), -dip) # (+dip: the front goes down)
	var out := PackedVector3Array()
	for p in path:
		var q := pivot + turn_by * (p - pivot)
		q.y = maxf(q.y, 0.25) # never through the ground
		out.append(q)
	return out
