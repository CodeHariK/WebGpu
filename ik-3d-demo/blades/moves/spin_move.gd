## Spin (Q): both blades orbit you, opposite each other, TURNS times round; every turn each enemy
## can be hit again. The circle is quarter-circle Beziers, in a frame fixed when the spin starts.
class_name SpinMove
extends BladeMove

const RADIUS := 3.8
const TURNS := 3
const TIME := 1.05


func _init(owner: BladesDemo) -> void:
	super(owner)
	key = "spin"


func start() -> void:
	if not demo.blades_free():
		return
	for blade in demo.blades:
		begin(blade)
		blade.delay = 0.0 # opposite each other from the start
		blade.spin_right = demo.right()
		blade.spin_forward = demo.facing
		var hand := demo.hand(blade.side)
		var start_point := spin_path(blade)[0]
		launch(blade, PackedVector3Array([hand, hand.lerp(start_point, 0.33), hand.lerp(start_point, 0.66), start_point]), 0.1, "accelerate")


## TURNS full circles round the player, 4 quarter-circle Beziers per turn. The right blade starts
## on your right (angle 0), the left one opposite (angle π); both go the same way round.
func spin_path(blade: Blade) -> PackedVector3Array:
	var center := demo.player + Vector3(0, 1.1, 0)
	var start_angle := 0.0 if blade.side > 0.0 else PI
	var k := Bezier.QUARTER_CIRCLE * RADIUS
	var path := PackedVector3Array()
	for q in TURNS * 4:
		var a0 := start_angle + q * PI / 2.0
		var a1 := a0 + PI / 2.0
		var p0 := _circle(blade, center, a0)
		var p3 := _circle(blade, center, a1)
		path.append_array([p0, p0 + _tangent(blade, a0) * k, p3 - _tangent(blade, a1) * k, p3])
	return path


func live_path(blade: Blade) -> PackedVector3Array:
	return spin_path(blade) if blade.phase == Blade.SPIN else PackedVector3Array()


## A new turn: every enemy can be hit again.
func after_tip(blade: Blade) -> bool:
	if blade.phase == Blade.SPIN:
		var turn := int(blade.eased * TURNS)
		if turn != blade.turn:
			blade.turn = turn
			blade.hit.clear()
	return false


func hits_during(phase: int) -> bool:
	return phase == Blade.OUT or phase == Blade.SPIN


func chain_on_path(_blade: Blade) -> bool:
	return false


func next_phase(blade: Blade) -> void:
	if blade.phase == Blade.OUT:
		blade.start(Blade.SPIN, spin_path(blade), TIME, "linear")
	else:
		super(blade)


func _circle(
		blade: Blade,
		center: Vector3,
		angle: float
) -> Vector3:
	return center + (blade.spin_right * cos(angle) + blade.spin_forward * sin(angle)) * RADIUS


func _tangent(
		blade: Blade,
		angle: float
) -> Vector3:
	return -blade.spin_right * sin(angle) + blade.spin_forward * cos(angle)
