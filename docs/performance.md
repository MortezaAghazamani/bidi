# Performance

This document describes the performance characteristics and
optimizations of the `bidi` library.

## Design goals

- **Zero external dependencies** — no ICU, no Windows APIs for the
  core.
- **Fast for typical text** — most texts are short paragraphs.
- **Incremental editing** — text editors should not re-analyze the
  whole text on every keystroke.
- **Low memory overhead** — per-paragraph caches are small.
- **Portability** — works on Windows, Linux, and macOS without
  `#ifdef` for `sizeof(wchar_t)`.

## Internal encoding

The text is stored internally as **UTF-32** (`std::u32string`).

This gives:

- **O(1) access** to each code point (no surrogate handling).
- **Portability** (`char32_t` is exactly 4 bytes everywhere).
- **Simple processing** (no UTF-16 surrogate logic in the algorithm).
- **Direct compatibility** with the algorithm's `vector<char32_t>`
  interface (no conversion at the analysis boundary).

UTF-8 is used for input and output:

- `setText(std::string_view)` decodes UTF-8 once (table-based
  decoder).
- `textView()` / `text()` / `toVisualString()` encode from a lazily
  built UTF-8 cache.
- `insert` / `replace` with UTF-8 decode the new text and store it
  as UTF-32.

The UTF-8 cache is invalidated whenever the text changes.

### Why not UTF-16?

UTF-16 was considered and rejected:

- It requires surrogate-pair handling throughout the algorithm.
- It is not portable (`wchar_t` is 16-bit on Windows, 32-bit on
  Linux/macOS).
- Users ultimately need UTF-8 (for I/O and rendering); UTF-16 is an
  extra conversion layer with no benefit.

UTF-32 is the simplest and most portable choice.

### Why not UTF-8 internally?

UTF-8 is variable-length (1–4 bytes per code point), which makes
random access O(n) and complicates the algorithm. UTF-8 is ideal for
storage and I/O but not for processing.

## Caching strategy (BidiObject)

### Per-paragraph cache

The `BidiObject` caches analysis per paragraph. Each paragraph has:

- `text` — the paragraph's UTF-32 text.
- `analysis` — the paragraph's `ParagraphAnalysis` (levels, types,
  runs, etc.).
- `cachedResult` — the `AnalysisResult` for the paragraph (built
  lazily).

**No separate UTF-32 code point vector** is needed (unlike a UTF-16
design), because the paragraph text is already UTF-32.

### Incremental editing

When `insert`/`erase`/`replace` is called:

1. The paragraph structure is rebuilt (split at `B`).
2. Old paragraph caches are matched by text content using an
   `unordered_map`.
3. Paragraphs with unchanged text reuse their analysis.
4. Only new or modified paragraphs are re-analyzed.

**Complexity:** O(P) for the lookup, where P = number of paragraphs.
Analysis cost is O(N) for the affected paragraphs only.

For a text editor, typing a character in one paragraph re-analyzes
only that paragraph. Paragraphs 1..P-1 reuse their cached results.

### Whole-text cache

`result()` builds the global `AnalysisResult` from per-paragraph
results. This is O(N) in the total text length, but it is computed
lazily and cached. After an edit, the global cache is invalidated,
but per-paragraph caches are preserved.

### UTF-8 cache

The UTF-8 representation of the whole text is built lazily when
`textView()`, `text()`, or `paragraphTextView(i)` is first called.
`utf8ParagraphStart` records the byte offset of each paragraph in the
cached UTF-8 string, so `paragraphTextView(i)` is O(1).

The UTF-8 cache is invalidated whenever the text changes.

### Paragraph boundaries cache

`paragraphStart` is maintained in `Impl` and updated whenever the
paragraph structure is rebuilt. `paragraphContaining(pos)` uses
binary search over this array: O(log P).

## Caching strategy (ShapedText)

`ShapedText` wraps a `BidiObject` and caches the **shaped visual
string** (UTF-8 and UTF-32):

- The cache is built lazily on first call to
  `toShapedVisualString()` / `toShapedVisualStringUtf32()`.
- The underlying `BidiObject` analysis is reused (not recomputed).
- The cache is invalidated whenever the text changes.

**Complexity:** O(N) for the first build, O(1) after.

Two precomputed arrays make the shaping pass O(N):

- `prevNonT[i]` — the index of the previous non-transparent
  character before `i` (or −1).
- `nextNonT[i]` — the index of the next non-transparent character
  after `i` (or −1).

This avoids the O(N²) that would result from scanning backward and
forward for every character.

## Caching strategy (InputTextModel)

`InputTextModel` builds on `ShapedText` and maintains its own
**metrics cache**:

- Shaped visual string (`visual()`, `visualUtf8()`).
- Visual ↔ logical boundary mappings.
- Per-visual-slot x-offsets (`xPos`) and total width.
- Per-line data in multiline mode.

### Invalidation

Any edit invalidates the metrics. The metrics are rebuilt lazily on
the next query. Character-width queries go through the caller's
`MeasureWidthFn` (which, in the ImGui widget, is a thin wrapper
around the active `ImFont`).

### Complexity

Metrics rebuild: O(N) in the number of characters. Queries
(`xPos`, `logicalToVisual`, `visualToLogical`) are O(1) after the
rebuild.

## Memory usage

For a text with N code points and P paragraphs:

### BidiObject

- `text` (UTF-32): 4 × N bytes.
- Per-paragraph cache: ~200-300 bytes per paragraph (plus `analysis`
  arrays, roughly 20 bytes per code point).
- UTF-8 cache: ~1-4 × N bytes (built lazily).
- Global result (built lazily): ~20 × N bytes.

### ShapedText

- Shaped visual string: 4 × N bytes (UTF-32) + ~1-4 × N bytes
  (UTF-8 cache).
- Two `int32_t` arrays of size N (`prevNonT`, `nextNonT`): 8 × N
  bytes.
- The underlying `BidiObject` cache (see above).

### InputTextModel

- The underlying `ShapedText` cache (see above).
- Mapping arrays: ~8 × N bytes.
- x-offsets: 4 × (N+1) bytes.
- Undo stack: `undoLimit` × (4 × N + 24) bytes.

**Note:** the global result, UTF-8 cache, and shaped caches are only
built when the corresponding API is called. If the application uses
only `paragraphResult()` / `paragraphTextViewUtf32()`, the global
result is never built.

For a 1 MB text (in UTF-8), typical usage:

- Text: ~4 MB (UTF-32).
- Per-paragraph analysis: ~20 MB.
- UTF-8 cache: ~1 MB.
- Global result: ~20 MB (if `result()` is called).
- Shaped cache: ~4-8 MB.
- InputTextModel mappings: ~12 MB.

`clearCaches()` frees the analysis and UTF-8 caches, keeping only
`text`.

## Future optimizations

- **Memory optimization** — for Imbidi::InputText
- **Per-line cache** — for scrolling editors.
- **Incremental rebuild** — only rescan from the first affected
  paragraph instead of the whole text.
- **`std::pmr` allocators** — reduce allocation overhead.
- **Combined SIMD + OpenMP approach:** — Combine thread-level
  parallelism (OpenMP) with data-level parallelism (SIMD) to maximize
  CPU utilization and throughput for performance-critical kernels.
