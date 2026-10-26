// src/detail/arabic_persian_shaping_table.h
#pragma once
#include <cstdint>

namespace bidi::detail {

// Joining types from ArabicShaping.txt.
enum class JoiningType : uint8_t {
    U = 0,  // Non-joining
    R = 1,  // Right-joining
    L = 2,  // Left-joining
    D = 3,  // Dual-joining
    C = 4,  // Join-causing
    T = 5   // Transparent
};

struct ArabicPersianShapingEntry {
    char32_t codepoint;
    JoiningType type;
};

// Looks up the joining type of `cp`. Returns JoiningType::U if not
// found.
JoiningType joiningTypeOf(char32_t cp);

// Returns the Arabic/Persian Presentation Form for `cp` given its
// joining context. Returns `cp` unchanged if no form applies.
//
// `prevType` and `nextType` are the joining types of the previous
// and next non-transparent characters (or U at boundaries).
char32_t arabicPersianPresentationForm(char32_t cp,
                                       JoiningType prevType,
                                       JoiningType nextType);

} // namespace bidi::detail
