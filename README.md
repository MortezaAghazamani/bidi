# bidi — C++ Unicode Bidirectional Algorithm Library

[![GitHub](https://img.shields.io/badge/GitHub-MortezaAghazamani%2Fbidi-blue?logo=github)](https://github.com/MortezaAghazamani/bidi)

A self-contained, dependency-free C++17 implementation of the
[Unicode Bidirectional Algorithm (UAX #9)](https://www.unicode.org/reports/tr9/),
with **Arabic/Persian shaping** and an optional **bidi-aware ImGui
InputText widget**.

**100% conformant** with the official Unicode conformance test
(`BidiCharacterTest.txt`): **91,707 / 91,707 tests passed**.

The primary motivation for this library is to make it possible to
**edit RTL languages (Persian, Arabic, Hebrew) in ImGui**. ImGui's
built-in `InputText` does not handle bidirectional text, cursor
movement, selection, or Arabic/Persian shaping. This library
provides all of that out of the box.

---

## Features

- **Full UAX #9 implementation**
  - P1–P3 (paragraph direction detection)
  - X1–X10 (explicit levels, isolates LRI/RLI/FSI/PDI)
  - W1–W7 (weak types)
  - N0 (bracket pairing, BD16)
  - N1–N2 (neutral types)
  - I1–I2 (implicit levels)
  - L1–L4 (reordering + mirroring)

- **Bracket pairing** with canonical equivalence
- **Mirroring** (UAX #9 L4, `BidiMirroring.txt`)
- **Isolates** (LRI/RLI/FSI/PDI)
- **Multi-paragraph** text
- **UTF-8 and UTF-32** input/output
- **UTF-32 internal storage** for O(1) code point access and
  portability (no `wchar_t` dependency)
- **Per-paragraph cache** with incremental editing
- **Arabic/Persian shaping** (`ShapedText`) for renderers without
  OpenType (including presentation forms, transparent combining
  marks, and lam-alef ligatures)
- **Bidi-aware ImGui InputText widget** (`bidi::imbidi::InputText`)
  - Single-line and multiline
  - LTR / RTL / mixed text
  - Cursor, selection (Shift+arrows, drag, double/triple/quad
    click), Home/End, word navigation
  - Undo/redo (Ctrl+Z / Ctrl+Y)
  - Copy / paste / cut
  - Horizontal and vertical scrolling
  - Auto base level, or force LTR/RTL
- **Zero external dependencies** for the core library — no ICU,
  no Windows APIs. (The ImGui widget depends on ImGui; the SDL3
  demo depends on SDL3 and OpenGL.)

---

## Quick Start

### Core library

```cpp
#include "bidi/bidi.h"
#include <cstdio>

int main() {
    bidi::BidiObject obj;
    obj.setText("Hello \u0633\u0644\u0627\u0645 world");

    std::string visual = obj.toVisualString();
    std::u32string visual32 = obj.toVisualStringUtf32();

    auto& r = obj.result();
    std::printf("paragraph level = %d\n", r.paragraphLevel);
    for (auto& run : r.runs) {
        std::printf("run: level=%d dir=%s\n",
                    run.level,
                    run.direction == bidi::Direction::LTR ? "LTR" : "RTL");
    }
    return 0;
}
```

### ShapedText (Arabic/Persian connected forms)

```cpp
#include "bidi/shaped_text.h"

bidi::ShapedText shaped;
shaped.setText("درود Hello دنیا");

// Visual string with connected Persian letters.
std::u32string visual = shaped.toShapedVisualStringUtf32();
```

### ImGui widget

```cpp
#include "bidi/imbidi/input_text.h"

static bidi::imbidi::InputText g_input;

void DrawUI() {
    ImGui::Begin("Editor");
    g_input.render("##editor", ImVec2(600, 0));
    ImGui::End();
}
```

---

## Building

```bash
# This will only buid and test the core library
mkdir build
cd build
cmake ..
cmake --build . --config Release
ctest -C Release --output-on-failure
```

### Building with VCPKG (Recomanded)

- [VCPKG_PORT](https://gist.github.com/MortezaAghazamani/36b116e09491980bfef908439c93cde9)
- Create a folder named `bidi` within the vcpkg ports directory, then download the files and place them into the folder you just created.

```bash
# By default core + imgui will be installd
vcpkg install bidi
```

### Requirements

- C++17 or later
- CMake 3.16+
- ImGui (only if `BIDI_BUILD_IMGUI=ON`)
- SDL3 + OpenGL (only for the `imgui_demo` example)

### CMake options

| Option | Default | Description |
| -------- | --------- | ------------- |
| `BIDI_BUILD_TESTS` | `ON` | Build the test suite. |
| `BIDI_BUILD_EXAMPLES` | `ON` | Build the examples. |
| `BIDI_BUILD_DOCS` | `OFF` | Build Doxygen documentation. |
| `BIDI_BUILD_IMGUI` | `OFF` | Build the bidi-aware ImGui widget (`bidi::imbidi::InputText`). |

To build with the ImGui widget:

```bash
cmake .. -DBIDI_BUILD_IMGUI=ON
```

### Unicode data files

The library needs Unicode data files to generate its tables.
Download them once:

```powershell
.\tools\download_ucd.ps1
```

Then generate the tables:

```powershell
python tools\gen_bidi_class.py UnicodeData.txt src\detail\generated\bidi_class_ranges.inc
python tools\gen_bidi_brackets.py BidiBrackets.txt src\detail\generated\bidi_brackets_ranges.inc
python tools\gen_bidi_mirroring.py BidiMirroring.txt src\detail\generated\bidi_mirroring_ranges.inc
python tools\gen_arabic_persian_shaping.py ArabicShaping.txt src\detail\generated\arabic_persian_shaping_ranges.inc
```

The generated `.inc` files are committed to the repository, so the
build does not require Python after the first generation.

---

## API Overview

### `bidi::BidiObject`

Main class for bidi analysis.

| Method | Description |
| -------- | ------------- |
| `setText(std::string_view)` | Set UTF-8 text. Invalidates caches. |
| `setText(std::u32string_view)` | Set UTF-32 text. Invalidates caches. |
| `insert(pos, ...)` / `erase(pos, len)` / `replace(pos, len, ...)` | Edit operations (UTF-8 and UTF-32 overloads). Preserve caches for unchanged paragraphs. |
| `setBaseLevel(int)` | `-1` = auto, `0` = LTR, `1` = RTL. |
| `textView()` / `textViewUtf32()` | Zero-copy views (UTF-8 / UTF-32). |
| `text()` / `textUtf32()` | Copies (UTF-8 / UTF-32). |
| `result()` | Full analysis (lazy, cached). |
| `toVisualString()` / `toVisualStringUtf32()` | Visual string with mirroring. |
| `paragraphCount()`, `paragraphTextView(i)`, `paragraphTextViewUtf32(i)`, `paragraphText(i)`, `paragraphTextUtf32(i)`, `paragraphResult(i)`, `paragraphLevel(i)`, `paragraphContaining(pos)` | Per-paragraph access. |
| `directionAt(pos)`, `levelAt(pos)`, `isMirrored(pos)`, `runAt(pos)`, `logicalToVisualAt(pos)`, `visualToLogicalAt(pos)` | Per-character queries. |
| `invalidateCache()`, `clearCaches()`, `reserve(size)` | Cache management. |

### `bidi::ShapedText`

Wraps `BidiObject`. Produces a **shaped visual string** (with Arabic/Persian presentation forms applied) for renderers without OpenType shaping.

| Method | Description |
| -------- | ------------- |
| `setText`, `insert`, `erase`, `replace`, `setBaseLevel` | Same as `BidiObject`. |
| `textView()`, `textViewUtf32()`, `text()`, `textUtf32()` | Logical text access. |
| `toShapedVisualString()` / `toShapedVisualStringUtf32()` | Shaped visual string (copy). |
| `shapedVisualView()` / `shapedVisualViewUtf32()` | Zero-copy view. |
| `bidi()` | Access the underlying `BidiObject`. |

### `bidi::InputTextModel`

Pure editing logic (no GUI dependency). Stores UTF-32 text, tracks cursor/selection, and provides visual queries via `ShapedText`.

| Method | Description |
| -------- | ------------- |
| `setText`, `insert`, `backspace`, `deleteForward` | Editing. |
| `moveVisual(delta, select)`, `moveWord(delta, select)`, `moveHome(select)`, `moveEnd(select)` | Navigation. |
| `setCursor(pos)`, `setSelection(start, end)`, `selectAll()`, `selectWordAt(pos)` | Cursor/selection. |
| `visual()`, `visualUtf8()`, `xPos(v)`, `logicalToVisual(L)`, `visualToLogical(V)` | Visual queries. |
| `computeSelectionRects(originX)` | Single-line selection rectangles. |
| `setMultiline(bool)`, `lineCount()`, `lineInfo(i)`, `cursorPix(w, pad)`, `computeSelectionRectsMulti(w, pad)`, `moveUpLine(w, pad)`, `moveDownLine(w, pad)` | Multiline support. |
| `undo()`, `redo()`, `setUndoLimit(n)` | Undo/redo. |

### `bidi::imbidi::InputText`

ImGui widget (only when `BIDI_BUILD_IMGUI=ON`).

```cpp
bidi::imbidi::InputText input(
    /*multiline=*/false,
    /*base_level=*/bidi::imbidi::BaseLevel::Auto,
    /*undo_limit=*/200);

input.setText("درود Hello دنیا");
input.render("##input", ImVec2(400, 0));
```

| Method | Description |
| -------- | ------------- |
| `setText(...)`, `text()`, `textUtf8()` | Text access. |
| `setBaseLevel(BaseLevel)`, `baseLevel()` | Base level (`Auto`, `LTR`, `RTL`). |
| `setMultiline(bool)`, `isMultiline()` | Multiline mode. |
| `setLineHeight(float)`, `lineHeight()` | Line height. |
| `setUndoLimit(size_t)`, `undoLimit()` | Undo history. |
| `cursor()`, `selectionStart()`, `selectionEnd()`, `hasSelection()`, `focused()`, `editing()` | State. |
| `selectAll()`, `clearSelection()` | Selection. |
| `render(id, size)` | Draw the widget. |

---

## Example: Text Editor with Per-Paragraph Cache

```cpp
#include "bidi/bidi.h"

bidi::BidiObject obj;
obj.setText("درود\nدنیا\nخداحافظ");

// User types a character in paragraph 0.
obj.insert(4, " ای");

// Only paragraph 0 is re-analyzed.
// Paragraphs 1 and 2 use their cached results.

auto& para0 = obj.paragraphResult(0);
renderParagraph(obj.paragraphTextViewUtf32(0), para0);
```

**How it works:** the text is split at paragraph separators (`B`).
Each paragraph has its own cache. `insert`/`erase`/`replace` preserve
caches for paragraphs whose text did not change.

---

## Example: ShapedText (Arabic/Persian)

Renderers without OpenType shaping (such as ImGui's default text
renderer) display Persian/Arabic letters in isolated form. Use
`ShapedText` to get connected forms:

```cpp
#include "bidi/shaped_text.h"

bidi::ShapedText shaped;
shaped.setText("درود Hello دنیا");

// Visual string with connected letters.
std::u32string visual = shaped.toShapedVisualStringUtf32();
```

The shaping is based on `ArabicShaping.txt` (Joining_Type) and the
Arabic Presentation Forms (U+FE70..U+FEFF, U+FB50..U+FDFF).

---

## Example: Bidi-aware ImGui InputText

```cpp
#include "bidi/imbidi/input_text.h"

static bidi::imbidi::InputText g_input(
    /*multiline=*/false,
    bidi::imbidi::BaseLevel::Auto,
    /*undo_limit=*/200);

void DrawUI() {
    ImGui::Begin("Editor");
    ImGui::Text("Type Persian, Arabic, Hebrew, or English:");

    g_input.render("##editor", ImVec2(600, 0));

    ImGui::Text("Cursor: %zu | Selection: %zu..%zu",
                g_input.cursor(),
                g_input.selectionStart(),
                g_input.selectionEnd());
    ImGui::End();
}
```

The widget supports:

- **Typing** in any language (UTF-8 input from ImGui).
- **Cursor movement** with Left/Right (visual) and Ctrl+Left/Right
  (word-wise).
- **Selection** with Shift+arrows, mouse drag, and double/triple/quad
  click.
- **Home/End**, **Backspace/Delete**, **Ctrl+A/C/X/V**.
- **Undo/redo** with Ctrl+Z / Ctrl+Y.
- **Multiline** editing with Up/Down and Enter.
- **Auto base level** (or force LTR/RTL).

---

## Encoding

The library stores text internally as **UTF-32** (`std::u32string`).
This gives:

- **O(1) access** to each code point (no surrogate pairs).
- **Portability** (`char32_t` is 4 bytes everywhere; no `wchar_t`
  dependency).
- **Simple processing** (no UTF-16 surrogate handling).

Input and output are available as both UTF-8 and UTF-32.

**All `pos` parameters are in UTF-32 code units** (one unit per
Unicode code point).

---

## Views vs. copies

Methods ending in `View` return zero-copy views. They are fast but
become invalid after any text modification.

Methods without `View` return copies. They are safe to store but
allocate memory.

```cpp
// Fast, but invalidated by modification:
std::u32string_view v = obj.textViewUtf32();

// Safe, but copies:
std::u32string s = obj.textUtf32();
```

---

## Conformance

This library passes **all 91,707 tests** in the official Unicode
`BidiCharacterTest.txt`, including:

- Brackets (including canonical equivalence)
- Isolates (LRI/RLI/FSI/PDI)
- Numbers (EN/AN)
- Explicit formatting codes (RLE/LRE/RLO/LRO/PDF/BN)
- Multi-paragraph text
- Deeply nested brackets (BD16 max depth overflow)

Run the conformance test with:

```bash
./build/Debug/test_unicode_conformance tests/data/BidiCharacterTest.txt
```

---

## How It Works

The library follows UAX #9 step by step:

1. **Input** is converted to UTF-32 (from UTF-8 if needed).
2. **Paragraph splitting** at `B`.
3. For each paragraph:
   1. **P2/P3**: detect paragraph level.
   2. **X1–X8**: compute explicit levels and override statuses.
   3. **X9**: mark removed characters.
   4. **X10**: build isolating run sequences.
   5. For each sequence:
      1. **W1–W7**: resolve weak types.
      2. **N0**: process bracket pairs (BD16).
      3. **N1–N2**: resolve neutral types.
      4. **I1–I2**: compute implicit levels.
   6. **L1**: reset levels of separators and trailing whitespace.
   7. **L2**: reorder characters by descending level.
   8. **L4**: mark mirrored characters.
4. **Merge paragraphs** into a global visual order.
5. **Output** is produced as UTF-8 or UTF-32.

**Shaping** (Arabic/Persian presentation forms) is applied on top of
the visual order by `ShapedText`, using the Joining_Type property
from `ArabicShaping.txt`. See [`docs/algorithm.md`](docs/algorithm.md)
for details.

---

## Project Structure

```
bidi/
├── include/bidi/
│   ├── bidi.h                   # BidiObject, AnalysisResult, Run
│   ├── shaped_text.h            # ShapedText (Arabic/Persian shaping)
│   ├── input_model.h            # InputTextModel (pure editing logic)
│   ├── text_utils.h             # unshapeToLogical, shapeToVisual
│   └── imbidi/
│       └── input_text.h         # bidi::imbidi::InputText (ImGui widget)
├── src/
│   ├── bidi.cpp
│   ├── shaped_text.cpp
│   ├── input_model.cpp
│   ├── text_utils.cpp
│   ├── imbidi/
│   │   └── input_text.cpp
│   └── detail/
│       ├── utf.{h,cpp}
│       ├── bidi_class_table.{h,cpp}
│       ├── bidi_brackets_table.{h,cpp}
│       ├── bidi_mirroring_table.{h,cpp}
│       ├── arabic_persian_shaping_table.{h,cpp}
│       ├── bracket_pairer.{h,cpp}
│       ├── explicit_levels.{h,cpp}
│       ├── weak_types.{h,cpp}
│       ├── neutral_types.{h,cpp}
│       ├── implicit_levels.{h,cpp}
│       ├── reorder.{h,cpp}
│       ├── bidi_algorithm.{h,cpp}
│       └── generated/           # Generated .inc files
├── tests/
│   ├── test_*.cpp
│   └── data/BidiCharacterTest.txt
├── examples/
│   ├── basic.cpp
│   ├── editor.cpp
│   └── imgui_demo.cpp           # SDL3 + OpenGL3 ImGui demo
└── tools/
    ├── download_ucd.ps1
    ├── gen_bidi_class.py
    ├── gen_bidi_brackets.py
    ├── gen_bidi_mirroring.py
    └── gen_arabic_persian_shaping.py
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.

Copyright (c) 2026 Morteza Aghazamani

---

## Future Work

- **ImGui-style widget API:** Refactor from a class-based design to a
function-based API, allowing users to use it directly like other ImGui
widgets without instantiating an object.

---

## Project Document

- [Api](docs/api.md)
- [Algorithm](docs/algorithm.md)
- [Performance](docs/performance.md)

---

## References

- [UAX #9: Unicode Bidirectional Algorithm](https://www.unicode.org/reports/tr9/)
- [Unicode Character Database](https://www.unicode.org/Public/UCD/latest/ucd/)
- [BidiCharacterTest.txt](https://www.unicode.org/Public/UCD/latest/ucd/BidiCharacterTest.txt)
- [BidiMirroring.txt](https://www.unicode.org/Public/UCD/latest/ucd/BidiMirroring.txt)
- [ArabicShaping.txt](https://www.unicode.org/Public/UCD/latest/ucd/ArabicShaping.txt)
