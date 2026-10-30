## The skyship fight's screen overlay, drawn with plain _draw calls:
##
##   top middle    the boss: name, phase pips, the core's hit points, the shield's cycle (up /
##                 down and for how long) and the turrets left (red blocks)
##   left          (F1) every key: movement, weapons, each weapon's moves, the boss, the view
##   bottom left   your hit points and the dash's readiness
##   bottom        (O) the debug timeline: the attack playing as a bar split into its telegraph /
##                 fire / recover stages with a play head, the next attacks in the loop and the
##                 last few that played
##   middle        the crosshair (mouse look); a red flash when you're hurt; "DOWN" / "SKYSHIP
##                 DOWN" at the end
class_name SkyshipHud
extends Control

const STAGE_COLORS := [Color(0.4, 0.4, 0.4), Color(1.0, 0.8, 0.2), Color(1.0, 0.25, 0.15), Color(0.45, 0.6, 0.75)]

var demo: SkyshipDemo
var show_timeline := true
var show_keys := true ## F1: every key, blades demo and skyship, down the left

## The key list: [heading, [key, what], …] groups.
const KEYS := [
	["MOVE", ["WASD", "move (camera relative)"], ["Shift", "dash (i-frames)"], ["mouse", "turn the camera / aim (crosshair)"],
		["wheel", "camera distance"], ["U", "camera: mouse look / auto wide"], ["Esc", "free the mouse (click: look again)"]],
	["WEAPON", ["M", "switch: blades / hammers / revolver"]],
	["BLADES", ["L click", "throw"], ["R click", "sweep"], ["Q", "spin"], ["E", "pull"], ["F", "slice"], ["G", "bind"], ["Space", "rush"],
		["X", "whirl"], ["Z", "slam"], ["H", "lasso (H again: throw)"], ["J", "ground anchor"], ["K", "rip (K again: tug)"]],
	["HAMMERS", ["L click", "meteor"], ["R click", "wrecking swing"], ["Q", "tornado"], ["Z", "ground pound"], ["Space", "jump smash"]],
	["REVOLVER", ["L click", "shoot"], ["R click", "fan the hammer"], ["aim", "at a turret (shield down)"]],
	["BOSS", ["1 2 3", "stitch, bots, clock"], ["4 5 6", "shadow, snipe, pods"], ["Y", "timeline on / off"], ["O", "debug timeline"],
		["P", "boss panel"], ["I", "invulnerable"]],
	["VIEW", ["L", "daylight / night"], ["T", "slow motion"], ["C", "blade curves"], ["V", "fire step"], ["B", "body"],
		["Tab", "blade tuning"], ["R", "reset"], ["F1", "hide this list"]],
]

var _font: Font


func _init() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_font = ThemeDB.fallback_font


func _draw() -> void:
	if demo == null or demo.ship == null:
		return
	var screen := size
	if demo.hurt_flash > 0.0:
		draw_rect(Rect2(Vector2.ZERO, screen), Color(1.0, 0.0, 0.0, demo.hurt_flash * 0.25))
	_draw_crosshair(screen)
	_draw_boss(screen)
	_draw_player(screen)
	if show_timeline:
		_draw_timeline(screen)
	if show_keys and not demo.boss_panel.visible:
		_draw_keys()
	if demo.player_hp <= 0:
		_center(screen, "DOWN — R to try again", Color(1.0, 0.35, 0.3))
	elif demo.ship.defeated:
		_center(screen, "SKYSHIP DOWN", Color(1.0, 0.85, 0.4))


# Your aim (mouse look): a small ring and ticks in the middle of the screen.
func _draw_crosshair(screen: Vector2) -> void:
	if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		return
	var c := screen * 0.5
	var color := Color(1, 1, 1, 0.8)
	draw_arc(c, 9.0, 0.0, TAU, 24, color, 1.5)
	for d: Vector2 in [Vector2.LEFT, Vector2.RIGHT, Vector2.UP, Vector2.DOWN]:
		draw_line(c + d * 13.0, c + d * 19.0, color, 1.5)


func _draw_boss(screen: Vector2) -> void:
	var ship := demo.ship
	var width := 520.0
	var left := (screen.x - width) * 0.5
	var top := 40.0
	draw_string(_font, Vector2(left, top), "SKYSHIP", HORIZONTAL_ALIGNMENT_LEFT, -1, 18, Color(1.0, 0.85, 0.6))
	for i in Skyship.HEIGHTS.size(): # phase pips
		var done := i < ship.phase or ship.defeated
		var pip := Rect2(left + 92 + i * 20, top - 13, 14, 14)
		draw_rect(pip, Color(1.0, 0.5, 0.2) if i == ship.phase and not ship.defeated else (Color(0.3, 0.3, 0.3) if done else Color(0.7, 0.7, 0.7)), i == ship.phase)
	var state: String = Skyship.STATE_NAMES[ship.state]
	if ship.state == Skyship.State.FIGHT:
		if ship.shield.on:
			state = "SHIELD UP  %.1f s" % ship.shield.left
		else:
			state = "shield down %.1f s — shoot the turrets!" % ship.shield.left
	elif ship.state == Skyship.State.STAGGER:
		state = "CORE OPEN — hit it!" if ship.core.exposed else "shield down!"
	draw_string(_font, Vector2(left + 170, top), "%s   turrets %d/%d   bots %d" % [state, ship.turrets.living().size(), ship.turrets.turrets.size(), ship.bots.living().size()],
			HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Color.WHITE)
	var bar := Rect2(left, top + 8, width, 14)
	draw_rect(bar, Color(0, 0, 0, 0.6))
	var total := float(Skyship.HEIGHTS.size() * ship.core_hp_per_phase)
	var left_hp := 0.0 if ship.defeated else float((Skyship.HEIGHTS.size() - 1 - ship.phase) * ship.core_hp_per_phase + ship.core.hp)
	draw_rect(Rect2(bar.position, Vector2(width * left_hp / total, 14)), Color(1.0, 0.4, 0.15).lerp(Color.WHITE, ship.core.hurt_flash))
	for i in range(1, Skyship.HEIGHTS.size()):
		var x := left + width * i / Skyship.HEIGHTS.size()
		draw_line(Vector2(x, bar.position.y), Vector2(x, bar.end.y), Color(0, 0, 0, 0.8), 2.0)
	for i in ship.turrets.turrets.size(): # one red block per turret, filled by its hit points
		var turret := ship.turrets.turrets[i]
		var block := Rect2(left + width - (i + 1) * 44, top + 28, 40, 7)
		draw_rect(block, Color(0.2, 0.2, 0.22))
		if turret.alive:
			draw_rect(Rect2(block.position, Vector2(40.0 * turret.hp / turret.max_hp, 7)), Color(1.0, 0.3, 0.2).lerp(Color.WHITE, turret.hurt_flash))


