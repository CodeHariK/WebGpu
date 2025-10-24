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

@export var width = 512
@export var height = 512

# Biome Colors
const COLOR_SNOW = Color.WHITE
const COLOR_MOUNTAIN = Color.GRAY
const COLOR_DESERT = Color.YELLOW # Sandy Brown
const COLOR_ARID = Color.MAROON # Peru
const COLOR_MARSHY = Color.LIME_GREEN # Sea Green
const COLOR_DARK_FOREST = Color.DARK_GREEN # Dark Green
const COLOR_SPRING = Color.PINK # Lawn Green
const COLOR_WATER = Color.CYAN # Cornflower Blue

func get_biome(temp, moist, alt):
    # Altitude-based biomes take priority
    if alt > 0.75:
        return COLOR_MOUNTAIN
    if alt < 0.2:
        return COLOR_WATER

    # Temperature-based biomes
    if temp < 0.2:
        return COLOR_SNOW
    if temp > 0.8:
        if moist < 0.25:
            return COLOR_DESERT
        else:
            return COLOR_ARID # Hot but not dry enough for desert

    # Moisture and Temperature combined
    if moist > 0.7:
        if temp < 0.6:
            return COLOR_MARSHY

    if moist > 0.4 and temp < 0.4:
        return COLOR_DARK_FOREST

    # Default biome
    return COLOR_SPRING

func draw_map():
    if not temperature_noise or not moisture_noise or not altitude_noise:
        return

    var image = Image.create(width, height, false, Image.FORMAT_RGB8)
    
    for x in width:
        for y in height:
            # Get noise values in 0.0 to 1.0 range directly from noise objects
            var temp = (temperature_noise.get_noise_2d(x, y) + 1.0) / 2.0
            var moist = (moisture_noise.get_noise_2d(x, y) + 1.0) / 2.0
            var alt = (altitude_noise.get_noise_2d(x, y) + 1.0) / 2.0
            
            var biome_color = get_biome(temp, moist, alt)
            image.set_pixel(x, y, biome_color)

    var image_texture = ImageTexture.create_from_image(image)
    self.texture = image_texture

func _ready():
    # If noise resources aren't set, create new ones to avoid errors
    if not temperature_noise: temperature_noise = FastNoiseLite.new()
    if not moisture_noise: moisture_noise = FastNoiseLite.new()
    if not altitude_noise: altitude_noise = FastNoiseLite.new()
    
    # Create and display the biome map
    draw_map()

func _input(event):
    if event.is_action_pressed("ui_accept"):
        # Re-seed the noise for a new map and redraw
        temperature_noise.seed = randi()
        moisture_noise.seed = randi()
        altitude_noise.seed = randi()
        draw_map()
