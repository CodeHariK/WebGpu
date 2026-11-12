class_name Terrain
extends Node3D
## The ground, TerrainHeight.SIZE metres square, cut into chunks CHUNK_SIZE metres on a side.
## Chunking is what lets the renderer skip work: each chunk has its own bounding box, so the ones
## outside the camera's view are skipped (frustum culling), the far ones draw their coarse mesh
## (TerrainChunk's visibility ranges), and the ones behind a hill are skipped too (occlusion
## culling: each chunk's occluder; Project Settings > Rendering > Occlusion Culling is on).

const CHUNK_SIZE := 32.0

@export var material: Material

var chunks: Array[TerrainChunk] = []


func _ready() -> void:
	var per_side := int(TerrainHeight.SIZE / CHUNK_SIZE)
	var corner := -TerrainHeight.SIZE * 0.5
	for cz in per_side:
		for cx in per_side:
			var chunk := TerrainChunk.new()
			chunk.name = "Chunk_%d_%d" % [cx, cz]
			add_child(chunk)
			chunk.build(Vector2(corner + cx * CHUNK_SIZE, corner + cz * CHUNK_SIZE), CHUNK_SIZE, material)
			chunks.append(chunk)


## The visibility ranges on or off: off, every chunk draws its fine mesh at any distance.
func set_ranges(p_on: bool) -> void:
	for chunk in chunks:
		chunk.set_ranges(p_on)
