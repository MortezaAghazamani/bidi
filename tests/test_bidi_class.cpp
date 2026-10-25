// tests/test_bidi_class.cpp
#include <cstdio>
#include "detail/bidi_class_table.h"

using namespace bidi::detail;

static const char* name(BidiClass c) {
    switch (c) {
        case BidiClass::L: return "L";
        case BidiClass::R: return "R";
        case BidiClass::AL: return "AL";
        case BidiClass::EN: return "EN";
        case BidiClass::ES: return "ES";
        case BidiClass::ET: return "ET";
        case BidiClass::AN: return "AN";
        case BidiClass::CS: return "CS";
        case BidiClass::NSM: return "NSM";
        case BidiClass::BN: return "BN";
        case BidiClass::B: return "B";
        case BidiClass::S: return "S";
        case BidiClass::WS: return "WS";
        case BidiClass::ON: return "ON";
        case BidiClass::LRE: return "LRE";
        case BidiClass::RLE: return "RLE";
        case BidiClass::PDF: return "PDF";
        case BidiClass::LRO: return "LRO";
        case BidiClass::RLO: return "RLO";
        case BidiClass::LRI: return "LRI";
        case BidiClass::RLI: return "RLI";
        case BidiClass::FSI: return "FSI";
        case BidiClass::PDI: return "PDI";
        default: return "?";
    }
}

int main() {
    struct Test {
        char32_t cp;
        BidiClass expected;
        const char* name;
    };

    const Test tests[] = {
        { 0x0041, BidiClass::L,   "A -> L" },
        { 0x05D0, BidiClass::R,   "Hebrew alef -> R" },
        { 0x0627, BidiClass::AL,  "Arabic alef -> AL" },
        { 0x0030, BidiClass::EN,  "0 -> EN" },
        { 0x0660, BidiClass::AN,  "Arabic-Indic 0 -> AN" },
        { 0x0028, BidiClass::ON,  "( -> ON" },
        { 0x0029, BidiClass::ON,  ") -> ON" },
        { 0x0020, BidiClass::WS,  "space -> WS" },
        { 0x000A, BidiClass::B,   "LF -> B" },
        { 0x0331, BidiClass::NSM, "combining macron -> NSM" },
        { 0x200D, BidiClass::BN,  "ZWJ -> BN" },
        { 0x2066, BidiClass::LRI, "LRI -> LRI" },
        { 0x2069, BidiClass::PDI, "PDI -> PDI" },
        { 0x202A, BidiClass::LRE, "LRE -> LRE" },
        { 0x202C, BidiClass::PDF, "PDF -> PDF" },
    };

    int passed = 0;
    int failed = 0;

    for (const auto& t : tests) {
        BidiClass actual = bidiClassOf(t.cp);
        if (actual == t.expected) {
            std::printf("[PASS] %s\n", t.name);
            ++passed;
        } else {
            std::printf("[FAIL] %s (got %s)\n", t.name, name(actual));
            ++failed;
        }
    }

    std::printf("\n=== Results: %d passed, %d failed ===\n",
                passed, failed);
    return (failed == 0) ? 0 : 1;
}
