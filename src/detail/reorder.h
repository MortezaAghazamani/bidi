// src/detail/reorder.h
#pragma once
#include <cstdint>
#include <vector>
#include "bidi_class_table.h"

namespace bidi::detail {

// Applies L1: resets the level of S, B, and trailing whitespace
// to the paragraph level.
//
// `originalTypes` is the class of each character BEFORE any processing,
// because L1 refers to the original class (e.g. NSM that became WS in N1
// is NOT reset by L1).
void applyL1(
    const std::vector<BidiClass>& originalTypes,
    std::vector<int>& levels,
    int paragraphLevel
);

// Output of L2: a permutation of logical indices in visual order.
// Only non-removed (active) characters participate.
struct VisualOrder {
    // Visual position -> logical index. Size = number of active characters.
    std::vector<uint32_t> visualToLogical;

    // Logical index -> visual position.
    // For removed characters, the entry is meaningless (0 by default).
    // Size = total number of characters (including removed).
    std::vector<uint32_t> logicalToVisual;
};

// Applies L2: reorders the active characters by reversing contiguous runs
// at each descending level. Removed characters (X9) do not participate.
VisualOrder applyL2(
    const std::vector<int>& levels,
    const std::vector<uint8_t>& removed
);

} // namespace bidi::detail
