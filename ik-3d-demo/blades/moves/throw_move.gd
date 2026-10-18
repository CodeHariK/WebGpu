## Throw (left click): both blades whip out wide on their own sides and converge on the mouse,
## stick for a beat (in an enemy if they hit one on the way), and reel back along a new curve.
class_name ThrowMove
extends BladeMove


var stick := 0.08 ## seconds a blade stays stuck in an enemy it hit on the way
var linger := 0.06 ## seconds the blades stay at the aim when they hit nothing


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "throw"


## The tuning panel's sliders for this move (its curve: the wind-up page's sliders).
func tunables() -> Array:
	return [["stick", 0, 0.5, 0.01], ["linger", 0, 0.5, 0.01]]


func start(aim: Vector3) -> void:
	if demo.blades_free():
		throw_both(aim)


## A throw that hits something on the way stops there and sticks.
func after_hit(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		blade.hold(stick)


## Out → a beat at the aim → back.
func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		blade.hold(linger)
	else:
		super(blade)
