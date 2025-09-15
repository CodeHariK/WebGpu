extends Control

const SCALE = 300
var offset = Vector2(50, 50)

var voronoi_points = []

var delaunay_edges = []
var delaunay_voronoi_verts = []
var delaunay_voronoi_edges = []

func _ready():
    var node = MinecraftNode.new()
    
    voronoi_points = node.voronoi_test()

    # Get and scale delaunay points
    var delaunay_result = node.delaunator_test()
    delaunay_edges = delaunay_result["delaunay_edges"]
    delaunay_voronoi_verts = delaunay_result["delaunay_voronoi_verts"]
    delaunay_voronoi_edges = delaunay_result["delaunay_voronoi_edges"]

    queue_redraw()

func _draw():
    for i in range(0, voronoi_points.size(), 2):
        var a = offset + voronoi_points[i] * SCALE
        var b = offset + voronoi_points[i + 1] * SCALE
        draw_line(a, b, Color.GREEN, 4.0)
        draw_circle(a, 8, Color.BLACK)
        draw_circle(b, 8, Color.WHITE)


    for i in range(0, delaunay_edges.size(), 2):
        draw_line(offset + delaunay_edges[i] * SCALE, offset + delaunay_edges[i + 1] * SCALE, Color.BLUE)
        
    for i in range(0, delaunay_voronoi_edges.size(), 2):
        draw_line(offset + delaunay_voronoi_edges[i] * SCALE, offset + delaunay_voronoi_edges[i + 1] * SCALE, Color.RED, 2)
        
    draw_perpendicular_bisectors()

    for point in delaunay_voronoi_verts:
        draw_circle(offset + point * SCALE, 4, Color.AQUA)

func draw_perpendicular_bisectors():
    for i in range(0, delaunay_edges.size(), 2):
        var a = offset + delaunay_edges[i] * SCALE
        var b = offset + delaunay_edges[i + 1] * SCALE

        var mid = (a + b) / 2.0
        var dir = (b - a).normalized()
        var perp = Vector2(-dir.y, dir.x) # 90° rotation

        var length = 10 # adjust to taste
        var p1 = mid - perp * length
        var p2 = mid + perp * length

        draw_line(p1, p2, Color.WHITE, 1.0)
