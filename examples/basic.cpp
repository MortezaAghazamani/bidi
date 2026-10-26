// examples/basic.cpp
//
// Basic usage of the bidi library.
//
#include "bidi/bidi.h"

#include <cstdio>
#include <string>

static void printCodepoints(const char* label,
                            const std::u32string& s) {
    std::printf("%s", label);
    for (char32_t c : s) {
        std::printf(" U+%04X", static_cast<unsigned int>(c));
    }
    std::printf("\n");
}

int main() {
    std::printf("=== bidi basic example ===\n\n");

    // --- Example 1: Pure LTR ---
    {
        bidi::BidiObject obj;
        obj.setText(U"Hello, world!");

        auto& r = obj.result();
        std::printf("Example 1: \"Hello, world!\"\n");
        std::printf("  paragraphLevel = %d\n", r.paragraphLevel);
        std::printf("  runs = %zu\n", r.runs.size());
        for (auto& run : r.runs) {
            std::printf("    run: level=%d dir=%s start=%zu len=%zu\n",
                        run.level,
                        run.direction == bidi::Direction::LTR
                            ? "LTR" : "RTL",
                        run.logicalStart, run.logicalLength);
        }
        std::printf("\n");
    }

    // --- Example 2: Persian (RTL) ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u062F\u0631\u0648\u062F");

        auto& r = obj.result();
        std::printf("Example 2: \"درود\" (Persian)\n");
        std::printf("  paragraphLevel = %d\n", r.paragraphLevel);
        std::printf("  runs = %zu\n", r.runs.size());
        std::printf("  visual order (logical indices):");
        for (uint32_t v : r.visualToLogical) {
            std::printf(" %u", v);
        }
        std::printf("\n\n");
    }

    // --- Example 3: Mixed LTR + Persian ---
    {
        bidi::BidiObject obj;
        obj.setText(U"Hello \u062F\u0631\u0648\u062F world");

        auto& r = obj.result();
        std::printf("Example 3: \"Hello درود world\"\n");
        std::printf("  paragraphLevel = %d\n", r.paragraphLevel);
        std::printf("  runs = %zu\n", r.runs.size());
        for (auto& run : r.runs) {
            std::printf("    run: level=%d dir=%s start=%zu len=%zu\n",
                        run.level,
                        run.direction == bidi::Direction::LTR
                            ? "LTR" : "RTL",
                        run.logicalStart, run.logicalLength);
        }

        auto visual = obj.toVisualStringUtf32();
        printCodepoints("  visual codepoints:", visual);
        std::printf("\n");
    }

    // --- Example 4: Persian with numbers ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u0633\u0627\u0644 \u06F1\u06F4\u06F0\u06F3");

        auto& r = obj.result();
        std::printf("Example 4: \"سال ۱۴۰۳\" (Persian + digits)\n");
        std::printf("  paragraphLevel = %d\n", r.paragraphLevel);
        auto visual = obj.toVisualStringUtf32();
        printCodepoints("  visual codepoints:", visual);
        std::printf("\n");
    }

    // --- Example 5: Brackets in RTL ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u062F\u0631\u0648\u062F (\u062F\u0646\u06CC\u0627) "
                    U"\u062E\u062F\u0627\u062D\u0627\u0641\u0638");

        auto& r = obj.result();
        std::printf("Example 5: \"درود (دنیا) خداحافظ\"\n");
        std::printf("  paragraphLevel = %d\n", r.paragraphLevel);

        int mirrorCount = 0;
        for (size_t i = 0; i < r.mirrored.size(); ++i) {
            if (r.mirrored[i]) ++mirrorCount;
        }
        std::printf("  mirrored chars = %d\n", mirrorCount);

        auto visual = obj.toVisualStringUtf32();
        printCodepoints("  visual (with mirroring):", visual);

        auto& rref = r;
        auto visualNoMirror = rref.toVisualStringUtf32(
            obj.textViewUtf32(), false);
        printCodepoints("  visual (no mirroring):  ", visualNoMirror);
        std::printf("\n");
    }

    // --- Example 6: Multi-paragraph ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u062F\u0631\u0648\u062F\nHello\n"
                    U"\u062E\u062F\u0627\u062D\u0627\u0641\u0638");

        std::printf("Example 6: multi-paragraph\n");
        std::printf("  paragraphCount = %zu\n", obj.paragraphCount());

        for (size_t i = 0; i < obj.paragraphCount(); ++i) {
            auto& pr = obj.paragraphResult(i);
            std::printf("  paragraph %zu: level=%d runs=%zu\n",
                        i, pr.paragraphLevel, pr.runs.size());
        }
        std::printf("\n");
    }

    // --- Example 7: UTF-8 input ---
    {
        bidi::BidiObject obj;
        obj.setText("Hello \xD8\xAF\xD8\xB1\xD9\x88\xD8\xAF");

        std::printf("Example 7: UTF-8 input\n");
        std::printf("  paragraphLevel = %d\n", obj.result().paragraphLevel);

        auto utf8Visual = obj.toVisualString();
        std::printf("  visual (UTF-8) size = %zu\n", utf8Visual.size());
        std::printf("\n");
    }

    // --- Example 8: Incremental editing (cache) ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u062F\u0631\u0648\u062F\n\u062F\u0646\u06CC\u0627\n"
                    U"\u062E\u062F\u0627\u062D\u0627\u0641\u0638");

        std::printf("Example 8: incremental editing\n");
        std::printf("  before: paragraphs = %zu\n", obj.paragraphCount());

        obj.insert(4, U" \u0627\u06CC");

        std::printf("  after insert: paragraphs = %zu\n",
                    obj.paragraphCount());
        std::printf("  paragraph 0 text size = %zu\n",
                    obj.paragraphTextViewUtf32(0).size());
        std::printf("  (only paragraph 0 was re-analyzed)\n");
        std::printf("\n");
    }

    std::printf("=== Done ===\n");
    return 0;
}
