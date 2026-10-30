## Skyship boss fight: the blades demo's player (Sophia, the chain blades, every move, the hammers
## and the revolver — BladesDemo, unchanged) in a closed round arena under a hovering carrier
## that plays timed attack patterns (Skyship, ShipTimeline, attacks/).
##
## How the fight goes: the ship's laser turrets (under its arms) are the targets. Its shield
## cycles on and off; while it's down the ship hangs still — throw the chains at a turret or
## shoot it with the revolver (M). Every turret destroyed: the ship sinks, keels over toward you
## and opens the armour round its belly core — hit the core. Bots it drops hunt you meanwhile.
## Three phases: each lower, faster and with more at once; the third core hit-out crashes it.
##
## Everything the blades demo has still works (see blades_demo.gd for its keys). Added here:
##   U      camera: the wide one (default: far back and high, you + the ship + the attacks in view,
##          aim with the mouse pointer) ↔ mouse look (any yaw / pitch, crosshair aim, wheel zoom,
##          Esc frees the mouse, click to look again)
##   Shift  dash (a short burst with i-frames)        WASD moves relative to the camera
##   F1 the key list on screen (on at the start)
##   1 … 6  play an attack now: stitch missiles, repair bots, clock hands, shadow-is-safe, snipe,
##          pod barrage
##   L  daylight ↔ night         M (blades demo) weapons: blades, hammers, revolver — the revolver
##                               aims up at the ship's turrets (point at one)
##   Y  the boss's timeline on / off (off: it just hovers, attacks only on 1–4)
##   O  the debug timeline (stage bar, next attacks)   P  the boss panel (left)
##   I  invulnerable
##
## The ship's parts are ShipTargets in `enemies`, so the moves treat them like the robots; hit()
## adds damage on top. hurt() is the one way the player takes damage (attacks, bots, blasts).
##
## Test run: make skyship ARGS=--shot — an autopilot plays each attack, breaks the shield and
## opens the core; screenshots go to "Claude outputs/skyship/".
class_name SkyshipDemo
extends BladesDemo

const PLAYER_HP := 100
const MISSILE_DAMAGE := 20
const GUN_PICK := 2.5 ## m: the pointer this close to a turret (along its ray) aims the revolver at it
const IFRAMES := 0.8 ## s you can't be hurt again after a hit
const START := Vector3(0, 0, 25)
const SHOT_DIR := "res://Claude outputs/skyship"
## Autopilot: [name, seconds, action, screenshot]
const AUTO := [
	["arena", 1.2, "", true],
	["stitch: telegraph", 1.0, "stitch", true],
	["stitch: landing", 1.0, "", true],
	["bots: drop", 2.4, "bots", true],
	["bots: throw at one", 0.5, "throw_bot", true],
	["clock: telegraph", 1.2, "clock", true],
	["clock: fire", 2.6, "", true],
	["shadow: telegraph", 1.7, "shadow", true],
	["shadow: fire", 1.6, "", true],
	["snipe: tracking", 1.6, "snipe", true],
	["snipe: shot", 0.55, "", true],
	["mouse look: up at the ship", 0.6, "look_up", true],
	["wide camera again", 0.6, "wide", false],
	["pods: rising", 1.1, "pods", true],
	["pods: snaking missiles", 1.2, "", true],
	["gun: draw the revolver", 0.5, "draw", false],
	["gun: fan a turret", 0.7, "gun", true],
	["chains: throw at a turret", 1.2, "chain_turret", true],
	["daylight", 0.8, "day", true],
	["turrets destroyed: stagger", 1.6, "break", true],
	["core open", 1.2, "", true],
	["core hit", 1.0, "hit_core", true],
	["panel", 0.3, "panel", true],
]

