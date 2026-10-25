// bidi/include/bidi/imbidi/input_text.h
//
// ImGui InputText widget for bidi-aware text editing.
//
// This header is only available when the library is built with
// BIDI_BUILD_IMGUI=ON (i.e., vcpkg install bidi[imgui]).
//
// The widget depends on ImGui and on bidi::InputTextModel.
//
#pragma once

#include "bidi/input_model.h"
#include "imgui.h"
#include <imgui_internal.h>
#include <string>
#include <string_view>

namespace bidi::imbidi {

    /**
     * @brief Base level for a paragraph or widget.
     */
    enum class BaseLevel : int {
        Auto = -1,   ///< Auto-detect from the text (P2/P3).
        LTR = 0,   ///< Force left-to-right.
        RTL = 1    ///< Force right-to-left.
    };

    /**
     * @brief Bidi-aware InputText widget for ImGui.
     *
     * Stores text as UTF-32 (logical order), provides bidi analysis and
     * Arabic/Persian shaping (via bidi::InputTextModel), and renders
     * using ImGui.
     *
     * Features:
     *  - Full UAX #9 bidi (LTR / RTL / mixed).
     *  - Arabic/Persian shaping (including transparent combining marks).
     *  - Single-line and multiline.
     *  - Selection (shift+arrow, drag, double / triple / quadruple click).
     *  - Undo/redo (Ctrl+Z, Ctrl+Y, with key repeat).
     *  - Horizontal and vertical scrolling.
     *  - Cursor/selection navigation (logical, visual, word-wise).
     *  - Copy / paste / cut.
     *  - Tab navigation (single-line) and Enter (multiline).
     *
     * @section usage Usage
     * @code
     *   // Simple (defaults: single-line, auto base level, undo limit 200).
     *   static bidi::imbidi::InputText w1;
     *
     *   // With configuration at construction.
     *   static bidi::imbidi::InputText w2(
     *       true,                          // multiline
     *       bidi::imbidi::BaseLevel::RTL,  // base level
     *       100                            // undo limit
     *   );
     *
     *   // In the frame:
     *   w1.render("##input", ImVec2(400, 0));
     * @endcode
     *
     * @section threading Thread safety
     * Not thread-safe. Use one object per thread.
     */
    class InputText {
    public:
        /**
          * @brief Construct an InputText widget.
          *
          * @param multiline   Enable multiline mode (default: false).
          * @param base_level  Base level: Auto, LTR, or RTL
          *                    (default: Auto).
          * @param undo_limit  Maximum undo history size (default: 200).
          */
        explicit InputText(bool multiline = false,
            BaseLevel base_level = BaseLevel::Auto,
            size_t undo_limit = 200);

        /// Destructor.
        ~InputText();

        /// Move constructor.
        InputText(InputText&&) noexcept;

        /// Move assignment.
        InputText& operator=(InputText&&) noexcept;

        /// Copy is disabled.
        InputText(const InputText&) = delete;

        /// Copy is disabled.
        InputText& operator=(const InputText&) = delete;

        // =================================================================
        // Text
        // =================================================================

        /// Set text from UTF-32 (logical order). Cursor moves to end.
        void setText(std::u32string_view text);

        /// Set text from UTF-8 (logical order). Cursor moves to end.
        void setText(std::string_view utf8);

        /// Returns a view of the text (UTF-32).
        std::u32string_view text() const { return m_model.text(); }

        /// Returns a copy of the text (UTF-8).
        std::string textUtf8() const { return m_model.textUtf8(); }

        // =================================================================
        // Cursor / selection (logical indices)
        // =================================================================

        /// Current cursor position (logical index, 0..n).
        size_t cursor() const { return m_model.cursor(); }

        /// Selection start (logical).
        size_t selectionStart() const { return m_model.selStart(); }

        /// Selection end (logical).
        size_t selectionEnd() const { return m_model.selEnd(); }

