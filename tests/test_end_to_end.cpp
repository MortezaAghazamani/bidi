// tests/test_end_to_end.cpp
#include <cstdio>
#include <vector>
#include "detail/bidi_algorithm.h"

using namespace bidi::detail;

static std::vector<char32_t> cps(std::initializer_list<char32_t> list) {
    return std::vector<char32_t>(list);
}

// Helper: build the visual string from a paragraph analysis,
// using Unicode escape sequences for readability.
static void dumpAnalysis(const char* name, ParagraphAnalysis& a) {
    std::printf("=== %s ===\n", name);
    std::printf("  paragraphLevel = %d\n", a.paragraphLevel);
    std::printf("  levels:  ");
    for (int l : a.levels) std::printf(" %d", l);
    std::printf("\n  visual order (logical indices):");
    for (uint32_t v : a.visualOrder.visualToLogical)
        std::printf(" %u", v);
    std::printf("\n");
}

int main() {
    // Test 1: pure LTR.
    {
        auto r = analyzeParagraph(cps({0x0041, 0x0042, 0x0043}), 0);
        dumpAnalysis("ABC (LTR)", r);
        // Expect: levels {0,0,0}, order {0,1,2}
    }

    // Test 2: pure RTL paragraph, all R chars.
    {
        auto r = analyzeParagraph(cps({0x05D0, 0x05D1, 0x05D2}), 1);
        dumpAnalysis("ABC (RTL base)", r);
        // Expect: levels {1,1,1}, order {2,1,0}
    }

    // Test 3: LTR text with one RTL word.
    // Logical: A B [RTL: G D] C
    // Expect: visual A B D G C (RTL word reversed)
    {
        auto r = analyzeParagraph(
            cps({0x0041, 0x0042, 0x05D0, 0x05D1, 0x0043}), 0);
        dumpAnalysis("AB<GD>C", r);
    }

    // Test 4: numbers in RTL text.
    // Logical: RTL RTL 1 2 3 RTL
    // Numbers should stay LTR but appear reversed position-wise.
    {
        auto r = analyzeParagraph(
            cps({0x05D0, 0x05D1, 0x0031, 0x0032, 0x0033, 0x05D2}), 1);
        dumpAnalysis("AB<123>D (RTL)", r);
    }

    // Test 5: parentheses in RTL context.
    // Logical: RTL "(" LTR "A" ")" RTL
    {
        auto r = analyzeParagraph(
            cps({0x05D0, 0x0028, 0x0041, 0x0029, 0x05D1}), 1);
        dumpAnalysis("RTL(LA)RTL", r);
    }

    return 0;
}