func _draw_player(screen: Vector2) -> void:
	var at := Vector2(20, screen.y - 46)
	draw_string(_font, at + Vector2(0, -6), "SOPHIA  %d" % demo.player_hp, HORIZONTAL_ALIGNMENT_LEFT, -1, 15, Color.WHITE)
	draw_rect(Rect2(at, Vector2(260, 12)), Color(0, 0, 0, 0.6))
	var share := clampf(demo.player_hp / float(SkyshipDemo.PLAYER_HP), 0.0, 1.0)
	draw_rect(Rect2(at, Vector2(260 * share, 12)), Color(0.35, 0.95, 0.45) if demo.iframes <= 0.0 else Color(1, 1, 1))
	var ready := 1.0 - clampf(demo.dash_cooldown / demo.dash_rest, 0.0, 1.0)
	draw_rect(Rect2(at + Vector2(0, 16), Vector2(260 * ready, 4)), Color(0.4, 0.8, 1.0) if ready >= 1.0 else Color(0.3, 0.4, 0.5))
	if demo.invulnerable:
		draw_string(_font, at + Vector2(130, -6), "invulnerable", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(1, 0.9, 0.4))


func _draw_timeline(screen: Vector2) -> void:
	var timeline := demo.ship.timeline
	var width := 560.0
	var left := (screen.x - width) * 0.5
	var top := screen.y - 92.0
	draw_rect(Rect2(left - 10, top - 22, width + 20, 84), Color(0, 0, 0, 0.45))
	var attack := timeline.current
	var label := "timeline  phase %d  pace %.1f%s" % [timeline.phase + 1, timeline.pace, "" if timeline.running else "  (paused)"]
	draw_string(_font, Vector2(left, top - 6), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(0.7, 0.85, 1.0))
	var bar := Rect2(left, top, width, 18)
	draw_rect(bar, Color(0.12, 0.12, 0.14))
	if attack != null and attack.running():
		var total := attack.length()
		var x := left
		for stage in [ShipAttack.Stage.TELEGRAPH, ShipAttack.Stage.FIRE, ShipAttack.Stage.RECOVER]:
			var w: float = width * attack.lengths[stage] / total
			var color: Color = STAGE_COLORS[stage]
			draw_rect(Rect2(x, top, w - 2, 18), color * Color(1, 1, 1, 0.9 if stage == attack.stage else 0.4))
			draw_string(_font, Vector2(x + 4, top + 14), ShipAttack.STAGE_NAMES[stage], HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color.BLACK)
			x += w
		var head := left + width * attack.elapsed() / total
		draw_line(Vector2(head, top - 4), Vector2(head, top + 22), Color.WHITE, 2.0)
		draw_string(_font, Vector2(left, top + 36), "▶ %s" % attack.title, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color.WHITE)
	else:
		draw_string(_font, Vector2(left, top + 36), "· resting" if demo.ship.state == Skyship.State.FIGHT else "· " + Skyship.STATE_NAMES[demo.ship.state],
				HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(0.7, 0.7, 0.7))
	draw_string(_font, Vector2(left + 200, top + 36), "next: " + " → ".join(timeline.upcoming(3)), HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(0.75, 0.75, 0.75))
	var past: Array[String] = []
	for entry: Array in timeline.history:
		past.append(entry[0])
	draw_string(_font, Vector2(left, top + 54), "played: " + ", ".join(past), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color(0.55, 0.55, 0.6))


func _draw_keys() -> void:
	var lines := 0
	for group: Array in KEYS:
		lines += group.size() + 1
	var at := Vector2(16, 48)
	var step := 13.5
	draw_rect(Rect2(at - Vector2(8, 14), Vector2(250, lines * step + 6)), Color(0, 0, 0, 0.45))
	for group: Array in KEYS:
		draw_string(_font, at, group[0], HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.45, 0.9, 1.0))
		at.y += step
		for i in range(1, group.size()):
			var row: Array = group[i]
			draw_string(_font, at + Vector2(6, 0), row[0], HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(1.0, 0.85, 0.5))
			draw_string(_font, at + Vector2(62, 0), row[1], HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.9, 0.9, 0.9))
			at.y += step
		at.y += 1.0


func _center(screen: Vector2, text: String, color: Color) -> void:
	draw_string(_font, Vector2(0, screen.y * 0.42), text, HORIZONTAL_ALIGNMENT_CENTER, screen.x, 44, color)
