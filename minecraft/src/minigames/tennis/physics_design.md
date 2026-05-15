# Tennis Ball Physics Design

## Manual Control (Recommended for Arcade/Snappy Feel)
If you want a game like *Mario Tennis* or *Wii Sports*, you should control the movement yourself using `move_and_collide`.

### Pros:
*   **Total Control:** You can implement "Special Shots," "Homing Balls," or "Time Slow" effects easily.
*   **Consistency:** The ball will bounce exactly how you code it, without physics engine "jitter."
*   **AI Friendly:** It is much easier for an AI to predict where the ball will land if the trajectory is a pure mathematical curve you control.
*   **No Tunneling:** You can ensure the ball never passes through the racket at high speeds.



### Mario Tennis Mechanics Reference
Mario Tennis is known for its "Rock-Paper-Scissors" shot system and "Magnetic" movement. Here is how it usually works:

*   **Buttons:**
    *   **A Button (Topspin):** Fast, high-velocity shot with a high bounce.
    *   **B Button (Slice):** Slower, curves in the air, and stays very low after the bounce.
    *   **Y Button (Flat):** The fastest standard shot, but predictable.
    *   **X Button (Lob/Drop):** Hitting Up + X lobs it over the opponent; Down + X drops it just over the net.
*   **Charging:** Players hold the button while moving to the ball to "charge" a shot. This increases power but slows down movement.
*   **Automatic Snap:** If a player swings while near the ball, the game slightly "warps" the player or the racket to ensure a clean hit. This makes it feel much better than a pure physical simulation.

---

### Implementation Plan for Your Tennis Game

#### 1. Environment (The Court)
*   **Spawn Ground:** A `StaticBody3D` with a `BoxShape3D`.
*   **Net:** A `StaticBody3D` with a thin box collider in the center.
*   **Markers:** `Node3D` markers for Service lines and Baseline to help the AI and scoring logic.

#### 2. Player (CharacterBody3D)
*   **Movement:** WASD movement.
*   **Collider:** **Cuboid Collider** (as requested). This is actually better than a capsule for tennis because it makes the "hittable area" very clear.
*   **Hit Detection:** An `Area3D` (the "Swing Zone") parented to the player. When the player presses an action button, we check if the Ball is inside this zone.
*   **State Machine:** `Idle`, `Running`, `Swinging`, `Stunned`.

#### 3. Ball (Manual Logic)
*   **Logic:** Use `move_and_collide` for the bounce.
*   **Physics:** 
    *   **Gravity:** Constant downward force.
    *   **Air Resistance:** Slight velocity decay.
    *   **Bounces:** `velocity = velocity.bounce(normal) * elasticity`.
*   **State:** `Serving`, `InPlay`, `Dead`.

#### 4. Controls (Input Map)
*   **WASD:** Movement.
*   **Space / J:** Topspin (Primary Shot).
*   **K:** Slice (Secondary Shot).
*   **L:** Lob/Drop.

---

### Proposed C++ Class Structure
I can help you build these classes. Here is the suggested hierarchy:

1.  **`TennisBall` (Inherits Node3D/CharacterBody3D):** Handles the manual trajectory and bounce logic.
2.  **`TennisPlayer` (Inherits CharacterBody3D):** Handles WASD movement and the "Swing Zone" detection.
3.  **`TennisManager` (Inherits Node):** Spawns the court, tracks the score, and resets the ball after a point.


