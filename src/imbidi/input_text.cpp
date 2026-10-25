// bidi/src/imbidi/input_text.cpp
#include "bidi/imbidi/input_text.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace bidi::imbidi {

// =====================================================================
// Font width measurer
// =====================================================================
float InputText::measureWidthImGui(const std::u32string& s) {
    ImFontBaked* baked = ImGui::GetFontBaked();
    if (!baked) return 1.0f;

    float w = 0.0f;
    for (char32_t cp : s) {
        const ImFontGlyph* g = baked->FindGlyph(static_cast<ImWchar>(cp));
        if (g) w += g->AdvanceX;
    }
    return w;
}

// =====================================================================
// Constructor / destructor
// =====================================================================
InputText::InputText(bool multiline,
    BaseLevel base_level,
    size_t undo_limit) {
    m_model.setWidthMeasurer(&InputText::measureWidthImGui);

    // Apply configuration.
    if (multiline) {
        m_model.setMultiline(true);
    }
    m_model.shaped().setBaseLevel(static_cast<int>(base_level));
    m_model.setUndoLimit(undo_limit);
    m_model.invalidateMetrics();
}

InputText::~InputText() = default;
InputText::InputText(InputText&&) noexcept = default;
InputText& InputText::operator=(InputText&&) noexcept = default;

// =====================================================================
// Config
// =====================================================================

void InputText::setBaseLevel(BaseLevel level) {
    m_model.shaped().setBaseLevel(static_cast<int>(level));
    m_model.invalidateMetrics();
}

BaseLevel InputText::baseLevel() const {
    int l = m_model.shaped().baseLevel();
    if (l == 0) return BaseLevel::LTR;
    if (l == 1) return BaseLevel::RTL;
    return BaseLevel::Auto;
}

void InputText::setMultiline(bool on) {
    m_model.setMultiline(on);
    m_scrollX = 0.0f;
    m_scrollY = 0.0f;
}

bool InputText::isMultiline() const {
    return m_model.isMultiline();
}

void InputText::setText(std::u32string_view text) {
    m_model.setText(text);
    resetBlink();
}

void InputText::setText(std::string_view utf8) {
    m_model.setText(utf8);
    resetBlink();
}

// =====================================================================
// Mouse position -> logical index (multiline-aware)
// =====================================================================
size_t InputText::logicalPosFromMouse(ImVec2 mousePos, ImVec2 textOrigin,
                                       float widgetWidth) const {
    float lineH = m_model.lineHeight();
    if (lineH <= 0.0f) lineH = ImGui::GetTextLineHeight();

    size_t nLines = m_model.lineCount();
    if (nLines == 0) return 0;

    float relY = mousePos.y - textOrigin.y + m_scrollY;
    if (relY < 0.0f) relY = 0.0f;
    size_t lineIdx = static_cast<size_t>(relY / lineH);
    if (lineIdx >= nLines) lineIdx = nLines - 1;

    const auto& ln = m_model.lineInfo(lineIdx);
    if (ln.visStart >= ln.visEnd) return ln.logicalStart;

    float lineX = textOrigin.x;
    if (ln.paragraphLevel & 1)
        lineX = textOrigin.x + widgetWidth - ln.width;

    float localX = mousePos.x - lineX;
    if (localX < 0.0f) localX = 0.0f;
    if (localX > ln.width) localX = ln.width;

    const size_t nVis = m_model.visualCount();
    size_t bestVis = ln.visStart;
    float bestDist = 1e9f;
    for (size_t v = ln.visStart; v <= ln.visEnd; ++v) {
        if (v >= nVis || ln.visStart >= nVis) break;
        float xi = m_model.xPos(v) - m_model.xPos(ln.visStart);
        float d = std::fabs(xi - localX);
        if (d < bestDist) { bestDist = d; bestVis = v; }
    }
    return m_model.visualToLogical(bestVis);
}

