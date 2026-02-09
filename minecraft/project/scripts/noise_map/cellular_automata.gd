extends Control

@export var MAP_WIDTH := 100
@export var MAP_HEIGHT := 100

@export var CELL_SIZE := 5
@export var NOISE_THRESHOLD := 0.0
@export var CA_ITERATIONS := 5

@export var RANDOM_MAP := true:
    set(value):
        RANDOM_MAP = value
        noise.seed = randi()
        _generate_cave_map()

@export var noise: FastNoiseLite

var cave_map: Array = []

func _ready() -> void:
    size = Vector2(MAP_WIDTH * CELL_SIZE, MAP_HEIGHT * CELL_SIZE)

    _generate_cave_map()

func _generate_cave_map():
    cave_map.clear()

    # Step 1: Initialize map using Perlin noise
    for y in MAP_HEIGHT:
        var row := []
        for x in MAP_WIDTH:
            var n := noise.get_noise_2d(float(x), float(y))
            var solid := n > NOISE_THRESHOLD
            row.append(solid)
        cave_map.append(row)

    # Step 2: Apply CA smoothing
    for _i in CA_ITERATIONS:
        cave_map = _run_cellular_step(cave_map)

    queue_redraw()

func _run_cellular_step(map: Array) -> Array:
    var new_map := []

    for y in MAP_HEIGHT:
        var row := []
        for x in MAP_WIDTH:
            var wall_count := _count_wall_neighbors(map, x, y)

            if map[y][x]:
                row.append(wall_count >= 4)
            else:
                row.append(wall_count >= 5)
        new_map.append(row)

    return new_map

func _count_wall_neighbors(map: Array, x: int, y: int) -> int:
    var count := 0
    for dy in range(-1, 2):
        for dx in range(-1, 2):
            if dx == 0 and dy == 0:
                continue

            var nx := x + dx
            var ny := y + dy

            if nx < 0 or ny < 0 or nx >= MAP_WIDTH or ny >= MAP_HEIGHT:
                count += 1 # Treat out-of-bounds as wall
            elif map[ny][nx]:
                count += 1
    return count

func marching_squares():
    var half := CELL_SIZE / 2.0

    for y in range(MAP_HEIGHT - 1):
        for x in range(MAP_WIDTH - 1):
            var top_left: bool = cave_map[y][x]
            var top_right: bool = cave_map[y][x + 1]
            var bottom_left: bool = cave_map[y + 1][x]
            var bottom_right: bool = cave_map[y + 1][x + 1]

            # Build 4-bit index
            var index := 0
            if top_left: index |= 1
            if top_right: index |= 2
            if bottom_right: index |= 4
            if bottom_left: index |= 8

            var cell_origin := Vector2(x * CELL_SIZE, y * CELL_SIZE)

            # Define midpoints
            var top = cell_origin + Vector2(half, 0)
            var right = cell_origin + Vector2(CELL_SIZE, half)
            var bottom = cell_origin + Vector2(half, CELL_SIZE)
            var left = cell_origin + Vector2(0, half)

            match index:
                1, 14:
                    draw_line(left, top, Color.RED, 1)
                2, 13:
                    draw_line(top, right, Color.RED, 1)
                3, 12:
                    draw_line(left, right, Color.RED, 1)
                4, 11:
                    draw_line(right, bottom, Color.RED, 1)
                5:
                    draw_line(left, top, Color.RED, 1)
                    draw_line(right, bottom, Color.RED, 1)
                6, 9:
                    draw_line(top, bottom, Color.RED, 1)
                7, 8:
                    draw_line(left, bottom, Color.RED, 1)
                10:
                    draw_line(top, right, Color.RED, 1)
                    draw_line(left, bottom, Color.RED, 1)
                _:
                    pass # 0 or 15 (fully empty or full) = no contour


func _draw() -> void:
    draw_rect(Rect2(Vector2.ZERO, size), Color.DARK_GRAY)

    for y in MAP_HEIGHT:
        for x in MAP_WIDTH:
            var solid: bool = cave_map[y][x]
            var color := Color.BLACK if solid else Color.WHITE
            draw_rect(Rect2(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE), color)

    marching_squares()

func _process(_delta: float) -> void:
    queue_redraw()

    if Engine.is_editor_hint():
        return

func _input(event):
    if event is InputEventKey and event.pressed and event.keycode == KEY_SPACE:
        _generate_cave_map()
