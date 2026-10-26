// src/detail/weak_types.h
#pragma once
#include <vector>
#include "bidi_class_table.h"
#include "explicit_levels.h"

namespace bidi::detail {

// Applies W1-W7 to a single sequence (a maximal run of characters
// sharing the same embedding level, bounded by isolate initiators,
// PDIs, and paragraph boundaries).
//
// `types` is modified in place. Removed characters (X9) must not be
// present in the sequence; the caller is responsible for excluding them.
//
// `sos` and `eos` are the start-of-sequence and end-of-sequence
// directions, already computed by the caller.
void resolveWeakTypes(
    std::vector<BidiClass>& types,
    BidiClass sos,
    BidiClass eos
);

} // namespace bidi::detail
