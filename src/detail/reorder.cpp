// src/detail/reorder.cpp
#include "reorder.h"
#include <algorithm>

namespace bidi::detail {

namespace {

// Returns true if the original class counts as whitespace for L1.
// Per L1, this includes WS, S, and B. Explicit formatting codes
// (BN, RLE, LRE, etc.) are also treated as whitespace for L1
// because they are "ignored" and take the surrounding whitespace.
inline bool isWhitespaceForL1(BidiClass t) {
    switch (t) {
        case BidiClass::WS:
        case BidiClass::S:
        case BidiClass::B:
        case BidiClass::BN:
        case BidiClass::LRE:
        case BidiClass::RLE:
        case BidiClass::LRO:
        case BidiClass::RLO:
        case BidiClass::PDF:
            return true;
        default:
            return false;
    }
}

} // namespace

void applyL1(
    const std::vector<BidiClass>& originalTypes,
    std::vector<int>& levels,
    int paragraphLevel)
{
    const size_t n = originalTypes.size();
    if (n == 0) return;

    // Step 1: reset S and B to paragraph level.
    for (size_t i = 0; i < n; ++i) {
        BidiClass t = originalTypes[i];
        if (t == BidiClass::S || t == BidiClass::B)
            levels[i] = paragraphLevel;
    }

    // Step 2: reset whitespace immediately before any S or B.
    for (size_t i = 0; i < n; ++i) {
        if (originalTypes[i] != BidiClass::S &&
            originalTypes[i] != BidiClass::B)
            continue;

        size_t j = i;
        while (j > 0) {
            --j;
            if (!isWhitespaceForL1(originalTypes[j]))
                break;
            levels[j] = paragraphLevel;
        }
    }

    // Step 3: reset trailing whitespace at the end of the text.
    size_t j = n;
    while (j > 0) {
        --j;
        if (!isWhitespaceForL1(originalTypes[j]))
            break;
        levels[j] = paragraphLevel;
    }
}

VisualOrder applyL2(
    const std::vector<int>& levels,
    const std::vector<uint8_t>& removed)
{
    const size_t n = levels.size();
    VisualOrder order;
    order.logicalToVisual.assign(n, 0);

    // Collect indices of active characters (not removed by X9).
    std::vector<uint32_t> active;
    active.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        if (!removed[i])
            active.push_back(i);
    }

    const size_t m = active.size();
    order.visualToLogical = active;

    if (m == 0)
        return order;

    // Find the maximum level among active characters.
    int maxLevel = 0;
    for (uint32_t idx : active)
        maxLevel = std::max(maxLevel, levels[idx]);

    // Apply L2: from highest level down to lowest odd level,
    // reverse contiguous runs whose level is >= the current threshold.
    for (int level = maxLevel; level > 0; --level) {
        size_t i = 0;
        while (i < m) {
            if (levels[order.visualToLogical[i]] < level) {
                ++i;
                continue;
            }
            size_t start = i;
            while (i < m && levels[order.visualToLogical[i]] >= level)
                ++i;
            std::reverse(order.visualToLogical.begin() + start,
                         order.visualToLogical.begin() + i);
        }
    }

    // Build the inverse mapping for active characters.
    // Removed characters are left as 0 (their logicalToVisual is undefined).
    for (uint32_t v = 0; v < m; ++v)
        order.logicalToVisual[order.visualToLogical[v]] = v;

    return order;
}

} // namespace bidi::detail
