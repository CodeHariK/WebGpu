## Stitch line: a row of missile markers is sewn across the floor through where you stand — one
## after another during the telegraph, like a sewing machine's stitches — then the missiles land
## down the row in the same order, a beat apart. The row runs across you (side to side as the
## boss camera sees it), so the way out is toward or away from the ship; with `lines` 2 a second
## row crosses the first at a right angle (a plus on you), so you pick a quarter and get into it.
##
## params: count (missiles per row), spacing (m between them), gap (s between landings),
##         flight (s each missile's dive takes), lines (1 or 2), + the stage lengths.
class_name StitchMissiles
extends ShipAttack


func _init() -> void:
	key = "stitch"
	title = "stitch line"


func _on_begin(p: Dictionary) -> void:
	var count := int(p.get("count", 9))
	var spacing := float(p.get("spacing", 1.5))
	var gap := float(p.get("gap", 0.11))
	var flight := float(p.get("flight", 0.7))
	var lines := int(p.get("lines", 1))
	var center := ShipKit.flat(demo.player)
	var across := center - ship.ground_point()
	across = across.normalized() if across.length() > 0.5 else Vector3.FORWARD
	var telegraph: float = lengths[Stage.TELEGRAPH]
	var pods := ship.pods()
	var order := 0
	for line in lines:
		var direction := across.cross(Vector3.UP) if line == 0 else across
		var flip := 1.0 if line == 0 or randf() < 0.5 else -1.0 # the second row may stitch either way
		for i in count:
			var along := (float(i) - (count - 1) * 0.5) * spacing * flip
			var target := SkyshipArena.inside(center + direction * along, 0.8)
			var source: Vector3 = pods[order % pods.size()]
			var reveal := telegraph * float(i) / count # sewn across during the telegraph
			var wait := telegraph + order * gap - flight * 0.5
			ship.missiles.launch(source, target, maxf(wait, reveal), flight, reveal)
			order += 1
	ship.hold(true)


func _on_stop() -> void:
	ship.hold(false)