// =====================================================================
// Render
// =====================================================================
void InputText::render(const char* id, ImVec2 size) {
    ImGui::PushID(id);
    m_model.ensureMetrics();

    // SDL3 IME fix.
    if (m_editMode) {
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.PlatformImeData.WantTextInput == false) {
            g.PlatformImeData.WantVisible = true;
            g.PlatformImeData.WantTextInput = true;
        }
    }

    const float height = size.y > 0 ? size.y
                                    : ImGui::GetTextLineHeight() + 8.0f;
    const float width  = size.x > 0 ? size.x : 200.0f;

    m_lastWidth = width;
    m_lastPaddingX = 4.0f;
    ImVec2 mp = ImGui::GetIO().MousePos;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 end(pos.x + width, pos.y + height);

    const float lineH = ImGui::GetTextLineHeight();
    m_model.setLineHeight(lineH);

    const bool isRTL = (m_model.paragraphLevel() & 1) != 0;

    // Single-line horizontal scroll clamp.
    const float maxScroll =
        std::max(0.0f, m_model.totalWidth() - width + 8.0f);
    if (isRTL) {
        if (m_scrollX < -maxScroll) m_scrollX = -maxScroll;
        if (m_scrollX >  maxScroll) m_scrollX =  maxScroll;
    } else {
        if (m_scrollX < 0.0f)       m_scrollX = 0.0f;
        if (m_scrollX > maxScroll)  m_scrollX = maxScroll;
    }

    // Multiline vertical scroll clamp.
    if (m_model.isMultiline()) {
        float totalHeight = m_model.lineCount() * lineH;
        float viewHeight = height - 8.0f;
        float maxScrollY = std::max(0.0f, totalHeight - viewHeight);
        if (m_scrollY < 0.0f) m_scrollY = 0.0f;
        if (m_scrollY > maxScrollY) m_scrollY = maxScrollY;
    }

    // Compute textOrigin (RTL-aware for single-line only).
    ImVec2 textOrigin(pos.x + 4.0f, pos.y + 4.0f);
    if (isRTL && !m_model.isMultiline()) {
        float rightEdge = pos.x + width - 4.0f;
        float newX = rightEdge - m_model.totalWidth();
        if (newX < pos.x + 4.0f) newX = pos.x + 4.0f;
        textOrigin.x = newX;
    }
    textOrigin.x -= m_scrollX;

    // Request keyboard focus only once per focus event.
    if (m_editMode) {
        ImGuiContext& g = *ImGui::GetCurrentContext();
        g.WantTextInputNextFrame = 1;
        if (m_needsFocus) {
            ImGui::SetKeyboardFocusHere();
            m_needsFocus = false;
        }
    }

    // InvisibleButton with AllowOverlap so border-resize works.
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##bidi_input", ImVec2(width, height),
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_PressedOnClick |
                           ImGuiButtonFlags_EnableNav);

    const ImGuiID myId  = ImGui::GetItemID();
    const bool    clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool    hovered = ImGui::IsItemHovered() ||
                            (mp.x >= pos.x && mp.x <= end.x &&
                             mp.y >= pos.y && mp.y <= end.y);
    const bool navFocused = ImGui::IsItemFocused();

    // -----------------------------------------------------------------
    // Focus tracking (Tab vs Up/Down).
    // -----------------------------------------------------------------
    const int currFrame = ImGui::GetFrameCount();
    if (!m_editMode) {
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
            m_lastNavWasTab = true;
            m_lastNavWasUpDown = false;
            m_lastNavFrame = currFrame;
        } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) ||
                   ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
            m_lastNavWasUpDown = true;
            m_lastNavWasTab = false;
            m_lastNavFrame = currFrame;
        }
    }

    const bool justGainedNav = navFocused && !m_wasNavFocused;
    m_wasNavFocused = navFocused;

    // -----------------------------------------------------------------
    // Focus transitions.
    // -----------------------------------------------------------------
    if (clicked) {
        m_focused    = true;
        m_editMode   = true;
        m_needsFocus = true;
        resetBlink();
    } else if (justGainedNav) {
        m_focused    = true;
        m_needsFocus = true;
        const bool recentNav = (currFrame - m_lastNavFrame) <= 3;
        if (recentNav && m_lastNavWasUpDown) {
            m_editMode = false;
        } else if (recentNav && m_lastNavWasTab) {
            m_editMode = true;
            m_model.selectAll();
            m_cursorMovedByKey = true;
            resetBlink();
        } else {
            m_editMode = false;
        }
        m_lastNavWasTab = false;
        m_lastNavWasUpDown = false;
    } else if (!navFocused && m_focused) {
        m_focused  = false;
        m_editMode = false;
        m_dragging = false;
        m_dragAnchor = -1;
    }

    // Click outside → clear focus.
    if (m_focused && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !hovered && !clicked) {
        m_focused  = false;
        m_editMode = false;
        m_dragging = false;
        m_dragAnchor = -1;
        ImGui::ClearActiveID();
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.NavId == myId) {
            g.NavId = 0;
        }
    }

    // Enter (NavFocus, not editing) → edit + select all.
    if (m_focused && !m_editMode) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            m_editMode = true;
            m_justEnteredEdit = true;
            m_model.selectAll();
            m_needsFocus = true;
            m_cursorMovedByKey = true;
            resetBlink();
        }
    }

    // Escape (editing) → leave edit mode.
    if (m_editMode && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        m_editMode   = false;
        m_needsFocus = false;
        m_model.clearSelection();
    }

    // Claim keys (only while editing).
    if (m_editMode) {
        ImGui::SetKeyOwner(ImGuiKey_LeftArrow, myId);
        ImGui::SetKeyOwner(ImGuiKey_RightArrow, myId);
        ImGui::SetKeyOwner(ImGuiKey_Home, myId);
        ImGui::SetKeyOwner(ImGuiKey_End, myId);
        ImGui::SetKeyOwner(ImGuiKey_Backspace, myId);
        ImGui::SetKeyOwner(ImGuiKey_Delete, myId);
        ImGui::SetKeyOwner(ImGuiKey_UpArrow, myId);
        ImGui::SetKeyOwner(ImGuiKey_DownArrow, myId);

        ImGuiContext& g = *ImGui::GetCurrentContext();
        g.NavCursorVisible = true;
    }

    // -----------------------------------------------------------------
    // Mouse wheel.
    // -----------------------------------------------------------------
    if (m_editMode && hovered) {
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        float wy = ImGui::GetIO().MouseWheel;
        float wh = ImGui::GetIO().MouseWheelH;
        if (m_model.isMultiline()) {
            if (wy != 0.0f) {
                m_scrollY -= wy * 30.0f;
                float totalHeight = m_model.lineCount() * lineH;
                float viewHeight = height - 8.0f;
                float maxScrollY = std::max(0.0f, totalHeight - viewHeight);
                if (m_scrollY < 0.0f) m_scrollY = 0.0f;
                if (m_scrollY > maxScrollY) m_scrollY = maxScrollY;
            }
        }
        else {
            float delta = (wy + wh) * 30.0f;
            if (delta != 0.0f) {
                m_scrollX -= delta;
                if (isRTL) {
                    if (m_scrollX < -maxScroll) m_scrollX = -maxScroll;
                    if (m_scrollX > maxScroll) m_scrollX = maxScroll;
                }
                else {
                    if (m_scrollX < 0.0f)       m_scrollX = 0.0f;
                    if (m_scrollX > maxScroll)  m_scrollX = maxScroll;
                }
            }
        }
        ImGui::GetIO().MouseWheel = 0.0f;
        ImGui::GetIO().MouseWheelH = 0.0f;
    }

    // -----------------------------------------------------------------
    // Input (only when editing). Moved before drawing so that the
    // cursor is up-to-date before we auto-scroll and draw.
    // -----------------------------------------------------------------
    if (m_editMode) {
        ImGui::GetIO().WantTextInput = true;
        handleInput();
    }

    // -----------------------------------------------------------------
    // Click handling.
    // -----------------------------------------------------------------
    if (clicked) {
        ImVec2 clickPos = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];

        double now = ImGui::GetTime();
        bool isDouble = (now - m_lastClickTime < 0.35) &&
            (std::fabs(clickPos.x - m_lastClickX) < 5.0f);
        bool isTriple = isDouble && m_tripleClickPending;
        bool isQuad = isDouble && m_quadClickPending;
        m_lastClickTime = now;
        m_lastClickX = clickPos.x;
        m_tripleClickPending = isDouble;
        m_quadClickPending = isTriple;

        if (isQuad) {
            m_model.selectAll();
            m_quadClickPending = false;
            m_tripleClickPending = false;
            m_cursorMovedByKey = true;
        }
        else if (isTriple) {
            if (m_model.isMultiline()) {
                size_t logical = logicalPosFromMouse(clickPos, textOrigin, width);
                size_t li = m_model.lineOfLogical(logical);
                const auto& ln = m_model.lineInfo(li);
                m_model.setSelection(ln.logicalStart, ln.logicalEnd);
            }
            else {
                m_model.selectAll();
            }
            m_tripleClickPending = false;
            m_cursorMovedByKey = true;
        }
        else if (isDouble) {
            handleDoubleClick(textOrigin);
            m_cursorMovedByKey = true;
        }
        else if (ImGui::GetIO().KeyShift) {
            size_t logical;
            if (m_model.isMultiline()) {
                logical = logicalPosFromMouse(clickPos, textOrigin, width);
            }
            else {
                float localX = clickPos.x - textOrigin.x;
                if (localX < 0.0f) localX = 0.0f;
                if (localX > m_model.totalWidth()) localX = m_model.totalWidth();
                const size_t nVis = m_model.visualCount();
                size_t bestVis = 0; float bd = 1e9f;
                for (size_t v = 0; v <= nVis; ++v) {
                    float d = std::fabs(m_model.xPos(v) - localX);
                    if (d < bd) { bd = d; bestVis = v; }
                }
                logical = m_model.visualToLogical(bestVis);
            }
            m_model.setSelection(m_model.selStart(), logical);
            m_dragging = false;
            m_dragAnchor = -1;
            m_cursorMovedByKey = true;
        }
        else {
            m_dragging = true;
            m_dragAnchor = -1;
            m_cursorMovedByKey = true;

            size_t logical;
            if (m_model.isMultiline()) {
                logical = logicalPosFromMouse(clickPos, textOrigin, width);
            }
            else {
                float localX = clickPos.x - textOrigin.x;
                if (localX < 0.0f) localX = 0.0f;
                if (localX > m_model.totalWidth()) localX = m_model.totalWidth();
                const size_t nVis = m_model.visualCount();
                size_t bestVis = 0; float bd = 1e9f;
                for (size_t v = 0; v <= nVis; ++v) {
                    float d = std::fabs(m_model.xPos(v) - localX);
                    if (d < bd) { bd = d; bestVis = v; }
                }
                logical = m_model.visualToLogical(bestVis);
            }
            m_model.setCursor(logical);
            m_model.setSelection(logical, logical);
            m_dragAnchor = static_cast<int>(
                m_model.logicalToVisual(m_model.cursor()));
        }
    }

    // -----------------------------------------------------------------
    // Auto-scroll Y (multiline) — only when the cursor was moved by
    // a key or a click this frame. Mouse wheel does NOT set
    // m_cursorMovedByKey, so wheel scrolling is not overridden.
    // -----------------------------------------------------------------
    if (m_model.isMultiline() && m_editMode && m_cursorMovedByKey) {
        auto cp = m_model.cursorPix(width, 4.0f);
        float viewHeight = height - 8.0f;
        float cy = cp.y - m_scrollY;
        if (cy < 0.0f) m_scrollY += cy;
        else if (cy + lineH > viewHeight)
            m_scrollY += (cy + lineH - viewHeight);
        float totalHeight = m_model.lineCount() * lineH;
        float maxScrollY = std::max(0.0f, totalHeight - viewHeight);
        if (m_scrollY < 0.0f) m_scrollY = 0.0f;
        if (m_scrollY > maxScrollY) m_scrollY = maxScrollY;
    }

    // -----------------------------------------------------------------
    // Auto-scroll X (single-line only).
    // -----------------------------------------------------------------
        if (!m_model.isMultiline() && m_editMode && m_cursorMovedByKey) {
        // Use a fresh textOrigin (based on current text) for cursorInWidget.
        // The early textOrigin was computed before handleInput, so it
        // reflects the pre-edit width.
        ImVec2 fresh(pos.x + 4.0f, pos.y + 4.0f);
        if (isRTL) {
            float rightEdge = pos.x + width - 4.0f;
            float newX = rightEdge - m_model.totalWidth();
            if (newX < pos.x + 4.0f) newX = pos.x + 4.0f;
            fresh.x = newX;
        }
        fresh.x -= m_scrollX;

        size_t v = m_model.logicalToVisual(m_model.cursor());
        float cursorX = m_model.xPos(v);
        float cursorInWidget = (fresh.x + cursorX) - pos.x;

        if (cursorInWidget < 4.0f) {
            m_scrollX -= (4.0f - cursorInWidget);
        } else if (cursorInWidget > width - 4.0f) {   // '>' not '>='
            m_scrollX += (cursorInWidget - (width - 4.0f)) + 4.0f;
        }

        if (isRTL) {
            if (m_scrollX < -maxScroll) m_scrollX = -maxScroll;
            if (m_scrollX >  maxScroll) m_scrollX =  maxScroll;
        } else {
            if (m_scrollX < 0.0f)       m_scrollX = 0.0f;
            if (m_scrollX > maxScroll)  m_scrollX = maxScroll;
        }
    }
    m_cursorMovedByKey = false;

    // Recompute textOrigin for drawing, using fresh totalWidth and scroll.
    // This mirrors multiline behavior (per-line x is computed inside the
    // draw loop after handleInput), which prevents the 1-frame overshoot
    // that caused the RTL shake in single-line mode.
    textOrigin = ImVec2(pos.x + 4.0f, pos.y + 4.0f);
    if (isRTL && !m_model.isMultiline()) {
        float rightEdge = pos.x + width - 4.0f;
        float newX = rightEdge - m_model.totalWidth();
        if (newX < pos.x + 4.0f) newX = pos.x + 4.0f;
        textOrigin.x = newX;
    }
    textOrigin.x -= m_scrollX;

    // -----------------------------------------------------------------
    // Drawing (uses current scrollY / scrollX / cursor / selection).
    // -----------------------------------------------------------------
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::RenderFrame(pos, end, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameBorderSize, 0.0f);

    dl->PushClipRect(pos, end, true);

    if (m_model.isMultiline()) {
        for (size_t i = 0; i < m_model.lineCount(); ++i) {
            const auto& ln = m_model.lineInfo(i);
            float ly = pos.y + 4.0f + ln.y - m_scrollY;
            if (ly + lineH < pos.y || ly > end.y) continue;
            if (ln.visEnd <= ln.visStart) continue;

            std::u32string lineVis = std::u32string(
                m_model.visual().substr(ln.visStart,
                    ln.visEnd - ln.visStart));

            std::string lineUtf8;
            lineUtf8.reserve(lineVis.size() * 2);
            for (char32_t c : lineVis) {
                if (c < 0x80) lineUtf8.push_back((char)c);
                else if (c < 0x800) {
                    lineUtf8.push_back((char)(0xC0 | (c >> 6)));
                    lineUtf8.push_back((char)(0x80 | (c & 0x3F)));
                }
                else if (c < 0x10000) {
                    lineUtf8.push_back((char)(0xE0 | (c >> 12)));
                    lineUtf8.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                    lineUtf8.push_back((char)(0x80 | (c & 0x3F)));
                }
                else {
                    lineUtf8.push_back((char)(0xF0 | (c >> 18)));
                    lineUtf8.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
                    lineUtf8.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                    lineUtf8.push_back((char)(0x80 | (c & 0x3F)));
                }
            }

            float lx = pos.x + 4.0f - m_scrollX;
            if (ln.paragraphLevel & 1)
                lx = pos.x + width - 4.0f - ln.width - m_scrollX;

            dl->AddText(ImVec2(lx, ly),
                ImGui::GetColorU32(ImGuiCol_Text),
                lineUtf8.c_str());
        }

        if (m_editMode && m_model.hasSelection()) {
            auto rects = m_model.computeSelectionRectsMulti(width, 4.0f);
            ImU32 selCol = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
            for (auto& r : rects) {
                dl->AddRectFilled(
                    ImVec2(pos.x + r.x1 - m_scrollX,
                        pos.y + 4.0f + r.y1 - m_scrollY),
                    ImVec2(pos.x + r.x2 - m_scrollX,
                        pos.y + 4.0f + r.y2 - m_scrollY),
                    selCol);
            }
        }

        if (m_editMode) {
            double now = ImGui::GetTime();
            bool blinkOn = std::fmod(now - m_blinkReset, 1.0) < 0.5;
            if (blinkOn) {
                auto cp = m_model.cursorPix(width, 4.0f);
                float x = pos.x + cp.x - m_scrollX;
                float y = pos.y + 4.0f + cp.y - m_scrollY;
                dl->AddLine(ImVec2(x, y),
                    ImVec2(x, y + lineH),
                    ImGui::GetColorU32(ImGuiCol_Text), 1.5f);
            }
        }
    }
    else {
        if (m_editMode && m_model.hasSelection())
            drawSelection(dl, textOrigin, lineH);

        if (!m_model.visualUtf8().empty()) {
            dl->AddText(textOrigin,
                ImGui::GetColorU32(ImGuiCol_Text),
                std::string(m_model.visualUtf8()).c_str());
        }

        if (m_editMode) {
            double now = ImGui::GetTime();
            bool blinkOn = std::fmod(now - m_blinkReset, 1.0) < 0.5;
            if (blinkOn)
                drawCursor(dl, textOrigin, lineH);
        }
    }

    dl->PopClipRect();

    // -----------------------------------------------------------------
    // Mouse drag.
    // -----------------------------------------------------------------
    if (m_dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            handleMouseDrag(textOrigin);
        }
        else {
            m_dragging = false;
            m_dragAnchor = -1;
        }
    }

    // Hide NavCursor while editing.
    if (m_editMode) {
        ImGui::GetCurrentContext()->NavCursorVisible = false;
    }

    ImGui::PopID();
}

