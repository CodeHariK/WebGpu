## The squad's plan and who plays which part in it.
##   PINCER    pressure in front + flank left + flank right + one behind (fewer members: pressure
##             first, then behind, then the flanks; more: extra pressure)
##   SURROUND  every member gets its own slot on a ring round the player, evenly spaced from the
##             front (slot 0 is in front, so someone always keeps the player busy)
## choose(): SURROUND when there are 4+ members and the player stands in the open (most cells
## round it are in its line of sight: nowhere to hide, so close every way out), else PINCER.
## assign(): turns the plan into parts (role + slot angle), works out where each part stands
## round the player and gives each part to the nearest free member (greedy, nearest pair first),
## so nobody crosses the arena to take a spot next to someone else.
class_name SquadPlan
extends RefCounted

enum Kind { PINCER, SURROUND }

const OPEN_RADIUS := 5.0 ## cells this close to the player decide whether it stands in the open …
const OPEN_SHARE := 0.7 ## … when at least this share of them is in its line of sight
const PINCER_ORDER: Array[PositionScorer.Role] = [
	PositionScorer.Role.PRESSURE, PositionScorer.Role.BEHIND, PositionScorer.Role.FLANK_LEFT, PositionScorer.Role.FLANK_RIGHT]


static func choose(member_count: int, grid: TacticalGrid, visibility: PlayerVisibility, player_feet: Vector3) -> Kind:
	if member_count < 4:
		return Kind.PINCER
	var near := 0
	var seen := 0
	for cell in grid.cell_count():
		if grid.positions[cell].distance_to(player_feet) < OPEN_RADIUS:
			near += 1
			if visibility.sight.size() > cell and visibility.sight[cell] == PlayerVisibility.Sight.SEEN:
				seen += 1
	return Kind.SURROUND if near > 0 and float(seen) / near >= OPEN_SHARE else Kind.PINCER


## The parts of `plan` for `count` members: [{role, angle}] (angle NAN = the role's own).
static func parts(plan: Kind, count: int) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for i in count:
		if plan == Kind.SURROUND:
			result.append({"role": PositionScorer.Role.SURROUND, "angle": wrapf(360.0 * i / count, -180.0, 180.0)})
		else:
			var role: PositionScorer.Role = PINCER_ORDER[i] if i < PINCER_ORDER.size() else PositionScorer.Role.PRESSURE
			result.append({"role": role, "angle": NAN})
	return result


## Give each member a part of `plan`: sets member.role and member.slot_angle. Returns the members
## whose role changed.
static func assign(plan: Kind, members: Array, player_feet: Vector3, facing: Vector3) -> Array:
	var todo := parts(plan, members.size())
	var spots: Array[Vector3] = []
	for part in todo:
		spots.append(_spot(part, player_feet, facing))
	var free_members := members.duplicate()
	var free_parts := range(todo.size())
	var changed := []
	while not free_members.is_empty():
		var best_member: Object = null
		var best_part := -1
		var best_distance := INF
		for member: Object in free_members:
			for part: int in free_parts:
				var distance: float = member.spider.global_position.distance_to(spots[part])
				if distance < best_distance:
					best_distance = distance
					best_member = member
					best_part = part
		var role: PositionScorer.Role = todo[best_part].role
		if best_member.role != role or not _same_angle(best_member.slot_angle, todo[best_part].angle):
			changed.append(best_member)
		best_member.role = role
		best_member.slot_angle = todo[best_part].angle
		free_members.erase(best_member)
		free_parts.erase(best_part)
	return changed


static func _same_angle(a: float, b: float) -> bool:
	return (is_nan(a) and is_nan(b)) or is_equal_approx(a, b)


# Where a part stands: on its ring (middle) at its angle round the player's facing.
static func _spot(part: Dictionary, player_feet: Vector3, facing: Vector3) -> Vector3:
	var profile: Dictionary = PositionScorer.PROFILES[part.role]
	var angle: float = profile.angle if is_nan(part.angle) else part.angle
	var ring: Vector2 = profile.ring
	return player_feet + facing.rotated(Vector3.UP, deg_to_rad(angle)) * (ring.x + ring.y) * 0.5
