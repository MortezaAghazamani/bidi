// bidi/include/bidi/input_model.h
//
// InputTextModel: pure editing logic for a bidi-aware text input.
//
// This class has NO dependency on ImGui or any GUI framework. It
// stores the text as UTF-32, maintains cursor and selection in
// logical order, and exposes the visual order (via the underlying
// ShapedText) for rendering.
//
// Typical usage:
//
//   bidi::InputTextModel model;
//   model.setText("درود Hello دنیا");
//   model.setCursor(0);
//   model.moveVisual(+1, /*select=*/false);
//   model.insert("!");
//   ...
//
// For rendering, the caller should use the visual queries:
//   - visual(), visualUtf8()  : the shaped visual string.
//   - xPos(v), visualCount()  : per-visual-slot x offsets.
//   - logicalToVisual(L),
//     visualToLogical(V)      : mapping between orders.
//   - computeSelectionRects(),
//     computeSelectionRectsMulti()
//                              : selection rectangles in pixel space.
//
// For a complete list of features, see the public methods below.
//
#pragma once

#include "bidi/shaped_text.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bidi {

/**
 * @brief Pure editing logic for a bidi-aware text input.
 *
 * Stores text as UTF-32 (logical order). Cursor and selection are
 * logical indices. Provides visual queries (via ShapedText) and
 * editing operations (insert, delete, cursor movement, undo/redo,
 * multiline support).
 *
 * @section dependencies Dependencies
 * This class depends only on `bidi::ShapedText` (which depends only
 * on `bidi::BidiObject`). It has NO dependency on ImGui or any GUI
 * framework, so it can be reused with any renderer.
 *
 * @section thread_safety Thread safety
 * Not thread-safe. Use one instance per thread.
 */
class InputTextModel {
public:
    InputTextModel() = default;

    // =================================================================
    // Text management
    // =================================================================

    /**
     * @brief Set the text from UTF-32.
     *
     * Invalidates all caches (visual string, mappings, lines).
     * Cursor is placed at the end.
     */
    void setText(std::u32string_view text);

    /**
     * @brief Set the text from UTF-8.
     */
    void setText(std::string_view utf8);

    /// Returns a view of the logical text (UTF-32).
    std::u32string_view text() const { return m_shaped.textViewUtf32(); }

    /// Returns a copy of the logical text (UTF-8).
    std::string textUtf8() const;

    // =================================================================
    // Cursor / selection (logical indices)
    // =================================================================

    /// Current cursor position (logical index, 0..n).
    size_t cursor()     const { return m_cursor; }
    /// Selection start (logical).
    size_t selStart()   const { return m_selStart; }
    /// Selection end (logical).
    size_t selEnd()     const { return m_selEnd; }
    /// True if there is an active selection.
    bool   hasSelection() const { return m_selStart != m_selEnd; }

    /**
     * @brief Move the cursor to a logical position.
     *
     * Clears the current selection. Clamps to [0, n].
     */
    void setCursor(size_t logicalPos);

    /// Clears the selection (collapses to cursor).
    void clearSelection() { m_selStart = m_selEnd = m_cursor; }

    /// Selects the entire text.
    void selectAll();

    /**
     * @brief Set the selection range.
     *
     * Both endpoints are logical. The cursor is placed at `selEnd`.
     */
    void setSelection(size_t selStart, size_t selEnd);

    /**
     * @brief Select the word containing (or adjacent to) `logicalPos`.
     *
     * If `logicalPos` is on a separator, the next word is selected.
     */
    void selectWordAt(size_t logicalPos);

    // =================================================================
    // Visual queries (single-line)
    // =================================================================

    /// Shaped visual string (UTF-32). Cached.
    std::u32string_view visual() const;

    /// Shaped visual string (UTF-8). Cached.
    std::string_view    visualUtf8() const;

    /// x-offset (in pixels) of visual slot `v` (0..visualCount()).
    float xPos(size_t v) const;

    /// Number of visual slots (shaped characters).
    size_t visualCount() const { return m_visual.size(); }

    /// Total width of the shaped visual string (pixels).
    float totalWidth() const { return m_totalWidth; }

    /// Map logical index to visual index.
    size_t logicalToVisual(size_t logical) const;

    /// Map visual index to logical index.
    size_t visualToLogical(size_t visual) const;

    /// Paragraph level (0 = LTR, 1 = RTL).
    int paragraphLevel() const;

    // =================================================================
    // Font metrics
    // =================================================================

    /// Function type for measuring the width of a UTF-32 string.
    using MeasureWidthFn = float(*)(const std::u32string&);

    /**
     * @brief Set the function used to measure character widths.
     *
     * The model uses this function to compute `xPos()`. Typically
     * you pass a function that uses your font's `AdvanceX`.
     */
    void setWidthMeasurer(MeasureWidthFn fn) { m_measureWidth = fn; }

    /// Force metrics (visual string, mappings, xPos) to be rebuilt.
    void invalidateMetrics() { m_metricsDirty = true; }

    /// Ensure metrics are up to date (called automatically by queries).
    void ensureMetrics() const;

    // =================================================================
    // Editing operations
    // =================================================================

    /**
     * @brief Move the cursor by `delta` visual steps.
     *
     * `delta = -1` = Left arrow, `delta = +1` = Right arrow.
     * In LTR, Left = previous logical; in RTL, Left = next logical.
     * If `select`, extends the selection; else collapses.
     *
     * If there is a selection and `select` is false, the cursor
     * collapses to the visual edge (left for -1, right for +1).
     */
    void moveVisual(int delta, bool select);

    /**
     * @brief Move the cursor by `delta` words.
     *
     * Same direction rules as `moveVisual`.
     */
    void moveWord(int delta, bool select);

    /// Move the cursor to the start of the text.
    void moveHome(bool select);

