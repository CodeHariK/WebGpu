## The squad's spot-picking: each member, in turn, scores the cells around where the squad thinks
## the player is (PositionScorer) and claims the best; claims made earlier this round push later
## ones apart. A member only moves its claim when a new cell is clearly better (HYSTERESIS), so it
## doesn't dither between two near-equal spots.
## Also sets each member's route costs: everyone goes round the player (PERSONAL_SPACE, so getting
## to the far side doesn't mean walking through it); hidden roles also pay for cells the player
## can see (exposure).
class_name SquadPositions
extends RefCounted

const HYSTERESIS := 0.75 ## a new cell must beat the current claim's score by this much
const PERSONAL_SPACE := 2.0 ## metres round the player that routes avoid …
const PERSONAL_SPACE_COST := 6.0 ## … at this cost per cell
const SEEN_COST := 4.0 ## route cost per cell the player can see (hidden roles only) …
const IN_VIEW_COST := 10.0 ## … and per cell in its view right now


## Re-score and re-claim for every member. `player_feet` = where the squad thinks the player stands.
## Calls squad.claim(member, cell) when a member's claim changes.
static func rescore(squad: Squad, player_feet: Vector3) -> void:
	var grid := squad.grid
	var context := PositionScorer.Context.new()
	context.grid = grid
	context.visibility = squad.visibility
	context.player = player_feet
	context.facing = squad.visibility.facing
	var around := _personal_space_costs(grid, player_feet)
	var exposure := _exposure_costs(squad.visibility, around)
	var claimed: Array[Vector3] = []
	for member in squad.members:
		context.from = member.spider.global_position
		context.others = claimed
		context.angle = member.slot_angle
		member.scores = PositionScorer.score_all(member.role, context)
		var best := PositionScorer.best(member.scores)
		var current: float = member.scores.get(member.cell, -INF)
		if best >= 0 and (member.cell < 0 or member.scores[best] > current + HYSTERESIS):
			squad.claim(member, best)
		if member.cell >= 0:
			claimed.append(grid.positions[member.cell])
		var hidden_role: bool = PositionScorer.PROFILES[member.role].sight < 0
		member.route_cost = exposure if hidden_role else around


static func _personal_space_costs(grid: TacticalGrid, player_feet: Vector3) -> PackedFloat32Array:
	var costs := PackedFloat32Array()
	costs.resize(grid.cell_count())
	for cell in grid.cell_count():
		if grid.positions[cell].distance_to(player_feet) < PERSONAL_SPACE:
			costs[cell] = PERSONAL_SPACE_COST
	return costs


# Exposure plus the personal-space costs (just those if the player's view isn't swept yet).
static func _exposure_costs(visibility: PlayerVisibility, around: PackedFloat32Array) -> PackedFloat32Array:
	var exposure := visibility.exposure_costs(SEEN_COST, IN_VIEW_COST)
	if exposure.size() != around.size():
		return around
	for cell in exposure.size():
		exposure[cell] += around[cell]
	return exposure
