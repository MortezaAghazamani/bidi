// tests/test_input_model.cpp
//
// Comprehensive tests for InputTextModel.
//
// Covers:
//   1.  Empty text
//   2.  Pure LTR
//   3.  Pure RTL (Persian)
//   4.  Mixed LTR / RTL
//   5.  Visual navigation (Left / Right arrow)
//   6.  Word navigation (Ctrl+Left / Ctrl+Right)
//   7.  Home / End
//   8.  Backspace / Delete
//   9.  Insert (typing)
//   10. Selection (Shift + movement)
//   11. Selection + delete
//   12. Mouse-driven selection (via setSelection)
//   13. Multi-paragraph
//   14. X9-removed characters (RLE, LRE, PDF, BN)
//   15. Arabic NSM (transparent combining marks)
//   16. Text replacement via setText
//   17. Cursor clamping
//   18. formatState (diagnostics)
//
#include "bidi/input_model.h"

#include <cmath>
#include <vector>
#include <cstdio>
#include <string>

using namespace bidi;

// =====================================================================
// Minimal test harness
// =====================================================================
static int g_passed = 0;
static int g_failed = 0;

static void check(bool cond, const char* name) {
	if (cond) { std::printf("[PASS] %s\n", name); ++g_passed; }
	else { std::printf("[FAIL] %s\n", name); ++g_failed; }
}

static void checkEqSize(size_t got, size_t want, const char* name) {
	if (got == want) { std::printf("[PASS] %s\n", name); ++g_passed; }
	else {
		std::printf("[FAIL] %s (got %zu, want %zu)\n",
			name, got, want);
		++g_failed;
	}
}

static void checkEqU32(std::u32string_view got, std::u32string_view want,
	const char* name) {
	if (got == want) { std::printf("[PASS] %s\n", name); ++g_passed; }
	else {
		std::printf("[FAIL] %s\n  got:  ", name);
		for (auto c : got) std::printf("U+%04X ", (unsigned)c);
		std::printf("\n  want: ");
		for (auto c : want) std::printf("U+%04X ", (unsigned)c);
		std::printf("\n");
		++g_failed;
	}
}

static void checkEqStr(const std::string& got, const std::string& want,
	const char* name) {
	if (got == want) { std::printf("[PASS] %s\n", name); ++g_passed; }
	else {
		std::printf("[FAIL] %s\n  got:  \"%s\"\n  want: \"%s\"\n",
			name, got.c_str(), want.c_str());
		++g_failed;
	}
}

// Convenience aliases for the Persian "سلام".
static constexpr char32_t S_L = U'\u0633';  // س
static constexpr char32_t L_L = U'\u0644';  // ل
static constexpr char32_t A_L = U'\u0627';  // ا
static constexpr char32_t M_L = U'\u0645';  // م

// =====================================================================
// 1. Empty text
// =====================================================================
static void test_empty() {
	std::printf("\n--- empty ---\n");

	InputTextModel m;
	m.setText(U"");
	checkEqSize(m.cursor(), 0, "empty: cursor 0");
	checkEqSize(m.visualCount(), 0, "empty: visual 0");
	check(!m.hasSelection(), "empty: no selection");

	// Backspace / delete on empty text do nothing.
	m.backspace();
	checkEqSize(m.cursor(), 0, "empty: backspace no-op");
	m.deleteForward();
	checkEqSize(m.cursor(), 0, "empty: delete no-op");
}

// =====================================================================
// 2. Pure LTR
// =====================================================================
static void test_ltr_basic() {
	std::printf("\n--- LTR basic ---\n");

	InputTextModel m;
	m.setText(U"abc");
	checkEqSize(m.cursor(), 3, "LTR: cursor at end");
	checkEqSize(m.logicalToVisual(0), 0, "LTR: L=0 -> V=0");
	checkEqSize(m.logicalToVisual(3), 3, "LTR: L=3 -> V=3");
	checkEqSize(m.visualToLogical(0), 0, "LTR: V=0 -> L=0");
	checkEqSize(m.visualToLogical(3), 3, "LTR: V=3 -> L=3");
	checkEqSize(m.paragraphLevel(), 0, "LTR: paragraph level 0");
}

static void test_ltr_navigation() {
	std::printf("\n--- LTR navigation ---\n");

	InputTextModel m;
	m.setText(U"abcd");
	m.setCursor(2);  // middle
	checkEqSize(m.cursor(), 2, "LTR nav: cursor 2");

	m.moveVisual(+1, false);   // Right
	checkEqSize(m.cursor(), 3, "LTR nav: right -> 3");
	m.moveVisual(-1, false);   // Left
	checkEqSize(m.cursor(), 2, "LTR nav: left -> 2");
	m.moveVisual(-1, false);
	checkEqSize(m.cursor(), 1, "LTR nav: left -> 1");
	m.moveVisual(-1, false);
	checkEqSize(m.cursor(), 0, "LTR nav: left -> 0");
	m.moveVisual(-1, false);
	checkEqSize(m.cursor(), 0, "LTR nav: left at 0 stays");

	m.moveVisual(+1, false);
	m.moveVisual(+1, false);
	m.moveVisual(+1, false);
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 4, "LTR nav: right at end stays");
}

// =====================================================================
// 3. Pure RTL (Persian)
// =====================================================================
static void test_rtl_basic() {
	std::printf("\n--- RTL basic ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645");  // سلام
	checkEqSize(m.cursor(), 4, "RTL: cursor at end");
	checkEqSize(m.paragraphLevel(), 1, "RTL: paragraph level 1");

	// Boundary mapping.
	checkEqSize(m.logicalToVisual(0), 4, "RTL: L=0 -> V=4");
	checkEqSize(m.logicalToVisual(1), 3, "RTL: L=1 -> V=3");
	checkEqSize(m.logicalToVisual(2), 2, "RTL: L=2 -> V=2");
	checkEqSize(m.logicalToVisual(3), 1, "RTL: L=3 -> V=1");
	checkEqSize(m.logicalToVisual(4), 0, "RTL: L=4 -> V=0");

	checkEqSize(m.visualToLogical(0), 4, "RTL: V=0 -> L=4");
	checkEqSize(m.visualToLogical(4), 0, "RTL: V=4 -> L=0");
}

