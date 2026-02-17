pub fn register(r: *Registry) void {
    r.addModule(SmoothMeshPart);
}

test "godot version is 4.x" {
    // Tests run inside Godot via `zig build test`
    try std.testing.expectEqual(4, godot.version.major);
}

const std = @import("std");
const godot = @import("godot");
const Registry = godot.extension.Registry;

const SmoothMeshPart = @import("smooth_mesh.zig");
