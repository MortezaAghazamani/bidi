// examples/editor.cpp
//
// Simulates a text editor using the bidi library with per-paragraph
// caching.
//
#include "bidi/bidi.h"

#include <cstdio>
#include <string>

static void renderParagraph(const std::u32string& text,
                            const bidi::AnalysisResult& result)
{
    for (auto& run : result.runs) {
        std::printf("[");
        for (size_t i = 0; i < run.logicalLength; ++i) {
            char32_t c = text[run.logicalStart + i];
            if (c < 0x80) {
                std::printf("%c", (char)c);
            } else {
                std::printf("<U+%04X>", (unsigned)c);
            }
        }
        std::printf("]%s ", run.direction == bidi::Direction::LTR
                            ? "LTR" : "RTL");
    }
    std::printf("\n");
}

int main() {
    std::printf("=== bidi editor example ===\n\n");

    bidi::BidiObject obj;

    obj.setText(U"\u062F\u0631\u0648\u062F\n"
                U"\u062F\u0646\u06CC\u0627\n"
                U"\u062E\u062F\u0627\u062D\u0627\u0641\u0638");

    auto printState = [&](const char* label) {
        std::printf("%s\n", label);
        std::printf("  paragraphs = %zu\n", obj.paragraphCount());
        for (size_t i = 0; i < obj.paragraphCount(); ++i) {
            std::printf("  [%zu] ", i);
            std::u32string paraText(obj.paragraphTextViewUtf32(i));
            renderParagraph(paraText, obj.paragraphResult(i));
        }
        std::printf("\n");
    };

    printState("Initial state:");

    obj.insert(4, U" \u0627\u06CC");
    printState("After insert(4, \" ای\") in paragraph 0:");

    obj.insert(7, U"\n");
    printState("After insert(7, \"\\n\") — splits paragraph 0:");

    obj.erase(7, 1);
    printState("After erase(7, 1) — merges paragraphs back:");

    // Replace paragraph 1 with "جهان".
    size_t para1Start = 0;
    for (size_t i = 0; i < obj.paragraphCount(); ++i) {
        if (i == 1) break;
        para1Start += obj.paragraphTextViewUtf32(i).size();
    }
    obj.replace(para1Start, obj.paragraphTextViewUtf32(1).size() - 1,
                U"\u062C\u0647\u0627\u0646\n");
    printState("After replacing paragraph 1 with \"جهان\":");

    if (obj.paragraphCount() > 0 &&
        obj.paragraphTextViewUtf32(0).size() > 5)
    {
        obj.erase(5, 1);
        printState("After erase(5, 1) in paragraph 0:");
    }

    std::printf("=== Done ===\n");
    return 0;
}