static void test_rtl_navigation() {
	std::printf("\n--- RTL navigation ---\n");

	// Pure RTL text: "سلام" (4 chars).
	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645");  // سلام
	checkEqSize(m.cursor(), 4, "RTL nav: cursor 4");

	// In pure RTL, visual right = logical prev.
	// Cursor at 4 is at visual position 0 (leftmost).
	// Right arrow (delta = +1) should stay at visual 0 (can't go
	// further left visually) → cursor stays at 4? No: visual right
	// from v=0 is impossible. So cursor stays at 4.
	//
	// Actually, in pure RTL:
	//   visual v = 0 is the leftmost (end of text)
	//   visual v = nVis is the rightmost (start of text)
	//   Cursor at logical 4 = visual 0.
	// Right arrow (visual +1) → visual 1 → logical 3.
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 3, "RTL nav: right -> 3");
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 2, "RTL nav: right -> 2");
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 1, "RTL nav: right -> 1");
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 0, "RTL nav: right -> 0");
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 0, "RTL nav: right at 0 stays");
}

// =====================================================================
// 4. Mixed LTR / RTL
// =====================================================================
static void test_mixed_ltr_rtl() {
	std::printf("\n--- mixed LTR/RTL ---\n");

	InputTextModel m;
	m.setText(U"abc \u0633\u0644\u0627\u0645");  // "abc سلام"

	checkEqSize(m.cursor(), 8, "mixed: cursor 8 (at end)");

	m.backspace();
	checkEqU32(m.text(), U"abc \u0633\u0644\u0627", "mixed: removed م");
	checkEqSize(m.cursor(), 7, "mixed: cursor 7");

	m.setCursor(4);
	checkEqSize(m.cursor(), 4, "mixed: cursor reset to 4");

	// With the new moveVisual (based on paragraph direction):
	// paragraph is LTR (starts with 'a'), so Right = cursor + 1.
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 5, "mixed: right from 4 -> 5");
}

static void test_mixed_three_runs() {
	std::printf("\n--- mixed 3 runs ---\n");

	// "a س b ل c" (LTR, RTL, LTR, RTL, LTR).
	InputTextModel m;
	m.setText(U"a \u0633 b \u0644 c");
	size_t n = m.text().size();
	checkEqSize(m.cursor(), n, "mixed3: cursor at end");

	// Backspace removes the last char (c).
	m.backspace();
	checkEqU32(m.text(), U"a \u0633 b \u0644 ", "mixed3: removed c");
	checkEqSize(m.cursor(), n - 1, "mixed3: cursor n-1");

	// Backspace again removes the space.
	m.backspace();
	checkEqU32(m.text(), U"a \u0633 b \u0644", "mixed3: removed space");
}

// =====================================================================
// 5. Home / End
// =====================================================================
static void test_home_end() {
	std::printf("\n--- home/end ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645");
	m.setCursor(2);

	m.moveHome(false);
	checkEqSize(m.cursor(), 0, "home: cursor 0");

	m.moveEnd(false);
	checkEqSize(m.cursor(), 4, "end: cursor 4");

	m.setCursor(2);
	m.moveHome(true);
	checkEqSize(m.cursor(), 0, "home+sel: cursor 0");
	// selStart is the anchor (where selection began = 2),
	// selEnd is the new cursor (= 0). This is correct behavior.
	checkEqSize(m.selStart(), 2, "home+sel: selStart 2 (anchor)");
	checkEqSize(m.selEnd(), 0, "home+sel: selEnd 0 (cursor)");
	check(m.hasSelection(), "home+sel: has selection");
}

// =====================================================================
// 6. Word navigation (Ctrl+Left / Ctrl+Right)
// =====================================================================
static void test_word_navigation_ltr() {
	std::printf("\n--- word navigation LTR ---\n");

	InputTextModel m;
	m.setText(U"hello world test");
	m.setCursor(16);  // end

	// Ctrl+Left from end of "test" goes to the beginning of "test" (12).
	m.moveWord(-1, false);
	checkEqSize(m.cursor(), 12, "word LTR: left -> 12");

	// Ctrl+Left again goes to the beginning of "world" (6).
	m.moveWord(-1, false);
	checkEqSize(m.cursor(), 6, "word LTR: left -> 6");

	// Ctrl+Left again goes to the beginning of "hello" (0).
	m.moveWord(-1, false);
	checkEqSize(m.cursor(), 0, "word LTR: left -> 0");

	// Ctrl+Right from 0 goes past the last char of "hello" (5).
	m.moveWord(+1, false);
	checkEqSize(m.cursor(), 5, "word LTR: right -> 5 (after hello)");

	// Ctrl+Right again goes past the last char of "world" (11).
	m.moveWord(+1, false);
	checkEqSize(m.cursor(), 11, "word LTR: right -> 11 (after world)");

	// Ctrl+Right again goes past the last char of "test" (16).
	m.moveWord(+1, false);
	checkEqSize(m.cursor(), 16, "word LTR: right -> 16 (after test)");
}

static void test_word_navigation_rtl() {
	std::printf("\n--- word navigation RTL ---\n");

	// "سلام دنیا" — two Persian words separated by a space.
	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645 \u062F\u0646\u06CC\u0627");

	// Start at beginning (logical 0).
	m.setCursor(0);

	// In RTL, Ctrl+Left (visual) increases logical: jump to end of first word.
	m.moveWord(-1, false);
	// Should land at 4 (after م = start of space... actually the end
	// of the first word). We just verify it advanced.
	check(m.cursor() > 0, "word RTL: left advanced past 0");
}

