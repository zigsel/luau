//! Pure helpers over the Luau type-variance `Polarity` enum (Luau/Polarity.h).
//! A self-contained type-system primitive — no parse or solver state needed.

const c = @import("bindings");

pub const Polarity = enum(c_int) {
    none = c.LUAU_POLARITY_NONE,
    positive = c.LUAU_POLARITY_POSITIVE,
    negative = c.LUAU_POLARITY_NEGATIVE,
    mixed = c.LUAU_POLARITY_MIXED,
    unknown = c.LUAU_POLARITY_UNKNOWN,

    pub fn isPositive(self: Polarity) bool {
        return c.luau_polarity_is_positive(@intFromEnum(self)) != 0;
    }
    pub fn isNegative(self: Polarity) bool {
        return c.luau_polarity_is_negative(@intFromEnum(self)) != 0;
    }
    pub fn isKnown(self: Polarity) bool {
        return c.luau_polarity_is_known(@intFromEnum(self)) != 0;
    }
    pub fn invert(self: Polarity) Polarity {
        return @enumFromInt(c.luau_polarity_invert(@intFromEnum(self)));
    }
    pub fn unionWith(self: Polarity, other: Polarity) Polarity {
        return @enumFromInt(c.luau_polarity_union(@intFromEnum(self), @intFromEnum(other)));
    }
    pub fn intersect(self: Polarity, other: Polarity) Polarity {
        return @enumFromInt(c.luau_polarity_intersect(@intFromEnum(self), @intFromEnum(other)));
    }
};
