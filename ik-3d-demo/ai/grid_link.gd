## A hand-placed connection the grid scan can't find by itself: a ladder up a wall or a jump over
## a gap. Put one GridLink at each end; point `other` at the far one (only one of the pair needs
## it). TacticalGrid snaps both ends to their nearest cells and adds the link.
##   LADDER  climb between levels (spiders leap it)
##   JUMP    a leap across a gap or up onto a ledge
@tool
class_name GridLink
extends Marker3D

enum Kind { LADDER, JUMP }

@export var other: GridLink ## the far end
@export var kind := Kind.JUMP
@export var two_way := true ## false = only from this end to `other`
@export var cost := 3.0 ## extra path cost on top of the distance (leaps are slow and risky)

const GROUP := "grid_links"


func _enter_tree() -> void:
	add_to_group(GROUP)
