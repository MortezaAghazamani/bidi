# API Reference

Complete reference for the `bidi` library.

## Table of Contents

- [Encoding](#encoding)
- [Views vs. copies](#views-vs-copies)
- [`bidi::BidiObject`](#bidibidobject)
- [`bidi::ShapedText`](#bidishapedtext)
- [`bidi::InputTextModel`](#bidiinputtextmodel)
- [`bidi::imbidi::InputText`](#bidiimbidiinputtext)
- [`bidi::AnalysisResult`](#bidianalysisresult)
- [`bidi::Run`](#bidirun)
- [`bidi::Direction`](#bididirection)
- [`bidi::imbidi::BaseLevel`](#bidiimbidibaselevel)

---

## Encoding

The library stores text internally as **UTF-32** (`std::u32string`).
Input and output are available as both UTF-8 and UTF-32.

- All `pos` parameters are in **UTF-32 code units** (one unit per
  Unicode code point).
- `std::u32string` / `std::u32string_view` for UTF-32.
- `std::string` / `std::string_view` for UTF-8.

---

## Views vs. copies

Methods ending in `View` return zero-copy views. They are fast but
become invalid after any text modification.

Methods without `View` return copies. They are safe to store but
allocate memory.

```cpp
bidi::BidiObject obj;
obj.setText("Hello");

std::u32string_view v = obj.textViewUtf32();  // zero-copy
std::u32string s = obj.textUtf32();           // copy
```

---

## `bidi::BidiObject`

The main class for bidi analysis.

### Construction

```cpp
bidi::BidiObject obj;
```

`BidiObject` is movable but not copyable.

### Thread safety

Not thread-safe. Use one object per thread.

### Cache invalidation

- `setText`, `insert`, `erase`, `replace` invalidate caches for
  affected paragraphs and the full-text cache.
- `setBaseLevel` invalidates all caches.
- `invalidateCache()` clears only the full-text cache.
- `clearCaches()` clears all caches (frees memory).

### Text management

| Method | Description |
| -------- | ------------- |
| `void setText(std::string_view utf8)` | Set the text from UTF-8. |
| `void setText(std::u32string_view utf32)` | Set the text from UTF-32. |
| `void insert(size_t pos, std::string_view utf8)` | Insert UTF-8 text at `pos` (UTF-32 code units). |
| `void insert(size_t pos, std::u32string_view utf32)` | Insert UTF-32 text. |
| `void erase(size_t pos, size_t length)` | Erase UTF-32 code units. |
| `void replace(size_t pos, size_t length, std::string_view utf8)` | Replace with UTF-8 text. |
| `void replace(size_t pos, size_t length, std::u32string_view utf32)` | Replace with UTF-32 text. |
| `void setBaseLevel(int level)` | `-1` = auto, `0` = LTR, `1` = RTL. |

### Whole-text access

| Method | Description |
| -------- | ------------- |
| `std::string_view textView() const` | Zero-copy UTF-8 view. |
| `std::u32string_view textViewUtf32() const` | Zero-copy UTF-32 view. |
| `std::string text() const` | UTF-8 copy. |
| `std::u32string textUtf32() const` | UTF-32 copy. |
| `int baseLevel() const` | Current base level (`-1`, `0`, or `1`). |

### Analysis

| Method | Description |
| -------- | ------------- |
| `const AnalysisResult& result() const` | Full-text analysis (lazy, cached). |
| `std::string toVisualString() const` | Visual string (UTF-8, with mirroring). |
| `std::u32string toVisualStringUtf32() const` | Visual string (UTF-32, with mirroring). |

### Per-paragraph analysis

| Method | Description |
| -------- | ------------- |
| `size_t paragraphCount() const` | Number of paragraphs. |
| `std::string_view paragraphTextView(size_t i) const` | Zero-copy UTF-8 view of paragraph `i`. |
| `std::u32string_view paragraphTextViewUtf32(size_t i) const` | Zero-copy UTF-32 view. |
| `std::string paragraphText(size_t i) const` | UTF-8 copy. |
| `std::u32string paragraphTextUtf32(size_t i) const` | UTF-32 copy. |
| `const AnalysisResult& paragraphResult(size_t i) const` | Analysis of paragraph `i` (lazy, cached). |
| `int paragraphLevel(size_t i) const` | Paragraph level (0 or 1). |
| `size_t paragraphContaining(size_t pos) const` | Paragraph index containing `pos` (binary search: O(log P)). |

### Per-character queries

All positions are in **UTF-32 code units**. These methods use the
paragraph cache: O(log P) + O(paragraph).

| Method | Description |
| -------- | ------------- |
| `Direction directionAt(size_t pos) const` | Direction at `pos`. |
| `int levelAt(size_t pos) const` | Embedding level at `pos`. |
| `bool isMirrored(size_t pos) const` | True if the character should be mirrored. |
| `const Run* runAt(size_t pos) const` | Run containing `pos` (or `nullptr`). |
| `size_t logicalToVisualAt(size_t logicalPos) const` | Logical → visual position. |
| `size_t visualToLogicalAt(size_t visualPos) const` | Visual → logical position. |

### Cache management

| Method | Description |
| -------- | ------------- |
| `void invalidateCache()` | Invalidate the full-text cache. |
| `void clearCaches()` | Clear all caches (frees memory). |
| `void reserve(size_t textSize)` | Reserve capacity for `textSize` UTF-32 code units. |

---

## `bidi::ShapedText`

Wraps `BidiObject`. Produces a **shaped visual string** with
Arabic/Persian presentation forms applied. See UAX #9 L3 and the
Unicode Standard, Chapter 9 (Middle East-I).

### Construction

```cpp
bidi::ShapedText shaped;
```

Movable, not copyable.

### Text management

Same as `BidiObject`:

| Method | Description |
| -------- | ------------- |
| `setText(std::string_view)` / `setText(std::u32string_view)` | Set the text. |
| `insert(pos, ...)` / `erase(pos, len)` / `replace(pos, len, ...)` | Edit (UTF-8 and UTF-32 overloads). |
| `setBaseLevel(int)` | Base level. |

### Text access

| Method | Description |
| -------- | ------------- |
| `textView()` / `textViewUtf32()` | Zero-copy views of the **logical** text. |
| `text()` / `textUtf32()` | Copies of the logical text. |
| `baseLevel()` | Current base level. |

### Shaped output

| Method | Description |
| -------- | ------------- |
| `std::string toShapedVisualString() const` | Shaped visual string (UTF-8, copy). |
| `std::u32string toShapedVisualStringUtf32() const` | Shaped visual string (UTF-32, copy). |
| `std::string_view shapedVisualView() const` | Zero-copy UTF-8 view. |
| `std::u32string_view shapedVisualViewUtf32() const` | Zero-copy UTF-32 view. |

**Warning:** views are invalidated by any text modification.

### Access underlying BidiObject

| Method | Description |
|--------|-------------|
| `const BidiObject& bidi() const` | The underlying bidi object. |

### Cache management

| Method | Description |
|--------|-------------|
| `void invalidateCache()` | Invalidate the shaped cache. |
| `void clearCaches()` | Clear all caches. |

---

## `bidi::InputTextModel`

Pure editing logic for a bidi-aware text input. **No GUI dependency.**
Can be reused with any renderer.

### Text management

| Method | Description |
| -------- | ------------- |
| `void setText(std::u32string_view text)` | Set the text from UTF-32. Cursor moves to end. |
| `void setText(std::string_view utf8)` | Set the text from UTF-8. |
| `std::u32string_view text() const` | Logical text view (UTF-32). |
| `std::string textUtf8() const` | Logical text (UTF-8 copy). |

### Cursor / selection

| Method | Description |
| -------- | ------------- |
| `size_t cursor() const` | Current cursor (logical index). |
| `size_t selStart() const` / `size_t selEnd() const` | Selection (logical). |
| `bool hasSelection() const` | True if selection is active. |
| `void setCursor(size_t logicalPos)` | Move cursor, clear selection. |
| `void clearSelection()` | Collapse selection. |
| `void selectAll()` | Select everything. |
| `void setSelection(size_t selStart, size_t selEnd)` | Set selection range. |
| `void selectWordAt(size_t logicalPos)` | Select the word at (or adjacent to) `pos`. |

### Visual queries (single-line)

| Method | Description |
| -------- | ------------- |
| `std::u32string_view visual() const` | Shaped visual string (UTF-32). |
| `std::string_view visualUtf8() const` | Shaped visual string (UTF-8). |
| `float xPos(size_t v) const` | x-offset of visual slot `v`. |
| `size_t visualCount() const` | Number of visual slots. |
| `float totalWidth() const` | Total width (pixels). |
| `size_t logicalToVisual(size_t L) const` | Logical → visual. |
| `size_t visualToLogical(size_t V) const` | Visual → logical. |
| `int paragraphLevel() const` | Paragraph level (0 or 1). |

### Font metrics

| Method | Description |
| -------- | ------------- |
| `void setWidthMeasurer(MeasureWidthFn fn)` | Set the character-width function. |
| `void invalidateMetrics()` | Force metrics to be rebuilt. |
| `void ensureMetrics() const` | Ensure metrics are up to date. |

### Navigation

| Method | Description |
| -------- | ------------- |
| `void moveVisual(int delta, bool select)` | Move by visual steps (Left/Right). |
| `void moveWord(int delta, bool select)` | Move by words. |
| `void moveHome(bool select)` | Move to start. |
| `void moveEnd(bool select)` | Move to end. |

### Editing

| Method | Description |
| -------- | ------------- |
| `void insert(std::u32string_view text)` | Insert UTF-32. |
| `void insert(std::string_view utf8)` | Insert UTF-8. |
| `void backspace()` | Delete the char before the cursor. |
| `void deleteForward()` | Delete the char after the cursor. |
| `void deleteSelection()` | Delete the selection. |
| `void backspaceWord()` | Delete the word before the cursor. |
| `void deleteWordForward()` | Delete the word after the cursor. |

### Undo / redo

| Method | Description |
| -------- | ------------- |
| `void undo()` | Undo the last edit. |
| `void redo()` | Redo the last undone edit. |
| `bool canUndo() const` / `bool canRedo() const` | Availability checks. |
| `void setUndoLimit(size_t limit)` / `size_t undoLimit() const` | History limit (default 200). |
| `void clearUndoHistory()` | Clear history. |

### Selection rectangles

| Method | Description |
|--------|-------------|
| `std::vector<std::pair<float,float>> computeSelectionRects(float originX) const` | Single-line selection rectangles. |

### Multiline support

| Method | Description |
| -------- | ------------- |
| `void setMultiline(bool on)` / `bool isMultiline() const` | Toggle multiline. |
| `void setLineHeight(float h)` / `float lineHeight() const` | Line height. |
| `size_t lineCount() const` | Number of lines. |
| `const LineInfo& lineInfo(size_t i) const` | Line metadata. |
| `size_t lineOfLogical(size_t pos) const` | Line containing `pos`. |
| `CursorPix cursorPix(float w, float pad) const` | Cursor position (pixels). |
| `std::vector<SelectionRect> computeSelectionRectsMulti(float w, float pad) const` | Multiline selection rectangles. |
| `void moveUpLine(float w, float pad)` / `void moveDownLine(float w, float pad)` | Line-wise navigation. |

### Diagnostics

| Method | Description |
|--------|-------------|
| `std::string formatState() const` | Textual snapshot of the current state. |

---

## `bidi::imbidi::InputText`

ImGui widget (only when `BIDI_BUILD_IMGUI=ON`). Wraps an
`InputTextModel` and draws/edits the text using ImGui.

### Construction

```cpp
bidi::imbidi::InputText input(
    bool multiline = false,
    bidi::imbidi::BaseLevel base_level = bidi::imbidi::BaseLevel::Auto,
    size_t undo_limit = 200);
```

### Text

| Method | Description |
| -------- | ------------- |
| `void setText(std::u32string_view)` / `void setText(std::string_view)` | Set text. |
| `std::u32string_view text() const` | UTF-32 view. |
| `std::string textUtf8() const` | UTF-8 copy. |

### State

| Method | Description |
| -------- | ------------- |
| `size_t cursor() const` | Cursor position. |
| `size_t selectionStart() const` / `size_t selectionEnd() const` | Selection. |
| `bool hasSelection() const` | Selection active. |
| `bool focused() const` | Widget has focus. |
| `bool editing() const` | Widget is in edit mode. |
| `void selectAll()` / `void clearSelection()` | Selection control. |

### Configuration

| Method | Description |
| -------- | ------------- |
| `void setBaseLevel(BaseLevel)` / `BaseLevel baseLevel() const` | Base level. |
| `void setMultiline(bool)` / `bool isMultiline() const` | Multiline mode. |
| `void setLineHeight(float)` / `float lineHeight() const` | Line height. |
| `void setUndoLimit(size_t)` / `size_t undoLimit() const` | Undo limit. |

### Model access

| Method | Description |
|--------|-------------|
| `bidi::InputTextModel& model()` / `const bidi::InputTextModel& model() const` | Access the underlying model. |

### Rendering

```cpp
void render(const char* id, ImVec2 size = ImVec2(0, 0));
```

Draw the widget. `size = (0, 0)` uses the default size.

---

## `bidi::AnalysisResult`

### Fields

| Field | Type | Description |
| ------- | ------ | ------------- |
| `levels` | `vector<uint8_t>` | Per-character embedding level. |
| `classes` | `vector<uint8_t>` | Per-character `Bidi_Class`. |
| `directions` | `vector<Direction>` | Per-character resolved direction. |
| `mirrored` | `vector<uint8_t>` | 1 if the character should be mirrored. |
| `invisibleIndices` | `vector<uint32_t>` | Indices of characters removed by X9. |
| `runs` | `vector<Run>` | Visual runs in display order. |
| `visualToLogical` | `vector<uint32_t>` | Visual position → logical index. |
| `logicalToVisual` | `vector<uint32_t>` | Logical index → visual position. |
| `paragraphLevel` | `int` | Paragraph level of the first paragraph. |
| `paragraphLevels` | `vector<int>` | Per-character paragraph level. |
| `paragraphStart` | `vector<uint32_t>` | Paragraph boundaries. |

### Methods

| Method | Description |
|--------|-------------|
| `std::string toVisualString(std::string_view logicalText, bool applyMirroring = true) const` | Build UTF-8 visual string. |
| `std::u32string toVisualStringUtf32(std::u32string_view logicalText, bool applyMirroring = true) const` | Build UTF-32 visual string. |

---

## `bidi::Run`

| Field | Type | Description |
| ------- | ------ | ------------- |
| `logicalStart` | `size_t` | Start index in logical order (UTF-32). |
| `logicalLength` | `size_t` | Length in logical order (UTF-32). |
| `level` | `int` | Embedding level (even = LTR, odd = RTL). |
| `direction` | `Direction` | `Direction::LTR` or `Direction::RTL`. |
| `containsInvisible` | `bool` | True if the run contains removed characters. |

---

## `bidi::Direction`

```cpp
enum class Direction : uint8_t { LTR, RTL, Neutral };
```

---

## `bidi::imbidi::BaseLevel`

```cpp
enum class BaseLevel : int {
    Auto = -1,   // Auto-detect from the text (P2/P3).
    LTR  = 0,    // Force left-to-right.
    RTL  = 1     // Force right-to-left.
};
```