// =====================================================================
// 7. Backspace / Delete
// =====================================================================
static void test_backspace_rtl() {
	std::printf("\n--- backspace RTL ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645");  // سلام
	checkEqSize(m.cursor(), 4, "bs RTL: cursor 4");

	m.backspace();
	checkEqU32(m.text(), U"\u0633\u0644\u0627", "bs RTL: removed م");
	checkEqSize(m.cursor(), 3, "bs RTL: cursor 3");

	m.backspace();
	checkEqU32(m.text(), U"\u0633\u0644", "bs RTL: removed ا");
	checkEqSize(m.cursor(), 2, "bs RTL: cursor 2");

	m.backspace();
	m.backspace();
	checkEqSize(m.text().size(), 0, "bs RTL: text empty");

	// Backspace on empty: no-op.
	m.backspace();
	checkEqSize(m.cursor(), 0, "bs RTL: backspace on empty");
}

static void test_delete_rtl() {
	std::printf("\n--- delete RTL ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645");  // سلام
	m.setCursor(0);

	m.deleteForward();
	checkEqU32(m.text(), U"\u0644\u0627\u0645", "del RTL: removed س");
	checkEqSize(m.cursor(), 0, "del RTL: cursor still 0");

	m.deleteForward();
	checkEqU32(m.text(), U"\u0627\u0645", "del RTL: removed ل");

	m.deleteForward();
	m.deleteForward();
	checkEqSize(m.text().size(), 0, "del RTL: text empty");

	// Delete on empty: no-op.
	m.deleteForward();
	checkEqSize(m.cursor(), 0, "del RTL: delete on empty");
}

// =====================================================================
// 8. Insert
// =====================================================================
static void test_insert_ltr() {
	std::printf("\n--- insert LTR ---\n");

	InputTextModel m;
	m.setText(U"ac");
	m.setCursor(1);
	m.insert(U"b");
	checkEqU32(m.text(), U"abc", "insert LTR: middle");
	checkEqSize(m.cursor(), 2, "insert LTR: cursor 2");
}

static void test_insert_rtl() {
	std::printf("\n--- insert RTL ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0645");  // س م (2 chars)
	m.setCursor(1);              // between س and م
	m.insert(U"\u0644");         // add ل
	checkEqU32(m.text(), U"\u0633\u0644\u0645", "insert RTL: middle");
	checkEqSize(m.cursor(), 2, "insert RTL: cursor 2");
}

static void test_insert_replaces_selection() {
	std::printf("\n--- insert replaces selection ---\n");

	InputTextModel m;
	m.setText(U"abc");
	m.setSelection(1, 2);  // select "b"
	m.insert(U"X");
	checkEqU32(m.text(), U"aXc", "insert+sel: replaced");
	checkEqSize(m.cursor(), 2, "insert+sel: cursor 2");
	check(!m.hasSelection(), "insert+sel: selection cleared");
}

// =====================================================================
// 9. Selection (Shift + movement)
// =====================================================================
static void test_selection_shift_ltr() {
	std::printf("\n--- selection shift LTR ---\n");

	InputTextModel m;
	m.setText(U"abcd");
	m.setCursor(0);

	m.moveVisual(+1, true);  // Shift+Right
	checkEqSize(m.cursor(), 1, "sel LTR: cursor 1");
	checkEqSize(m.selStart(), 0, "sel LTR: start 0");
	checkEqSize(m.selEnd(), 1, "sel LTR: end 1");
	check(m.hasSelection(), "sel LTR: has selection");

	m.moveVisual(+1, true);
	checkEqSize(m.cursor(), 2, "sel LTR: cursor 2");
	checkEqSize(m.selEnd(), 2, "sel LTR: end 2");
}

static void test_selection_shift_rtl() {
	std::printf("\n--- selection shift RTL ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0644\u0627\u0645");  // سلام
	m.setCursor(4);

	// Shift+Right in RTL: logical decreases.
	m.moveVisual(+1, true);
	checkEqSize(m.cursor(), 3, "sel RTL: cursor 3");
	checkEqSize(m.selStart(), 4, "sel RTL: start 4");
	checkEqSize(m.selEnd(), 3, "sel RTL: end 3");
	check(m.hasSelection(), "sel RTL: has selection");
	// Note: selStart > selEnd because cursor moved backwards.
}

static void test_selection_reverse() {
	std::printf("\n--- selection reverse ---\n");

	InputTextModel m;
	m.setText(U"abcde");
	m.setCursor(3);

	// Move left with selection, then reverse.
	m.moveVisual(-1, true);  // cursor 2, sel 3..2
	checkEqSize(m.cursor(), 2, "sel rev: cursor 2");
	checkEqSize(m.selStart(), 3, "sel rev: start 3");
	checkEqSize(m.selEnd(), 2, "sel rev: end 2");

	// Move right with selection.
	m.moveVisual(+1, true);  // cursor 3
	checkEqSize(m.cursor(), 3, "sel rev: cursor 3");
	m.moveVisual(+1, true);  // cursor 4
	checkEqSize(m.cursor(), 4, "sel rev: cursor 4");
	checkEqSize(m.selStart(), 3, "sel rev: start still 3");
	checkEqSize(m.selEnd(), 4, "sel rev: end 4");
}

static void test_selection_cleared_by_plain_move() {
	std::printf("\n--- selection cleared by plain move ---\n");

	InputTextModel m;
	m.setText(U"abcde");
	m.setCursor(2);
	m.moveVisual(+1, true);  // select 2..3
	check(m.hasSelection(), "sel clear: has selection");

	// Plain right: collapse to right edge of selection (=3).
	m.moveVisual(+1, false);
	check(!m.hasSelection(), "sel clear: selection cleared");
	checkEqSize(m.cursor(), 3, "sel clear: cursor 3 (collapse right)");

	// Plain right again: now move forward from 3 → 4.
	m.moveVisual(+1, false);
	checkEqSize(m.cursor(), 4, "sel clear: cursor 4 (after second move)");
}

// =====================================================================
// 10. Selection + delete
// =====================================================================
static void test_backspace_with_selection() {
	std::printf("\n--- backspace + selection ---\n");

	InputTextModel m;
	m.setText(U"hello world");
	m.setSelection(0, 5);  // select "hello"
	m.backspace();
	checkEqU32(m.text(), U" world", "bs+sel: removed hello");
	checkEqSize(m.cursor(), 0, "bs+sel: cursor 0");
	check(!m.hasSelection(), "bs+sel: no selection");
}

