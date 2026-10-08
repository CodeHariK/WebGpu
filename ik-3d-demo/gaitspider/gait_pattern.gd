## When each leg steps: one shared clock (phase 0..1 per cycle) and a fixed offset per leg.
## A leg is in the air while its own phase (clock + offset, wrapped) is below `swing` (= 1 − duty),
## and on the ground for the rest of the cycle.
##
## Offsets come from three numbers (k = pair index from the front, 0..3; side: left 0, right 1):
##   offset = side × side_shift + (k mod 2) × alternate + k × wave
##   side_shift  left and right legs of a pair are this far apart (0.5 = opposite)
##   alternate   neighbouring legs on one side are this far apart (0.5 = opposite)
##   wave        an extra shift per pair: the back legs lift a little before the front ones, so a
##               ripple runs forward along each side (the metachronal wave of real spiders)
##
## Presets:
##   SPIDER     alternating tetrapod (L1 R2 L3 R4 vs R1 L2 R3 L4) with a back-to-front wave, legs
##              down ~75% of the time — what the reference video shows
##   TETRAPOD   the same sets lifting exactly together (no wave): stiff, robotic
##   RIPPLE     one wave per side, each leg a quarter cycle after the one behind: slow, creepy
##   INSECT     6 legs, alternating tripods, half the time down: a fast scuttle
class_name GaitPattern
extends RefCounted

enum Preset { SPIDER, TETRAPOD, RIPPLE, INSECT }

const PRESETS := {
	Preset.SPIDER: {"legs": 8, "duty": 0.75, "side_shift": 0.5, "alternate": 0.5, "wave": 0.08},
	Preset.TETRAPOD: {"legs": 8, "duty": 0.6, "side_shift": 0.5, "alternate": 0.5, "wave": 0.0},
	Preset.RIPPLE: {"legs": 8, "duty": 0.8, "side_shift": 0.5, "alternate": 0.0, "wave": 0.25},
	Preset.INSECT: {"legs": 6, "duty": 0.5, "side_shift": 0.5, "alternate": 0.5, "wave": 0.0},
}

var duty := 0.75 ## share of the cycle a foot is on the ground
var side_shift := 0.5
var alternate := 0.5
var wave := 0.08


func apply(preset: Preset) -> int:
	var p: Dictionary = PRESETS[preset]
	duty = p.duty
	side_shift = p.side_shift
	alternate = p.alternate
	wave = p.wave
	return p.legs


func swing() -> float:
	return 1.0 - duty


func offset(pair: int, side: int) -> float:
	var right := 1 if side > 0 else 0
	return fposmod(right * side_shift + (pair % 2) * alternate + pair * wave, 1.0)


## The leg's own phase now (0..1). In the air while it is below swing().
func leg_phase(clock: float, pair: int, side: int) -> float:
	return fposmod(clock + offset(pair, side), 1.0)
