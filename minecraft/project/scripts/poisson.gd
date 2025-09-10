extends Control

@export var noise: FastNoiseLite

func _ready():
	var width = 100
	var height = 100
	var spacing = 10.0
	
	var positions = generate_poisson_points(width, height, spacing)

	var noise = OpenSimplexNoise.new()
	noise.seed = 12345
	noise.octaves = 4
	noise.period = 20.0
	noise.persistence = 0.8

	for pos in positions:
		var noise_val = noise.get_noise_2d(pos.x, pos.y) # range: [-1, 1]
		var normalized = clamp(noise_val / 2.0, -0.5, 0.5)
		var rotation_deg = (normalized + 0.5) * 360.0

		# Place your object (e.g., tree, rock)
		var instance = preload("res://Tree.tscn").instantiate()
		instance.position = pos
		instance.rotation_degrees = rotation_deg
		add_child(instance)

func generate_poisson_points(width: float, height: float, min_dist: float) -> Array:
	var points := []
	var rng = RandomNumberGenerator.new()
	rng.seed = 42 # fixed seed for determinism

	var cols = int(width / min_dist)
	var rows = int(height / min_dist)

	for x in range(cols):
		for y in range(rows):
			var px = (x + rng.randf()) * min_dist
			var py = (y + rng.randf()) * min_dist

			# Optional: only keep points within bounds
			if px < width and py < height:
				points.append(Vector2(px, py))

	return points


https: / / github.com / Nebukam / PCGExtendedToolkit