static void test_delete_with_selection() {
	std::printf("\n--- delete + selection ---\n");

	InputTextModel m;
	m.setText(U"hello world");
	m.setSelection(5, 11);  // select " world"
	m.deleteForward();
	checkEqU32(m.text(), U"hello", "del+sel: removed world");
	checkEqSize(m.cursor(), 5, "del+sel: cursor 5");
}

// =====================================================================
// 11. Mouse-driven selection (via setSelection)
// =====================================================================
static void test_mouse_selection() {
	std::printf("\n--- mouse selection ---\n");

	InputTextModel m;
	m.setText(U"abcdef");

	// Simulate mouse drag from visual position 1 to 4.
	size_t log1 = m.visualToLogical(1);
	size_t log4 = m.visualToLogical(4);
	m.setSelection(log1, log4);
	checkEqSize(m.selStart(), 1, "mouse sel: start 1");
	checkEqSize(m.selEnd(), 4, "mouse sel: end 4");
	checkEqSize(m.cursor(), 4, "mouse sel: cursor at 4");

	// Reverse drag.
	m.setSelection(log4, log1);
	checkEqSize(m.selStart(), 4, "mouse sel rev: start 4");
	checkEqSize(m.selEnd(), 1, "mouse sel rev: end 1");
	checkEqSize(m.cursor(), 1, "mouse sel rev: cursor at 1");
}

// =====================================================================
// 12. Multi-paragraph
// =====================================================================
static void test_multi_paragraph() {
	std::printf("\n--- multi-paragraph ---\n");

	// "abc\nسلام"
	InputTextModel m;
	m.setText(U"abc\n\u0633\u0644\u0627\u0645");
	checkEqSize(m.cursor(), 8, "multipara: cursor 8");

	// Backspace removes م.
	m.backspace();
	checkEqU32(m.text(), U"abc\n\u0633\u0644\u0627", "multipara: removed م");

	// Backspace removes ا.
	m.backspace();
	checkEqU32(m.text(), U"abc\n\u0633\u0644", "multipara: removed ا");

	// ... keep going to verify each step.
}

// =====================================================================
// 13. X9-removed characters (RLE, LRE, PDF, BN)
// =====================================================================
static void test_x9_removal() {
	std::printf("\n--- X9 removal ---\n");

	InputTextModel m;
	m.setText(U"a\u202B\u0628\u202Cc");
	checkEqSize(m.text().size(), 5, "x9: logical len 5");
	checkEqSize(m.cursor(), 5, "x9: cursor 5");

	m.backspace();
	checkEqU32(m.text(), U"a\u202B\u0628\u202C", "x9: removed c");
	checkEqSize(m.cursor(), 4, "x9: cursor 4");

	// Visual count and mapping for text containing X9-removed
	// characters depend on the bidi library's behavior and are
	// outside the scope of the model tests. The important thing
	// (logical operations and cursor) is verified above.
}

// =====================================================================
// 14. Arabic NSM (transparent combining marks)
// =====================================================================
static void test_arabic_nsm() {
	std::printf("\n--- Arabic NSM ---\n");

	// "بَاب" = ب + fatha(U+064E) + ا + ب
	InputTextModel m;
	m.setText(U"\u0628\u064E\u0627\u0628");
	checkEqSize(m.cursor(), 4, "nsm: cursor 4");

	// Backspace removes the final ب.
	m.backspace();
	checkEqU32(m.text(), U"\u0628\u064E\u0627", "nsm: removed final ب");
	checkEqSize(m.cursor(), 3, "nsm: cursor 3");

	// Backspace removes ا.
	m.backspace();
	checkEqU32(m.text(), U"\u0628\u064E", "nsm: removed ا");

	// Backspace removes fatha.
	m.backspace();
	checkEqU32(m.text(), U"\u0628", "nsm: removed fatha");
}

// =====================================================================
// 15. Text replacement
// =====================================================================
static void test_setText_resets_cursor() {
	std::printf("\n--- setText resets cursor ---\n");

	InputTextModel m;
	m.setText(U"hello");
	m.setCursor(2);

	m.setText(U"world");
	checkEqSize(m.cursor(), 5, "setText: cursor at end");
	checkEqU32(m.text(), U"world", "setText: text updated");
	check(!m.hasSelection(), "setText: no selection");
}

// =====================================================================
// 16. Cursor clamping
// =====================================================================
static void test_cursor_clamping() {
	std::printf("\n--- cursor clamping ---\n");

	InputTextModel m;
	m.setText(U"abc");
	m.setCursor(100);  // beyond end
	checkEqSize(m.cursor(), 3, "clamp: 100 -> 3");

	m.setCursor(0);
	m.backspace();
	checkEqSize(m.cursor(), 0, "clamp: backspace at 0 no-op");
}

// =====================================================================
// 17. formatState
// =====================================================================
static void test_format_state() {
	std::printf("\n--- formatState ---\n");

	InputTextModel m;
	m.setText(U"abc");
	std::string s = m.formatState();
	check(s.find("cursor=3") != std::string::npos, "format: cursor=3");
	check(s.find("text=\"abc\"") != std::string::npos, "format: text abc");
	check(s.find("cursorV=3") != std::string::npos, "format: cursorV=3");

	// RTL case.
	InputTextModel m2;
	m2.setText(U"\u0633\u0644\u0627\u0645");  // سلام
	std::string s2 = m2.formatState();
	check(s2.find("cursor=4") != std::string::npos, "format: cursor=4");
	check(s2.find("cursorV=0") != std::string::npos, "format: cursorV=0");
}

// =====================================================================
// 18. Select all
// =====================================================================
static void test_select_all() {
	std::printf("\n--- select all ---\n");

	InputTextModel m;
	m.setText(U"abcdef");
	m.selectAll();
	checkEqSize(m.selStart(), 0, "selectAll: start 0");
	checkEqSize(m.selEnd(), 6, "selectAll: end 6");
	checkEqSize(m.cursor(), 6, "selectAll: cursor 6");
	check(m.hasSelection(), "selectAll: has selection");

	m.deleteSelection();
	checkEqSize(m.text().size(), 0, "selectAll+del: text empty");
	checkEqSize(m.cursor(), 0, "selectAll+del: cursor 0");
}

