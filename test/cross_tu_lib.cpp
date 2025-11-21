#include "cross_tu_lib.hpp"

Storage mkAny() { return Storage{42}; }

Storage mkBigAny() { return Storage{std::in_place_type<__int128>, 42}; }