var ship: Skyship
var sparks: OrbSparks
var overlay: SkyshipHud
var boss_panel := OrbPanel.new()
var player_hp := PLAYER_HP
var iframes := 0.0
var hurt_flash := 0.0
var invulnerable := false
var framing := false ## U: the auto wide camera (follows the ship by itself; aim with the mouse pointer) ↔ mouse look (default)
var frame_back := 16.0 ## m the wide camera sits behind you (plus a share of your distance to the ship)
var frame_up := 11.0 ## m above you (plus a share of the distance)
var cam_distance := 18.0 ## m from the camera's pivot (wheel zooms): far back, so the ship and the attacks round you are in view
var cam_yaw := 0.0 ## radians round you (0: behind, along +Z)
var cam_pitch := 0.4 ## radians: + looks down on you, − looks up from below
var look_speed := 0.0035 ## radians per pixel of mouse motion
var run_speed := 7.5 ## m/s (the arena is big: you have to run)
var dash_speed := 24.0 ## m/s
var dash_time := 0.18 ## s
var dash_rest := 0.6 ## s between dashes
var dash_cooldown := 0.0
var last_hurt := ""
var daylight := false ## L: daylight ↔ night

var _dash_left := 0.0
var _dash_direction := Vector3.ZERO
var _cam_eye := Vector3.ZERO
var _cam_focus := Vector3.ZERO
var _auto := false
var _auto_step := -1
var _auto_left := 0.0
var _shot := 0
var _auto_shot_taken := false
var _auto_orbit := false ## the autopilot's mouse-look shot


func _ready() -> void:
	SkyshipArena.build(self)
	sparks = OrbSparks.new()
	add_child(sparks)
	ship = Skyship.new()
	add_child(ship)
	ship.setup(self, sparks)
	overlay = SkyshipHud.new()
	overlay.demo = self
	$Hud.add_child(overlay)
	_build_panel()
	super()
	_ground.visible = false
	_set_look(true)
	revolver.shot_range = 150.0 # the ship is far up and away
	hud.add_theme_font_size_override("font_size", 14) # one status line (the keys are SkyshipHud's list)
	_set_daylight(false)
	_fov = 70.0 # a wide view: the arena is big and the ship is high
	_auto = "--shot" in OS.get_cmdline_user_args()
	if _auto:
		invulnerable = true
		ship.timeline.running = false
	else:
		_apply_mouse_mode()


func _reset() -> void:
	super()
	if ship == null:
		return
	for enemy in enemies: # the blades demo's robots: not in this fight
		enemy.mesh.queue_free()
	enemies.clear()
	player = START
	facing = Vector3.FORWARD
	player_hp = PLAYER_HP
	iframes = 0.0
	hurt_flash = 0.0
	dash_cooldown = 0.0
	_dash_left = 0.0
	ship.reset()


func simulate(delta: float) -> void:
	if _looking(): # mouse look: you aim with the crosshair in the middle of the screen
		mouse = get_viewport().get_visible_rect().size * 0.5
	super(delta)
	iframes = maxf(iframes - delta, 0.0)
	hurt_flash = maxf(hurt_flash - delta * 2.5, 0.0)
	dash_cooldown = maxf(dash_cooldown - delta, 0.0)
	ship.tick(game_delta)
	if _auto:
		_autopilot(delta)


# --- damage --------------------------------------------------------------------------------------

## The player takes `amount` (unless dashing / just hit / invulnerable / already down).
func hurt(amount: int, why: String) -> void:
	if invulnerable or iframes > 0.0 or player_hp <= 0 or ship.defeated:
		return
	player_hp = maxi(player_hp - amount, 0)
	last_hurt = why
	iframes = IFRAMES
	hurt_flash = 1.0
	shake = maxf(shake, 0.35)
	sparks.burst(player + Vector3.UP, Vector3.UP, 14, 4.0, 1.0, 0.35, 0.04, 0.0, Color(1.0, 0.4, 0.3))


## A missile landed: hurts you inside `radius`, and the ship's own bots (knocked away).
func blast(at: Vector3, radius: float) -> void:
	if ShipKit.flat(player).distance_to(at) < radius:
		hurt(MISSILE_DAMAGE, "missile")
	for bot in ship.bots.living():
		var away := ShipKit.flat(bot.position) - at
		if away.length() < radius:
			bot.damage(1)
			bot.flash = 1.0
			bot.velocity = away.normalized() * 5.0 + Vector3.UP * 4.0
	shake = maxf(shake, 0.15)


## A blade hits a ship part: the blades demo's hit (flash, sparks, hit-stop, the move's say), then
## damage — hammers hit twice as hard.
func hit(blade: Blade, index: int, knock: bool) -> void:
	super(blade, index, knock)
	var target := enemies[index] as ShipTarget
	if target == null or target.kind == ShipTarget.Kind.TURRET: # (a turret counts its own hits: ShipTarget)
		return
	target.damage(2 if hammers() else 1)
	if target == ship.core:
		ship.core_hit()


