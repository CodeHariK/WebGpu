#ifndef MC_PHYSICS_H
#define MC_PHYSICS_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cstdint>

namespace godot {

/**
 * @brief Centralized collision layers for the Marching Cubes project.
 * Layers are defined as bits (1 << (layer_number - 1)).
 */
enum MCCollisionLayer : uint32_t {
    LAYER_NONE     = 0,
    LAYER_TERRAIN  = 1,          // Layer 1
    LAYER_OBJECTS  = 1 << 3,     // Layer 4 (8)
    LAYER_CORNERS  = 1 << 5,     // Layer 6 (32)
    LAYER_CELLS    = 1 << 6      // Layer 7 (64) - Debug/Dual Grid
};

/**
 * @brief Utility class for physics operations.
 */
class MCPhysics {
public:
    /**
     * @brief Creates a StaticBody3D with a BoxShape3D and attaches it to p_parent.
     * @param p_parent The parent node (e.g., a MeshInstance3D).
     * @param p_layer The collision layer to set.
     * @param p_size The size of the box shape.
     * @param p_offset The local offset of the collider.
     * @return The created StaticBody3D.
     */
    static class StaticBody3D* create_static_box_collider(
        Node3D* p_parent, 
        uint32_t p_layer, 
        const Vector3& p_size = Vector3(1.0f, 1.0f, 1.0f),
        const Vector3& p_offset = Vector3(0.0f, 0.0f, 0.0f)
    );
};

} // namespace godot

#endif // MC_PHYSICS_H
