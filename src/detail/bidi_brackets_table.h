// detail/bidi_brackets_table.h
#pragma once
#include <cstdint>

namespace bidi::detail {

enum class BracketType : uint8_t { None = 0, Open = 1, Close = 2 };

struct BracketEntry {
    char32_t codepoint;
    char32_t pairedCanonical;  // canonical opener/closer for comparison
    BracketType type;
};

// Looks up the bracket entry for the given code point.
// Returns {0, 0, BracketType::None} if the code point is not a bracket.
BracketEntry lookupBracket(char32_t cp);

} // namespace bidi::detail