## The aim, with aim assist: a ship part the pointer / crosshair is over (a turret — up on the
## ship —, a bot, the open core) is the aim, led by where it will be when the blades
## get there (the revolver's shot is instant: no lead). Otherwise the blades demo's point on the
## floor plane.
func mouse_point() -> Vector3:
	if aim_override == Vector3.INF and ship != null:
		var target := _target_under_mouse()
		if target != null:
			if weapon == 2:
				return target.position
			var travel := windup_time + throw_time + throw_per_m * player.distance_to(target.position) + 0.1
			return target.position + target.motion * travel
	return super()


# The nearest ship part along the mouse ray, within GUN_PICK of it (null: none).
func _target_under_mouse() -> ShipTarget:
	var from := camera.project_ray_origin(mouse)
	var direction := camera.project_ray_normal(mouse)
	var best := INF
	var picked: ShipTarget = null
	for enemy in enemies:
		var target := enemy as ShipTarget
		if target == null or not target.alive or target.position.y < -10.0 or (target.kind == ShipTarget.Kind.CORE and not target.exposed):
			continue
		if target.kind == ShipTarget.Kind.TURRET and not target.exposed and weapon != 2: # (shots still spark off the shield)
			continue
		var along := (target.position - from).dot(direction)
		if along > 0.0 and along < best and (from + direction * along).distance_to(target.position) < GUN_PICK:
			best = along
			picked = target
	return picked


# --- the player ------------------------------------------------------------------------------------

## WASD relative to the camera, the dash, the arena wall and the ship's hull when it's down low.
func _move_player(delta: float) -> void:
	if moves.rush.rushing or moves.anchor.hauling or moves.leap.carrying:
		_keep_inside()
		return
	var forward := -camera.global_transform.basis.z
	forward = Vector3(forward.x, 0, forward.z).normalized()
	var side := forward.cross(Vector3.UP)
	var input := (side * (float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)))
			+ forward * (float(Input.is_key_pressed(KEY_W)) - float(Input.is_key_pressed(KEY_S))))
	if _dash_left > 0.0:
		_dash_left -= delta
		player += _dash_direction * dash_speed * delta
		if sparks != null:
			sparks.burst(player + Vector3.UP * 0.3, -_dash_direction, 1, 2.0, 0.4, 0.2, 0.03, 0.0, Color(0.5, 0.85, 1.0))
	else:
		player += input.limit_length(1.0) * run_speed * delta
	if player.y > 0.0 or player_velocity.y > 0.0: # in the air: keep the launch, fall
		player_velocity.y -= GRAVITY * delta
		player += player_velocity * delta
		if player.y <= 0.0:
			player.y = 0.0
			player_velocity = Vector3.ZERO
			shake = maxf(shake, 0.12)
	var aim := mouse_point()
	var flat := Vector3(aim.x - player.x, 0, aim.z - player.z)
	if flat.length() > 0.3:
		facing = flat.normalized()
	_keep_inside()


func _keep_inside() -> void:
	var inside := SkyshipArena.inside(player, 0.5)
	player.x = inside.x
	player.z = inside.z
	if ship.state == Skyship.State.STAGGER or ship.state == Skyship.State.CRASH: # the hull is down here
		var off := ShipKit.flat(player) - ship.ground_point()
		var hull := Skyship.HULL * Skyship.SIZE
		if off.length() < hull:
			var out := off.normalized() if off.length() > 1e-3 else Vector3.BACK
			var pushed := ship.ground_point() + out * hull
			player.x = pushed.x
			player.z = pushed.z


func _dash() -> void:
	if dash_cooldown > 0.0 or player_hp <= 0:
		return
	var forward := -camera.global_transform.basis.z
	forward = Vector3(forward.x, 0, forward.z).normalized()
	var side := forward.cross(Vector3.UP)
	var input := (side * (float(Input.is_key_pressed(KEY_D)) - float(Input.is_key_pressed(KEY_A)))
			+ forward * (float(Input.is_key_pressed(KEY_W)) - float(Input.is_key_pressed(KEY_S))))
	_dash_direction = input.normalized() if input.length() > 0.1 else facing
	_dash_left = dash_time
	dash_cooldown = dash_rest
	iframes = maxf(iframes, dash_time + 0.08)


