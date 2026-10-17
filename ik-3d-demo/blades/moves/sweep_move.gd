## Sweep (right click): each blade swings out to its own side, then sweeps a quarter circle round
## to the front, where the two meet (just short of crossing). The chains stay taut.
class_name SweepMove
extends BladeMove

const RADIUS := 4.5
const TIME := 0.22


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "sweep"


func start() -> void:
	if not demo.blades_free():
		return
	for blade in demo.blades:
		begin(blade)
		var hand := demo.hand(blade.side)
		var start_point := sweep_path(blade)[0]
		var lift := Vector3.UP * 0.5
		launch(blade, PackedVector3Array([hand, hand.lerp(start_point, 0.33) + lift, hand.lerp(start_point, 0.66) + lift, start_point]), 0.1, "accelerate")


## A quarter circle from the blade's side round to the front: one cubic Bezier with handles
## Bezier.QUARTER_CIRCLE × radius long, along the circle's tangents.
func sweep_path(blade: Blade) -> PackedVector3Array:
	var side := demo.right() * blade.side
	var center := demo.player + Vector3(0, 1.1, 0)
	var a := center + side * RADIUS
	var b := center + demo.facing * RADIUS + side * 0.3
	var k := Bezier.QUARTER_CIRCLE * RADIUS
	return PackedVector3Array([a, a + demo.facing * k, b + side * k, b])


func live_path(blade: Blade) -> PackedVector3Array:
	return sweep_path(blade) if blade.phase == Blade.SWEEP else PackedVector3Array()


func hits_during(phase: int) -> bool:
	return phase == Blade.OUT or phase == Blade.SWEEP


func chain_on_path(_blade: Blade) -> bool:
	return false


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		blade.start(Blade.SWEEP, sweep_path(blade), TIME, "smooth")
	else:
		super(blade)
