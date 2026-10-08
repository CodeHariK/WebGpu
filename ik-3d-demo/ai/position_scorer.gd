## Where should a squad member stand? Every TacticalGrid cell near the player gets a score for a
## role (higher = better); the squad claims the best cell per member.
##
## Terms (see the weights below):
##   ring      distance from the player inside the role's [near, far] band
##   angle     where around the player, measured from the player's facing: 0° = in front,
##             +90° = the player's left, −90° = its right, 180° = behind (a surround slot gives
##             each member its own angle: Context.angle)
##   sight     the role wants to be seen (pressure: keep the player busy) or hidden (flankers)
##   in view   flankers lose a lot for being where the player is looking right now
##   cover     hidden roles like a cell with something solid between them and the player
##   spacing   away from cells the rest of the squad has claimed
##   level     about the player's height (pressure likes a little height)
##   travel    a bit closer to where the member is now (straight line; paths would be dearer)
##   edge      not right on a ledge
class_name PositionScorer
extends RefCounted

enum Role { PRESSURE, FLANK_LEFT, FLANK_RIGHT, BEHIND, SURROUND, AMBUSH } ## AMBUSH spots come from AmbushPlanner

const PROFILES := {
	Role.PRESSURE: {"ring": Vector2(4.0, 7.0), "angle": 0.0, "angle_weight": 1.0, "sight": 1, "avoid_view": 0.0},
	Role.FLANK_LEFT: {"ring": Vector2(2.5, 5.0), "angle": 105.0, "angle_weight": 2.5, "sight": -1, "avoid_view": 3.0},
	Role.FLANK_RIGHT: {"ring": Vector2(2.5, 5.0), "angle": -105.0, "angle_weight": 2.5, "sight": -1, "avoid_view": 3.0},
	Role.BEHIND: {"ring": Vector2(2.5, 5.0), "angle": 180.0, "angle_weight": 2.5, "sight": -1, "avoid_view": 3.0},
	Role.SURROUND: {"ring": Vector2(3.0, 4.5), "angle": 0.0, "angle_weight": 3.0, "sight": 0, "avoid_view": 0.0},
	Role.AMBUSH: {"ring": Vector2(2.5, 6.5), "angle": 0.0, "angle_weight": 0.0, "sight": -1, "avoid_view": 3.0},
}

const RING_WEIGHT := 1.5 ## per metre outside the band
const ANGLE_WEIGHT := 3.0 ## × the role's angle_weight, for being 180° off
const SIGHT_BONUS := 2.0
const COVER_BONUS := 0.5
const SPACING := 3.0 ## metres: closer than this to a claimed cell costs …
const SPACING_WEIGHT := 1.5 ## … this much per metre inside
const LEVEL_WEIGHT := 0.5 ## per metre above or below the player
const HEIGHT_BONUS := 0.4 ## pressure: per metre above the player (up to 2 m)
const TRAVEL_WEIGHT := 0.1 ## per metre from the member
const EDGE_COST := 0.5
const SEARCH_MARGIN := 3.0 ## cells up to the band's far edge + this are candidates


## What the scoring needs to know (filled by the squad each re-score).
class Context:
	var grid: TacticalGrid
	var visibility: PlayerVisibility
	var player := Vector3.ZERO ## the player's feet
	var facing := Vector3.FORWARD ## flat
	var from := Vector3.ZERO ## the member being placed
	var others: Array[Vector3] = [] ## cells the rest of the squad has claimed
	var angle := NAN ## degrees: this member's own angle (a surround slot); NAN = the role's


## Scores for every candidate cell: {cell: score}.
static func score_all(role: Role, context: Context) -> Dictionary:
	var scores := {}
	var reach: float = PROFILES[role].ring.y + SEARCH_MARGIN
	var grid := context.grid
	var home := grid.nearest_cell(context.from)
	for cell in grid.cell_count():
		if home >= 0 and grid.region[cell] != grid.region[home]:
			continue # can't get there (and back): a crate top, a ledge only reached by dropping
		var offset := grid.positions[cell] - context.player
		if Vector2(offset.x, offset.z).length() <= reach and absf(offset.y) < 3.0:
			scores[cell] = score(cell, role, context)
	return scores


## The best cell in `scores` (from score_all); −1 if empty.
static func best(scores: Dictionary) -> int:
	var best_cell := -1
	var best_score := -INF
	for cell: int in scores:
		if scores[cell] > best_score:
			best_score = scores[cell]
			best_cell = cell
	return best_cell


static func score(cell: int, role: Role, context: Context) -> float:
	var profile: Dictionary = PROFILES[role]
	var grid := context.grid
	var position := grid.positions[cell]
	var offset := position - context.player
	var flat := Vector3(offset.x, 0.0, offset.z)
	var distance := flat.length()
	var total := 0.0
	var ring: Vector2 = profile.ring
	total -= maxf(maxf(ring.x - distance, distance - ring.y), 0.0) * RING_WEIGHT
	if distance > 0.1:
		var angle := rad_to_deg(context.facing.signed_angle_to(flat, Vector3.UP))
		var wanted: float = profile.angle if is_nan(context.angle) else context.angle
		var off := absf(wrapf(angle - wanted, -180.0, 180.0)) / 180.0
		total -= off * profile.angle_weight * ANGLE_WEIGHT
	total += _sight_score(cell, profile, context.visibility)
	for other in context.others:
		total -= maxf(SPACING - position.distance_to(other), 0.0) * SPACING_WEIGHT
	total -= absf(offset.y) * LEVEL_WEIGHT
	if profile.sight > 0:
		total += clampf(offset.y, 0.0, 2.0) * (HEIGHT_BONUS + LEVEL_WEIGHT) # height doesn't count against pressure
	total -= position.distance_to(context.from) * TRAVEL_WEIGHT
	if grid.edge[cell] == 1:
		total -= EDGE_COST
	return total


static func _sight_score(cell: int, profile: Dictionary, visibility: PlayerVisibility) -> float:
	if visibility == null or visibility.sight.size() <= cell:
		return 0.0
	var seen := visibility.sight[cell] == PlayerVisibility.Sight.SEEN
	var total := 0.0
	if profile.sight > 0 and seen:
		total += SIGHT_BONUS
	elif profile.sight < 0 and visibility.sight[cell] == PlayerVisibility.Sight.HIDDEN:
		total += SIGHT_BONUS + COVER_BONUS * visibility.cover[cell]
	if seen and visibility.in_view(cell):
		total -= profile.avoid_view
	return total