# --- drawing -----------------------------------------------------------------------------------

func _draw(delta: float, view_delta: float) -> void:
	super(delta, view_delta)
	ship.draw(delta)
	if not is_editing() and not preview.active:
		if framing and not _auto_orbit:
			_frame_camera(view_delta)
		else:
			_orbit_camera(view_delta)
	body.visible = iframes <= 0.0 or _dash_left > 0.0 or fmod(iframes * 14.0, 1.0) < 0.6 # blink while hurt
	overlay.queue_redraw()


## The third-person camera: it orbits a point over your shoulder by the mouse (any yaw, pitch from
## well below to nearly straight down), stays inside the wall and above the floor. U switches
## back to the wide camera.
func _orbit_camera(view_delta: float) -> void:
	var pivot := player + Vector3.UP * 1.7
	var direction := Vector3(sin(cam_yaw) * cos(cam_pitch), sin(cam_pitch), cos(cam_yaw) * cos(cam_pitch))
	var flat_direction := Vector3(direction.x, 0, direction.z)
	var reach := cam_distance
	if flat_direction.length() > 1e-3:
		reach = _inside_distance(ShipKit.flat(pivot), flat_direction.normalized(), cam_distance * flat_direction.length()) / flat_direction.length()
	var eye := pivot + direction * minf(reach, cam_distance)
	eye.y = maxf(eye.y, 0.3)
	_cam_eye = _cam_eye.lerp(eye, 1.0 - exp(-20.0 * view_delta)) if _cam_eye != Vector3.ZERO else eye
	var jolt := Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * shake * 0.4
	camera.position = _cam_eye + jolt
	camera.look_at(pivot - direction * 4.0 + Vector3.UP * 1.2) # past you and a little up, so the crosshair reaches far
	camera.h_offset = 0.0 if is_editing() else 1.4 * minf(cam_distance / 8.0, 2.0) # over the shoulder: you stand left of the crosshair


## L: daylight (a clear blue sky, a high white sun, no haze) ↔ night (_night()).
func _set_daylight(on: bool) -> void:
	daylight = on
	if not on:
		_night()
		return
	_set_look(false)
	var environment: Environment = $WorldEnvironment.environment
	var sky: ProceduralSkyMaterial = environment.sky.sky_material
	sky.sky_top_color = Color(0.25, 0.48, 0.85)
	sky.sky_horizon_color = Color(0.7, 0.8, 0.9)
	sky.ground_horizon_color = Color(0.6, 0.65, 0.7)
	environment.fog_enabled = false
	environment.glow_enabled = true
	environment.glow_intensity = 0.4
	environment.glow_bloom = 0.0
	var sun: DirectionalLight3D = $Sun
	sun.light_energy = 1.3
	sun.light_color = Color(1.0, 0.97, 0.9)


## Scarier than the blades demo's dusk: a near-black red sky, thin haze, a dim low red sun —
## the ship's lights, the lasers and the searchlight do the lighting.
func _night() -> void:
	var environment: Environment = $WorldEnvironment.environment
	var sky: ProceduralSkyMaterial = environment.sky.sky_material
	sky.sky_top_color = Color(0.02, 0.015, 0.03)
	sky.sky_horizon_color = Color(0.22, 0.05, 0.04)
	sky.ground_horizon_color = Color(0.1, 0.03, 0.03)
	environment.fog_density = 0.0035
	environment.fog_light_color = Color(0.18, 0.05, 0.05)
	environment.glow_enabled = true
	environment.glow_intensity = 0.9
	environment.glow_bloom = 0.15
	var sun: DirectionalLight3D = $Sun
	sun.light_energy = 0.35
	sun.light_color = Color(1.0, 0.45, 0.35)


