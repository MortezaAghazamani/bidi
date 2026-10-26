// tests/test_bidi_object.cpp
#include <cstdio>
#include <string>
#include "bidi/bidi.h"

static void printCodepoints(const char* label,
                            const std::u32string& s) {
    std::printf("%s", label);
    for (char32_t c : s) {
        std::printf(" U+%04X", static_cast<unsigned int>(c));
    }
    std::printf("\n");
}

int main() {
    bidi::BidiObject obj;

    // Test 1: LTR text with Persian word.
    obj.setText(U"Hello \u062F\u0631\u0648\u062F world");
    auto visual = obj.toVisualStringUtf32();
    printCodepoints("visual (LTR context):", visual);

    auto& r = obj.result();
    std::printf("paragraphLevel = %d\n", r.paragraphLevel);
    std::printf("runs = %zu\n", r.runs.size());
    for (size_t i = 0; i < r.runs.size(); ++i) {
        auto& run = r.runs[i];
        std::printf("  run %zu: start=%zu len=%zu level=%d dir=%s\n",
                    i, run.logicalStart, run.logicalLength, run.level,
                    run.direction == bidi::Direction::LTR ? "LTR" : "RTL");
    }

    // Test 2: Pure Persian text.
    bidi::BidiObject obj2;
    obj2.setText(U"\u0633\u0644\u0627\u0645 \u062F\u0646\u06CC\u0627");
    auto visual2 = obj2.toVisualStringUtf32();
    printCodepoints("visual (RTL):", visual2);
    std::printf("paragraphLevel = %d\n", obj2.result().paragraphLevel);

    // Test 3: LTR with explicit RLE.
    bidi::BidiObject obj3;
    obj3.setText(U"abc \u202B\u0633\u0644\u0627\u0645\u202C def");
    auto visual3 = obj3.toVisualStringUtf32();
    printCodepoints("visual (with RLE):", visual3);

    // Test 4: Persian sentence with English word inside.
    bidi::BidiObject obj4;
    obj4.setText(U"\u0627\u06CC\u0646 \u06CC\u06A9 test \u0627\u0633\u062A");
    auto visual4 = obj4.toVisualStringUtf32();
    printCodepoints("visual (Persian + English):", visual4);
    std::printf("paragraphLevel = %d\n", obj4.result().paragraphLevel);

    // Test 5: Multi-paragraph.
    bidi::BidiObject obj5;
    obj5.setText(U"Hello \u0633\u0644\u0627\u0645\nworld");
    auto visual5 = obj5.toVisualStringUtf32();
    printCodepoints("visual (multi-paragraph):", visual5);

    auto& r5 = obj5.result();
    std::printf("paragraphs = %zu\n", r5.paragraphStart.size() - 1);
    for (size_t i = 0; i + 1 < r5.paragraphStart.size(); ++i) {
        std::printf("  paragraph %zu: start=%u end=%u level=%d\n",
                    i, r5.paragraphStart[i], r5.paragraphStart[i + 1],
                    r5.paragraphLevels[r5.paragraphStart[i]]);
    }

    return 0;
}