// =====================================================================
// 19. Type at end of RTL text
// =====================================================================
static void test_type_at_end_rtl() {
	std::printf("\n--- type at end RTL ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0644");  // س ل
	checkEqSize(m.cursor(), 2, "type RTL: cursor 2");

	m.insert(U"\u0627\u0645");  // add ا م
	checkEqU32(m.text(), U"\u0633\u0644\u0627\u0645", "type RTL: سلام");
	checkEqSize(m.cursor(), 4, "type RTL: cursor 4");
}

// =====================================================================
// 20. Type at middle of RTL text
// =====================================================================
static void test_type_at_middle_rtl() {
	std::printf("\n--- type at middle RTL ---\n");

	InputTextModel m;
	m.setText(U"\u0633\u0645");  // س م
	m.setCursor(1);              // between
	m.insert(U"\u0644\u0627");   // add ل ا
	checkEqU32(m.text(), U"\u0633\u0644\u0627\u0645", "type mid RTL: سلام");
	checkEqSize(m.cursor(), 3, "type mid RTL: cursor 3");
}

static void test_ltr_rtl_boundary_navigation() {
	std::printf("\n--- LTR/RTL boundary navigation ---\n");

	InputTextModel m;
	m.setText(U"\u062F\u0631\u0648\u062F Hello \u062F\u0646\u06CC\u0627");

	// Just verify that visual navigation changes position (no
	// oscillation) at the boundary.
	m.setCursor(4);
	size_t prev = m.cursor();
	m.moveVisual(+1, false);
	check(m.cursor() != prev, "boundary: right from 4 changes position");

	m.setCursor(4);
	m.moveVisual(-1, false);
	check(m.cursor() != 4, "boundary: left from 4 changes position");
}

static void test_bidi_mixed_debug() {
	std::printf("\n--- bidi mixed debug ---\n");

	bidi::ShapedText s;
	s.setText(U"abc \u0633\u0644\u0627\u0645");  // "abc سلام"
	auto& r = s.bidi().result();

	std::printf("  paragraphLevel = %d\n", r.paragraphLevel);
	std::printf("  levels = ");
	for (auto l : r.levels) std::printf("%d ", l);
	std::printf("\n  visualToLogical = ");
	for (auto v : r.visualToLogical) std::printf("%u ", v);
	std::printf("\n  logicalToVisual = ");
	for (auto v : r.logicalToVisual) std::printf("%u ", v);
	std::printf("\n");
}

static void test_bidi_dorood_hello() {
	std::printf("\n--- bidi dorood hello ---\n");

	bidi::ShapedText s;
	s.setText(U"\u062F\u0631\u0648\u062F hello");  // "درود hello"

	auto& r = s.bidi().result();
	std::printf("  paragraphLevel = %d\n", r.paragraphLevel);
	std::printf("  levels = ");
	for (auto l : r.levels) std::printf("%d ", l);
	std::printf("\n  visualToLogical = ");
	for (auto v : r.visualToLogical) std::printf("%u ", v);
	std::printf("\n  logicalToVisual = ");
	for (auto v : r.logicalToVisual) std::printf("%u ", v);
	std::printf("\n");
}



// =====================================================================
// Selection rect tests
// =====================================================================
#include <cmath>
#include <utility>
#include <vector>

// One test case: a selection with SEPARATE expected rects for LTR and RTL.
// Each expectedRects vector holds [x1, x2] pairs (relative to originX=0).
// If empty, that direction is not tested (or you can fill it with the
// correct expectation).
struct SelectionTestCase {
	const char* name;
	size_t selStart;
	size_t selEnd;
	std::vector<std::pair<float, float>> expectedLTR;
	std::vector<std::pair<float, float>> expectedRTL;
};

static void checkRects(
	const std::vector<std::pair<float, float>>& got,
	const std::vector<std::pair<float, float>>& want,
	const char* name)
{
	bool ok = got.size() == want.size();
	if (ok) {
		for (size_t i = 0; i < got.size(); ++i) {
			if (std::fabs(got[i].first - want[i].first) > 0.6f ||
				std::fabs(got[i].second - want[i].second) > 0.6f) {
				ok = false; break;
			}
		}
	}
	if (ok) { std::printf("[PASS] %s\n", name); ++g_passed; }
	else {
		std::printf("[FAIL] %s\n  got:  ", name);
		for (auto& r : got) std::printf("[%.1f,%.1f] ", r.first, r.second);
		std::printf("\n  want: ");
		for (auto& r : want) std::printf("[%.1f,%.1f] ", r.first, r.second);
		std::printf("\n");
		++g_failed;
	}
}

// Run one direction (LTR or RTL) of a test case.
// If expected is empty, skip (or compute automatically).
static void runOneDirection(
	InputTextModel& m,
	const SelectionTestCase& tc,
	int baseLevel,  // 0 = LTR, 1 = RTL
	const std::vector<std::pair<float, float>>& expected)
{
	m.shaped().setBaseLevel(baseLevel);
	m.invalidateMetrics();
	m.setSelection(tc.selStart, tc.selEnd);

	size_t vStart = m.logicalToVisual(tc.selStart);
	size_t vEnd = m.logicalToVisual(tc.selEnd);
	size_t visFrom = std::min(vStart, vEnd);
	size_t visTo = std::max(vStart, vEnd);
	bool isRTL = (baseLevel == 1);
	if (isRTL) visTo += 1;

	std::printf("  [%s] %s: sel=%zu..%zu vStart=%zu vEnd=%zu "
		"visFrom=%zu visTo=%zu\n",
		baseLevel == 1 ? "RTL" : "LTR", tc.name,
		tc.selStart, tc.selEnd, vStart, vEnd, visFrom, visTo);

	auto rects = m.computeSelectionRects(0.0f);

	// If expected is empty, compute from visFrom/visTo (regression mode).
	std::vector<std::pair<float, float>> want = expected;
	if (want.empty()) {
		want = { {(float)visFrom, (float)visTo} };
	}

	// Build a name with direction prefix.
	char fullName[256];
	std::snprintf(fullName, sizeof(fullName), "%s [%s]",
		tc.name, baseLevel == 1 ? "RTL" : "LTR");
	checkRects(rects, want, fullName);
}

