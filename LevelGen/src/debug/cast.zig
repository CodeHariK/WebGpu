/// Generic convert/cast helper:
/// - handles optional wrapping/unwrapping
/// - integer <-> float conversions using @floatFromInt / @intFromFloat
/// - integer <-> integer via @intCast
/// - pointer -> pointer via @ptrCast
/// - fallback uses `@as`
/// Example:
///   const f: f64 = cast(f64, 42);   // 42 -> 42.0
///   const i: i32 = cast(i32, 3.9);   // 3.0 (trunc)
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
