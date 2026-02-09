extends Control

@onready var radial_menu: Control = $"."

@export var bg_color: Color = Color(0, 0, 0, 0.5)
@export var line_color: Color = Color(1, 1, 1)
@export var sector_count: int = 6
@export var inner_radius: float = 64.0
@export var outer_radius: float = 256.0
@export var sector_segments: int = 16
@export var gap_angle_deg: float = 5.0
@export var sector_colors: Array[Color] = []

var menu_visible: bool = true
var hovered_sector: int = -1

func _ready() -> void:
    if sector_colors.size() < sector_count:
        # Default to rainbow colors
        for i in range(sector_count):
            sector_colors.append(Color.from_hsv(float(i) / sector_count, 1.0, 1.0))
    set_process(true)
    set_process_input(true)
    radial_menu.visible = menu_visible

func _draw() -> void:
    if not menu_visible:
        return

    var center = size / 2
    draw_circle(center, outer_radius, bg_color)

    var angle_per_sector = 360.0 / sector_count
    var gap_radians = deg_to_rad(gap_angle_deg)

    for i in range(sector_count):
        var start_angle = deg_to_rad(i * angle_per_sector) + gap_radians / 2
        var end_angle = deg_to_rad((i + 1) * angle_per_sector) - gap_radians / 2
        var color = sector_colors[i % sector_colors.size()]
        if i == hovered_sector:
            color = color.lightened(0.3)
        _draw_sector(center, inner_radius, outer_radius, start_angle, end_angle, color)

    draw_circle(center, inner_radius, bg_color.darkened(0.5))

func _draw_sector(center: Vector2, inner_r: float, outer_r: float, start_angle: float, end_angle: float, color: Color) -> void:
    var points := []
    
    if sector_segments == 0:
        # Full smooth circle, approximate with many segments (e.g. 32)
        var steps: int = 32
        var step_angle: float = (end_angle - start_angle) / steps
        
        # Outer arc
        for i in range(steps + 1):
            var angle = start_angle + i * step_angle
            points.append(center + Vector2.RIGHT.rotated(angle) * outer_r)
            
        # Inner arc (reverse)
        for i in range(steps, -1, -1):
            var angle = start_angle + i * step_angle
            points.append(center + Vector2.RIGHT.rotated(angle) * inner_r)
            
    elif sector_segments == 1:
        # One-sided polygon (triangle sector)
        var outer_start = center + Vector2.RIGHT.rotated(start_angle) * outer_r
        var outer_end = center + Vector2.RIGHT.rotated(end_angle) * outer_r
        var inner_end = center + Vector2.RIGHT.rotated(end_angle) * inner_r
        var inner_start = center + Vector2.RIGHT.rotated(start_angle) * inner_r

        points.append(outer_start)
        points.append(outer_end)
        points.append(inner_end)
        points.append(inner_start)
        
    else:
        # Polygonal with multiple segments
        var steps: int = max(2, sector_segments)
        var step_angle: float = (end_angle - start_angle) / steps
        
        # Outer arc
        for i in range(steps + 1):
            var angle = start_angle + i * step_angle
            points.append(center + Vector2.RIGHT.rotated(angle) * outer_r)
            
        # Inner arc (reverse)
        for i in range(steps, -1, -1):
            var angle = start_angle + i * step_angle
            points.append(center + Vector2.RIGHT.rotated(angle) * inner_r)
            
    if points.size() > 2:
        draw_colored_polygon(points, color)


func _process(_delta: float) -> void:
    queue_redraw()

    if Engine.is_editor_hint():
        return

    if Input.is_action_just_pressed("toggle_radial_menu") and InputMap.has_action("toggle_radial_menu"):
        menu_visible = !menu_visible
        radial_menu.visible = menu_visible

    if menu_visible:
        _update_hovered_sector()

func _update_hovered_sector() -> void:
    var center = size / 2
    var mouse_pos = get_local_mouse_position()
    var dist = (mouse_pos - center).length()
    
    if dist < inner_radius or dist > outer_radius:
        hovered_sector = -1
        return

    var angle = atan2(mouse_pos.y - center.y, mouse_pos.x - center.x)
    if angle < 0:
        angle += TAU

    var angle_per_sector = TAU / sector_count
    hovered_sector = int(angle / angle_per_sector)

func _input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        _update_hovered_sector()