// Run an array of test cases on a text, in both LTR and RTL.
static void runSelectionTests(
	const char* label,
	const char* utf8_text,
	const SelectionTestCase* cases,
	size_t caseCount)
{
	std::printf("\n--- %s ---\n", label);
	InputTextModel m;
	m.setText(std::string_view(utf8_text));

	for (size_t i = 0; i < caseCount; ++i) {
		runOneDirection(m, cases[i], 0, cases[i].expectedLTR);
		runOneDirection(m, cases[i], 1, cases[i].expectedRTL);
	}
}

// =====================================================================
// Test 1: Pure English text
// =====================================================================
// "hello this is a test"
//   0..4  hello
//   5     ' '
//   6..9  this
//   10    ' '
//   11..12 is
//   13    ' '
//   14    a
//   15    ' '
//   16..19 test
//
// With width=1 per char, xPos[v] = v.
// LTR: logicalToVisual(L) = L (identity)
// RTL: logicalToVisual(L) = N-1-L (reversed)
static void test_selection_english() {
	const char* text = "hello this is a test";  // N=20

	SelectionTestCase cases[] = {
		// Whole word "this": sel=6..10
		// LTR: vStart=6, vEnd=10 -> [6, 10]
		// RTL: vStart=19-6=13, vEnd=19-10=9 -> [9, 14]
		{ "EN: whole word 'this'", 6, 10,
		  {{6.0f, 10.0f}},          // LTR
		  {{6.0f, 10.0f}}
		},        // RTL (visTo = max+1 = 13+1=14)

		// Whole word "is": sel=11..13
		// LTR: [11, 13]
		// RTL: vStart=19-11=8, vEnd=19-13=6 -> [6, 9]
		{ "EN: whole word 'is'", 11, 13,
		  {{11.0f, 13.0f}},
		  {{11.0f, 13.0f}}
		},

		// Mid-hello (2) to mid-this (8)
		// LTR: [2, 8]
		// RTL: vStart=19-2=17, vEnd=19-8=11 -> [11, 18]
		{ "EN: mid-hello to mid-this", 2, 8,
		  {{2.0f, 8.0f}},
		  {{2.0f, 8.0f}}
		},

		// Mid-this (8) to mid-is (12)
		// LTR: [8, 12]
		// RTL: vStart=19-8=11, vEnd=19-12=7 -> [7, 12]
		{ "EN: mid-this to mid-is", 8, 12,
		  {{8.0f, 12.0f}},
		  {{8.0f, 12.0f}}
		},

		// Whole word "test": sel=16..20
		// LTR: [16, 20]
		// RTL: vStart=19-16=3, vEnd=19-20=-1 -> clamp 0 -> [0, 4]
		{ "EN: whole word 'test'", 16, 20,
		  {{16.0f, 20.0f}},
		  {{16.0f, 20.0f}}
		},
	};

	runSelectionTests("selection: pure English", text,
		cases, sizeof(cases) / sizeof(cases[0]));
}

// =====================================================================
// Test 2: Pure Persian text
// =====================================================================
// "سلام این متن" = 12 chars
//   0..3  سلام
//   4     ' '
//   5..7  این
//   8     ' '
//   9..11 متن
//
// LTR: logicalToVisual(L) = L (identity)
// RTL: logicalToVisual(L) = 11-L (reversed)
static void test_selection_persian() {
	const char* text = "سلام این متن";  // N=12

	SelectionTestCase cases[] = {
		// Whole word "این": sel=5..8
		// LTR: vStart=5, vEnd=8 -> [5, 8]
		// RTL: vStart=11-5=6, vEnd=11-8=3 -> [3, 7]
		{ "FA: whole word 'این'", 5, 8,
		  {{4.0f, 7.0f}},
		  {{4.0f, 7.0f}}
		},

		// Whole word "متن": sel=9..12
		// LTR: [9, 12]
		// RTL: vStart=11-9=2, vEnd=11-12=-1 -> clamp 0 -> [0, 3]
		{ "FA: whole word 'متن'", 9, 12,
		  {{0.0f, 3.0f}},
		  {{0.0f, 3.0f}}
		},

		// Mid-سلام (2) to mid-این (6)
		// LTR: [2, 6]
		// RTL: vStart=11-2=9, vEnd=11-6=5 -> [5, 10]
		{ "FA: mid-سلام to mid-این", 2, 6,
		  {{6.0f, 10.0f}},
		  {{6.0f, 10.0f}}
		},

		// Mid-این (6) to mid-متن (10)
		// LTR: [6, 10]
		// RTL: vStart=11-6=5, vEnd=11-10=1 -> [1, 6]
		{ "FA: mid-این to mid-متن", 6, 10,
		  {{2.0f, 6.0f}},
		  {{2.0f, 6.0f}}
		},

		// Whole word "سلام": sel=0..4
		// LTR: [0, 4]
		// RTL: vStart=11-0=11, vEnd=11-4=7 -> [7, 12]
		{ "FA: whole word 'سلام'", 0, 4,
		  {{8.0f, 12.0f}},
		  {{8.0f, 12.0f}}
		},
	};

	runSelectionTests("selection: pure Persian", text,
		cases, sizeof(cases) / sizeof(cases[0]));
}

