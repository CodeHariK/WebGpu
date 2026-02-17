const std = @import("std");
const Allocator = std.mem.Allocator;

const godot = @import("godot");
const Registry = godot.extension.Registry;
const Node3d = godot.class.Node3d;
const Engine = godot.class.Engine;
const Mesh = godot.class.Mesh;
const Node = godot.class.Node;
const Object = godot.class.Object;
const Vector3 = godot.builtin.Vector3;
const Vector2 = godot.builtin.Vector2;
const Vector2i = godot.builtin.Vector2i;
const ArrayMesh = godot.class.ArrayMesh;
const PackedVector3Array = godot.builtin.PackedVector3Array;
const PackedInt32Array = godot.builtin.PackedInt32Array;
const MeshInstance3D = godot.class.MeshInstance3d;
const Curve = godot.class.Curve;
const Noise = godot.class.Noise;
const Array = godot.builtin.Array;
const Variant = godot.builtin.Variant;
const Material = godot.class.Material;
const StringName = godot.builtin.StringName;
const String = godot.builtin.String;
const PropertyHint = godot.global.PropertyHint;

pub const SmoothMeshPart = @This();

allocator: Allocator,
base: *Node3d,

vertices: PackedVector3Array,
indices: PackedInt32Array,

part_size: f32 = 32.0,

height_curve: ?*Curve = null,
noise: ?*Noise = null,
terrain_material: ?*Material,

const Part = struct {
    mesh: MeshInstance3D,
    visible: bool,
    has_collision: bool,
    last_visible_time: f64,
};

pub fn register(r: *Registry) void {
    const class = r.createClass(SmoothMeshPart, r.allocator, .auto);

    class.addProperty("height_curve", .{
        .hint = .property_hint_resource_type,
        .hint_string = .fromLatin1("Curve"),
    });
    class.addProperty("noise", .{
        .hint = .property_hint_resource_type,
        .hint_string = .fromLatin1("Noise"),
    });
    class.addProperty("terrain_material", .{
        .hint = .property_hint_resource_type,
        .hint_string = .fromLatin1("Material"),
    });
}

pub fn create(allocator: *Allocator) !*SmoothMeshPart {
    const self = try allocator.create(SmoothMeshPart);

    self.* = .{
        .allocator = allocator.*,
        .base = .init(),
        .vertices = PackedVector3Array.init(),
        .indices = PackedInt32Array.init(),
        .height_curve = null,
        .noise = null,
        .terrain_material = null,
    };
    self.base.setInstance(SmoothMeshPart, self);
    return self;
}

pub fn _ready(self: *SmoothMeshPart) void {
    if (Engine.isEditorHint()) return;

    _ = self;
}

pub fn destroy(self: *SmoothMeshPart, allocator: *Allocator) void {
    self.base.destroy();
    allocator.destroy(self);
}

pub fn _enterTree(self: *SmoothMeshPart) void {
    if (Engine.isEditorHint()) return;

    std.log.info("SmoothMesh initialized", .{});

    // Enable per-frame callbacks
    self.base.setProcess(true);

    if (self.height_curve == null or self.noise == null or self.terrain_material == null) {
        std.log.warn("Null SmoothMeshPart", .{});
        return;
    }
    // Example: create and add a generated terrain mesh at (0,0)
    const idx = Vector2i.initXY(0, 0);
    const m = self.generateSmoothPartMesh(.fromComptimeLatin1("Mine"), idx, true);
    self.base.addChild(.upcast(m), .{});
}

pub fn _exitTree(self: *SmoothMeshPart) void {
    _ = self;
}

pub fn _process(self: *SmoothMeshPart, _: f64) void {
    _ = self;
}

// --- Property getters/setters ---

pub fn getHeightCurve(self: *SmoothMeshPart) ?*Curve {
    return self.height_curve;
}

pub fn setHeightCurve(self: *SmoothMeshPart, curve: *Curve) void {
    self.height_curve = curve;
}

pub fn getNoise(self: *SmoothMeshPart) ?*Noise {
    return self.noise;
}

pub fn setNoise(self: *SmoothMeshPart, n: *Noise) void {
    self.noise = n;
}

pub fn getTerrainMaterial(self: *SmoothMeshPart) ?*Material {
    return self.terrain_material;
}

pub fn setTerrainMaterial(self: *SmoothMeshPart, mat: *Material) void {
    self.terrain_material = mat;
}

