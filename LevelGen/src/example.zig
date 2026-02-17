pub fn register(r: *Registry) void {
    // Register a class inline - simplest approach
    r.addClass(SpriteNode, r.allocator, .auto);

    // Use modules to organize registration into separate files
    r.addModule(ExampleNode);
    r.addModule(GuiNode);
    r.addModule(SignalNode);
    r.addModule(My3dNode);

    r.addModule(Hello3D);

    r.addModule(SmoothMeshPart);
}

test "godot version is 4.x" {
    // Tests run inside Godot via `zig build test`
    try std.testing.expectEqual(4, godot.version.major);
}

const std = @import("std");
const godot = @import("godot");
const Registry = godot.extension.Registry;

const ExampleNode = @import("ExampleNode.zig");
const GuiNode = @import("GuiNode.zig");
const SignalNode = @import("SignalNode.zig");
const SpriteNode = @import("SpriteNode.zig");
const My3dNode = @import("My3dNode.zig");
const Hello3D = @import("Hello3D.zig");
const SmoothMeshPart = @import("terrain/smooth_mesh.zig");