## The wide camera: behind you on the side away from the ship, far and high enough that you, the
## ship and the floor round you (where the attacks land) are all in view; the further the ship,
## the further back and higher it goes. It looks at a point between you and the ship. You aim
## with the mouse pointer (as in the blades demo).
func _frame_camera(view_delta: float) -> void:
	var to_ship := ship.ground_point() - ShipKit.flat(player)
	var gap := to_ship.length()
	if gap > 1.0:
		var want := atan2(-to_ship.x, -to_ship.z) # behind you, looking toward the ship
		cam_yaw = lerp_angle(cam_yaw, want, 1.0 - exp(-1.5 * view_delta))
	var back := Vector3(sin(cam_yaw), 0, cos(cam_yaw))
	var eye := player + back * (frame_back + gap * 0.3) + Vector3.UP * (frame_up + gap * 0.15)
	var focus := player.lerp(ship.global_position, 0.4)
	focus.y = lerpf(1.0, ship.global_position.y, 0.3)
	var follow := 1.0 - exp(-5.0 * view_delta)
	_cam_eye = eye if _cam_eye == Vector3.ZERO else _cam_eye.lerp(eye, follow)
	_cam_focus = focus if _cam_focus == Vector3.ZERO else _cam_focus.lerp(focus, follow)
	var jolt := Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * shake * 0.5
	camera.position = _cam_eye + jolt
	camera.look_at(_cam_focus)
	camera.h_offset = 0.0


func _set_framing(on: bool) -> void:
	framing = on
	_apply_mouse_mode()


## Captured (mouse look) or a free pointer (the wide camera, panels open).
func _apply_mouse_mode() -> void:
	var free := framing or boss_panel.visible or is_editing() or _auto
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE if free else Input.MOUSE_MODE_CAPTURED


func _looking() -> bool:
	return Input.mouse_mode == Input.MOUSE_MODE_CAPTURED and not is_editing()


# How far from `from` along `direction` (flat, unit) the camera can go and stay inside the wall.
static func _inside_distance(from: Vector3, direction: Vector3, wanted: float) -> float:
	var limit := SkyshipArena.RADIUS - 0.3
	var b := from.dot(direction)
	var c := from.length_squared() - limit * limit
	var room := -b + sqrt(maxf(b * b - c, 0.0)) # where the ray meets the wall's circle
	return clampf(room, 0.5, wanted)


## One short status line (the full key list is SkyshipHud's, F1).
func _draw_hud() -> void:
	var weapon_state: String = WEAPONS[weapon]
	if weapon == 2:
		weapon_state += "  " + revolver.status()
	hud.text = "weapon (M): %s   camera (U): %s   timeline (Y): %s%s   F1 keys" % [weapon_state, "wide" if framing else "mouse look",
		"on" if ship.timeline.running else "OFF", "   hurt by: %s" % last_hurt if last_hurt != "" and hurt_flash > 0.0 else ""]


# --- the boss panel (P) ------------------------------------------------------------------------

