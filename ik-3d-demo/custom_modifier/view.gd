## Viewer for node_3d.tscn. The original scene has no camera or light (it was meant to be
## looked at in the editor viewport), so running it shows nothing. This adds a camera, a light,
## a red ball for CustomModifier's look-at target, and moves that target in a circle so the
## modifier has something to follow.
##   Space: CustomModifier on / off (influence 1 / 0) — compare the bone with and without it
##   A:     animation play / pause
extends Node3D

@export var orbit_center := Vector3(0, 3, 10) ## the model stands at (0, 0, 10)
@export var orbit_radius := 3.0
@export var turns_per_second := 0.25

@onready var _modifier: SkeletonModifier3D = $Demo/model/Armature/Skeleton3D/CustomModifier
@onready var _animation: AnimationPlayer = $Demo/model/AnimationPlayer
@onready var _pose_label: Label3D = $Demo/model/Label3D
@onready var _marker: Node3D = $TargetMarker
@onready var _hud: Label = $Hud/Label

var _angle := 0.0


func _ready() -> void:
	_modifier.influence = 1.0
	_animation.play("move")
	# The original label is huge and far to the side; shrink it and put it above the model.
	_pose_label.pixel_size = 0.006
	_pose_label.position = Vector3(-6.5, 7.0, 0)


func _process(delta: float) -> void:
	_angle = fmod(_angle + delta * turns_per_second * TAU, TAU)
	var target := orbit_center + Vector3(cos(_angle), 0.5 * sin(_angle * 2.0), sin(_angle)) * orbit_radius
	_modifier.set("target_coordinate", target)
	_marker.global_position = target
	_hud.text = "Space: CustomModifier %s (influence %.0f)\nA: animation %s" % [
		"ON" if _modifier.influence > 0.5 else "off",
		_modifier.influence,
		"playing" if _animation.is_playing() else "paused",
	]


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	match key.keycode:
		KEY_SPACE:
			_modifier.influence = 0.0 if _modifier.influence > 0.5 else 1.0
		KEY_A:
			if _animation.is_playing():
				_animation.pause()
			else:
				_animation.play()
