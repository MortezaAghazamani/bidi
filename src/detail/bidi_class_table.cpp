// src/detail/bidi_class_table.cpp
#include "bidi_class_table.h"
#include <algorithm>
#include <iterator>

namespace bidi::detail {

namespace {

struct Range {
    char32_t start;
    char32_t end;
    BidiClass cls;
};

// Include the generated range table.
// The file is produced by tools/gen_bidi_class.py from UnicodeData.txt.
// The data is committed so the build does not depend on Python.
static constexpr Range kRanges[] = {
#include "generated/bidi_class_ranges.inc"
};

} // namespace

BidiClass bidiClassOf(char32_t cp) {
    auto it = std::upper_bound(
        std::begin(kRanges), std::end(kRanges), cp,
        [](char32_t v, const Range& r) { return v < r.start; });

    if (it != std::begin(kRanges)) {
        const Range& r = *(it - 1);
        if (cp >= r.start && cp <= r.end)
            return r.cls;
    }

    return BidiClass::L;
}

} // namespace bidi::detail
