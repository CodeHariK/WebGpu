## The music clock everything in the hall dances to (no audio yet): beats per minute, which beat
## and bar we're on, and how far through the current beat we are.
##
##   phase  0 → 1 across one beat (0 = the hit)
##   pulse  1 on the hit, falling fast to 0 (a "thump" envelope for lights)
##   beat   counts up forever;  bar = beat / 4
class_name DiscoBeat
extends Node

signal beat_hit(beat: int)
signal bar_hit(bar: int)

const BEATS_PER_BAR := 4

var bpm := 118.0
var playing := true

var beat := 0
var phase := 0.0
var time := 0.0 ## seconds of music played


func _process(delta: float) -> void:
	if not playing:
		return
	time += delta
	var beats := time * bpm / 60.0
	var now := int(beats)
	phase = beats - now
	while beat < now:
		beat += 1
		beat_hit.emit(beat)
		if beat % BEATS_PER_BAR == 0:
			bar_hit.emit(beat / BEATS_PER_BAR)


## 1 on the beat, decaying to 0; `sharpness` higher = shorter thump.
func pulse(sharpness := 6.0) -> float:
	return exp(-phase * sharpness)


func bar() -> int:
	return beat / BEATS_PER_BAR


## Which beat inside the bar (0 … 3).
func beat_in_bar() -> int:
	return beat % BEATS_PER_BAR
