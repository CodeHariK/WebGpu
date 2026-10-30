## One of the skyship's attack patterns. Every attack plays the same three stages, so the player
## can always read it the same way:
##
##   TELEGRAPH  it shows what's coming (markers, lines, a red floor) and does no harm
##   FIRE       it hurts
##   RECOVER    it winds down — the opening to hit back
##
## The stage lengths and the attack's own numbers come from the timeline's data (ShipTimeline:
## one entry = an attack key + a params dictionary), so a harder phase is the same attack with
## different data. A new attack is one file in attacks/ that extends this and overrides the
## _on_* hooks; ShipTimeline.ATTACKS lists them.
##
##   _on_begin(params)      read your params, place telegraphs
##   _on_stage(stage)       a stage just started
##   _tick(stage, t, delta) every game frame; t = 0 … 1 through the stage
##   _on_stop()             hide everything (ended, cut off, reset)
class_name ShipAttack
extends Node3D

enum Stage { IDLE, TELEGRAPH, FIRE, RECOVER }
const STAGE_NAMES := ["idle", "telegraph", "fire", "recover"]

var key := "" ## its name in the timeline data
var title := "" ## shown on the debug timeline
var demo: SkyshipDemo
var ship: Skyship
var stage := Stage.IDLE
var time := 0.0 ## seconds into the current stage
var lengths := {Stage.TELEGRAPH: 1.0, Stage.FIRE: 2.0, Stage.RECOVER: 0.8}
var params := {}


func _ready() -> void:
	top_level = true


func setup(owner: SkyshipDemo, boss: Skyship) -> void:
	demo = owner
	ship = boss
	_build()


## Start it with this entry's params (missing stage lengths keep the attack's own defaults).
func begin(new_params: Dictionary) -> void:
	params = new_params
	lengths[Stage.TELEGRAPH] = float(params.get("telegraph", lengths[Stage.TELEGRAPH]))
	lengths[Stage.FIRE] = float(params.get("fire", lengths[Stage.FIRE]))
	lengths[Stage.RECOVER] = float(params.get("recover", lengths[Stage.RECOVER]))
	_on_begin(params)
	_enter(Stage.TELEGRAPH)


func running() -> bool:
	return stage != Stage.IDLE


## Cut it off now (the ship staggered, a reset, a forced attack).
func stop() -> void:
	stage = Stage.IDLE
	_on_stop()


## Seconds the whole attack takes, and how far through it is (0 … 1).
func length() -> float:
	return lengths[Stage.TELEGRAPH] + lengths[Stage.FIRE] + lengths[Stage.RECOVER]


func elapsed() -> float:
	match stage:
		Stage.TELEGRAPH: return time
		Stage.FIRE: return lengths[Stage.TELEGRAPH] + time
		Stage.RECOVER: return lengths[Stage.TELEGRAPH] + lengths[Stage.FIRE] + time
	return 0.0


func tick(delta: float) -> void:
	if stage == Stage.IDLE:
		return
	time += delta
	var span: float = maxf(lengths[stage], 1e-3)
	_tick(stage, clampf(time / span, 0.0, 1.0), delta)
	if time >= span:
		if stage == Stage.RECOVER:
			stop()
		else:
			_enter((stage + 1) as Stage)


func _enter(next: Stage) -> void:
	stage = next
	time = 0.0
	_on_stage(next)


# --- hooks ---------------------------------------------------------------------------------------

func _build() -> void:
	pass


func _on_begin(_params: Dictionary) -> void:
	pass


func _on_stage(_stage: Stage) -> void:
	pass


func _tick(_stage: Stage, _t: float, _delta: float) -> void:
	pass


func _on_stop() -> void:
	pass
