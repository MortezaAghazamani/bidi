// src/detail/explicit_levels.h
#pragma once
#include <cstdint>
#include <vector>
#include "bidi_class_table.h"

namespace bidi::detail {

// Per-character state produced by the X rules (X1-X8).
struct ExplicitLevelsResult {
    // Embedding level of each character. Always the same size as the input.
    std::vector<int> levels;

    // Bidi_Class of each character after applying override status
    // (X6) and after resolving isolate initiators/PDI (X5a-X5c, X6a).
    // Removed characters (X9) keep their original class here; they are
    // marked as removed by the `removed` mask instead.
    std::vector<BidiClass> types;

    // True for characters that X9 removes from further processing
    // (RLE, LRE, RLO, LRO, PDF, BN). These characters still have
    // a level assigned (used for cursor mapping) but do not take part
    // in the W/N/I rules.
    std::vector<uint8_t> removed;

    // Index of the matching PDI for each isolate initiator (LRI/RLI/FSI).
    // Set to kNoMatch if there is no matching PDI.
    std::vector<int32_t> matchingPDI;

    // Index of the matching isolate initiator for each PDI.
    // Set to kNoMatch if there is no matching initiator.
    std::vector<int32_t> matchingIsolate;

    static constexpr int32_t kNoMatch = -1;
};

// Applies X1-X8 from UAX #9 to the given sequence.
//
// `paragraphLevel` must be 0 or 1 and is typically the result of P2/P3.
// `originalTypes` is the Bidi_Class of each character before any processing.
ExplicitLevelsResult computeExplicitLevels(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& originalTypes,
    int paragraphLevel
);

} // namespace bidi::detail
