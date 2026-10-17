## A fire trail behind one blade: a ribbon of triangles swept by the blade over the last
## LIFETIME seconds, drawn with fire_trail.gdshader.
##
## Every frame the owner calls add_sample(base, tip) with the blade's two ends. The ribbon joins
## consecutive samples: two vertices per sample (base, tip), two triangles between samples.
## Each vertex's UV carries what the shader needs:
##   UV.x  age: 0 = this frame, 1 = LIFETIME ago (about to vanish)
##   UV.y  across: 0 at the blade's base, 1 at its tip
## When the blade stops moving no samples arrive, the old ones age out and the trail shrinks away.
class_name FireTrail
extends MeshInstance3D

const LIFETIME := 0.3 ## seconds a sample stays in the trail
const MIN_STEP := 0.02 ## a new sample only when the tip moved this far (no slivers when still)

var _mesh := ImmediateMesh.new()
var _bases := PackedVector3Array()
var _tips := PackedVector3Array()
var _times := PackedFloat64Array()


func _init(shader_material: ShaderMaterial) -> void:
	mesh = _mesh
	material_override = shader_material
	cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	top_level = true # samples are world positions


func add_sample(
		base: Vector3,
		tip: Vector3
) -> void:
	if not _tips.is_empty() and _tips[-1].distance_to(tip) < MIN_STEP:
		return
	_bases.append(base)
	_tips.append(tip)
	_times.append(_now())


## Drop samples older than LIFETIME and rebuild the ribbon (call once per frame).
func rebuild() -> void:
	var now := _now()
	var keep := 0
	while keep < _times.size() and now - _times[keep] > LIFETIME:
		keep += 1
	if keep > 0:
		_bases = _bases.slice(keep)
		_tips = _tips.slice(keep)
		_times = _times.slice(keep)
	_mesh.clear_surfaces()
	if _tips.size() < 2:
		return
	_mesh.surface_begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in _tips.size() - 1: # quad between sample i (older) and i + 1 (newer)
		var age_a := clampf((now - _times[i]) / LIFETIME, 0.0, 1.0)
		var age_b := clampf((now - _times[i + 1]) / LIFETIME, 0.0, 1.0)
		_vertex(_bases[i], age_a, 0.0)
		_vertex(_tips[i], age_a, 1.0)
		_vertex(_tips[i + 1], age_b, 1.0)
		_vertex(_bases[i], age_a, 0.0)
		_vertex(_tips[i + 1], age_b, 1.0)
		_vertex(_bases[i + 1], age_b, 0.0)
	_mesh.surface_end()


func clear_trail() -> void:
	_bases.clear()
	_tips.clear()
	_times.clear()
	_mesh.clear_surfaces()


func _vertex(
		position: Vector3,
		age: float,
		across: float
) -> void:
	_mesh.surface_set_uv(Vector2(age, across))
	_mesh.surface_add_vertex(position)


static func _now() -> float:
	return Time.get_ticks_usec() / 1_000_000.0
