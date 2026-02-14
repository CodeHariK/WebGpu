const std = @import("std");
const Allocator = std.mem.Allocator;

const godot = @import("godot");
const Registry = godot.extension.Registry;
const Node3d = godot.class.Node3d;
const MeshInstance3d = godot.class.MeshInstance3d;
const TorusMesh = godot.class.TorusMesh;
const Engine = godot.class.Engine;
const Mesh = godot.class.Mesh;
const Node = godot.class.Node;
const Vector3 = godot.builtin.Vector3;
const Vector2 = godot.builtin.Vector2;
const Vector2i = godot.builtin.Vector2i;

const Hello3D = @This();

allocator: Allocator,
base: *Node3d,
mesh_inst: ?*MeshInstance3d = null,
spin_multiplier: f32 = 1.0,
angle: Vector3 = .{ .x = @as(f32, 0.0), .y = @as(f32, 0.0), .z = @as(f32, 0.0) },

pub fn register(r: *Registry) void {
    // Create class so we can add GDScript-exposed methods and properties
    const class = r.createClass(Hello3D, r.allocator, .auto);
    class.addMethod("toggle_spin", .auto);
    class.addProperty("spin_multiplier", .auto);
}

pub fn create(allocator: *Allocator) !*Hello3D {
    const self = try allocator.create(Hello3D);
    self.* = .{
        .allocator = allocator.*,
        .base = .init(),
    };
    self.base.setInstance(Hello3D, self);
    return self;
}

pub fn _ready(self: *Hello3D) void {
    if (Engine.isEditorHint()) return;

    _ = self;
}

pub fn destroy(self: *Hello3D, allocator: *Allocator) void {
    self.base.destroy();
    allocator.destroy(self);
}

pub fn _enterTree(self: *Hello3D) void {
    if (Engine.isEditorHint()) return;

    std.log.info("My3dNode initialized", .{});

    // Enable per-frame callbacks
    self.base.setProcess(true);

    // Add a torus mesh instance (cast to Mesh resource)
    const mesh_inst = MeshInstance3d.init();
    const torus = TorusMesh.init();
    const mesh_ptr: *Mesh = @ptrCast(torus);
    mesh_inst.setMesh(mesh_ptr);
    // Make the mesh big so it's clearly visible
    mesh_inst.setScale(Vector3.initXYZ(3.0, 3.0, 3.0));

    // Parent mesh directly to `base` (no separate `mesh_root`)
    self.mesh_inst = mesh_inst;
    self.base.addChild(.upcast(mesh_inst), .{});
}

pub fn _exitTree(self: *Hello3D) void {
    _ = self;
}

pub fn _process(self: *Hello3D, _: f64) void {
    // Apply spin multiplier to control speed from GDScript
    const dx: f32 = 0.02 * self.spin_multiplier;
    const dy: f32 = 0.015 * self.spin_multiplier;
    const dz: f32 = 0.01 * self.spin_multiplier;
    self.angle.x += dx;
    self.angle.y += dy;
    self.angle.z += dz;

    // Rotate the MeshInstance directly (no `mesh_root`)
    if (self.mesh_inst) |mi| mi.setRotation(self.angle);
    // std.log.debug("angles = {any} {any} {any} spin: {any}", .{ self.angle.x, self.angle.y, self.angle.z, self.spin_multiplier });
}

pub fn setSpinMultiplier(self: *Hello3D, m: f64) void {
    self.spin_multiplier = @floatCast(m);
    std.log.info("spin_multiplier set to {d}", .{m});
}

pub fn getSpinMultiplier(self: *Hello3D) f64 {
    return @as(f64, self.spin_multiplier);
}

pub fn toggleSpin(self: *Hello3D) void {
    const currently = self.base.isProcessing();
    const new_state = !currently;
    self.base.setProcess(new_state);
    std.log.info("spin toggled to {s}", .{if (new_state) "on" else "off"});
}