        /// True if there is an active selection.
        bool hasSelection() const { return m_model.hasSelection(); }

        /// True if the widget currently has edit focus.
        bool focused() const { return m_focused; }

        /// True if the widget is currently in edit mode.
        bool editing() const { return m_editMode; }

        /// Select the entire text.
        void selectAll() { m_model.selectAll(); resetBlink(); }

        /// Clear the selection.
        void clearSelection() { m_model.clearSelection(); }

        // =================================================================
        // Base level
        // =================================================================

        /**
         * @brief Set the base level.
         *
         * @param level Auto (detect), LTR (force), or RTL (force).
         */
        void setBaseLevel(BaseLevel level);

        /// Current base level (Auto, LTR, or RTL).
        BaseLevel baseLevel() const;

        // =================================================================
        // Multiline
        // =================================================================

        /// Enable or disable multiline mode.
        void setMultiline(bool on);

        /// True if multiline is enabled.
        bool isMultiline() const;

        // =================================================================
        // Line height
        // =================================================================

        /**
         * @brief Set the line height (pixels).
         *
         * @param h Line height. 0 = auto (ImGui text line height).
         */
        void setLineHeight(float h) { m_model.setLineHeight(h); }

        /// Current line height (0 = auto).
        float lineHeight() const { return m_model.lineHeight(); }

        // =================================================================
        // Undo limit
        // =================================================================

        /// Set the maximum undo history size (default: 200).
        void setUndoLimit(size_t limit) { m_model.setUndoLimit(limit); }

        /// Current maximum undo history size.
        size_t undoLimit() const { return m_model.undoLimit(); }

        // =================================================================
        // Model access (advanced)
        // =================================================================

        bidi::InputTextModel& model() { return m_model; }
        const bidi::InputTextModel& model() const { return m_model; }

        // =================================================================
        // Render
        // =================================================================

        /**
         * @brief Render the widget.
         *
         * @param id    ImGui label (must be unique).
         * @param size  Widget size. If (0, 0), uses `defaultSize()`.
         */
        void render(const char* id, ImVec2 size = ImVec2(0, 0));

    private:
        bidi::InputTextModel m_model;

        // Focus / interaction state.
        bool   m_focused = false;
        bool   m_editMode = false;
        bool   m_needsFocus = false;
        bool   m_dragging = false;
        int    m_dragAnchor = -1;
        double m_blinkReset = 0.0;

        // Click tracking.
        double m_lastClickTime = -1.0;
        float  m_lastClickX = 0.0f;
        bool   m_tripleClickPending = false;
        bool   m_quadClickPending = false;
        bool   m_justEnteredEdit = false;

        // Scrolling.
        float  m_scrollX = 0.0f;
        float  m_scrollY = 0.0f;
        float  m_lastWidth = 0.0f;
        float  m_lastPaddingX = 4.0f;

        // Auto-scroll trigger.
        bool   m_cursorMovedByKey = false;

        // Nav tracking.
        bool m_wasNavFocused = false;
        bool m_lastNavWasTab = false;
        bool m_lastNavWasUpDown = false;
        int  m_lastNavFrame = -100;

        void resetBlink() { m_blinkReset = ImGui::GetTime(); }

        // Input handling.
        void handleInput();
        void handleMouseDrag(ImVec2 textOrigin);
        void handleDoubleClick(ImVec2 textOrigin);
        void copySelection();
        void cutSelection();
        void pasteClipboard();

        // Drawing helpers.
        void drawSelection(ImDrawList* dl, ImVec2 origin, float lineH) const;
        void drawCursor(ImDrawList* dl, ImVec2 origin, float lineH) const;

        /// Convert mouse position to logical index (multiline-aware).
        size_t logicalPosFromMouse(ImVec2 mousePos, ImVec2 textOrigin,
            float widgetWidth) const;

        /// Measure width using the active ImGui font.
        static float measureWidthImGui(const std::u32string& s);
    };

} // namespace bidi::imbidi
