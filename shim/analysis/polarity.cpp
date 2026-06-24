// extern "C" shim for Luau Polarity. See polarity.h for the public contract.
#include "polarity.h"

#include "Luau/Polarity.h"

using namespace Luau;

extern "C" int luau_polarity_is_positive(int p) {
    return isPositive(static_cast<Polarity>(p)) ? 1 : 0;
}
extern "C" int luau_polarity_is_negative(int p) {
    return isNegative(static_cast<Polarity>(p)) ? 1 : 0;
}
extern "C" int luau_polarity_is_known(int p) {
    return isKnown(static_cast<Polarity>(p)) ? 1 : 0;
}
extern "C" int luau_polarity_invert(int p) {
    return static_cast<int>(invert(static_cast<Polarity>(p)));
}
extern "C" int luau_polarity_union(int a, int b) {
    return static_cast<int>(static_cast<Polarity>(a) | static_cast<Polarity>(b));
}
extern "C" int luau_polarity_intersect(int a, int b) {
    return static_cast<int>(static_cast<Polarity>(a) & static_cast<Polarity>(b));
}