// =====================================================================
// Input handling
// =====================================================================
void InputText::handleInput() {
    ImGuiIO& io = ImGui::GetIO();
    const bool ctrl  = io.KeyCtrl;
    const bool shift = io.KeyShift;

    // Tab / Shift+Tab: leave edit mode and let ImGui move focus.
    // (Only in single-line mode; in multiline, Tab does nothing.)
    if (m_editMode && !m_model.isMultiline() &&
        ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        m_editMode = false;
        m_needsFocus = false;
        m_dragging = false;
        m_model.clearSelection();
        ImGui::ClearActiveID();
        return;
    }

    // Enter (multiline only).
    if (m_model.isMultiline() && !m_justEnteredEdit) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, true) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, true)) {
            std::u32string nl = U"\n";
            m_model.insert(nl);
            resetBlink();
            m_cursorMovedByKey = true;
        }
    }
    m_justEnteredEdit = false;

    if (io.InputQueueCharacters.Size > 0) {
        std::u32string buf;
        buf.reserve(io.InputQueueCharacters.Size);

        for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
            ImWchar wc = io.InputQueueCharacters[i];
            char32_t cp;

            if (sizeof(ImWchar) == 2 &&
                wc >= 0xD800 && wc <= 0xDBFF &&
                i + 1 < io.InputQueueCharacters.Size) {
                ImWchar lo = io.InputQueueCharacters[i + 1];
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((wc - 0xD800) << 10) + (lo - 0xDC00);
                    ++i;
                } else {
                    cp = static_cast<char32_t>(wc);
                }
            } else {
                cp = static_cast<char32_t>(wc);
            }

            if (cp == U'\t') continue;
            if (cp == U'\r' || cp == U'\n') continue;
            if (cp >= 0x20) buf.push_back(cp);
        }
        io.InputQueueCharacters.resize(0);

        if (!buf.empty()) {
            m_model.insert(buf);
            resetBlink();
            m_cursorMovedByKey = true;
        }
    }

    if (ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelection();
        if (ImGui::IsKeyPressed(ImGuiKey_X, false)) cutSelection();
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) pasteClipboard();
        if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            m_model.selectAll();
            resetBlink();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Z, true)) {
            if (ImGui::GetIO().KeyShift) m_model.redo();
            else m_model.undo();
            resetBlink();
            m_cursorMovedByKey = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y, true)) {
            m_model.redo();
            resetBlink();
            m_cursorMovedByKey = true;
        }
    }

    if (ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
            { m_model.moveWord(-1, shift); resetBlink(); m_cursorMovedByKey = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
            { m_model.moveWord(+1, shift); resetBlink(); m_cursorMovedByKey = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
            { m_model.moveHome(shift); resetBlink(); m_cursorMovedByKey = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_End, false))
            { m_model.moveEnd(shift); resetBlink(); m_cursorMovedByKey = true; }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true))
            { m_model.backspaceWord(); resetBlink(); m_cursorMovedByKey = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, true))
            { m_model.deleteWordForward(); resetBlink(); m_cursorMovedByKey = true; }
    } else {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
            { m_model.moveVisual(-1, shift); resetBlink(); m_cursorMovedByKey = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
            { m_model.moveVisual(+1, shift); resetBlink(); m_cursorMovedByKey = true; }

        if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
            size_t target;
            if (m_model.isMultiline()) {
                size_t li = m_model.lineOfLogical(m_model.cursor());
                target = m_model.lineInfo(li).logicalStart;
            } else {
                target = 0;
            }
            if (shift) m_model.setSelection(m_model.cursor(), target);
            else       m_model.setCursor(target);
            resetBlink();
            m_cursorMovedByKey = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_End, false)) {
            size_t target;
            if (m_model.isMultiline()) {
                size_t li = m_model.lineOfLogical(m_model.cursor());
                target = m_model.lineInfo(li).logicalEnd;
                auto txt = m_model.text();
                if (target > 0 && target <= txt.size() &&
                    txt[target - 1] == U'\n') target--;
            } else {
                target = m_model.text().size();
            }
            if (shift) m_model.setSelection(m_model.cursor(), target);
            else       m_model.setCursor(target);
            resetBlink();
            m_cursorMovedByKey = true;
        }

        if (m_model.isMultiline()) {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
                m_model.moveUpLine(m_lastWidth, m_lastPaddingX);
                resetBlink();
                m_cursorMovedByKey = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
                m_model.moveDownLine(m_lastWidth, m_lastPaddingX);
                resetBlink();
                m_cursorMovedByKey = true;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true))
            { m_model.backspace(); resetBlink(); m_cursorMovedByKey = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, true))
            { m_model.deleteForward(); resetBlink(); m_cursorMovedByKey = true; }
    }
}

