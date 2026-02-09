@tool
extends TextureRect

@export var temperature_noise: FastNoiseLite:
    set(value):
        temperature_noise = value
        if Engine.is_editor_hint():
            draw_map()
@export var moisture_noise: FastNoiseLite:
    set(value):
        moisture_noise = value
        if Engine.is_editor_hint():
            draw_map()
@export var altitude_noise: FastNoiseLite:
    set(value):
        altitude_noise = value
        if Engine.is_editor_hint():
            draw_map()
@export var river_noise: FastNoiseLite:
    set(value):
        river_noise = value
        if Engine.is_editor_hint():
            draw_map()

# New Biome Colors based on your request
const COLOR_OCEAN = Color("00008b") # DarkBlue
const COLOR_LAKE = Color.PINK # RoyalBlue

const COLOR_BEACH = Color("fafad2") # LightGoldenrodYellow
const COLOR_JUNGLE = Color.LIGHT_GREEN # ForestGreen

const COLOR_PLAINS = Color.MAGENTA # Color("7cfc00") # LawnGreen

const COLOR_DESERT = Color.BLACK # Color("f4a460") # SandyBrown
const COLOR_MOUNTAIN = Color("808080") # Gray
const COLOR_SNOW = Color.WHITE

func get_biome(temp, moist, alt, river):
    # return Color(temp, temp, temp)
    # return Color(moist, moist, moist)
    # return Color(alt, alt, alt)
    # return Color(alt * river, alt * river, alt * river)
    var altriver = 1 if alt * river > 0.1 else 0
    return Color(altriver, altriver, altriver)

    # # Ocean
    # if river < 0.3:
    #     return COLOR_OCEAN
    
    # # Beach
    # elif river >= 0.3 and river < 0.5:
    #     return COLOR_BEACH

    # return Color.BLACK
    
    # # Land Biomes
    # elif alt >= 0.5 and alt < 0.8:
    #     # Lakes (high moisture overrides other land biomes)
    #     if moist >= 0.9:
    #         return COLOR_LAKE

    #     # Jungle (hot and wet)
    #     elif temp > 0.6 and moist >= 0.4 and moist < 0.9:
    #         return COLOR_JUNGLE
    #     # Desert (hot and dry)
    #     elif temp > 0.6 and moist < 0.4:
    #         return COLOR_DESERT
    #     # Plains (temperate)
    #     else: # temp <= 0.6
    #         return COLOR_PLAINS
            
    # # Mountains
    # elif alt >= 0.8 and alt < 0.95:
    #     return COLOR_MOUNTAIN
    # # Snow
    # else: # alt >= 0.95
    #     return COLOR_SNOW

func draw_map():
    if not temperature_noise or not moisture_noise or not altitude_noise:
        return

    var current_size = get_size()
    var width = int(current_size.x)
    var height = int(current_size.y)

    if width <= 0 or height <= 0:
        return

    var image = Image.create_empty(width, height, false, Image.FORMAT_RGB8)
    
    for x in width:
        for y in height:
            # Get noise values in 0.0 to 1.0 range directly from noise objects
            var temp = (temperature_noise.get_noise_2d(x, y) + 1.0) / 2.0
            var moist = (moisture_noise.get_noise_2d(x, y) + 1.0) / 2.0
            var alt = (altitude_noise.get_noise_2d(x, y) + 1.0) / 2.0
            var river = (river_noise.get_noise_2d(x, y) + 1.0) / 2.0
            
            var biome_color = get_biome(temp, moist, alt, river)
            image.set_pixel(x, y, biome_color)

    var image_texture = ImageTexture.create_from_image(image)
    self.texture = image_texture

func _ready():
    # If noise resources aren't set, create new ones to avoid errors
    if not temperature_noise: temperature_noise = FastNoiseLite.new()
    if not moisture_noise: moisture_noise = FastNoiseLite.new()
    if not altitude_noise: altitude_noise = FastNoiseLite.new()
    
    # Connect the resized signal to automatically redraw the map when the control's size changes.
    if not is_connected("resized", Callable(self, "draw_map")):
        connect("resized", Callable(self, "draw_map"))

    # Create and display the biome map
    draw_map()
