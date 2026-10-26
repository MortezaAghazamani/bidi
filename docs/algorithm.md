# Algorithm

This library implements the
[Unicode Bidirectional Algorithm (UAX #9)](https://www.unicode.org/reports/tr9/)
step by step, plus Arabic/Persian shaping and a bidi-aware editing
model on top.

## Pipeline

For a given text, the analysis proceeds as follows:

### 1. UTF conversion

Input is converted to **UTF-32** (`std::u32string`). The internal
representation is always UTF-32, so the algorithm operates on
`char32_t` code points directly (no surrogate pairs, no UTF-16
handling).

- UTF-8 input is decoded once (table-based decoder).
- UTF-32 input is stored as-is.

### 2. Paragraph splitting

The text is split at `B` (paragraph separator). Each paragraph is
analyzed independently.

### 3. Per-paragraph analysis

For each paragraph:

#### a. P2/P3 — Paragraph level detection

The first strong character (`L`, `R`, or `AL`) determines the
paragraph direction. Isolate initiators are skipped.

- If `L` is found first → paragraph level 0 (LTR).
- If `R` or `AL` is found first → paragraph level 1 (RTL).
- If no strong character → paragraph level 0.

#### b. X1–X8 — Explicit levels

Compute explicit levels and override statuses. The directional status
stack holds up to 125 entries (BD2).

- `RLE`, `LRE`, `RLO`, `LRO`: push a new entry.
- `RLI`, `LRI`, `FSI`: push a new isolate entry.
- `PDF`: pop the top non-isolate entry.
- `PDI`: pop until an isolate entry is removed.
- `B`: reset to paragraph level.

#### c. X9 — Removed characters

Mark removed characters (`RLE`, `LRE`, `RLO`, `LRO`, `PDF`, `BN`).
These characters still have levels (for cursor mapping) but do not
participate in W/N/I rules.

#### d. X10 — Isolating run sequences

Build isolating run sequences. A run sequence may include multiple
level runs joined through isolate initiator / PDI pairs.

#### e. Per-sequence analysis

For each sequence:

1. **W1–W7**: resolve weak types (NSM, EN, AN, ES, CS, ET, AL).
2. **N0**: process bracket pairs (BD16). Bracket pairing uses the
   canonical decomposition of brackets for comparison.
3. **N1–N2**: resolve neutral types based on surrounding strong types.
4. **I1–I2**: compute implicit levels from the resolved types.

#### f. L1 — Reset levels

Reset levels of separators and trailing whitespace to the paragraph
level.

#### g. L2 — Reorder

Reorder characters by descending level.

#### h. L4 — Mirroring

Mark mirrored characters (level is odd and the character has a mirror).

### 4. Merge paragraphs

Merge paragraphs into a global visual order.

### 5. Output

Produce the visual string as UTF-8 or UTF-32. Mirrored characters are
replaced with their mirrors (unless disabled). Removed characters
(marked in X9) are skipped.

---

## Encoding

The library uses **UTF-32** internally:

- O(1) access to each code point.
- No surrogate-pair handling.
- Portable across platforms (`char32_t` is 4 bytes everywhere; no
  `wchar_t` dependency).

UTF-8 is used for input and output:

- `setText(std::string_view)` decodes UTF-8 once.
- `textView()` / `text()` / `toVisualString()` encode from a lazily
  built UTF-8 cache.

---

## Caching

The `BidiObject` caches analysis per paragraph. When the text is
edited with `insert`/`erase`/`replace`, only the affected paragraphs
are re-analyzed. Paragraphs whose text did not change reuse their
cached results.

This is possible because UAX #9 defines the paragraph as the atomic
unit of analysis. Words and lines are **not** valid units for
re-analysis: N rules and N0 need context that may cross word or line
boundaries.

### Edit handling

When `insert`/`erase`/`replace` is called:

1. The text is modified.
2. Paragraph structure is rebuilt (split at `B`).
3. Old paragraph caches are matched by text content.
4. Paragraphs whose text is unchanged reuse their cached analysis.
5. Only new or modified paragraphs are re-analyzed.

### Per-paragraph API

For text editors, `paragraphResult(i)` returns the analysis of a
single paragraph. This is useful for rendering only the paragraphs
that changed.

---

## Mirroring

UAX #9 L4 specifies that a character with `Bidi_Mirrored = true`
should be displayed with its mirror glyph when its resolved level is
odd (RTL).

The `AnalysisResult::mirrored` array marks these characters.
`toVisualString()` / `toVisualStringUtf32()` (with `mirroring = true`)
replace them with their mirrors from `BidiMirroring.txt`.

---

## Shaping (Arabic/Persian)

After Bidi produces the visual order, `ShapedText` applies
Arabic/Persian shaping: characters are replaced with their
presentation forms (isolated, initial, medial, final) based on the
Joining_Type property from `ArabicShaping.txt`.

This is required for renderers that do not perform OpenType shaping
(such as ImGui's default text renderer). Renderers that use HarfBuzz
or DirectWrite do not need this step.

### Algorithm

1. For each character, look up its Joining_Type
   (`R`, `L`, `D`, `C`, `U`, `T`).
2. Determine whether it joins to the previous character:
   - `self ∈ {R, D, C}` AND `prev ∈ {L, D, C}`.
3. Determine whether it joins to the next character:
   - `self ∈ {L, D, C}` AND `next ∈ {R, D, C}`.
4. Select the form:
   - Both join → **medial**
   - Join prev only → **final**
   - Join next only → **initial**
   - Neither → **isolated**
5. Replace with the presentation form.

**Transparent characters** (`T` — Arabic diacritics, combining marks)
are skipped when looking for the previous/next joining type. They
keep their original code point in the output, so they render on top
of the shaped letter.

### Example

"دنیا" (dal, noon, farsi-yeh, alef):

| Char | Self | Prev | Next | Joins prev? | Joins next? | Form |
| ------ | ------ | ------ | ------ | ------------- | ------------- | ------ |
| د | R | U | D | no | no | isolated |
| ن | D | R | D | yes | yes | medial |
| ی | D | D | R | yes | yes | medial |
| ا | R | D | U | no (self=R) | no | final |

### Cache

`ShapedText` caches the shaped visual string (UTF-8 and UTF-32).
The cache is invalidated whenever the text changes.

### Usage

```cpp
bidi::ShapedText shaped;
shaped.setText("درود Hello دنیا");

// Visual string with connected forms.
std::u32string visual = shaped.toShapedVisualStringUtf32();
```

---

## Editing model (InputTextModel)

`InputTextModel` builds on `ShapedText` and provides pure editing
logic (no GUI dependency). It can be reused with any renderer.

### Responsibilities

- Stores text as UTF-32 (logical order).
- Tracks cursor and selection as **logical indices**.
- Exposes the shaped visual string and per-visual-slot x-offsets.
- Provides navigation: visual steps (Left/Right), word steps
  (Ctrl+Left/Right), Home/End, line-wise (multiline).
- Provides editing: insert, backspace, delete, word-wise delete,
  selection delete.
- Undo/redo with configurable history limit.
- Multiline support (line splitting, line info, cursor pixels,
  selection rectangles).

### Visual ↔ logical mapping

The mapping is **boundary-based**: `logicalToVisual(L)` and
`visualToLogical(V)` map boundaries (0..n) rather than characters.
This is required for correct cursor positioning at run boundaries
in mixed-direction text.

For RTL characters, the visual position of a logical boundary is
**inverted** relative to LTR (because RTL runs are displayed
right-to-left). `InputTextModel` accounts for this, so the caller can
position the cursor using `xPos(logicalToVisual(cursor))`.

### Metrics

Character widths are supplied by the caller through
`setWidthMeasurer(fn)`. The model uses it to compute the x-offset of
each visual slot. This keeps `InputTextModel` independent of any
particular font API.

### Cache invalidation

`InputTextModel` rebuilds its metrics (visual string, mapping,
x-offsets) lazily. Any edit invalidates the metrics and (in
multiline mode) the line cache.

---

## ImGui widget (imbidi::InputText)

`bidi::imbidi::InputText` wraps `InputTextModel` and renders it with
ImGui. It handles:

- Focus and edit-mode transitions (Tab / Enter / click / Escape).
- IME (SDL3 fix for `WantTextInput`).
- Keyboard input: arrows (visual), Ctrl+arrows (word-wise),
  Home/End, Backspace/Delete, Ctrl+Backspace/Delete, Ctrl+A/C/X/V,
  Ctrl+Z / Ctrl+Y.
- Mouse: click to position cursor, drag to select, double/triple/quad
  click (word / line / all).
- Scrolling: horizontal (single-line), vertical (multiline), with
  auto-scroll to keep the cursor visible.
- Multiline: Up/Down line navigation, Enter to insert a newline.
- Base level: Auto / LTR / RTL.

The widget is optional. It is only built when
`BIDI_BUILD_IMGUI=ON` (and ImGui is available).

---

## Conformance

The Bidi analysis passes all 91,707 tests in `BidiCharacterTest.txt`.
Run:

```bash
./build/Debug/test_unicode_conformance tests/data/BidiCharacterTest.txt
```
