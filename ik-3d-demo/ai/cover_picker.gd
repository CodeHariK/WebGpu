## Picks a spot to take cover from the player, using PlayerVisibility.
## A candidate is a COVER cell (hidden, with something solid right beside it toward the player).
## Score = distance from the creature, minus a bonus for peek cells (one step from a cell the
## player can see — a creature can lean out and fire). Lowest score wins.
## Distance is a straight line for now; it should become path length once routes cost exposure.
class_name CoverPicker
extends RefCounted


## The best cover cell for a creature at `from`; −1 if there's none.
static func nearest_cover(grid: TacticalGrid, visibility: PlayerVisibility, from: Vector3, peek_bonus := 2.0) -> int:
	var best := -1
	var best_score := INF
	for cell in grid.cell_count():
		if visibility.cover[cell] == 0 or visibility.sight[cell] != PlayerVisibility.Sight.HIDDEN:
			continue
		var score := grid.positions[cell].distance_to(from)
		if visibility.peek(cell):
			score -= peek_bonus
		if score < best_score:
			best_score = score
			best = cell
	return best
