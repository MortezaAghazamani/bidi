// src/detail/bidi_brackets_table.cpp
#include "bidi_brackets_table.h"
#include <algorithm>
#include <iterator>

namespace bidi::detail {

namespace {

struct RawEntry {
    char32_t cp;
    char32_t paired;
    BracketType type;
};

inline char32_t canonicalizeOpener(char32_t cp) {
    switch (cp) {
        case 0x2329: return 0x3008;
        case 0x3008: return 0x3008;
        case 0x232A: return 0x3009;
        case 0x3009: return 0x3009;
        default:     return cp;
    }
}

inline char32_t canonicalOpenerOf(char32_t cp, char32_t paired, BracketType type) {
    if (type == BracketType::Open)
        return canonicalizeOpener(cp);
    return canonicalizeOpener(paired);
}

// Generated from BidiBrackets.txt.
static constexpr RawEntry kRaw[] = {
#include "generated/bidi_brackets_ranges.inc"
};

} // namespace

BracketEntry lookupBracket(char32_t cp) {
    auto it = std::lower_bound(
        std::begin(kRaw), std::end(kRaw), cp,
        [](const RawEntry& e, char32_t v) { return e.cp < v; });

    if (it == std::end(kRaw) || it->cp != cp)
        return { 0, 0, BracketType::None };

    return {
        it->cp,
        canonicalOpenerOf(it->cp, it->paired, it->type),
        it->type
    };
}

} // namespace bidi::detail
