// src/detail/implicit_levels.h
#pragma once
#include <vector>
#include "bidi_class_table.h"

namespace bidi::detail {

// Applies I1 and I2 to assign the final embedding level of each
// character in a sequence.
//
// `types` is the resolved type after W and N rules.
// `levels` is modified in place: entries are updated to their final level.
void resolveImplicitLevels(
    const std::vector<BidiClass>& types,
    std::vector<int>& levels
);

} // namespace bidi::detail
