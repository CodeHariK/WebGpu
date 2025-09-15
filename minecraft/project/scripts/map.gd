extends CanvasLayer

@export_range(128, 4096, 1, "exp") var dimension: int = 512

@onready var heightmap_rect: TextureRect = $TextureRect

var noise: FastNoiseLite
var gradient: Gradient
var gradient_tex: GradientTexture1D

var po2_dimensions: int

func _init() -> void:
    # Create a noise function as the basis for our heightmap.
    noise = FastNoiseLite.new()
    noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
    noise.fractal_octaves = 5
    noise.fractal_lacunarity = 1.9

    # Create a gradient to function as overlay.
    gradient = Gradient.new()
    gradient.add_point(0.6, Color(0.9, 0.9, 0.9, 1.0))
    gradient.add_point(0.8, Color(1.0, 1.0, 1.0, 1.0))
    # The gradient will start black, transition to grey in the first 70%, then to white in the last 30%.
    gradient.reverse()

    # Create a 1D texture (single row of pixels) from gradient.
    gradient_tex = GradientTexture1D.new()
    gradient_tex.gradient = gradient


# func _init() -> void:
#     # Create a noise function as the basis for our heightmap.
#     noise = FastNoiseLite.new()

#     # Use ridged fractal type to emphasize sharper peaks/mountains
#     noise.fractal_type = FastNoiseLite.FRACTAL_RIDGED
#     # Increase number of octaves for more detail / jaggedness
#     noise.fractal_octaves = 7
#     # Increase lacunarity to give more frequency change between octaves
#     noise.fractal_lacunarity = 2.5
#     # Maybe increase fractal gain (persistence) so higher octaves contribute more
#     noise.fractal_gain = 0.8

#     # (Optionally) enable domain warp for more distortion/roughness
#     noise.domain_warp_enabled = true
#     noise.domain_warp_type = FastNoiseLite.DOMAIN_WARP_SIMPLEX
#     noise.domain_warp_fractal_gain = 0.5
#     noise.domain_warp_fractal_octaves = 3
#     noise.domain_warp_fractal_lacunarity = 3.0

#     # Create a gradient to function as overlay.
#     gradient = Gradient.new()
#     # Using steeper falloff — tweak positions and colors as needed
#     gradient.add_point(0.0, Color(0, 0, 0, 1)) # edges fully dark
#     gradient.add_point(0.5, Color(0.0, 0.0, 0.1, 1)) # low dark
#     gradient.add_point(0.7, Color(0.5, 0.5, 0.5, 1)) # mid
#     gradient.add_point(0.9, Color(1.0, 1.0, 1.0, 1)) # near center bright
#     # You can reverse or not depending on how you want mapping
#     # gradient.reverse()

#     gradient_tex = GradientTexture1D.new()
#     gradient_tex.gradient = gradient

func _ready() -> void:
    po2_dimensions = nearest_po2(dimension)

    noise.frequency = 0.003 / (float(po2_dimensions) / 512.0)

    compute_island_cpu()

func compute_island_cpu() -> void:
    noise.seed = randi()
    var heightmap := noise.get_image(po2_dimensions, po2_dimensions, false, false)

    var center := Vector2i(po2_dimensions, po2_dimensions) / 2

    # for y in range(0, po2_dimensions):
    #     for x in range(0, po2_dimensions):
    #         var coord := Vector2i(x, y)
    #         var pixel := heightmap.get_pixelv(coord)
    #         # Calculate the distance between the coord and the center.
    #         var distance := Vector2(center).distance_to(Vector2(coord))
    #         # As the X and Y dimensions are the same, we can use center.x as a proxy for the distance
    #         # from the center to an edge.
    #         var gradient_color := gradient.sample(distance / float(center.x))
    #         # We use the v ('value') of the pixel here. This is not the same as the luminance we use
    #         # in the compute shader, but close enough for our purposes here.
    #         pixel.v *= gradient_color.v
    #         if pixel.v < 0.2:
    #             pixel.v = 0.0
    #         heightmap.set_pixelv(coord, pixel)


    for y in range(po2_dimensions):
        for x in range(po2_dimensions):
            var coord := Vector2i(x, y)
            var pixel := heightmap.get_pixelv(coord)
            # map noise value to 0‑1 from whatever range, if needed
            var h = pixel.v # expects v in 0‑1; you might remap if it's negative
            # optional: take absolute or ridged transform to emphasize peaks further
            # Example: ridged transform
            h = 1.0 - abs(2.0 * (h) - 1.0)

            # Distance based falloff (normalized 0 -> 1)
            var dist_norm = center.distance_to(Vector2(coord)) / float(center.x)
            # Apply steeper falloff: raise to a power >1
            dist_norm = pow(dist_norm, 3.0) # sharper drop near edges

            var grad_col = gradient.sample(dist_norm)
            # Multiply height by gradient
            h *= grad_col.v

            # Sharper threshold for water
            if h < 0.3:
                h = 0.0

            # Put back into pixel
            pixel.v = h
            heightmap.set_pixelv(coord, pixel)


    var tex := ImageTexture.create_from_image(heightmap)
    heightmap_rect.texture = tex
