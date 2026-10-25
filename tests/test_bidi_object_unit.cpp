// tests/test_bidi_object_unit.cpp
#include <cstdio>
#include <string>
#include <string_view>
#include "bidi/bidi.h"

static int passed = 0;
static int failed = 0;

static void check(bool condition, const char* name) {
    if (condition) {
        std::printf("[PASS] %s\n", name);
        ++passed;
    } else {
        std::printf("[FAIL] %s\n", name);
        ++failed;
    }
}

// Helper: convert a wide string literal to UTF-32 (for test data).
static std::u32string U32(const wchar_t* s) {
    std::u32string out;
    for (const wchar_t* p = s; *p; ++p)
        out.push_back(static_cast<char32_t>(*p));
    return out;
}

int main() {
    // --- Test 1: Empty text ---
    {
        bidi::BidiObject obj;
        obj.setText(U"");
        auto& r = obj.result();
        check(r.levels.empty(), "empty text: levels empty");
        check(r.runs.empty(), "empty text: runs empty");
        check(r.paragraphStart.size() == 1 &&
              r.paragraphStart[0] == 0,
              "empty text: paragraphStart");
    }

    // --- Test 2: Pure LTR ---
    {
        bidi::BidiObject obj;
        obj.setText(U"hello");
        auto& r = obj.result();
        check(r.paragraphLevel == 0, "pure LTR: paragraphLevel 0");
        check(r.levels.size() == 5, "pure LTR: levels size 5");
        check(r.levels[0] == 0 && r.levels[4] == 0,
              "pure LTR: all levels 0");
        check(r.runs.size() == 1, "pure LTR: one run");
        check(r.runs[0].level == 0, "pure LTR: run level 0");
        check(r.runs[0].direction == bidi::Direction::LTR,
              "pure LTR: run direction LTR");
        check(r.runs[0].logicalStart == 0, "pure LTR: run start 0");
        check(r.runs[0].logicalLength == 5, "pure LTR: run length 5");
    }

    // --- Test 3: Pure Persian (RTL) ---
    {
        bidi::BidiObject obj;
        // "درود" = U+062F U+0631 U+0648 U+062F
        obj.setText(U"\u062F\u0631\u0648\u062F");
        auto& r = obj.result();
        check(r.paragraphLevel == 1, "pure RTL: paragraphLevel 1");
        check(r.levels.size() == 4, "pure RTL: levels size 4");
        check(r.levels[0] == 1 && r.levels[3] == 1,
              "pure RTL: all levels 1");
        check(r.runs.size() == 1, "pure RTL: one run");
        check(r.runs[0].direction == bidi::Direction::RTL,
              "pure RTL: run direction RTL");
    }

    // --- Test 4: LTR with Persian word ---
    {
        bidi::BidiObject obj;
        obj.setText(U"hi \u062F\u0631\u0648\u062F");
        auto& r = obj.result();
        check(r.paragraphLevel == 0, "mixed: paragraphLevel 0");
        check(r.runs.size() == 2, "mixed: two runs");
        check(r.runs[0].direction == bidi::Direction::LTR,
              "mixed: first run LTR");
        check(r.runs[1].direction == bidi::Direction::RTL,
              "mixed: second run RTL");
    }

    // --- Test 5: toVisualString keeps all visible chars ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc \u062F\u0631\u0648\u062F def");
        auto visual = obj.toVisualString();
        check(visual.size() == obj.textView().size(),
              "toVisualString: same length as text");
    }

    // --- Test 6: Explicit RLE is invisible ---
    {
        bidi::BidiObject obj;
        // "a [RLE] ب [PDF] c"
        obj.setText(U"a\u202B\u0628\u202Cc");
        auto& r = obj.result();
        check(r.levels.size() == 5, "RLE: levels size 5");
        check(r.invisibleIndices.size() == 2,
            "RLE: two invisible chars");
        check(r.invisibleIndices[0] == 1, "RLE: RLE invisible");
        check(r.invisibleIndices[1] == 3, "RLE: PDF invisible");

        // Use the UTF-32 visual string to check character count.
        auto visual32 = obj.toVisualStringUtf32();
        check(visual32.size() == 3, "RLE: visual has 3 chars");
        check(visual32[0] == U'a', "RLE: visual[0] = a");
        check(visual32[1] == U'\u0628', "RLE: visual[1] = ب");
        check(visual32[2] == U'c', "RLE: visual[2] = c");

        // The UTF-8 visual string has 4 bytes (a=1, ب=2, c=1).
        auto visual8 = obj.toVisualString();
        check(visual8.size() == 4, "RLE: UTF-8 visual has 4 bytes");
    }

    // --- Test 7: Cache invalidation ---
    {
        bidi::BidiObject obj;
        obj.setText(U"hello");
        auto& r1 = obj.result();
        int level1 = r1.paragraphLevel;

        obj.setText(U"\u062F\u0631\u0648\u062F");
        auto& r2 = obj.result();
        int level2 = r2.paragraphLevel;

        check(level1 == 0, "cache: first result LTR");
        check(level2 == 1, "cache: after setText, RTL");
    }

    // --- Test 8: Multi-paragraph ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");
        auto& r = obj.result();
        check(r.paragraphStart.size() == 3,
              "multi-para: 3 paragraphStart entries");
        check(r.paragraphStart[0] == 0, "multi-para: start 0");
        check(r.paragraphStart[1] == 4, "multi-para: start 4");
        check(r.paragraphStart[2] == 8, "multi-para: end 8");
        check(r.paragraphLevels[0] == 0,
              "multi-para: para 0 LTR");
        check(r.paragraphLevels[4] == 1,
              "multi-para: para 1 RTL");
    }

    // --- Test 9: setBaseLevel ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc");
        obj.setBaseLevel(1);
        auto& r = obj.result();
        check(r.paragraphLevel == 1,
              "setBaseLevel(1): paragraphLevel 1");
    }

    // --- Test 10: visualToLogical consistency ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc \u062F\u0631\u0648\u062F");
        auto& r = obj.result();
        check(r.visualToLogical.size() ==
              r.logicalToVisual.size(),
              "mapping: sizes match");
        for (size_t v = 0; v < r.visualToLogical.size(); ++v) {
            uint32_t logical = r.visualToLogical[v];
            check(r.logicalToVisual[logical] == v,
                  "mapping: inverse consistent");
        }
    }

    // --- Test 11: Persian-Indic digits (AN class) ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u0633\u0627\u0644 \u06F1\u06F4\u06F0\u06F3");
        auto& r = obj.result();
        check(r.paragraphLevel == 1,
              "Persian digits: RTL paragraph");
    }

    // --- Test 12: Insert preserves cache ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u062F\u0631\u0648\u062F\n\u062F\u0646\u06CC\u0627");
        auto& r1 = obj.result();
        (void)r1;

        check(obj.paragraphCount() == 2, "insert: initially 2 paragraphs");

        obj.insert(4, U" \u0627\u06CC");

        check(obj.paragraphCount() == 2, "insert: still 2 paragraphs");

        auto para0 = obj.paragraphTextViewUtf32(0);
        auto para1 = obj.paragraphTextViewUtf32(1);
        check(para0.size() == 8, "insert: para0 grew");
        check(para1 == U"\u062F\u0646\u06CC\u0627",
              "insert: para1 unchanged");
    }

    // --- Test 13: Insert newline splits paragraph ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc");
        check(obj.paragraphCount() == 1, "newline: 1 paragraph");

        obj.insert(1, U"\n");
        check(obj.paragraphCount() == 2, "newline: 2 paragraphs");
        check(obj.paragraphTextViewUtf32(0) == U"a\n",
              "newline: para0 = a\\n");
        check(obj.paragraphTextViewUtf32(1) == U"bc",
              "newline: para1 = bc");
    }

    // --- Test 14: Erase removes newline ---
    {
        bidi::BidiObject obj;
        obj.setText(U"a\nbc");
        check(obj.paragraphCount() == 2, "erase: 2 paragraphs");

        obj.erase(1, 1);
        check(obj.paragraphCount() == 1, "erase: 1 paragraph");
        check(obj.paragraphTextViewUtf32(0) == U"abc",
              "erase: para0 = abc");
    }

    // --- Test 15: paragraphContaining ---
    {
        bidi::BidiObject obj;
        obj.setText(U"ab\ncd\nef");
        check(obj.paragraphContaining(0) == 0, "containing: 0 in para 0");
        check(obj.paragraphContaining(2) == 0, "containing: 2 in para 0");
        check(obj.paragraphContaining(3) == 1, "containing: 3 in para 1");
        check(obj.paragraphContaining(5) == 1, "containing: 5 in para 1");
        check(obj.paragraphContaining(6) == 2, "containing: 6 in para 2");
    }

    // --- Test 16: Per-paragraph result ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");
        auto& p0 = obj.paragraphResult(0);
        auto& p1 = obj.paragraphResult(1);

        check(p0.paragraphLevel == 0, "para result: p0 LTR");
        check(p1.paragraphLevel == 1, "para result: p1 RTL");
        check(p0.levels.size() == 4, "para result: p0 levels size");
        check(p1.levels.size() == 4, "para result: p1 levels size");
    }

    // --- Test 17: UTF-8 support ---
    {
        bidi::BidiObject obj;
        // "Hello درود" in UTF-8
        obj.setText("Hello \xD8\xAF\xD8\xB1\xD9\x88\xD8\xAF");
        auto& r = obj.result();
        check(r.paragraphLevel == 0, "UTF-8: paragraphLevel 0");
        check(r.runs.size() == 2, "UTF-8: two runs");
        auto utf8 = obj.toVisualString();
        check(!utf8.empty(), "UTF-8: visual string non-empty");
    }

    // --- Test 18: BidiClass access ---
    {
        bidi::BidiObject obj;
        obj.setText(U"a\u0628");
        auto& r = obj.result();
        check(r.classes.size() == 2, "classes: size 2");
    }

    // --- Test 19: Direction access ---
    {
        bidi::BidiObject obj;
        obj.setText(U"a\u0628");
        auto& r = obj.result();
        check(r.directions.size() == 2, "directions: size 2");
        check(r.directions[0] == bidi::Direction::LTR,
              "directions: a is LTR");
        check(r.directions[1] == bidi::Direction::RTL,
              "directions: ب is RTL");
    }

    // --- Test 20: Mirroring ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u0627\u0628\u062C (\u0633\u0644\u0627\u0645)");
        auto& r = obj.result();
        bool foundMirror = false;
        for (size_t i = 0; i < r.mirrored.size(); ++i) {
            if (r.mirrored[i]) foundMirror = true;
        }
        check(foundMirror, "mirroring: some chars are mirrored");
    }

    // --- Test 21: insert with UTF-8 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\nxyz");
        obj.insert(3, " \xD8\xAF\xD8\xB1\xD9\x88\xD8\xAF");
        check(obj.paragraphCount() == 2,
              "insert UTF-8: still 2 paragraphs");
        check(obj.paragraphTextViewUtf32(0).size() > 4,
              "insert UTF-8: para0 grew");
    }

    // --- Test 22: replace with UTF-8 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"hello");
        obj.replace(0, 5, "\xD8\xAF\xD8\xB1\xD9\x88\xD8\xAF");
        auto& r = obj.result();
        check(r.paragraphLevel == 1,
              "replace UTF-8: paragraph is RTL");
    }

    // --- Test 23: paragraphTextUtf8 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");

        auto utf8_0 = obj.paragraphText(0);
        auto utf8_1 = obj.paragraphText(1);

        check(utf8_0 == "abc\n", "paragraphTextUtf8: para 0");
        check(!utf8_1.empty(), "paragraphTextUtf8: para 1 non-empty");
        check(utf8_1.size() == 8,
              "paragraphTextUtf8: para 1 is 8 bytes");
    }

    // --- Test 24: textUtf8 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc \u062F\u0631\u0648\u062F");

        auto utf8 = obj.text();
        check(!utf8.empty(), "textUtf8: non-empty");
        check(utf8.size() == 12, "textUtf8: 12 bytes");

        obj.insert(0, U"X");
        auto utf8_2 = obj.text();
        check(utf8_2.size() == 13, "textUtf8: 13 bytes after insert");
        check(utf8_2[0] == 'X', "textUtf8: starts with X");
    }

    // --- Test 25: paragraphTextUtf8 (view) ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");

        auto utf8_0 = obj.paragraphTextView(0);
        auto utf8_1 = obj.paragraphTextView(1);

        check(utf8_0 == "abc\n", "paragraphTextView: para 0");
        check(utf8_1.size() == 8, "paragraphTextView: para 1 is 8 bytes");
    }

    // --- Test 26: paragraphLevel ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");

        check(obj.paragraphLevel(0) == 0, "paragraphLevel: para 0 LTR");
        check(obj.paragraphLevel(1) == 1, "paragraphLevel: para 1 RTL");
    }

    // --- Test 27: directionAt / levelAt ---
    {
        bidi::BidiObject obj;
        obj.setText(U"a\u0628");

        check(obj.directionAt(0) == bidi::Direction::LTR,
              "directionAt: a is LTR");
        check(obj.directionAt(1) == bidi::Direction::RTL,
              "directionAt: ب is RTL");
        check(obj.levelAt(0) == 0, "levelAt: a level 0");
        check(obj.levelAt(1) == 1, "levelAt: ب level 1");
    }

    // --- Test 28: isMirrored ---
    {
        bidi::BidiObject obj;
        obj.setText(U"\u0627\u0628\u062C (\u0633\u0644\u0627\u0645)");

        bool foundMirror = false;
        for (size_t i = 0; i < obj.textView().size(); ++i) {
            if (obj.isMirrored(i)) foundMirror = true;
        }
        check(foundMirror, "isMirrored: found a mirrored char");
    }

    // --- Test 29: runAt ---
    {
        bidi::BidiObject obj;
        obj.setText(U"Hello \u062F\u0631\u0648\u062F world");

        auto* run0 = obj.runAt(0);
        check(run0 != nullptr, "runAt: run at 0");
        if (run0) {
            check(run0->direction == bidi::Direction::LTR,
                  "runAt: run at 0 is LTR");
        }

        auto* runPersian = obj.runAt(6);
        check(runPersian != nullptr, "runAt: run at 6 (Persian)");
        if (runPersian) {
            check(runPersian->direction == bidi::Direction::RTL,
                  "runAt: Persian run is RTL");
        }
    }

    // --- Test 30: clearCaches ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc");

        auto& r1 = obj.result();
        (void)r1;
        auto u1 = obj.text();
        (void)u1;

        obj.clearCaches();

        auto& r2 = obj.result();
        check(r2.paragraphLevel == 0, "clearCaches: recomputes");
        auto u2 = obj.text();
        check(u2.size() == 3, "clearCaches: UTF-8 recomputes");
    }

    // --- Test 31: logicalToVisualAt / visualToLogicalAt ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc");

        check(obj.logicalToVisualAt(0) == 0,
              "logicalToVisualAt: 0 -> 0");
        check(obj.logicalToVisualAt(2) == 2,
              "logicalToVisualAt: 2 -> 2");
        check(obj.visualToLogicalAt(0) == 0,
              "visualToLogicalAt: 0 -> 0");
    }

    // --- Test 32: textView vs text ---
    {
        bidi::BidiObject obj;
        obj.setText(U"hello");

        auto view = obj.textView();
        auto copy = obj.text();

        check(view == "hello", "textView: content");
        check(copy == "hello", "text: content");
    }

    // --- Test 33: textViewUtf32 vs textUtf32 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc \u062F\u0631\u0648\u062F");

        auto view32 = obj.textViewUtf32();
        auto copy32 = obj.textUtf32();

        check(view32 == U"abc \u062F\u0631\u0648\u062F",
              "textViewUtf32: content");
        check(copy32 == U"abc \u062F\u0631\u0648\u062F",
              "textUtf32: content");
    }

    // --- Test 34: paragraphTextView vs paragraphText ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");

        auto view = obj.paragraphTextView(0);
        auto copy = obj.paragraphText(0);

        check(view == "abc\n", "paragraphTextView: content");
        check(copy == "abc\n", "paragraphText: content");
    }

    // --- Test 35: paragraphTextViewUtf32 vs paragraphTextUtf32 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\n\u062F\u0631\u0648\u062F");

        auto view32 = obj.paragraphTextViewUtf32(0);
        auto copy32 = obj.paragraphTextUtf32(0);

        check(view32 == U"abc\n", "paragraphTextViewUtf32: content");
        check(copy32 == U"abc\n", "paragraphTextUtf32: content");
    }

    // --- Test 36: toVisualStringUtf32 ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc \u062F\u0631\u0648\u062F");

        auto visual = obj.toVisualStringUtf32();
        check(!visual.empty(), "toVisualStringUtf32: non-empty");
    }

    // --- Test 37: Surrogate (emoji) in UTF-32 ---
    {
        // U+1F600 (grinning face) — as a single UTF-32 code point.
        bidi::BidiObject obj;
        obj.setText(U"\U0001F600");
        check(obj.textViewUtf32().size() == 1,
              "emoji: 1 UTF-32 code unit");
        check(obj.result().levels.size() == 1,
              "emoji: 1 level");
    }

    // --- Test 38: Mixed ASCII + emoji (no surrogates in UTF-32) ---
    {
        bidi::BidiObject obj;
        obj.setText(U"abc\U0001F600def");
        check(obj.textViewUtf32().size() == 7,
              "mixed emoji: 7 UTF-32 code units");
        check(obj.result().levels.size() == 7,
              "mixed emoji: 7 levels");
    }

    // --- Test 39: Long ASCII text ---
    {
        std::u32string text;
        for (int i = 0; i < 100; ++i)
            text += U"abcdefgh";

        bidi::BidiObject obj;
        obj.setText(text);
        check(obj.textViewUtf32().size() == 800,
              "long ASCII: 800 UTF-32 code units");
        check(obj.result().levels.size() == 800,
              "long ASCII: 800 levels");
    }

    std::printf("\n=== Results: %d passed, %d failed ===\n",
                passed, failed);
    return (failed == 0) ? 0 : 1;
}
