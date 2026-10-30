## Bots: the belly hatch glows red (telegraph), then drops small bots one by one; they fall, land
## and come for you (ShipBots). Kill them, bind them, or lasso one and throw it into the others.
##
## params: count (bots dropped), max (no more than this many alive at once), + the stage lengths.
class_name BotDrop
extends ShipAttack

var _left := 0 ## still to drop
var _wait := 0.0


func _init() -> void:
	key = "bots"
	title = "bots"


func _on_begin(p: Dictionary) -> void:
	_left = mini(int(p.get("count", 3)), int(p.get("max", 6)) - ship.bots.living().size())
	ship.hold(true)


func _tick(now: Stage, t: float, delta: float) -> void:
	match now:
		Stage.TELEGRAPH: ship.hatch = t
		Stage.FIRE:
			ship.hatch = 1.0
			_wait -= delta
			if _left > 0 and _wait <= 0.0:
				_left -= 1
				_wait = lengths[Stage.FIRE] / maxf(float(params.get("count", 3)), 1.0)
				var toss := Vector3(randf_range(-1, 1), 0, randf_range(-1, 1)).normalized() * randf_range(3.0, 7.0)
				ship.bots.spawn(ship.belly() + Vector3.DOWN * 0.4, toss)
		Stage.RECOVER: ship.hatch = 1.0 - t


func _on_stop() -> void:
	if ship != null:
		ship.hatch = 0.0
		ship.hold(false)
