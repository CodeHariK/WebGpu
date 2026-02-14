const std = @import("std");
const SmoothMeshPart = @import("smooth_mesh.zig").SmoothMeshPart;
const Vector2i = @import("godot").builtin.Vector2i;
const Vector3 = @import("godot").builtin.Vector3;
const PackedInt32Array = @import("godot").builtin.PackedInt32Array;
const cast = @import("../debug/cast.zig").cast;

pub fn generateTerrainHeights(self: *SmoothMeshPart, indexPos: Vector2i, dim: usize, _: f32, height_curve_sampling: bool) PackedInt32Array {
    const offset = Vector3.initXYZ(
        cast(f64, indexPos.x) * self.part_size,
        0.0,
        cast(f64, indexPos.y) * self.part_size,
    );

    var heights = PackedInt32Array.init();
    _ = heights.resize(@intCast(dim * dim));

    const use_curve = (self.height_curve != null) and height_curve_sampling;

    for (0..dim) |z_idx| {
        for (0..dim) |x_idx| {
            var h: f64 = 0.5;
            if (self.noise != null) {
                h += self.noise.?.getNoise2d(
                    offset.x + cast(f32, x_idx),
                    offset.z + cast(f32, z_idx),
                );
            } else if (use_curve) {
                h = self.height_curve.?.sample(h);
            }

            const height_i: i32 = @intFromFloat(std.math.floor(h * self.part_size));
            heights.index(x_idx + z_idx * dim).* = height_i;
        }
    }

    return heights;
}
