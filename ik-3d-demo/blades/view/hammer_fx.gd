## The hammer heads' impact effects (from the old hammer demo), all pure functions of the time
## since the impact (HammerImpacts ages them):
##   bursts   what an impact throws out: chunks (rubble, scrap) on ballistic arcs that land and lie
##            there, and glowing sparks that fly fast and die quickly — each burst's random numbers
##            come from its own seed, so a replay throws the same pieces
##   crater   a dark scorch on the ground where a ground pound landed, fading
class_name HammerFX
extends Node3D

const MAX_CHUNKS := 160
const MAX_SPARKS := 200
const GRAVITY := 25.0
const CHUNK_LIFE := 2.5 ## seconds a chunk lies there before it's gone
const SPARK_LIFE := 0.45

## One impact: where, how long ago, how hard, what it throws.
class Burst:
	var origin := Vector3.ZERO
	var since := 0.0
	var seed := 0
	var chunks := 0
	var sparks := 0
	var power := 6.0 ## m/s
	var chunk_color := Color(0.25, 0.2, 0.15)
	var up := Vector3.UP ## the side it sprays out of

var _chunks := MultiMeshInstance3D.new()
var _sparks := MultiMeshInstance3D.new()
var _crater := MeshInstance3D.new()


func _ready() -> void:
	_chunks.multimesh = _multimesh(MAX_CHUNKS, false)
	add_child(_chunks)
	_sparks.multimesh = _multimesh(MAX_SPARKS, true)
	add_child(_sparks)
	var disc := CylinderMesh.new()
	disc.top_radius = 1.0
	disc.bottom_radius = 1.0
	disc.height = 0.02
	_crater.mesh = disc
	var scorch := StandardMaterial3D.new()
	scorch.albedo_color = Color(0.05, 0.04, 0.03, 0.85)
	scorch.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_crater.material_override = scorch
	_crater.visible = false
	add_child(_crater)


## Throw out every burst in `bursts` (each as it is `since` seconds after its impact).
func draw_bursts(bursts: Array) -> void:
	var chunk_count := 0
	var spark_count := 0
	var chunk_mesh := _chunks.multimesh
	var spark_mesh := _sparks.multimesh
	for burst: Burst in bursts:
		var rng := RandomNumberGenerator.new()
		rng.seed = burst.seed
		for c in burst.chunks:
			var fly := _fly(rng, burst, 0.5, 1.0)
			var size := rng.randf_range(0.07, 0.2)
			var spin := rng.randf_range(4.0, 14.0)
			if chunk_count >= MAX_CHUNKS or burst.since > CHUNK_LIFE:
				continue
			var at := _ballistic(burst.origin, fly, burst.since)
			var shrink := clampf((CHUNK_LIFE - burst.since) / 0.5, 0.0, 1.0)
			var turned := Basis(fly.normalized().cross(Vector3.UP).normalized() if absf(fly.normalized().y) < 0.99 else Vector3.RIGHT, spin * minf(burst.since, _landing(burst.origin, fly)))
			chunk_mesh.set_instance_transform(chunk_count, Transform3D(turned.scaled(Vector3.ONE * size * shrink), at))
			chunk_mesh.set_instance_color(chunk_count, burst.chunk_color.darkened(rng.randf() * 0.4))
			chunk_count += 1
		for s in burst.sparks:
			var fly := _fly(rng, burst, 1.2, 2.2)
			var life := SPARK_LIFE * rng.randf_range(0.5, 1.0)
			if spark_count >= MAX_SPARKS or burst.since > life:
				continue
			var at := _ballistic(burst.origin, fly, burst.since)
			var streak := fly.normalized() if fly.length() > 0.01 else Vector3.UP
			var size := 0.05 * (1.0 - burst.since / life)
			var x := streak.cross(Vector3.UP).normalized() if absf(streak.y) < 0.99 else Vector3.RIGHT
			var y := streak.cross(x)
			spark_mesh.set_instance_transform(spark_count, Transform3D(Basis(x * size, y * size, streak * size * 5.0), at)) # stretched along its flight
			spark_mesh.set_instance_color(spark_count, Color(1.0, 0.6, 0.15).lerp(Color(1.0, 0.2, 0.05), burst.since / life))
			spark_count += 1
	chunk_mesh.visible_instance_count = chunk_count
	spark_mesh.visible_instance_count = spark_count


## A scorch at `at` (radius `radius`), `since` seconds old (negative: none).
func draw_crater(
		at: Vector3,
		radius: float,
		since: float
) -> void:
	_crater.visible = since >= 0.0 and since < 4.0
	if not _crater.visible:
		return
	var grow := clampf(since / 0.08, 0.0, 1.0)
	_crater.global_position = Vector3(at.x, 0.01, at.z)
	_crater.scale = Vector3(radius * grow, 1.0, radius * grow)
	(_crater.material_override as StandardMaterial3D).albedo_color.a = 0.85 * clampf((4.0 - since) / 1.0, 0.0, 1.0)


# A random launch velocity: out of the burst's `up` side, power × [low, high].
func _fly(
		rng: RandomNumberGenerator,
		burst: Burst,
		low: float,
		high: float
) -> Vector3:
	var dir := Vector3(rng.randf_range(-1.0, 1.0), rng.randf_range(-1.0, 1.0), rng.randf_range(-1.0, 1.0)).normalized()
	if dir.dot(burst.up) < 0.0:
		dir = -dir
	dir = (dir + burst.up * 0.6).normalized()
	return dir * burst.power * rng.randf_range(low, high)


# Thrown from `origin` at `velocity`, `t` seconds later: falls, and lies where it lands.
static func _ballistic(
		origin: Vector3,
		velocity: Vector3,
		t: float
) -> Vector3:
	var land := _landing(origin, velocity)
	var u := minf(t, land)
	var at := origin + velocity * u + Vector3.DOWN * 0.5 * GRAVITY * u * u
	at.y = maxf(at.y, 0.03)
	return at


# When something thrown from `origin` at `velocity` reaches the ground.
static func _landing(
		origin: Vector3,
		velocity: Vector3
) -> float:
	var h := maxf(origin.y, 0.0)
	return (velocity.y + sqrt(velocity.y * velocity.y + 2.0 * GRAVITY * h)) / GRAVITY


func _multimesh(
		count: int,
		glow: bool
) -> MultiMesh:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	var box := BoxMesh.new()
	box.size = Vector3.ONE
	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.roughness = 0.9
	if glow:
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	box.material = material
	multimesh.mesh = box
	multimesh.instance_count = count
	multimesh.visible_instance_count = 0
	return multimesh