func _build_panel() -> void:
	boss_panel.set_anchors_preset(Control.PRESET_LEFT_WIDE)
	boss_panel.offset_left = 10
	boss_panel.offset_right = 10 + OrbPanel.WIDTH
	boss_panel.offset_top = 140
	boss_panel.offset_bottom = -110
	boss_panel.visible = false
	$Hud.add_child(boss_panel)
	var p := boss_panel
	p.heading("Time")
	p.slider("time scale (T: 1 ↔ 0.2)", 0.05, 1.0, 0.05, func() -> float: return Engine.time_scale, func(v: float) -> void: Engine.time_scale = v)
	p.heading("Timeline")
	p.checkbox("running (Y)", func() -> bool: return ship.timeline.running, func(on: bool) -> void: ship.timeline.running = on)
	p.checkbox("debug timeline (O)", func() -> bool: return overlay.show_timeline, func(on: bool) -> void: overlay.show_timeline = on)
	p.slider("pace (× attack speed)", 0.3, 2.5, 0.05, func() -> float: return ship.timeline.pace, func(v: float) -> void: ship.timeline.pace = v)
	p.slider("rest between attacks (s)", 0.0, 3.0, 0.1, func() -> float: return ship.timeline.rest, func(v: float) -> void: ship.timeline.rest = v)
	for key: String in ["stitch", "bots", "clock", "shadow", "snipe", "pods"]:
		p.button("play %s now" % (ship.timeline.attacks[key] as ShipAttack).title, ship.timeline.force.bind(key))
	p.heading("Boss")
	p.slider("core hits per phase", 2, 20, 1, func() -> float: return ship.core_hp_per_phase, func(v: float) -> void: ship.core_hp_per_phase = int(v), "%d")
	p.slider("core open (s)", 2.0, 15.0, 0.5, func() -> float: return ship.exposed_time, func(v: float) -> void: ship.exposed_time = v)
	p.slider("bot speed (m/s)", 0.5, 6.0, 0.1, func() -> float: return ship.bots.speed, func(v: float) -> void: ship.bots.speed = v)
	p.slider("shield up (s)", 1.0, 15.0, 0.5, func() -> float: return ship.shield.up_time, func(v: float) -> void: ship.shield.up_time = v)
	p.slider("shield down (s)", 1.0, 15.0, 0.5, func() -> float: return ship.shield.down_time, func(v: float) -> void: ship.shield.down_time = v)
	p.slider("turret hits", 1, 20, 1, func() -> float: return ship.turrets.hp, func(v: float) -> void: ship.turrets.hp = int(v), "%d")
	p.button("destroy the turrets", _destroy_turrets)
	p.button("next phase", _next_phase)
	p.heading("Player")
	p.checkbox("invulnerable (I)", func() -> bool: return invulnerable, func(on: bool) -> void: invulnerable = on)
	p.checkbox("wide camera (U; off: mouse look)", func() -> bool: return framing, _set_framing)
	p.slider("wide camera: back (m)", 4.0, 40.0, 0.5, func() -> float: return frame_back, func(v: float) -> void: frame_back = v)
	p.slider("wide camera: up (m)", 2.0, 30.0, 0.5, func() -> float: return frame_up, func(v: float) -> void: frame_up = v)
	p.slider("camera distance (m, wheel)", 2.0, 20.0, 0.5, func() -> float: return cam_distance, func(v: float) -> void: cam_distance = v)
	p.slider("mouse look speed", 0.001, 0.01, 0.0005, func() -> float: return look_speed, func(v: float) -> void: look_speed = v, "%.4f")
	p.slider("run speed (m/s)", 3.0, 14.0, 0.5, func() -> float: return run_speed, func(v: float) -> void: run_speed = v)
	p.slider("dash speed (m/s)", 5.0, 30.0, 0.5, func() -> float: return dash_speed, func(v: float) -> void: dash_speed = v)
	p.slider("dash time (s)", 0.05, 0.5, 0.01, func() -> float: return dash_time, func(v: float) -> void: dash_time = v)
	p.slider("dash rest (s)", 0.1, 2.0, 0.05, func() -> float: return dash_rest, func(v: float) -> void: dash_rest = v)
	p.button("heal", func() -> void: player_hp = PLAYER_HP)


func _destroy_turrets() -> void:
	ship.shield.on = false
	for turret in ship.turrets.living():
		turret.exposed = true
		turret.damage(turret.hp)


func _next_phase() -> void:
	ship.phase = mini(ship.phase + 1, Skyship.HEIGHTS.size() - 1)
	ship.timeline.restart(ship.phase, 0.5)


# --- input ---------------------------------------------------------------------------------------

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and _looking():
		cam_yaw -= event.relative.x * look_speed
		cam_pitch = clampf(cam_pitch + event.relative.y * look_speed, -0.6, 1.35)
		return
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP or event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			cam_distance = clampf(cam_distance * (0.9 if event.button_index == MOUSE_BUTTON_WHEEL_UP else 1.1), 2.0, 40.0)
			return
		if not framing and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED and not boss_panel.visible and not is_editing() and not _auto:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED # mouse look: click back into the game
			return
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		return
	if event is InputEventKey and event.pressed and not event.echo:
		var attack: String = {KEY_1: "stitch", KEY_2: "bots", KEY_3: "clock", KEY_4: "shadow", KEY_5: "snipe", KEY_6: "pods"}.get(event.keycode, "")
		if attack != "":
			if ship.state == Skyship.State.FIGHT:
				ship.timeline.force(attack)
			return
		match event.keycode:
			KEY_SHIFT:
				_dash()
				return
			KEY_Y:
				ship.timeline.running = not ship.timeline.running
				return
			KEY_O:
				overlay.show_timeline = not overlay.show_timeline
				return
			KEY_P:
				boss_panel.visible = not boss_panel.visible
				_apply_mouse_mode()
				return
			KEY_TAB: # the blades tuning panel needs the mouse
				super(event)
				_apply_mouse_mode()
				return
			KEY_I:
				invulnerable = not invulnerable
				return
			KEY_L:
				_set_daylight(not daylight)
				return
			KEY_F1:
				overlay.show_keys = not overlay.show_keys
				return
			KEY_U:
				framing = not framing
				_apply_mouse_mode()
				return
	super(event)


