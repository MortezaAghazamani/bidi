// tests/test_explicit_levels.cpp
#include <cassert>
#include <cstdio>
#include <vector>
#include "detail/explicit_levels.h"
#include "detail/bidi_class_table.h"

using namespace bidi::detail;

struct XTest {
    const char* name;
    std::vector<char32_t> input;
    std::vector<BidiClass> types;
    int paragraphLevel;
    std::vector<int> expectedLevels;
};

static void runTest(const XTest& t) {
    auto r = computeExplicitLevels(t.input, t.types, t.paragraphLevel);

    bool ok = r.levels == t.expectedLevels;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name);
    if (!ok) {
        std::printf("  expected levels:");
        for (int l : t.expectedLevels) std::printf(" %d", l);
        std::printf("\n  got levels:");
        for (int l : r.levels) std::printf(" %d", l);
        std::printf("\n");
    }
}

int main() {
    // Pure LTR text: all level 0.
    runTest({
        "pure LTR",
        {0x0041, 0x0042, 0x0043},
        {BidiClass::L, BidiClass::L, BidiClass::L},
        0,
        {0, 0, 0}
    });

    // Pure RTL text: all level 1.
    runTest({
        "pure RTL",
        {0x05D0, 0x05D1, 0x05D2},
        {BidiClass::R, BidiClass::R, BidiClass::R},
        0,
        {0, 0, 0}  // levels before I; X rules don't change base level
    });

    // RLE: content at level 1.
    runTest({
        "LTR + RLE + LTR",
        {0x0041, 0x202B, 0x0042, 0x0043, 0x202C, 0x0044},
        {BidiClass::L, BidiClass::RLE, BidiClass::L,
         BidiClass::L, BidiClass::PDF, BidiClass::L},
        0,
        {0, 0, 1, 1, 0, 0}
    });

    // LRE in RTL paragraph.
    runTest({
        "RTL + LRE + RTL",
        {0x05D0, 0x202A, 0x05D1, 0x202C, 0x05D2},
        {BidiClass::R, BidiClass::LRE, BidiClass::R,
         BidiClass::PDF, BidiClass::R},
        1,
        {1, 1, 2, 1, 1}
    });

    // RLI content: level 1 inside, initiator and PDI at level 0.
    runTest({
        "LTR + RLI + LTR + PDI",
        {0x0041, 0x2067, 0x0042, 0x2069, 0x0043},
        {BidiClass::L, BidiClass::RLI, BidiClass::L,
         BidiClass::PDI, BidiClass::L},
        0,
        {0, 0, 1, 0, 0}
    });

    // LRI in RTL paragraph.
    runTest({
        "RTL + LRI + RTL + PDI",
        {0x05D0, 0x2066, 0x05D1, 0x2069, 0x05D2},
        {BidiClass::R, BidiClass::LRI, BidiClass::R,
         BidiClass::PDI, BidiClass::R},
        1,
        {1, 1, 2, 1, 1}
    });

    return 0;
}
