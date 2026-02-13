const My3dNode = @This();

pub fn register(r: *Registry) void {
    // Create class so we can add GDScript-exposed methods and properties
    const class = r.createClass(My3dNode, r.allocator, .auto);
    class.addMethod("set_spin_multiplier", .auto);
    class.addMethod("toggle_spin", .auto);
    class.addProperty("spin_multiplier", .auto);
}

allocator: Allocator,
base: *Control,
container: ?*SubViewportContainer = null,
viewport: ?*SubViewport = null,
world_root: ?*Node3d = null,
mesh_root: ?*Node3d = null,
mesh_inst: ?*MeshInstance3d = null,
spin_multiplier: f32 = 1.0,
angle: Vector3 = .{ .x = @as(f32, 0.0), .y = @as(f32, 0.0), .z = @as(f32, 0.0) },

pub fn create(allocator: *Allocator) !*My3dNode {
    const self = try allocator.create(My3dNode);
    self.* = .{
        .allocator = allocator.*,
        .base = Control.init(),
    };
    self.base.setInstance(My3dNode, self);
    return self;
}

pub fn destroy(self: *My3dNode, allocator: *Allocator) void {
    if (self.world_root) |w| w.destroy();
    if (self.viewport) |vp| vp.destroy();
    if (self.container) |c| c.destroy();
    self.base.destroy();
    allocator.destroy(self);
}

pub fn _enterTree(self: *My3dNode) void {
    if (Engine.isEditorHint()) return;

    // Ensure this node fills the area the parent gives it (use size_fill like SpriteNode)
    self.base.setHSizeFlags(.{ .size_fill = true });
    self.base.setVSizeFlags(.{ .size_fill = true });
    self.base.setAnchorsPreset(.preset_full_rect, .{});

    // Create a SubViewportContainer and SubViewport to host 3D content
    const c = SubViewportContainer.init();
    c.setHSizeFlags(.{ .size_fill = true });
    c.setVSizeFlags(.{ .size_fill = true });
    c.setAnchorsPreset(.preset_full_rect, .{});

    self.base.addChild(.upcast(c), .{});
    self.container = c;

    const vp = SubViewport.init();
    vp.setTransparentBackground(true);
    c.addChild(.upcast(vp), .{});
    self.viewport = vp;

    // Force initial sizes so the SubViewport matches the container
    const parent_size = c.getParentAreaSize();
    // let the container handle its own size; set the SubViewport pixel size to match parent area
    const vp_size = Vector2i.fromVector2(parent_size);
    vp.setSize(vp_size);

    // Create a world root for 3D objects
    const w = Node3d.init();
    vp.addChild(.upcast(w), .{});
    self.world_root = w;

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
    const mr = Node3d.init();
    mr.addChild(.upcast(mesh_inst), .{});
    self.mesh_inst = mesh_inst;
    self.mesh_root = mr;
    if (self.world_root) |wr| {
        if (self.mesh_root) |mr_non| wr.addChild(@ptrCast(mr_non), .{});
    }

    // Add a camera
    const cam = Camera3d.init();
    cam.setPosition(.{ .x = 0, .y = 0, .z = 3 });
    const cam_node: *Node = @ptrCast(cam);
    if (self.world_root) |wr| wr.addChild(cam_node, .{});

    // Add a light
    const light = DirectionalLight3d.init();
    light.setRotation(.{ .x = @as(f32, -0.5), .y = @as(f32, 0.5), .z = @as(f32, 0) });
    const light_node: *Node = @ptrCast(light);
    if (self.world_root) |wr2| wr2.addChild(light_node, .{});
}

pub fn _exitTree(self: *My3dNode) void {
    _ = self;
}

pub fn _process(self: *My3dNode, _: f64) void {
    // Apply spin multiplier to control speed from GDScript
    const dx: f32 = 0.02 * self.spin_multiplier;
    const dy: f32 = 0.015 * self.spin_multiplier;
    const dz: f32 = 0.01 * self.spin_multiplier;
    self.angle.x += dx;
    self.angle.y += dy;
    self.angle.z += dz;

    if (self.mesh_root) |m| m.setRotation(self.angle);
    // As a fallback, rotate the MeshInstance directly if present
    if (self.mesh_inst) |mi| mi.setRotation(self.angle);
    // std.log.debug("angles = {any} {any} {any} spin: {any}", .{ self.angle.x, self.angle.y, self.angle.z, self.spin_multiplier });

    // Keep SubViewport sized to the container in case the panel/splitter changed
    if (self.container) |cont| {
        const psize = cont.getParentAreaSize();
        cont.setSize(psize, .{});
        if (self.viewport) |vp2| {
            const vp_sz = Vector2i.fromVector2(psize);
            vp2.setSize(vp_sz);
        }
    }
}

pub fn setSpinMultiplier(self: *My3dNode, m: f64) void {
    self.spin_multiplier = @floatCast(m);
    std.log.info("spin_multiplier set to {d}", .{m});
}

pub fn getSpinMultiplier(self: *My3dNode) f64 {
    return @as(f64, self.spin_multiplier);
}

pub fn toggleSpin(self: *My3dNode) void {
    const currently = self.base.isProcessing();
    const new_state = !currently;
    self.base.setProcess(new_state);
    std.log.info("spin toggled to {s}", .{if (new_state) "on" else "off"});
}

const std = @import("std");
const Allocator = std.mem.Allocator;

const godot = @import("godot");
const Registry = godot.extension.Registry;
const Control = godot.class.Control;
const SubViewportContainer = godot.class.SubViewportContainer;
const SubViewport = godot.class.SubViewport;
const Node3d = godot.class.Node3d;
const MeshInstance3d = godot.class.MeshInstance3d;
const TorusMesh = godot.class.TorusMesh;
const Camera3d = godot.class.Camera3d;
const DirectionalLight3d = godot.class.DirectionalLight3d;
const Engine = godot.class.Engine;
const Mesh = godot.class.Mesh;
const Node = godot.class.Node;
const Vector3 = godot.builtin.Vector3;
const Vector2 = godot.builtin.Vector2;
const Vector2i = godot.builtin.Vector2i;
