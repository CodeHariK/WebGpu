## What the player can see, per TacticalGrid cell — the basis for hiding, cover, peeking and
## flanking. Kept up to date a few rays at a time.
##
## For each cell, one ray from where a creature's body would be (`body_height` above the cell)
## toward the player's eye:
##   reaches the eye         SEEN   (the player has line of sight to it)
##   blocked                 HIDDEN, and if the blocker is within `cover_reach` of the cell the
##                           cell is COVER (something solid right there between it and the player)
## On top of that, from the player's facing:
##   in_view   SEEN and inside the player's field of view right now (what the camera shows)
##   peek      HIDDEN but next to a SEEN cell: step out, shoot, step back
##
## update() casts `rays_per_frame` rays, nearest cells first, and sweeps the whole grid again and
## again; each cell remembers when it was last checked. A full sweep of ~1000 cells at 64 rays
## per frame takes ~16 frames.
class_name PlayerVisibility
extends RefCounted

enum Sight { UNKNOWN, HIDDEN, SEEN }

var grid: TacticalGrid
var rays_per_frame := 64
var body_height := 0.6 ## where on a creature standing on the cell the player has to see
var cover_reach := 1.5 ## a blocker this close to the cell makes it cover
var view_angle := deg_to_rad(110.0) ## the player's field of view (whole angle)
var view_range := 30.0 ## further than this counts as hidden

var eye := Vector3.ZERO ## the player's eye (or car camera)
var facing := Vector3.FORWARD ## where the player looks (flattened)

# Per cell (indexed like grid.positions).
var sight := PackedByteArray() ## Sight
var cover := PackedByteArray() ## 1 = hidden with a blocker right next to it
var checked_at := PackedFloat32Array() ## seconds (update's `time`) when last checked

var sweep_msec := 0.0 ## how long the last full sweep took (wall time between its first and last ray)

var _order := PackedInt32Array() ## this sweep's cells, nearest to the eye first
var _next := 0 ## position in _order
var _sweep_started := 0


## Check the next batch of cells. Call every frame with the player's eye and facing.
func update(time: float, player_eye: Vector3, player_facing: Vector3) -> void:
	eye = player_eye
	var flat := Vector3(player_facing.x, 0.0, player_facing.z)
	if flat.length() > 0.01:
		facing = flat.normalized()
	if sight.size() != grid.cell_count():
		_reset()
	for i in rays_per_frame:
		if _next >= _order.size():
			_start_sweep()
			if _order.is_empty():
				return
		_check(_order[_next], time)
		_next += 1


## Seen and inside the player's view cone right now.
func in_view(cell: int) -> bool:
	if sight[cell] != Sight.SEEN:
		return false
	var to_cell := grid.positions[cell] - eye
	to_cell.y = 0.0
	return to_cell.length() < 0.5 or facing.angle_to(to_cell.normalized()) <= view_angle * 0.5


## Hidden, but a walk away from a cell the player can see: a spot to pop out from.
func peek(cell: int) -> bool:
	if sight[cell] != Sight.HIDDEN:
		return false
	for link: Array in grid.links[cell]:
		if link[2] == TacticalGrid.Link.WALK and sight[link[0]] == Sight.SEEN:
			return true
	return false


func count(state: Sight) -> int:
	var total := 0
	for value in sight:
		if value == state:
			total += 1
	return total


func cover_count() -> int:
	var total := 0
	for value in cover:
		total += value
	return total


# One ray from the cell's body point toward the eye: hits nothing = seen; else hidden, and cover
# if the hit is close to the cell.
func _check(cell: int, time: float) -> void:
	var body := grid.positions[cell] + Vector3.UP * body_height
	checked_at[cell] = time
	if body.distance_to(eye) > view_range:
		sight[cell] = Sight.HIDDEN
		cover[cell] = 0
		return
	var hit := grid.ray(body, eye)
	if hit.is_empty():
		sight[cell] = Sight.SEEN
		cover[cell] = 0
	else:
		sight[cell] = Sight.HIDDEN
		cover[cell] = 1 if body.distance_to(hit.position) <= cover_reach else 0


func _start_sweep() -> void:
	var now := Time.get_ticks_usec()
	if not _order.is_empty():
		sweep_msec = (now - _sweep_started) / 1000.0
	_sweep_started = now
	var cells := range(grid.cell_count())
	cells.sort_custom(func(a: int, b: int) -> bool: return grid.positions[a].distance_squared_to(eye) < grid.positions[b].distance_squared_to(eye))
	_order = PackedInt32Array(cells)
	_next = 0


func _reset() -> void:
	sight.resize(grid.cell_count())
	sight.fill(Sight.UNKNOWN)
	cover.resize(grid.cell_count())
	cover.fill(0)
	checked_at.resize(grid.cell_count())
	checked_at.fill(-1.0)
	_order = PackedInt32Array()
	_next = 0
