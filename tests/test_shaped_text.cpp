// tests/test_shaped_text.cpp
#include "bidi/shaped_text.h"
#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { std::printf("[PASS] %s\n", name); ++passed; }
    else { std::printf("[FAIL] %s\n", name); ++failed; }
}

int main() {
    // Test 1: Persian "دنیا" should have connected forms.
    {
        bidi::ShapedText s;
        s.setText(U"\u062F\u0646\u06CC\u0627");  // دنیا
        auto shaped = s.toShapedVisualStringUtf32();
        check(shaped.size() == 4, "shaped: دنیا size 4");

        bool anyChanged = false;
        for (size_t i = 0; i < shaped.size(); ++i) {
            char32_t orig = s.textViewUtf32()[i];
            if (shaped[i] != orig) anyChanged = true;
        }
        check(anyChanged, "shaped: presentation forms applied");
    }

    // Test 2: LTR text is unchanged.
    {
        bidi::ShapedText s;
        s.setText(U"Hello");
        auto shaped = s.toShapedVisualStringUtf32();
        check(shaped == U"Hello", "shaped: LTR unchanged");
    }

    // Test 3: Mixed text.
    {
        bidi::ShapedText s;
        s.setText(U"\u062F\u0631\u0648\u062F Hello \u062F\u0646\u06CC\u0627");
        auto shaped = s.toShapedVisualStringUtf32();
        check(!shaped.empty(), "shaped: mixed non-empty");
    }

    // Test 4: Cache invalidation.
    {
        bidi::ShapedText s;
        s.setText(U"\u062F\u0646\u06CC\u0627");
        auto v1 = s.shapedVisualViewUtf32();
        (void)v1;

        s.setText(U"Hello");
        auto v2 = s.shapedVisualViewUtf32();
        check(v2 == U"Hello", "shaped: cache invalidated");
    }

        // Test 5: Arabic with fatha (transparent) does not break joining.
    // "بَاب" = beh + fatha + alef + beh
    // Expected shaped: beh-initial + alef-final + beh-isolated
    {
        bidi::ShapedText s;
        s.setText(U"\u0628\u064E\u0627\u0628");  // بَاب
        auto shaped = s.toShapedVisualStringUtf32();
        check(shaped.size() == 4, "shaped fatha: size 4");
        // In visual order (RTL), it's: beh-isolated, alef-final,
        // fatha (unchanged), beh-initial.
        // beh-initial = U+FE91
        // alef-final = U+FE8E
        // beh-isolated = U+FE8F
        // fatha = U+064E (unchanged)
        bool hasBehInitial = false;
        bool hasAlefFinal = false;
        bool hasBehIsolated = false;
        bool hasFatha = false;
        for (char32_t c : shaped) {
            if (c == 0xFE91) hasBehInitial = true;
            if (c == 0xFE8E) hasAlefFinal = true;
            if (c == 0xFE8F) hasBehIsolated = true;
            if (c == 0x064E) hasFatha = true;
        }
        check(hasBehInitial, "shaped fatha: beh-initial (FE91)");
        check(hasAlefFinal, "shaped fatha: alef-final (FE8E)");
        check(hasBehIsolated, "shaped fatha: beh-isolated (FE8F)");
        check(hasFatha, "shaped fatha: fatha preserved (064E)");
    }

    // Test 6: "سَلام" = seen + fatha + lam + alef + meem
    // Expected: seen-initial, lam-initial, alef-final, meem-isolated.
    {
        bidi::ShapedText s;
        s.setText(U"\u0633\u064E\u0644\u0627\u0645");  // سَلام
        auto shaped = s.toShapedVisualStringUtf32();
        bool hasSeenInitial = false;
        bool hasLamInitial = false;
        bool hasAlefFinal = false;
        bool hasMeemIsolated = false;
        bool hasLamMedial = false;
        for (char32_t c : shaped) {
            if (c == 0xFEB3) hasSeenInitial = true;  // seen-initial
            if (c == 0xFEE0) hasLamMedial = true;   // lam-medial (not initial!)
            if (c == 0xFE8E) hasAlefFinal = true;    // alef-final
            if (c == 0xFEE1) hasMeemIsolated = true; // meem-isolated
        }
        check(hasSeenInitial, "shaped fatha: seen-initial (FEB3)");
        check(hasLamMedial, "shaped fatha: lam-medial (FEE0)");
        check(hasAlefFinal, "shaped fatha: alef-final (FE8E)");
        check(hasMeemIsolated, "shaped fatha: meem-isolated (FEE1)");
    }

    std::printf("\n=== Results: %d passed, %d failed ===\n",
                passed, failed);
    return (failed == 0) ? 0 : 1;
}
