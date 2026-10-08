## Attack tokens (Doom's trick): only `count` squad members may attack at once. A member asks for
## a token when it's ready to attack; it gets one if one is free and hands it back when the attack
## ends. A returned token rests `cooldown` seconds before anyone can take it, so attacks come in a
## readable rhythm instead of all at once. The squad asks in priority order (best shot first).
class_name AttackTokens
extends RefCounted

var count := 1
var cooldown := 1.2 ## seconds a returned token rests

var holders: Array = [] ## whoever holds a token (Squad.Member)
var _resting: Array[float] = [] ## seconds left for each resting token


func update(delta: float) -> void:
	for i in range(_resting.size() - 1, -1, -1):
		_resting[i] -= delta
		if _resting[i] <= 0.0:
			_resting.remove_at(i)


func free_count() -> int:
	return maxi(count - holders.size() - _resting.size(), 0)


## True if `who` holds a token now (already had one, or got a free one).
func request(who: Object) -> bool:
	if holders.has(who):
		return true
	if free_count() == 0:
		return false
	holders.append(who)
	return true


func release(who: Object) -> void:
	if holders.has(who):
		holders.erase(who)
		_resting.append(cooldown)


func has(who: Object) -> bool:
	return holders.has(who)
