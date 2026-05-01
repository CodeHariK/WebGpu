# Overcooked Minigame Documentation

This document provides a high-level overview of the classes and structures used in the Overcooked-style minigame.

## Core Management

### [OvercookedManager](../src/minigames/overcooked/oc_manager.h)
The central hub for the minigame.
- **Responsibilities**: Manages active orders, scores, station/ingredient registration, and global inventory.
- **Key Methods**: 
    - `submit_ingredient()`: Validates a dish against active orders.
    - `spawn_random_order()`: Picks a recipe and adds it to the queue.
    - `try_consume_inventory()`: Checks and decrements stock before spawning items.

### [Inventory](../src/game_manager/inventory.h)
A generic resource management node.
- **Usage**: The `OvercookedManager` uses this to track ingredient quantities (Tomatoes, Lettuce, etc.).
- **Logic**: Simple `add_item` and `try_consume` methods with JSON persistence.

---

## Data Structures

### `OCRecipeRequirement` (Struct)
Defines a single component of a dish.
- `String type`: The ingredient name (e.g., "Tomato").
- `int state`: The required state (0=Raw, 1=Chopped, 2=Cooked).

### `OCOrder` (Struct)
An active customer request.
- `String dish_name`: Name displayed on the UI.
- `std::vector<OCRecipeRequirement> requirements`: List of items needed to complete the dish.
- `float time_remaining`: Countdown before the order expires.

### [OCRecipe](../src/minigames/overcooked/oc_recipe.h) (Resource)
A template for a dish, stored as JSON in `user://recipes/`.

---

## Entities

### [OCIngredient](../src/minigames/overcooked/oc_ingredient.h)
Base class for anything that can be picked up and processed.
- **States**: `STATE_RAW`, `STATE_CHOPPED`, `STATE_COOKED`, `STATE_BURNT`.
- **Logic**: Handles its own visual updates based on its current state.

### [OCPlate](../src/minigames/overcooked/oc_plate.h)
A special type of ingredient that can "hold" other ingredients.
- **Function**: Used to combine multiple processed items (e.g., Chopped Tomato + Chopped Lettuce) for delivery.

---

## Stations

### [OCStation](../src/minigames/overcooked/oc_station.h)
Base class for all interactive surfaces. Inherits from `Interactable`.
- **Methods**: `place_item()`, `take_item()`, and `interact()`.

### Specialized Stations:
- **[OCIngredientCrate](../src/minigames/overcooked/oc_crate.h)**: Spawns ingredients by consuming items from the manager's `Inventory`.
- **[OCCuttingStation](../src/minigames/overcooked/oc_cutting_station.h)**: Progressively changes an ingredient's state from `RAW` to `CHOPPED` when interacted with.
- **[OCCookingStation](../src/minigames/overcooked/oc_cooking_station.h)**: A timer-based station that cooks items (Raw -> Cooked -> Burnt).
- **[OCDeliveryStation](../src/minigames/overcooked/oc_delivery_station.h)**: The end-point. Call `submit_ingredient()` on the manager when an item is placed here.
