#ifndef MC_PHYSICS_H
#define MC_PHYSICS_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cstdint>

#include "../game_manager/game_constants.h"

namespace godot {

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

    /**
     * @brief Creates a StaticBody3D with a CylinderShape3D and attaches it to p_parent.
     * @param p_parent The parent node (e.g., a MeshInstance3D).
     * @param p_layer The collision layer to set.
     * @param p_radius The radius of the cylinder.
     * @param p_height The height of the cylinder.
     * @param p_offset The local offset of the collider.
     * @return The created StaticBody3D.
     */
    static class StaticBody3D* create_static_cylinder_collider(
        Node3D* p_parent, 
        uint32_t p_layer, 
        float p_radius = 0.5f,
        float p_height = 1.0f,
        const Vector3& p_offset = Vector3(0.0f, 0.0f, 0.0f)
    );

    /**
     * @brief Creates a StaticBody3D with a SphereShape3D and attaches it to p_parent.
     * @param p_parent The parent node (e.g., a MeshInstance3D).
     * @param p_layer The collision layer to set.
     * @param p_radius The radius of the sphere.
     * @param p_offset The local offset of the collider.
     * @return The created StaticBody3D.
     */
    static class StaticBody3D* create_static_sphere_collider(
        Node3D* p_parent, 
        uint32_t p_layer, 
        float p_radius = 0.4f,
        const Vector3& p_offset = Vector3(0.0f, 0.0f, 0.0f)
    );
};

} // namespace godot

#endif // MC_PHYSICS_H
