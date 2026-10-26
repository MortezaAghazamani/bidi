// src/detail/bidi_mirroring_table.cpp
#include "bidi_mirroring_table.h"
#include <algorithm>
#include <iterator>

namespace bidi::detail {

namespace {

struct MirrorEntry {
    char32_t cp;
    char32_t mirror;
};

// Generated from BidiMirroring.txt by tools/gen_bidi_mirroring.py.
// Sorted by cp for binary search.
static constexpr MirrorEntry kMirrors[] = {
#include "generated/bidi_mirroring_ranges.inc"
};

} // namespace

char32_t getMirror(char32_t cp) {
    auto it = std::lower_bound(
        std::begin(kMirrors), std::end(kMirrors), cp,
        [](const MirrorEntry& e, char32_t v) { return e.cp < v; });
    if (it != std::end(kMirrors) && it->cp == cp)
        return it->mirror;
    return cp;
}

bool hasMirror(char32_t cp) {
    auto it = std::lower_bound(
        std::begin(kMirrors), std::end(kMirrors), cp,
        [](const MirrorEntry& e, char32_t v) { return e.cp < v; });
    return it != std::end(kMirrors) && it->cp == cp;
}

} // namespace bidi::detail