// =====================================================================
// Mouse drag
// =====================================================================
void InputText::handleMouseDrag(ImVec2 textOrigin) {
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_dragging = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    size_t logical;
    size_t anchorLogical;

    if (m_model.isMultiline()) {
        logical = logicalPosFromMouse(io.MousePos, textOrigin, m_lastWidth);
        if (m_dragAnchor < 0) {
            m_dragAnchor = static_cast<int>(
                m_model.logicalToVisual(m_model.cursor()));
        }
        anchorLogical = m_model.visualToLogical(
            static_cast<size_t>(m_dragAnchor));
    } else {
        float localX = io.MousePos.x - textOrigin.x;
        if (localX < 0.0f) localX = 0.0f;
        if (localX > m_model.totalWidth()) localX = m_model.totalWidth();

        const size_t nVis = m_model.visualCount();
        size_t bestVis = 0; float bd = 1e9f;
        for (size_t v = 0; v <= nVis; ++v) {
            float d = std::fabs(m_model.xPos(v) - localX);
            if (d < bd) { bd = d; bestVis = v; }
        }
        logical = m_model.visualToLogical(bestVis);

        if (m_dragAnchor < 0) {
            m_dragAnchor = static_cast<int>(bestVis);
            m_model.setSelection(logical, logical);
            resetBlink();
            m_cursorMovedByKey = true;
            return;
        }
        anchorLogical = m_model.visualToLogical(
            static_cast<size_t>(m_dragAnchor));
    }

    m_model.setSelection(anchorLogical, logical);
    m_cursorMovedByKey = true;
    resetBlink();
}