    /// Move the cursor to the end of the text.
    void moveEnd(bool select);

    /// Insert text at the cursor (replaces selection).
    void insert(std::u32string_view text);

    /// Insert UTF-8 text at the cursor (replaces selection).
    void insert(std::string_view utf8);

    /// Delete the character before the cursor.
    void backspace();

    /// Delete the character after the cursor.
    void deleteForward();

    /// Delete the selection.
    void deleteSelection();

    /// Delete the word before the cursor (Ctrl+Backspace).
    void backspaceWord();

    /// Delete the word after the cursor (Ctrl+Delete).
    void deleteWordForward();

    // =================================================================
    // Configuration
    // =================================================================

    /**
     * @brief Set the maximum undo history size.
     *
     * Default is 200. When the limit is exceeded, the oldest entry
     * is discarded.
     */
    void setUndoLimit(size_t limit) { m_undoLimit = limit; }

    /// Current maximum undo history size.
    size_t undoLimit() const { return m_undoLimit; }

    // =================================================================
    // Undo / redo
    // =================================================================

    /// Undo the last edit.
    void undo();
    /// Redo the last undone edit.
    void redo();
    /// True if there is something to undo.
    bool canUndo() const { return !m_undoStack.empty(); }
    /// True if there is something to redo.
    bool canRedo() const { return !m_redoStack.empty(); }
    /// Clear the undo/redo history.
    void clearUndoHistory();

    // =================================================================
    // Underlying shaped text
    // =================================================================

    const ShapedText& shaped() const { return m_shaped; }
    ShapedText&       shaped()       { return m_shaped; }

    // =================================================================
    // Single-line selection rects
    // =================================================================

    /**
     * @brief Compute the selection rectangles for the current selection.
     *
     * Each rectangle is `[x1, x2]` relative to `originX` (pixels).
     * The selection may produce multiple rectangles when it spans
     * LTR and RTL runs.
     */
    std::vector<std::pair<float, float>>
        computeSelectionRects(float originX) const;

    // =================================================================
    // Multiline support
    // =================================================================

    /// Enable or disable multiline mode.
    void setMultiline(bool on);

    /// True if multiline mode is enabled.
    bool isMultiline() const { return m_multiline; }

    /// Set the height of each line in pixels.
    void setLineHeight(float h) {
        if (m_lineHeight != h) {
            m_lineHeight = h;
            m_linesDirty = true;
        }
    }

    /// Height of each line (pixels).
    float lineHeight() const { return m_lineHeight; }

    /// Information about one visual line (paragraph).
    struct LineInfo {
        size_t logicalStart;    ///< Logical start (inclusive).
        size_t logicalEnd;      ///< Logical end (exclusive).
        size_t visParaOffset;   ///< Global visual index of paragraph start.
        size_t visStart;        ///< Visible visual start (excl. trailing \n).
        size_t visEnd;          ///< Visible visual end (excl. trailing \n).
        float  y;               ///< Y offset from text origin.
        float  width;           ///< Visible width (pixels).
        int    paragraphLevel;  ///< 0 = LTR, 1 = RTL.
    };

    /// A rectangle in pixel space.
    struct SelectionRect { float x1, y1, x2, y2; };

    /// Cursor position in pixel space.
    struct CursorPix     { float x, y; size_t lineIndex; };

    /// Number of lines.
    size_t lineCount() const;

    /// Information about line `i`.
    const LineInfo& lineInfo(size_t i) const;

    /// The line containing `logicalPos`.
    size_t lineOfLogical(size_t logicalPos) const;

    /// Cursor position in pixel space.
    CursorPix cursorPix(float widgetWidth, float paddingX) const;

    /// Selection rectangles in pixel space (multiline aware).
    std::vector<SelectionRect>
        computeSelectionRectsMulti(float widgetWidth,
                                    float paddingX) const;

    /**
     * @brief Move cursor up one line (multiline).
     *
     * @param widgetWidth  Widget width (pixels).
     * @param paddingX     Horizontal padding (pixels).
     * @param select       If true, extend the selection (Shift+Up).
     */
    void moveUpLine(float widgetWidth, float paddingX,
        bool select = false);

    /**
     * @brief Move cursor down one line (multiline).
     *
     * @param widgetWidth  Widget width (pixels).
     * @param paddingX     Horizontal padding (pixels).
     * @param select       If true, extend the selection (Shift+Down).
     */
    void moveDownLine(float widgetWidth, float paddingX,
        bool select = false);    // =================================================================
    // Diagnostics
    // =================================================================

    /// Returns a textual snapshot of the current state (for testing).
    std::string formatState() const;

private:
    ShapedText m_shaped;

    size_t m_cursor   = 0;
    size_t m_selStart = 0;
    size_t m_selEnd   = 0;

    mutable bool                  m_metricsDirty = true;
    mutable std::u32string        m_visual;
    mutable std::string           m_visualUtf8;
    mutable std::vector<uint32_t> m_visToLog;
    mutable std::vector<uint32_t> m_logToVis;
    mutable std::vector<float>    m_xPos;
    mutable float                 m_totalWidth = 0.0f;

    MeasureWidthFn m_measureWidth = nullptr;

    struct UndoState {
        std::u32string text;
        size_t cursor, selStart, selEnd;
    };
    std::vector<UndoState> m_undoStack;
    std::vector<UndoState> m_redoStack;

    bool  m_multiline = false;
    float m_lineHeight = 0.0f;
    size_t m_undoLimit = 200;
    mutable std::vector<LineInfo> m_lines;
    mutable bool m_linesDirty = true;

    void pushUndoState();
    void clampCursor();
    void rebuildMetrics() const;
    void ensureLines() const;
    void rebuildLines() const;
};

} // namespace bidi
