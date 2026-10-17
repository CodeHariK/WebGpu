## Throw (left click): both blades whip out wide on their own sides and converge on the mouse,
## stick for a beat (in an enemy if they hit one on the way), and reel back along a new curve.
class_name ThrowMove
extends BladeMove


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "throw"


func start(aim: Vector3) -> void:
	if demo.blades_free():
		throw_both(aim)


## A throw that hits something on the way stops there and sticks.
func after_hit(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		blade.hold(0.08)
