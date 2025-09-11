extends Control

@export var WIDTH = 100
@export var HEIGHT = 100
@export var SPACING = 10.0

const BUCKETS := 20
const HISTOGRAM_WIDTH := 300
const HISTOGRAM_HEIGHT := 100

var hist_angle = []
var hist_radius = []

enum DistanceNorm {
    MANHATTAN,
    EUCLIDEAN,
    CHEBYSHEV
}

@export var NORM: DistanceNorm = DistanceNorm.MANHATTAN

@export var CELL_SIZE = 5.0

var poisson_points: Array = []
var voronoi_grid: Array = []

@export var noise_angle := FastNoiseLite.new()
@export var noise_radius := FastNoiseLite.new()
@export var biome_noise := FastNoiseLite.new()

func _ready():
    set_custom_minimum_size(Vector2(WIDTH, HEIGHT)) # Ensures visible area

    hist_angle.resize(BUCKETS)
    hist_radius.resize(BUCKETS)
    for i in BUCKETS:
        hist_angle[i] = 0
        hist_radius[i] = 0

    poisson_points = generate_poisson_points()

    generate_manhattan_voronoi()

    queue_redraw()

func _draw():
    var cols = int(WIDTH / CELL_SIZE)
    var rows = int(HEIGHT / CELL_SIZE)

    for x in range(cols):
        for y in range(rows):
            var owner_pos: Vector2 = poisson_points[voronoi_grid[x][y]]
            var color = Color(0.5, 0.5, 0.5) # fallback gray

            if owner_pos != null: # Make sure owner_pos is valid
                color = get_biome_color(owner_pos)

            var rect_pos = Vector2(x * CELL_SIZE, y * CELL_SIZE)
            draw_rect(Rect2(rect_pos, Vector2(CELL_SIZE, CELL_SIZE)), color)

    # Draw voronoi_grid
    for x in range(0, WIDTH + 1, int(SPACING)):
        draw_line(Vector2(x, 0), Vector2(x, HEIGHT), Color(0.7, 0.7, 0.7, 0.5), 1)

    for y in range(0, HEIGHT + 1, int(SPACING)):
        draw_line(Vector2(0, y), Vector2(WIDTH, y), Color(0.7, 0.7, 0.7, 0.5), 1)

    for point in poisson_points:
        draw_circle(point, 2, Color(0, 1, 0))

    # draw_histogram(hist_angle, Vector2(10, 10), Color(1, 0.5, 0.2, 0.8))
    # draw_histogram(hist_radius, Vector2(10, 150), Color(0.2, 0.7, 1.0, 0.8))


var poisson_map = {} # Dictionary: Vector2i -> Array of poisson_points

func generate_poisson_points() -> Array:
    var result := []
    var cols = int(WIDTH / SPACING)
    var rows = int(HEIGHT / SPACING)

    var RADIUS = SPACING * .4

    for x in range(cols):
        for y in range(rows):
            var cell_center = Vector2(x * SPACING + SPACING / 2.0, y * SPACING + SPACING / 2.0)

            var angle_val = (noise_angle.get_noise_2d(x, y) * 0.5) + 0.5
            var radius_val = noise_radius.get_noise_2d(x, y) * 0.5 + 0.5

            var angle = angle_val * TAU
            var radius = radius_val * RADIUS

            hist_angle.append(angle)
            hist_radius.append(radius)

            var offset = Vector2(cos(angle), sin(angle)) * radius
            var final_pos = cell_center + offset

            result.append(final_pos)

            poisson_map[Vector2i(x, y)] = final_pos

    return result

func draw_histogram(data: Array, origin: Vector2, color: Color):
    var histogram = bucketize(data, BUCKETS)

    print(histogram)

    var max_val = histogram.max()
    if max_val == 0:
        return

    var bar_width = HISTOGRAM_WIDTH / float(histogram.size())

    for i in histogram.size():
        var count = histogram[i]
        var bar_height = (float(count) / float(max_val)) * HISTOGRAM_HEIGHT

        var x = origin.x + i * bar_width
        var y = origin.y + (HISTOGRAM_HEIGHT - bar_height)

        var bar_size = Vector2(bar_width - 1, bar_height)
        draw_rect(Rect2(Vector2(x, y), bar_size), color)

func bucketize(data: Array, bucket_count: int) -> Array:
    var histogram := []
    histogram.resize(bucket_count)
    for i in bucket_count:
        histogram[i] = 0

    var min_val = data.min()
    var max_val = data.max()
    var diff = max_val - min_val

    for val in data:
        var norm = clamp((val - min_val) / diff, 0.0, 1.0)
        var index = int(floor(norm * bucket_count))
        index = clamp(index, 0, bucket_count - 1)
        histogram[index] += 1

    return histogram

func generate_manhattan_voronoi():
    var cols = int(WIDTH / CELL_SIZE)
    var rows = int(HEIGHT / CELL_SIZE)

    voronoi_grid.resize(cols)
    for x in range(cols):
        voronoi_grid[x] = []
        for y in range(rows):
            var cell_pos = Vector2(x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2)

            # Determine which Poisson voronoi_grid cell this pixel is in
            var grid_x = int(cell_pos.x / SPACING)
            var grid_y = int(cell_pos.y / SPACING)

            var closest = -1
            var min_dist = INF

            for dx in range(-1, 2):
                for dy in range(-1, 2):
                    var neighbor_cell = Vector2i(grid_x + dx, grid_y + dy)

                    if poisson_map.has(neighbor_cell):
                        var p = poisson_map[neighbor_cell]
                        var dist = distance(p, cell_pos)
                        if dist < min_dist:
                            min_dist = dist
                            closest = poisson_points.find(p) # get index to store

            voronoi_grid[x].append(closest)

func distance(p1: Vector2, p2: Vector2) -> float:
    match NORM:
        DistanceNorm.MANHATTAN:
            return abs(p1.x - p2.x) + abs(p1.y - p2.y)
        DistanceNorm.EUCLIDEAN:
            return p1.distance_to(p2)
        DistanceNorm.CHEBYSHEV:
            return max(abs(p1.x - p2.x), abs(p1.y - p2.y))
        _:
            return 0.0 # fallback

func get_biome_color(pos: Vector2) -> Color:
    var noise_val = (biome_noise.get_noise_2d(pos.x, pos.y) + 1) * 0.5 # Normalize [-1,1] to [0,1]
    if noise_val < 0.5:
        return Color(0, 0, 1) # Water blue
    # elif noise_val < 0.5:
    #     return Color(0, 1, 0) # Grass green
    else:
        return Color(1, 0, 0) # Mountain gray
