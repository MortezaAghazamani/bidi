// src/detail/bracket_pairer.h
#pragma once
#include <cstdint>
#include <vector>
#include "bidi_class_table.h"

namespace bidi::detail {

// A matched pair of brackets, identified by their indices in the
// sequence passed to resolveBrackets().
struct BracketPair {
    uint32_t opener;  // index of the opening bracket
    uint32_t closer;  // index of the matching closing bracket
};

// Implements BD16 (Bracket Pairing Algorithm) from UAX #9.
//
// `types` is the current Bidi_Class of each character in the sequence
// (after X9 removal of explicit codes, but before N0). The function
// only inspects entries whose class is ON, per BD16.
//
// `sos` and `eos` are the start/end-of-sequence directions, used
// internally for handling nested isolates (not currently used here,
// but kept in the signature for future extension and for N0).
//
// Returns the list of matched pairs, sorted by opener index.
std::vector<BracketPair> resolveBrackets(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& types,
    BidiClass sos,
    BidiClass eos
);

} // namespace bidi::detail
