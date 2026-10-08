## What the squad knows about the player, pooled from every member's senses (shared memory).
##   UNAWARE    nobody knows: each member does its own thing (its HunterBrain)
##   SPOTTING   a member is alert and sees the player: it barks, and after BARK_DELAY (the call
##              taking time to land) the whole squad is ALERT
##   ALERT      the squad knows: every tick the last sighting is shared to all members (so they all
##              stay alert), and the Squad steers them by role
##   SEARCHING  nobody has seen the player for LOSE_AFTER: sharing stops, each member's own senses
##              lose the player and search on their own; when all are back to unaware, so is the squad
## A member that spots the player again (searching makes that quick) starts SPOTTING again.
class_name SquadKnowledge
extends RefCounted

enum State { UNAWARE, SPOTTING, ALERT, SEARCHING }

const BARK_DELAY := 0.5 ## seconds from the spotter's call to everyone knowing
const LOSE_AFTER := 2.0 ## seconds with no member seeing the player → searching

var state := State.UNAWARE
var last_known := Vector3.INF ## the player's centre when last seen by anyone
var last_known_velocity := Vector3.ZERO
var unseen_for := 0.0 ## seconds since any member saw the player
var spotter: Object = null ## the member that called it (Squad.Member)

var _call_left := 0.0


## Pool the members' senses. `bark` is called as bark(member, text) when someone calls something out.
func update(delta: float, members: Array, bark: Callable) -> void:
	var seer: Object = _member_who_sees(members)
	if seer != null:
		var senses: CreatureSenses = seer.brain.senses
		last_known = senses.last_known
		last_known_velocity = senses.last_known_velocity
		unseen_for = 0.0
		if state == State.UNAWARE or state == State.SEARCHING:
			state = State.SPOTTING
			spotter = seer
			_call_left = BARK_DELAY
			bark.call(seer, "there!")
	else:
		unseen_for += delta
	match state:
		State.SPOTTING:
			_call_left -= delta
			if _call_left <= 0.0:
				state = State.ALERT
		State.ALERT:
			if unseen_for > LOSE_AFTER:
				state = State.SEARCHING
				bark.call(_nearest(members, last_known), "lost it")
			else:
				for member: Object in members:
					member.brain.senses.share(last_known, last_known_velocity)
		State.SEARCHING:
			if members.all(func(member: Object) -> bool: return member.brain.senses.awareness == CreatureSenses.Awareness.UNAWARE):
				state = State.UNAWARE


func knows() -> bool:
	return state == State.ALERT


# A member that is alert and sees the player right now (alert = past its own suspicion build-up).
static func _member_who_sees(members: Array) -> Object:
	for member: Object in members:
		var senses: CreatureSenses = member.brain.senses
		if senses.sees_player and senses.awareness == CreatureSenses.Awareness.ALERT:
			return member
	return null


static func _nearest(members: Array, point: Vector3) -> Object:
	var best: Object = null
	var best_distance := INF
	for member: Object in members:
		var distance: float = member.spider.global_position.distance_to(point)
		if distance < best_distance:
			best_distance = distance
			best = member
	return best
