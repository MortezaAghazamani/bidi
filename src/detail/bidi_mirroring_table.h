// src/detail/bidi_mirroring_table.h
#pragma once
#include <cstdint>

namespace bidi::detail {

// Returns the mirroring codepoint for `cp`, or `cp` if not mirrored.
char32_t getMirror(char32_t cp);

// Returns true if `cp` has a mirroring pair.
bool hasMirror(char32_t cp);

} // namespace bidi::detail
