@tool
extends MeshInstance3D
## The froth's mesh: a tube lying across the foot of the waterfall, its ends rounded off, built
## here (like Blender's bottom foam: a curve turned into a tube). Dense enough for the vertex
## shader to push lumps and ridges into it: `rings` along it, `segments` round it. UV: x along
## (0..1), y round (0..1). The froth shaders read tube_length / tube_radius (keep them the same).

@export var length := 10.0:
	set(value):
		length = value
		_build()
@export var radius := 1.6:
	set(value):
		radius = value
		_build()
@export var rings := 160:
	set(value):
		rings = value
		_build()
@export var segments := 48:
	set(value):
		segments = value
		_build()


func _ready() -> void:
	_build()


func _build() -> void:
	var positions := PackedVector3Array()
	var normals := PackedVector3Array()
	var uvs := PackedVector2Array()
	var indices := PackedInt32Array()
	for i in rings + 1:
		var t := float(i) / rings
		# round ends: the radius closes like a superellipse over the last few percent
		var end := absf(t * 2.0 - 1.0)
		var r := radius * pow(maxf(1.0 - pow(end, 8.0), 0.0), 0.5)
		var x := (t - 0.5) * length
		for j in segments + 1:
			var a := TAU * float(j) / segments
			var around := Vector3(0.0, cos(a), sin(a))
			positions.append(Vector3(x, 0.0, 0.0) + around * r)
			normals.append(around)
			uvs.append(Vector2(t, float(j) / segments))
	var row := segments + 1
	for i in rings:
		for j in segments:
			var k := i * row + j
			# clockwise seen from outside: Godot's front faces
			indices.append_array([k, k + row, k + 1, k + 1, k + row, k + row + 1])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = positions
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_INDEX] = indices
	var tube := ArrayMesh.new()
	tube.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	mesh = tube
