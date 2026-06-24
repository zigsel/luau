// extern "C" shim for Luau type-variance Polarity (Luau/Polarity.h): pure inline
// bit helpers over the Polarity enum, re-exposed as plain int functions. A
// self-contained type-system primitive (no parse / solver state needed).
#pragma once

#include "common.h"

LUAU_BEGIN_DECLS

// The Polarity enum values are part of this shim's ABI (mirroring the upstream
// `enum struct Polarity : uint8_t`).
enum LuauPolarity {
    LUAU_POLARITY_NONE = 0,     // 0b000
    LUAU_POLARITY_POSITIVE = 1, // 0b001
    LUAU_POLARITY_NEGATIVE = 2, // 0b010
    LUAU_POLARITY_MIXED = 3,    // 0b011
    LUAU_POLARITY_UNKNOWN = 4   // 0b100
};

int luau_polarity_is_positive(int p); // 0/1
int luau_polarity_is_negative(int p); // 0/1
int luau_polarity_is_known(int p);    // 0/1 (Unknown -> 0)
int luau_polarity_invert(int p);      // returns a LuauPolarity
int luau_polarity_union(int a, int b);     // a | b
int luau_polarity_intersect(int a, int b); // a & b

LUAU_END_DECLS