# --- autopilot (--shot) --------------------------------------------------------------------------

func _autopilot(delta: float) -> void:
	_auto_left -= delta
	if _auto_left > 0.0:
		return
	if _auto_step >= 0 and not _auto_shot_taken:
		var done: Array = AUTO[_auto_step]
		print("%-24s turret hp %d  shield %s  ship %s  phase %d  core %d  bots %d  attack %s  at %s  tilt %.2f" % [done[0], ship.turrets.turrets.reduce(func(sum: int, t: ShipTarget) -> int: return sum + t.hp, 0), ship.shield.on, Skyship.STATE_NAMES[ship.state], ship.phase + 1,
				ship.core.hp, ship.bots.living().size(), ship.timeline.current.title if ship.timeline.current != null else "-",
				ship.position.snapped(Vector3.ONE * 0.1), ship.transform.basis.y.angle_to(Vector3.UP)])
		if done[3]: # shoot this step's last frame before the next step changes anything
			_save_shot()
			_auto_shot_taken = true
			_auto_left = 0.05
			return
	_auto_shot_taken = false
	_auto_step += 1
	if _auto_step >= AUTO.size():
		_auto = false
		await get_tree().create_timer(0.3).timeout
		get_tree().quit()
		return
	var step: Array = AUTO[_auto_step]
	_auto_left = step[1]
	match step[2]:
		"stitch", "bots", "clock", "shadow", "snipe", "pods": ship.timeline.force(step[2])
		"look_up":
			framing = false
			_auto_orbit = true
			cam_pitch = -0.45
			var to_ship := ship.ground_point() - ShipKit.flat(player)
			cam_yaw = atan2(-to_ship.x, -to_ship.z)
		"wide":
			framing = true
			_auto_orbit = false
		"throw_bot":
			var bots := ship.bots.living()
			if not bots.is_empty():
				aim_override = bots[0].position
				press("throw")
				aim_override = Vector3.INF
		"draw":
			while weapon != 2:
				_switch_weapon()
			ship.shield.on = false # (a frame ahead, so the turrets are open when it shoots)
			ship.shield.left = ship.shield.down_time
		"gun":
			ship.shield.on = false
			ship.shield.left = ship.shield.down_time
			var turret: ShipTarget = ship.turrets.living()[0]
			mouse = camera.unproject_position(turret.position)
			print("  gun aims at %s (turret at %s)" % [mouse_point().snapped(Vector3.ONE * 0.1), turret.position.snapped(Vector3.ONE * 0.1)])
			revolver.fire()
		"chain_turret":
			while weapon != 0:
				_switch_weapon()
			ship.shield.on = false
			ship.shield.left = ship.shield.down_time
			var turret: ShipTarget = ship.turrets.living()[0]
			mouse = camera.unproject_position(turret.position)
			print("  chains aim at %s (turret at %s, moving %s)" % [mouse_point().snapped(Vector3.ONE * 0.1), turret.position.snapped(Vector3.ONE * 0.1), turret.motion.snapped(Vector3.ONE * 0.1)])
			press("throw")
		"day": _set_daylight(true)
		"break":
			while weapon != 0:
				_switch_weapon()
			_destroy_turrets()
		"hit_core":
			cam_pitch = 0.3
			var away := ShipKit.flat(player) - ship.ground_point()
			player = ship.ground_point() + (away.normalized() if away.length() > 0.1 else Vector3.BACK) * (Skyship.HULL * Skyship.SIZE + 0.5)
			aim_override = ship.core.position
			print("  core at %s exposed %s  blades free %s  player %s" % [ship.core.position.snapped(Vector3.ONE * 0.1), ship.core.exposed, blades_free(), player.snapped(Vector3.ONE * 0.1)])
			press("throw")
			aim_override = Vector3.INF
		"panel": boss_panel.visible = true


func _save_shot() -> void:
	var index := _shot
	_shot += 1
	await RenderingServer.frame_post_draw
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(SHOT_DIR))
	get_viewport().get_texture().get_image().save_png("%s/skyship_%02d.png" % [SHOT_DIR, index])
