## Noises the player (or anything) makes, delivered to every listening creature's senses.
## A noise is a position and a radius (how far it carries — footsteps ~2.5 m, a shot ~20 m).
## No walls muffle it yet (see Todo.md). `recent` keeps the last second of noises for debug rings.
class_name NoiseBus
extends RefCounted

const KEEP := 1.0 ## seconds a noise stays in `recent`

static var listeners: Array[CreatureSenses] = []
static var recent: Array[Dictionary] = [] ## {position, radius, time}


static func listen(senses: CreatureSenses) -> void:
	if not listeners.has(senses):
		listeners.append(senses)


static func stop_listening(senses: CreatureSenses) -> void:
	listeners.erase(senses)


## A noise at `position` that carries `radius` metres.
static func emit(position: Vector3, radius: float) -> void:
	var now := Time.get_ticks_msec() * 0.001
	recent = recent.filter(func(noise: Dictionary) -> bool: return now - noise.time < KEEP)
	recent.append({"position": position, "radius": radius, "time": now})
	for senses in listeners:
		senses.hear(position, radius)