// =====================================================================
// Double click
// =====================================================================
void InputText::handleDoubleClick(ImVec2 textOrigin) {
    ImGuiIO& io = ImGui::GetIO();

    size_t logical;
    if (m_model.isMultiline()) {
        logical = logicalPosFromMouse(io.MousePos, textOrigin, m_lastWidth);
    } else {
        float localX = io.MousePos.x - textOrigin.x;
        if (localX < 0.0f) localX = 0.0f;
        if (localX > m_model.totalWidth()) localX = m_model.totalWidth();

        const size_t nVis = m_model.visualCount();
        size_t bestVis = 0; float bd = 1e9f;
        for (size_t v = 0; v <= nVis; ++v) {
            float d = std::fabs(m_model.xPos(v) - localX);
            if (d < bd) { bd = d; bestVis = v; }
        }
        logical = m_model.visualToLogical(bestVis);
    }

    m_model.selectWordAt(logical);
    m_dragging = false;
    m_dragAnchor = -1;
    resetBlink();
}

// =====================================================================
// Clipboard
// =====================================================================
void InputText::copySelection() {
    if (!m_model.hasSelection()) return;
    size_t s = std::min(m_model.selStart(), m_model.selEnd());
    size_t e = std::max(m_model.selStart(), m_model.selEnd());
    auto sub = m_model.text().substr(s, e - s);

    std::string utf8;
    utf8.reserve(sub.size() * 2);
    for (char32_t cp : sub) {
        if (cp < 0x80) {
            utf8.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            utf8.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            utf8.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    ImGui::SetClipboardText(utf8.c_str());
}

void InputText::cutSelection() {
    copySelection();
    m_model.deleteSelection();
    resetBlink();
    m_cursorMovedByKey = true;
}

void InputText::pasteClipboard() {
    const char* clip = ImGui::GetClipboardText();
    if (!clip) return;
    std::string s(clip);
    std::string normalized;
    normalized.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') continue;
            normalized.push_back('\n');
            continue;
        }
        normalized.push_back(s[i]);
    }
    m_model.insert(std::string_view(normalized));
    resetBlink();
    m_cursorMovedByKey = true;
}

// =====================================================================
// Drawing helpers
// =====================================================================
void InputText::drawSelection(ImDrawList* dl, ImVec2 origin,
    float lineH) const {
    auto rects = m_model.computeSelectionRects(origin.x);
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
    for (auto& r : rects) {
        dl->AddRectFilled(ImVec2(r.first, origin.y),
            ImVec2(r.second, origin.y + lineH),
            col);
    }
}

void InputText::drawCursor(ImDrawList* dl, ImVec2 origin,
    float lineH) const {
    size_t v = m_model.logicalToVisual(m_model.cursor());
    const size_t nVis = m_model.visualCount();
    if (v > nVis) v = nVis;

    float x = origin.x + m_model.xPos(v);
    dl->AddLine(ImVec2(x, origin.y),
        ImVec2(x, origin.y + lineH),
        ImGui::GetColorU32(ImGuiCol_Text), 1.5f);
}

} // namespace bidi::imbidi
