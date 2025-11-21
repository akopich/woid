#include "cross_tu_lib.hpp"

template <typename Storage, typename IntType>
__attribute__((visibility("default"))) Storage mkAny(int i) {
    return Storage{std::in_place_type<IntType>, i};
}

template __attribute__((visibility("default"))) AnyMoveOnlyCombined
mkAny<AnyMoveOnlyCombined, int32_t>(int i);
template __attribute__((visibility("default"))) AnyMoveOnlyCombined
mkAny<AnyMoveOnlyCombined, __int128>(int i);

template __attribute__((visibility("default"))) AnyMoveOnlyDedicated
mkAny<AnyMoveOnlyDedicated, int32_t>(int i);
template __attribute__((visibility("default"))) AnyMoveOnlyDedicated
mkAny<AnyMoveOnlyDedicated, __int128>(int i);

template __attribute__((visibility("default"))) AnyCopyableCombined
mkAny<AnyCopyableCombined, int32_t>(int i);
template __attribute__((visibility("default"))) AnyCopyableCombined
mkAny<AnyCopyableCombined, __int128>(int i);

template __attribute__((visibility("default"))) AnyCopyableDedicated
mkAny<AnyCopyableDedicated, int32_t>(int i);
template __attribute__((visibility("default"))) AnyCopyableDedicated
mkAny<AnyCopyableDedicated, __int128>(int i);
