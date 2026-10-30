## The skyship's attack timeline: plain data, one loop of attacks per phase. Each entry is an
## attack key (one of the attacks made in setup()) and its params — stage lengths (telegraph / fire / recover, seconds) and
## the attack's own numbers. The ship plays a phase's entries in order, `rest` seconds apart, and
## loops; a harder phase is mostly the same attacks with shorter telegraphs and more of them.
## To change the fight, change PHASES — no code.
##
## `pace` scales the whole timeline's speed (the panel's slider): 1.5 = everything 50% faster.
## force(key) cuts in with one attack right now (the panel's buttons, keys 1–4); `running`
## off freezes the timeline after the current attack (the boss just hovers).
class_name ShipTimeline
extends RefCounted


## Missiles and lasers are fast; what gives you a chance is the telegraph: it shows the whole
## pattern early and big, so you see it from across the arena and run.
const PHASES := [
	[ # phase 1: one thing at a time
		["pods", {"telegraph": 1.2, "fire": 2.2, "recover": 0.6, "pods": 4, "near": 9.0, "far": 15.0, "salvo": 3, "gap": 0.45, "flight": 0.9, "arc": 7.0, "wave": 1.8, "waves": 3.0}],
		["snipe", {"telegraph": 0.5, "fire": 3.9, "recover": 0.6, "shots": 3, "track": 0.9, "lock": 0.3, "lag": 0.2}],
		["stitch", {"telegraph": 1.1, "fire": 1.6, "recover": 0.5, "count": 16, "spacing": 2.4, "gap": 0.05, "flight": 0.4}],
		["bots", {"telegraph": 0.7, "fire": 1.0, "recover": 0.5, "count": 3, "max": 5}],
		["clock", {"telegraph": 1.8, "fire": 6.0, "recover": 0.6, "hands": [[0.0, 75.0, 0.0]], "hub": 3.5, "door": 5.0}],
		["stitch", {"telegraph": 1.0, "fire": 1.8, "recover": 0.5, "count": 14, "spacing": 2.2, "gap": 0.045, "flight": 0.35, "lines": 2}],
		["shadow", {"telegraph": 1.6, "fire": 5.0, "recover": 0.8, "safe": 5.0, "drift": 4.5, "hop": 2.5, "rate": 40}],
	],
	[ # phase 2: lower, two hands, crossing stitches, quicker snipes
		["pods", {"telegraph": 1.0, "fire": 2.4, "recover": 0.5, "pods": 5, "near": 8.0, "far": 14.0, "salvo": 4, "gap": 0.38, "flight": 0.8, "arc": 8.0, "wave": 2.2, "waves": 3.0}],
		["stitch", {"telegraph": 0.9, "fire": 2.0, "recover": 0.4, "count": 16, "spacing": 2.2, "gap": 0.04, "flight": 0.35, "lines": 2}],
		["snipe", {"telegraph": 0.4, "fire": 4.4, "recover": 0.5, "shots": 4, "track": 0.75, "lock": 0.25, "lag": 0.15}],
		["bots", {"telegraph": 0.5, "fire": 1.0, "recover": 0.4, "count": 4, "max": 6}],
		["clock", {"telegraph": 1.5, "fire": 7.0, "recover": 0.5, "hands": [[0.0, 90.0, -6.0], [180.0, -70.0, 6.0]], "hub": 3.2, "door": 5.0}],
		["shadow", {"telegraph": 1.3, "fire": 6.0, "recover": 0.6, "safe": 4.5, "drift": 5.5, "hop": 2.0, "rate": 60}],
	],
	[ # phase 3: low and fast
		["pods", {"telegraph": 0.8, "fire": 2.6, "recover": 0.4, "pods": 6, "near": 7.0, "far": 13.0, "salvo": 5, "gap": 0.3, "flight": 0.7, "arc": 9.0, "wave": 2.6, "waves": 5.0}],
		["snipe", {"telegraph": 0.3, "fire": 4.8, "recover": 0.4, "shots": 5, "track": 0.6, "lock": 0.2, "lag": 0.1}],
		["clock", {"telegraph": 1.2, "fire": 7.0, "recover": 0.4, "hands": [[0.0, 110.0, -8.0], [120.0, 110.0, 0.0], [240.0, -80.0, 8.0]], "hub": 3.0, "door": 4.5}],
		["bots", {"telegraph": 0.4, "fire": 0.9, "recover": 0.3, "count": 4, "max": 7}],
		["stitch", {"telegraph": 0.8, "fire": 2.0, "recover": 0.4, "count": 18, "spacing": 2.0, "gap": 0.035, "flight": 0.3, "lines": 2}],
		["shadow", {"telegraph": 1.0, "fire": 7.0, "recover": 0.5, "safe": 4.0, "drift": 6.5, "hop": 1.6, "rate": 80}],
	],
]

var attacks := {} ## key → ShipAttack
var phase := 0
var index := 0 ## the next entry in the phase's loop
var current: ShipAttack
var rest := 0.45 ## seconds between attacks
var pace := 1.0
var running := true
var history: Array = [] ## [title, start time] of the last attacks (the debug timeline)
var clock := 0.0

var _wait := 1.5 ## seconds until the next attack


func setup(owner: SkyshipDemo, boss: Skyship) -> void:
	for attack: ShipAttack in [StitchMissiles.new(), ClockLaser.new(), ShadowLaser.new(), BotDrop.new(), SnipeLaser.new(), PodBarrage.new()]: # every attack the data may name
		boss.add_child(attack)
		attack.setup(owner, boss)
		attacks[attack.key] = attack


## Back to the start of `new_phase`'s loop (after a stagger / a reset).
func restart(new_phase: int, wait := 1.5) -> void:
	stop()
	phase = clampi(new_phase, 0, PHASES.size() - 1)
	index = 0
	_wait = wait


func stop() -> void:
	if current != null and current.running():
		current.stop()
	current = null


func tick(delta: float) -> void:
	clock += delta
	var step := delta * pace
	if current != null and current.running():
		current.tick(step)
		if current.running():
			return
		current = null
		_wait = rest
	if not running:
		return
	_wait -= step
	if _wait <= 0.0:
		var entries: Array = PHASES[phase]
		var entry: Array = entries[index % entries.size()]
		index += 1
		_start(entry[0], entry[1])


## Play `key` now with this phase's numbers for it (its first entry), cutting off the current one.
func force(key: String) -> void:
	stop()
	for entry: Array in PHASES[phase]:
		if entry[0] == key:
			_start(key, entry[1])
			return
	_start(key, {})


## The next `count` entries' titles (the debug timeline).
func upcoming(count: int) -> Array[String]:
	var names: Array[String] = []
	var entries: Array = PHASES[phase]
	for i in count:
		var entry: Array = entries[(index + i) % entries.size()]
		names.append((attacks[entry[0]] as ShipAttack).title)
	return names


func _start(key: String, params: Dictionary) -> void:
	current = attacks[key]
	current.begin(params)
	history.append([current.title, clock])
	if history.size() > 6:
		history.pop_front()
