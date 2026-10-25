// detail/bidi_class_table.h
#pragma once
#include <cstdint>

namespace bidi::detail {

// Mirrors the Bidi_Class property from UnicodeData.txt.
enum class BidiClass : uint8_t {
    L, R, AL, EN, ES, ET, AN, CS, NSM, BN,
    B, S, WS, ON,
    LRE, RLE, PDF, LRO, RLO,
    LRI, RLI, FSI, PDI,
    // Internal sentinel, never returned for a real code point.
    INVALID
};

// Returns the Bidi_Class of the given code point.
// Defaults to L for code points not covered by the table.
BidiClass bidiClassOf(char32_t cp);

// Returns true if the class is one of the "explicit" formatting codes
// that are removed in X9 (RLE, LRE, RLO, LRO, PDF, BN).
inline bool isRemovedByX9(BidiClass c) {
    switch (c) {
        case BidiClass::RLE: case BidiClass::LRE:
        case BidiClass::RLO: case BidiClass::LRO:
        case BidiClass::PDF: case BidiClass::BN:
            return true;
        default:
            return false;
    }
}

// Returns true if the class is an isolate initiator (LRI, RLI, FSI).
inline bool isIsolateInitiator(BidiClass c) {
    return c == BidiClass::LRI || c == BidiClass::RLI || c == BidiClass::FSI;
}

// Returns true if the class is a "strong" type for the purposes of
// direction detection in P2/P3 and N rules.
inline bool isStrong(BidiClass c) {
    return c == BidiClass::L || c == BidiClass::R || c == BidiClass::AL;
}

// Returns true if the class is a neutral or isolate (NI) for N rules.
inline bool isNeutralOrIsolate(BidiClass c) {
    switch (c) {
        case BidiClass::B: case BidiClass::S: case BidiClass::WS:
        case BidiClass::ON: case BidiClass::FSI: case BidiClass::LRI:
        case BidiClass::RLI: case BidiClass::PDI:
            return true;
        default:
            return false;
    }
}

} // namespace bidi::detail