pub fn generateSmoothPartMesh(self: *SmoothMeshPart, name: StringName, indexPos: Vector2i, height_curve_sampling: bool) *MeshInstance3D {
    const vert_dim_i32: i32 = @as(i32, @intFromFloat(self.part_size)) + 1;
    const vert_dim: usize = @intCast(vert_dim_i32);

    // Sample heights
    var heights = GenerateTerrainHeights(self, indexPos, vert_dim, 1.0, height_curve_sampling);

    // Build vertex buffer
    var vertices = PackedVector3Array.init();
    _ = vertices.resize(@intCast(vert_dim * vert_dim));
    for (0..vert_dim) |z| {
        for (0..vert_dim) |x| {
            const h_i: i32 = heights.index(x + z * vert_dim).*;
            const y: f32 = @as(f32, @floatFromInt(h_i));
            vertices.set(
                @intCast(x + z * vert_dim),
                Vector3.initXYZ(
                    @floatFromInt(x),
                    y,
                    @floatFromInt(z),
                ),
            );
        }
    }

    // Build index buffer (two triangles per quad)
    var indices = PackedInt32Array.init();
    const part_size_usize: usize = cast(usize, self.part_size);
    const quad_count: usize = part_size_usize * part_size_usize;
    _ = indices.resize(@intCast(quad_count * 6));
    var write_idx: usize = 0;
    for (0..part_size_usize) |z| {
        for (0..part_size_usize) |x| {
            const idx0: i32 = @intCast(x + z * vert_dim);
            const idx1: i32 = idx0 + 1;
            const idx2: i32 = @intCast(x + (z + 1) * vert_dim);
            const idx3: i32 = idx2 + 1;

            indices.index(write_idx).* = idx0;
            write_idx += 1;
            indices.index(write_idx).* = idx1;
            write_idx += 1;
            indices.index(write_idx).* = idx2;
            write_idx += 1;

            indices.index(write_idx).* = idx1;
            write_idx += 1;
            indices.index(write_idx).* = idx3;
            write_idx += 1;
            indices.index(write_idx).* = idx2;
            write_idx += 1;
        }
    }

    // Normals (flat upward normals)
    var normals = PackedVector3Array.init();
    _ = normals.resize(@intCast(vert_dim * vert_dim));
    for (0..vert_dim * vert_dim) |i| {
        normals.set(
            cast(i64, i),
            Vector3.initXYZ(0.0, 1.0, 0.0),
        );
    }

    // Build Godot `Array` of surface arrays
    var arrays = godot.builtin.Array.init();
    _ = arrays.resize(@intFromEnum(Mesh.ArrayType.array_max));

    _ = arrays.set(
        @intFromEnum(Mesh.ArrayType.array_vertex),
        Variant.init(PackedVector3Array, vertices),
    );
    _ = arrays.set(
        @intFromEnum(Mesh.ArrayType.array_normal),
        Variant.init(PackedVector3Array, normals),
    );
    _ = arrays.set(
        @intFromEnum(Mesh.ArrayType.array_index),
        Variant.init(PackedInt32Array, indices),
    );

    // Create ArrayMesh and add surface
    const arr_mesh = ArrayMesh.init();
    arr_mesh.addSurfaceFromArrays(Mesh.PrimitiveType.primitive_triangles, arrays, .{});

    // Create (or replace) a MeshInstance3D and assign mesh + transform
    // Always create a new MeshInstance3D for the generated surface
    var mi = MeshInstance3D.init();
    mi.setName(name);
    mi.setMesh(@ptrCast(arr_mesh));
    if (self.terrain_material) |mat| mi.setMaterialOverride(mat);
    mi.setPosition(Vector3.initXYZ(
        cast(f64, indexPos.x) * cast(f64, self.part_size),
        0.0,
        cast(f64, indexPos.y) * cast(f64, self.part_size),
    ));

    return mi;
}

pub fn GenerateTerrainHeights(self: *SmoothMeshPart, indexPos: Vector2i, dim: usize, _: f32, height_curve_sampling: bool) PackedInt32Array {
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

pub inline fn cast(comptime T: type, value: anytype) T {
    // Handle optional source
    switch (@typeInfo(@TypeOf(value))) {
        .optional => {
            if (value == null) return @as(T, null);
            const inner = value.?;
            // If destination is optional, convert to child and wrap
            switch (@typeInfo(T)) {
                .optional => |dst_opt| {
                    const child_t = dst_opt.child;
                    const conv = cast(child_t, inner);
                    return @as(T, conv);
                },
                else => return cast(T, inner),
            }
        },
        else => {},
    }

    // Handle optional destination (wrap result)
    switch (@typeInfo(T)) {
        .optional => |dst_opt| {
            const child_t = dst_opt.child;
            const conv = cast(child_t, value);
            return @as(T, conv);
        },
        else => {},
    }

    // Now both source and destination are non-optional — examine kinds
    const SrcTI = @typeInfo(@TypeOf(value));
    const DstTI = @typeInfo(T);

    const src_is_int = switch (SrcTI) {
        .int, .comptime_int => true,
        else => false,
    };
    const src_is_float = switch (SrcTI) {
        .float, .comptime_float => true,
        else => false,
    };
    const src_is_ptr = switch (SrcTI) {
        .pointer => true,
        else => false,
    };

    const dst_is_int = switch (DstTI) {
        .int, .comptime_int => true,
        else => false,
    };
    const dst_is_float = switch (DstTI) {
        .float, .comptime_float => true,
        else => false,
    };
    const dst_is_ptr = switch (DstTI) {
        .pointer => true,
        else => false,
    };

    if (src_is_int and dst_is_float) {
        return @as(T, @floatFromInt(value));
    }
    if (src_is_float and dst_is_int) {
        return @as(T, @intFromFloat(value));
    }
    if (src_is_int and dst_is_int) {
        return @intCast(value);
    }
    if (src_is_float and dst_is_float) {
        return @as(T, value);
    }
    if (src_is_ptr and dst_is_ptr) {
        return @ptrCast(value);
    }

    // Fallback
    return @as(T, value);
}