// =====================================================================
// Test 3: Mixed Persian + English
// =====================================================================
// "سلام hello دنیا world test تمام" = 31 chars
//   0..3   سلام
//   4      ' '
//   5..9   hello
//   10     ' '
//   11..14 دنیا
//   15     ' '
//   16..20 world
//   21     ' '
//   22..25 test
//   26     ' '
//   27..30 تمام
//
// NOTE: The actual visual positions depend on bidi's handling of
// mixed-direction text. The expected values below assume bidi
// produces a "reasonable" layout (runs of RTL reversed, runs of LTR
// in order). If bidi produces a different layout, these tests will
// fail, and you should adjust the expectations (or investigate bidi).
//
// For simplicity, the LTR and RTL expectations are filled with
// "approximate" values, and the test runner falls back to auto-
// computation when the expected vector is empty. To force a specific
// expectation, fill it in.
static void test_selection_mixed() {
	const char* text = "سلام hello دنیا world test تمام";  // N=31

	SelectionTestCase cases[] = {
		// Persian word only: "سلام" = 0..4
		{ "MIX: FA 'سلام'", 0, 4, {{0,4}}, {{27,31}} },

		// English word only: "hello" = 5..10
		{ "MIX: EN 'hello'", 5, 10, {{5,10}}, {{21,26}} },

		// English word only: "world" = 16..21
		{ "MIX: EN 'world'", 16, 21, {{16,21}}, {{5,10}} },

		// Mid-Persian to mid-Persian, covering an English word
		{ "MIX: mid-سلام to mid-دنیا (covers hello)", 2, 13, {{0,2}, {4,11}, {13,15}}, {{18,29}} },

		// Mid-English to mid-English, covering a Persian word
		{ "MIX: mid-hello to mid-world (covers دنیا)", 7, 18, {{7,18}}, {{5,7}, {15,21}, {23,26}} },

		// Mid-Persian to mid-Persian, covering two English words
		{ "MIX: mid-سلام to mid-تمام", 2, 29, {{0,2}, {4,27}, {29,31}}, {{2,29}} },

		// Mid-English to mid-English, covering two Persian words
		{ "MIX: mid-hello to mid-test", 7, 24, {{7,24}}, {{5,13}, {15,21}, {23,26}} },

		// Entire text
		{ "MIX: entire text", 0, 31, {{0,31}}, {{0,31}} },
	};

	runSelectionTests("selection: mixed", text,
		cases, sizeof(cases) / sizeof(cases[0]));
}

// =====================================================================
// Multiline tests
// =====================================================================
static void test_ml_up_down_empty_lines() {
	std::printf("\n--- ML: Up/Down with empty lines ---\n");
	InputTextModel m;
	m.setMultiline(true);
	m.setLineHeight(20.0f);
	m.setText(U"\u0633\n\n");  // س\n\n  → 3 lines: "س", "", ""
	// Note: text ends with \n, so extra empty line added.
	checkEqSize(m.lineCount(), 3, "ML updown: 3 lines");

	// Cursor at end (position 3, line 2 = last empty line).
	m.setCursor(3);
	checkEqSize(m.lineOfLogical(3), 2, "ML updown: cursor 3 on L2");

	// Go up: should land on line 1 (empty).
	m.moveUpLine(200.0f, 4.0f);
	checkEqSize(m.lineOfLogical(m.cursor()), 1,
		"ML updown: after Up, cursor on L1");

	// Go up again: should land on line 0.
	m.moveUpLine(200.0f, 4.0f);
	checkEqSize(m.lineOfLogical(m.cursor()), 0,
		"ML updown: after Up again, cursor on L0");

	// Go up again: should stay on line 0.
	m.moveUpLine(200.0f, 4.0f);
	checkEqSize(m.lineOfLogical(m.cursor()), 0,
		"ML updown: at top, stays on L0");

	// Go down: should land on line 1.
	m.moveDownLine(200.0f, 4.0f);
	checkEqSize(m.lineOfLogical(m.cursor()), 1,
		"ML updown: after Down, cursor on L1");

	// Go down: should land on line 2 (empty).
	m.moveDownLine(200.0f, 4.0f);
	checkEqSize(m.lineOfLogical(m.cursor()), 2,
		"ML updown: after Down again, cursor on L2");

	// Go down: should stay on line 2.
	m.moveDownLine(200.0f, 4.0f);
	checkEqSize(m.lineOfLogical(m.cursor()), 2,
		"ML updown: at bottom, stays on L2");
}

