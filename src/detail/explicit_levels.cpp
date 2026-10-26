// src/detail/explicit_levels.cpp
#include "explicit_levels.h"
#include <algorithm>

namespace bidi::detail {

namespace {

// BD2: maximum depth of the explicit embedding/isolate stack.
constexpr size_t kMaxDepth = 125;

// One entry on the directional status stack (X1).
struct StatusEntry {
    int level;
    BidiClass overrideStatus;  // ON means "no override"
    bool isolate;
};

// P2/P3-style computation of the implicit base level of a sequence,
// skipping over isolate contents (X5a-X5c). Returns 0 or 1.
int computeImplicitLevel(
    const std::vector<BidiClass>& types,
    size_t start,
    size_t end)
{
    for (size_t i = start; i < end; ++i) {
        switch (types[i]) {
            case BidiClass::L:
                return 0;
            case BidiClass::R:
            case BidiClass::AL:
                return 1;
            default:
                break;
        }
    }
    return 0;
}

// Computes the next embedding level for an RLE/RLO/RLI (RTL direction).
// Per X2/X3/X5a/X5b: least greater odd level.
int nextOddLevel(int current) {
    return (current + 1) | 1;
}

// Computes the next embedding level for an LRE/LRO/LRI (LTR direction).
// Per X2/X3/X5a/X5b: least greater even level.
int nextEvenLevel(int current) {
    return (current + 2) & ~1;
}

} // namespace

ExplicitLevelsResult computeExplicitLevels(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& originalTypes,
    int paragraphLevel)
{
    const size_t n = codepoints.size();
    ExplicitLevelsResult result;
    result.levels.assign(n, paragraphLevel);
    result.types = originalTypes;
    result.removed.assign(n, 0);
    result.matchingPDI.assign(n, ExplicitLevelsResult::kNoMatch);
    result.matchingIsolate.assign(n, ExplicitLevelsResult::kNoMatch);

    // X1: initialize the directional status stack.
    std::vector<StatusEntry> stack;
    stack.reserve(kMaxDepth);
    stack.push_back({ paragraphLevel, BidiClass::ON, false });

    // First pass: match isolate initiators with PDIs (X5a-X5c, X6a).
    // We do this before assigning levels so that FSI can determine its
    // own direction from the content between it and its matching PDI.
    {
        std::vector<uint32_t> isolateStack;
        for (size_t i = 0; i < n; ++i) {
            BidiClass t = originalTypes[i];
            if (isIsolateInitiator(t)) {
                isolateStack.push_back(static_cast<uint32_t>(i));
            }
            else if (t == BidiClass::PDI) {
                if (!isolateStack.empty()) {
                    uint32_t opener = isolateStack.back();
                    isolateStack.pop_back();
                    result.matchingPDI[opener] = static_cast<int32_t>(i);
                    result.matchingIsolate[i] = static_cast<int32_t>(opener);
                }
            }
        }
        // Unmatched initiators keep kNoMatch in matchingPDI,
        // unmatched PDIs keep kNoMatch in matchingIsolate.
    }

    // Second pass: assign levels and apply overrides (X2-X8).
    for (size_t i = 0; i < n; ++i) {
        BidiClass t = originalTypes[i];
        const StatusEntry& top = stack.back();

        switch (t) {
            // X2: RLE
            case BidiClass::RLE: {
                result.levels[i] = top.level;
                if (stack.size() < kMaxDepth) {
                    int newLevel = nextOddLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::ON, false });
                    }
                }
                result.removed[i] = 1;
                break;
            }
            // X3: LRE
            case BidiClass::LRE: {
                result.levels[i] = top.level;
                if (stack.size() < kMaxDepth) {
                    int newLevel = nextEvenLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::ON, false });
                    }
                }
                result.removed[i] = 1;
                break;
            }
            // X4: RLO
            case BidiClass::RLO: {
                result.levels[i] = top.level;
                if (stack.size() < kMaxDepth) {
                    int newLevel = nextOddLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::R, false });
                    }
                }
                result.removed[i] = 1;
                break;
            }
            // X5: LRO
            case BidiClass::LRO: {
                result.levels[i] = top.level;
                if (stack.size() < kMaxDepth) {
                    int newLevel = nextEvenLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::L, false });
                    }
                }
                result.removed[i] = 1;
                break;
            }
            // X5a: RLI
            case BidiClass::RLI: {
                result.levels[i] = top.level;
                if (top.overrideStatus != BidiClass::ON)
                    result.types[i] = top.overrideStatus;
                if (stack.size() < kMaxDepth) {
                    int newLevel = nextOddLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::ON, true });
                    }
                }
                break;
            }
            // X5b: LRI
            case BidiClass::LRI: {
                result.levels[i] = top.level;
                if (top.overrideStatus != BidiClass::ON)
                    result.types[i] = top.overrideStatus;
                if (stack.size() < kMaxDepth) {
                    int newLevel = nextEvenLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::ON, true });
                    }
                }
                break;
            }
            // X5c: FSI
            case BidiClass::FSI: {
                result.levels[i] = top.level;
                if (top.overrideStatus != BidiClass::ON)
                    result.types[i] = top.overrideStatus;

                // Determine the direction of the FSI from its content.
                int32_t pdiIdx = result.matchingPDI[i];
                size_t contentEnd = (pdiIdx >= 0)
                    ? static_cast<size_t>(pdiIdx)
                    : n;
                int innerLevel = computeImplicitLevel(
                    originalTypes, i + 1, contentEnd);

                if (stack.size() < kMaxDepth) {
                    int newLevel = (innerLevel == 1)
                        ? nextOddLevel(top.level)
                        : nextEvenLevel(top.level);
                    if (newLevel <= kMaxDepth) {
                        stack.push_back({ newLevel, BidiClass::ON, true });
                    }
                }
                break;
            }
            // X6a: PDI
            case BidiClass::PDI: {
                // The level of a PDI matches that of its initiator.
                int32_t initiator = result.matchingIsolate[i];
                if (initiator >= 0) {
                    // Pop entries until we remove the matching isolate.
                    while (stack.size() > 1) {
                        StatusEntry removed = stack.back();
                        stack.pop_back();
                        if (removed.isolate)
                            break;
                    }
                }
                const StatusEntry& newTop = stack.back();
                result.levels[i] = newTop.level;
                if (newTop.overrideStatus != BidiClass::ON)
                    result.types[i] = newTop.overrideStatus;
                break;
            }
            // X7: PDF
            case BidiClass::PDF: {
                result.levels[i] = top.level;
                // Pop up to the nearest non-isolate entry.
                if (!top.isolate && stack.size() > 1) {
                    // Only pop if the entry being removed is non-isolate.
                    stack.pop_back();
                }
                result.removed[i] = 1;
                break;
            }
            // X8: B
            case BidiClass::B: {
                // The paragraph separator gets the paragraph level.
                result.levels[i] = paragraphLevel;
                break;
            }
            // X6: everything else
            default: {
                result.levels[i] = top.level;
                if (top.overrideStatus != BidiClass::ON)
                    result.types[i] = top.overrideStatus;
                // BN is removed by X9.
                if (t == BidiClass::BN)
                    result.removed[i] = 1;
                break;
            }
        }
    }

    return result;
}

} // namespace bidi::detail
