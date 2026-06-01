#include "mc_physics.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

StaticBody3D* MCPhysics::create_static_box_collider(
    Node3D* p_parent, 
    uint32_t p_layer, 
    const Vector3& p_size,
    const Vector3& p_offset
) {
    if (!p_parent) return nullptr;

    StaticBody3D *sb = memnew(StaticBody3D);
    sb->set_collision_layer(p_layer);
    sb->set_collision_mask(0); // By default, static world doesn't need to scan others
    sb->set_position(p_offset);
    p_parent->add_child(sb);

    CollisionShape3D *cs = memnew(CollisionShape3D);
    sb->add_child(cs);

    Ref<BoxShape3D> shape;
    shape.instantiate();
    shape->set_size(p_size);
    cs->set_shape(shape);

    return sb;
}

StaticBody3D* MCPhysics::create_static_cylinder_collider(
    Node3D* p_parent, 
    uint32_t p_layer, 
    float p_radius,
    float p_height,
    const Vector3& p_offset
) {
    if (!p_parent) return nullptr;

    StaticBody3D *sb = memnew(StaticBody3D);
    sb->set_collision_layer(p_layer);
    sb->set_collision_mask(0);
    sb->set_position(p_offset);
    p_parent->add_child(sb);

    CollisionShape3D *cs = memnew(CollisionShape3D);
    sb->add_child(cs);

    Ref<CylinderShape3D> shape;
    shape.instantiate();
    shape->set_radius(p_radius);
    shape->set_height(p_height);
    cs->set_shape(shape);

    return sb;
}

StaticBody3D* MCPhysics::create_static_sphere_collider(
    Node3D* p_parent, 
    uint32_t p_layer, 
    float p_radius,
    const Vector3& p_offset
) {
    if (!p_parent) return nullptr;

    StaticBody3D *sb = memnew(StaticBody3D);
    sb->set_collision_layer(p_layer);
    sb->set_collision_mask(0);
    sb->set_position(p_offset);
    p_parent->add_child(sb);

    CollisionShape3D *cs = memnew(CollisionShape3D);
    sb->add_child(cs);

    Ref<SphereShape3D> shape;
    shape.instantiate();
    shape->set_radius(p_radius);
    cs->set_shape(shape);

    return sb;
}

} // namespace godot
