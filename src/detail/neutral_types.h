// src/detail/neutral_types.h
#pragma once
#include <vector>
#include "bidi_class_table.h"
#include "bracket_pairer.h"

namespace bidi::detail {

// Applies N0 using a precomputed list of bracket pairs.
// Must be called after W rules and before N1.
//
// `originalTypes` is the Bidi_Class of each character before any
// processing. It is used for context scanning in N0 (prior/following
// strong type lookup) because W7 may convert EN to L, which would
// otherwise affect the direction decision incorrectly.
//
// `types` and `levels` are modified in place.
void applyN0(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& originalTypes,
    std::vector<BidiClass>& types,
    std::vector<int>& levels,
    const std::vector<BracketPair>& pairs,
    BidiClass sos,
    BidiClass eos
);

// Applies N1 and N2 to a single sequence.
void resolveNeutralTypes(
    const std::vector<BidiClass>& originalTypes,
    std::vector<BidiClass>& types,
    const std::vector<int>& levels,
    BidiClass sos,
    BidiClass eos
);

} // namespace bidi::detail