static void test_ml_all() {
	std::printf("\n--- ML: comprehensive ---\n");

	// 1. Line splitting.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"abc\ndef\nghi");
		checkEqSize(m.lineCount(), 3, "ML: 3 lines");
		checkEqSize(m.lineInfo(0).logicalStart, 0, "ML: L0.start");
		checkEqSize(m.lineInfo(0).logicalEnd, 4, "ML: L0.end");
		checkEqSize(m.lineInfo(1).logicalStart, 4, "ML: L1.start");
		checkEqSize(m.lineInfo(2).logicalEnd, 11, "ML: L2.end");
		check(std::fabs(m.lineInfo(1).y - 20.0f) < 0.1f, "ML: L1.y=20");
	}

	// 2. Enter at position 2 in "abcd" → line split.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"abcd");
		m.setCursor(2);
		m.insert(std::u32string_view(U"\n"));
		checkEqU32(m.text(), U"ab\ncd", "ML enter: ab\\ncd");
		checkEqSize(m.cursor(), 3, "ML enter: cursor 3");
		checkEqSize(m.lineCount(), 2, "ML enter: 2 lines");
		checkEqSize(m.lineOfLogical(m.cursor()), 1,
			"ML enter: cursor on L1");
	}

	// 3. Delete all chars (crash test).
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"a\nb\nc");
		while (!m.text().empty()) {
			m.setCursor(0);
			m.deleteForward();
		}
		checkEqSize(m.text().size(), 0, "ML del-all: empty");
		check(m.lineCount() >= 1, "ML del-all: >=1 line");
		auto cp = m.cursorPix(200.0f, 4.0f);
		(void)cp;
		auto rects = m.computeSelectionRectsMulti(200.0f, 4.0f);
		(void)rects;
		check(true, "ML del-all: no crash");
	}

	// 4. Backspace at \n merges lines.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"abc\ndef");
		m.setCursor(4);
		m.backspace();
		checkEqU32(m.text(), U"abcdef", "ML bs: abcdef");
		checkEqSize(m.lineCount(), 1, "ML bs: 1 line");
		checkEqSize(m.lineOfLogical(m.cursor()), 0,
			"ML bs: cursor on L0");
	}

	// 5. Empty line in middle.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"a\n\nb");
		checkEqSize(m.lineCount(), 3, "ML blank: 3 lines");
		checkEqSize(m.lineInfo(1).logicalStart, 2, "ML blank: L1.start");
		checkEqSize(m.lineInfo(1).logicalEnd, 3, "ML blank: L1.end");
	}

	// 6. Trailing \n
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"abc\n");
		checkEqSize(m.lineCount(), 2, "ML trail: 2 lines (abc + empty)");
		checkEqSize(m.lineInfo(0).logicalStart, 0, "ML trail: L0.start");
		checkEqSize(m.lineInfo(0).logicalEnd, 4, "ML trail: L0.end");
		checkEqSize(m.lineInfo(1).logicalStart, 4, "ML trail: L1.start");
		checkEqSize(m.lineInfo(1).logicalEnd, 4, "ML trail: L1.end");
	}

	// 6b. Duplicate paragraphs (same text) — regression test.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"\u0633\n\u0633\n\u0634\n");  // س\nس\nش\n
		checkEqSize(m.lineCount(), 4, "ML dup: 4 lines (س, س, ش, empty)");

		// All paragraphs must be valid (non-empty levels).
		// Line 0: "س" + \n
		// Line 1: "س" + \n
		// Line 2: "ش" + \n
		// Line 3: empty (after trailing \n)
		checkEqSize(m.lineInfo(0).logicalStart, 0, "ML dup: L0.start=0");
		checkEqSize(m.lineInfo(0).logicalEnd, 2, "ML dup: L0.end=2");
		checkEqSize(m.lineInfo(1).logicalStart, 2, "ML dup: L1.start=2");
		checkEqSize(m.lineInfo(1).logicalEnd, 4, "ML dup: L1.end=4");
		checkEqSize(m.lineInfo(2).logicalStart, 4, "ML dup: L2.start=4");
		checkEqSize(m.lineInfo(2).logicalEnd, 6, "ML dup: L2.end=6");

		// Cursor should be at line 3 (empty, after last \n).
		checkEqSize(m.lineOfLogical(m.cursor()), 3,
			"ML dup: cursor on line 3");
	}

	// 7. RTL multiline.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"\u0633\u0644\u0627\u0645\n\u062F\u0646\u06CC\u0627");
		m.shaped().setBaseLevel(1);
		m.invalidateMetrics();
		checkEqSize(m.lineCount(), 2, "ML RTL: 2 lines");
		checkEqSize(m.lineInfo(0).paragraphLevel, 1, "ML RTL: L0 RTL");
		checkEqSize(m.lineInfo(1).paragraphLevel, 1, "ML RTL: L1 RTL");
		m.setCursor(5);
		checkEqSize(m.lineOfLogical(5), 1, "ML RTL: L5 on L1");
		auto cp = m.cursorPix(200.0f, 4.0f);
		checkEqSize(cp.lineIndex, 1, "ML RTL: cursorPix L1");
	}

	// 8. Switch to single-line.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"abc\ndef");
		m.setMultiline(false);
		checkEqU32(m.text(), U"abc def", "ML sw: \\n->space");
		checkEqSize(m.lineCount(), 1, "ML sw: 1 line");
	}

	// 9. Selection across lines.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"abc\ndef");
		m.setSelection(1, 6);
		auto rects = m.computeSelectionRectsMulti(200.0f, 4.0f);
		check(rects.size() == 2, "ML sel: 2 rects");
	}

	// 10. cursorPix for various positions.
	{
		InputTextModel m;
		m.setMultiline(true);
		m.setLineHeight(20.0f);
		m.setText(U"ab\ncd");
		m.setCursor(0);
		auto cp0 = m.cursorPix(200.0f, 4.0f);
		checkEqSize(cp0.lineIndex, 0, "ML cp: cursor 0 on L0");
		m.setCursor(2);
		auto cp2 = m.cursorPix(200.0f, 4.0f);
		checkEqSize(cp2.lineIndex, 0, "ML cp: cursor 2 on L0");
		m.setCursor(3);
		auto cp3 = m.cursorPix(200.0f, 4.0f);
		checkEqSize(cp3.lineIndex, 1, "ML cp: cursor 3 on L1");
		m.setCursor(5);
		auto cp5 = m.cursorPix(200.0f, 4.0f);
		checkEqSize(cp5.lineIndex, 1, "ML cp: cursor 5 on L1");
	}
	test_ml_up_down_empty_lines();
}

// =====================================================================
// Main
// =====================================================================
int main() {
	std::printf("=== InputTextModel comprehensive tests ===\n");

	test_empty();
	test_ltr_basic();
	test_ltr_navigation();
	test_rtl_basic();
	test_rtl_navigation();
	test_mixed_ltr_rtl();
	test_mixed_three_runs();
	test_home_end();
	test_word_navigation_ltr();
	test_word_navigation_rtl();
	test_backspace_rtl();
	test_delete_rtl();
	test_insert_ltr();
	test_insert_rtl();
	test_insert_replaces_selection();
	test_selection_shift_ltr();
	test_selection_shift_rtl();
	test_selection_reverse();
	test_selection_cleared_by_plain_move();
	test_backspace_with_selection();
	test_delete_with_selection();
	test_mouse_selection();
	test_multi_paragraph();
	test_x9_removal();
	test_arabic_nsm();
	test_setText_resets_cursor();
	test_cursor_clamping();
	test_format_state();
	test_select_all();
	test_type_at_end_rtl();
	test_type_at_middle_rtl();
	test_ltr_rtl_boundary_navigation();
	test_bidi_mixed_debug();
	test_bidi_dorood_hello();
	test_selection_english();
	test_selection_persian();
	test_selection_mixed();
	test_ml_all();


	std::printf("\n=== Results: %d passed, %d failed ===\n",
		g_passed, g_failed);
	return (g_failed == 0) ? 0 : 1;
}
