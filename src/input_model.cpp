// bidi/src/input_model.cpp
#include "bidi/input_model.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bidi {

// =====================================================================
// UTF-8 helper
// =====================================================================
namespace {

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string toUtf8(std::u32string_view s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char32_t cp : s) appendUtf8(out, cp);
    return out;
}

int wordClassOf(std::u32string_view txt, size_t i) {
    if (i >= txt.size()) return 0;
    char32_t c = txt[i];
    if (c == U' ' || c == U'\t' || c == U'\n' || c == U'\r') return 0;
    if ((c >= 0x0600 && c <= 0x06FF) ||
        (c >= 0x0750 && c <= 0x077F) ||
        (c >= 0xFB50 && c <= 0xFDFF) ||
        (c >= 0xFE70 && c <= 0xFEFF) ||
        (c >= 0x0590 && c <= 0x05FF))
        return 2;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
        return 1;
    return 0;
}

} // namespace

// =====================================================================
// Text management
// =====================================================================

void InputTextModel::setText(std::u32string_view text) {
    m_shaped.setText(text);
    m_cursor = text.size();
    m_selStart = text.size();
    m_selEnd = text.size();
    m_metricsDirty = true;
    clearUndoHistory();
}

void InputTextModel::setText(std::string_view utf8) {
    m_shaped.setText(utf8);
    size_t n = m_shaped.textViewUtf32().size();
    m_cursor = n;
    m_selStart = n;
    m_selEnd = n;
    m_metricsDirty = true;
    clearUndoHistory();
}

std::string InputTextModel::textUtf8() const {
    return toUtf8(m_shaped.textViewUtf32());
}

// =====================================================================
// Cursor / selection
// =====================================================================

void InputTextModel::setCursor(size_t logicalPos) {
    size_t n = m_shaped.textViewUtf32().size();
    if (logicalPos > n) logicalPos = n;
    m_cursor = logicalPos;
    m_selStart = m_selEnd = logicalPos;
}

void InputTextModel::selectAll() {
    size_t n = m_shaped.textViewUtf32().size();
    m_selStart = 0;
    m_selEnd = n;
    m_cursor = n;
}

void InputTextModel::setSelection(size_t selStart, size_t selEnd) {
    size_t n = m_shaped.textViewUtf32().size();
    if (selStart > n) selStart = n;
    if (selEnd > n) selEnd = n;
    m_selStart = selStart;
    m_selEnd = selEnd;
    m_cursor = selEnd;
}

void InputTextModel::selectWordAt(size_t logicalPos) {
    auto txt = m_shaped.textViewUtf32();
    const size_t n = txt.size();
    if (n == 0) {
        m_cursor = 0;
        m_selStart = m_selEnd = 0;
        return;
    }
    if (logicalPos > n) logicalPos = n;

    size_t anchor = logicalPos;
    if (anchor >= n) {
        if (anchor == 0) return;
        anchor = n - 1;
    }

    // If the clicked position is a separator, try to select the
    // *next* word (matching Word's behavior).
    if (wordClassOf(txt, anchor) == 0) {
        // Look forward for the next word char.
        size_t j = anchor + 1;
        while (j < n && wordClassOf(txt, j) == 0) ++j;
        if (j < n) {
            anchor = j;
        }
        else {
            // No next word; look backward.
            if (anchor > 0 && wordClassOf(txt, anchor - 1) != 0)
                anchor = anchor - 1;
            else {
                // Nothing to select; select the separator.
                m_selStart = logicalPos;
                m_selEnd = std::min(logicalPos + 1, n);
                m_cursor = m_selEnd;
                return;
            }
        }
    }

    // Expand left and right within the word.
    size_t s = anchor;
    while (s > 0 && wordClassOf(txt, s - 1) != 0) --s;
    size_t e = anchor + 1;
    while (e < n && wordClassOf(txt, e) != 0) ++e;

    m_selStart = s;
    m_selEnd = e;
    m_cursor = e;
}

// =====================================================================
// Visual queries
// =====================================================================

std::u32string_view InputTextModel::visual() const {
    ensureMetrics();
    return m_visual;
}

std::string_view InputTextModel::visualUtf8() const {
    ensureMetrics();
    return m_visualUtf8;
}

float InputTextModel::xPos(size_t v) const {
    ensureMetrics();
    if (v >= m_xPos.size()) return m_totalWidth;
    return m_xPos[v];
}

size_t InputTextModel::logicalToVisual(size_t logical) const {
    ensureMetrics();
    if (m_logToVis.empty()) return 0;
    if (logical >= m_logToVis.size()) return m_visual.size();
    return m_logToVis[logical];
}

size_t InputTextModel::visualToLogical(size_t visual) const {
    ensureMetrics();
    if (m_visToLog.empty()) return 0;
    if (visual >= m_visToLog.size()) return m_shaped.textViewUtf32().size();
    return m_visToLog[visual];
}

int InputTextModel::paragraphLevel() const {
    const auto& levels = m_shaped.bidi().result().paragraphLevels;
    return levels.empty() ? 0 : levels[0];
}

// =====================================================================
// Metrics
// =====================================================================

void InputTextModel::ensureMetrics() const {
    if (m_metricsDirty) rebuildMetrics();
}

void InputTextModel::rebuildMetrics() const {
    auto& self = const_cast<InputTextModel&>(*this);

    // Must be FIRST: any change to text/metrics invalidates lines.
    self.m_linesDirty = true;

    self.m_visual = self.m_shaped.toShapedVisualStringUtf32();
    self.m_visualUtf8 = toUtf8(self.m_visual);

    const size_t nLog = self.m_shaped.textViewUtf32().size();
    const size_t nVis = self.m_visual.size();

    const auto& r = self.m_shaped.bidi().result();

    self.m_visToLog.assign(nVis + 1, 0);
    self.m_logToVis.assign(nLog + 1, 0);

    if (nLog == 0) {
        self.m_logToVis[0] = 0;
        self.m_visToLog[0] = 0;
        self.m_metricsDirty = false;
        self.m_xPos.assign(1, 0.0f);
        self.m_totalWidth = 0.0f;
        return;
    }

    // -----------------------------------------------------------------
    // Build logical -> visual boundary mapping.
    // -----------------------------------------------------------------
    const bool paragraphRTL = (r.paragraphLevel & 1) != 0;

    auto levelOf = [&](size_t L) -> int {
        if (L < r.levels.size()) return r.levels[L];
        return 0;
        };

    for (size_t L = 0; L <= nLog; ++L) {
        if (L < nLog) {
            uint32_t v = r.logicalToVisual[L];
            int lvl = levelOf(L);
            bool charRTL = (lvl & 1) != 0;

            if (charRTL) {
                if (L == 0 || !(levelOf(L - 1) & 1)) {
                    self.m_logToVis[L] = std::min(v + 1,
                        static_cast<uint32_t>(nVis));
                }
                else {
                    self.m_logToVis[L] = r.logicalToVisual[L - 1];
                }
            }
            else {
                if (L == 0) {
                    self.m_logToVis[L] = v;
                }
                else if (levelOf(L - 1) & 1) {
                    self.m_logToVis[L] = r.logicalToVisual[L - 1];
                }
                else {
                    if (paragraphRTL) {
                        self.m_logToVis[L] =
                            std::min(r.logicalToVisual[L - 1] + 1,
                                static_cast<uint32_t>(nVis));
                    }
                    else {
                        self.m_logToVis[L] = v;
                    }
                }
            }
        }
        else {
            if (nLog == 0) {
                self.m_logToVis[L] = 0;
            }
            else {
                int lastLvl = levelOf(nLog - 1);
                bool lastRTL = (lastLvl & 1) != 0;
                if (lastRTL) {
                    self.m_logToVis[L] = r.logicalToVisual[nLog - 1];
                }
                else {
                    self.m_logToVis[L] =
                        std::min(r.logicalToVisual[nLog - 1] + 1,
                            static_cast<uint32_t>(nVis));
                }
            }
        }
    }

    // -----------------------------------------------------------------
    // Build visual -> logical inverse.
    // -----------------------------------------------------------------
    for (size_t V = 0; V <= nVis; ++V) {
        int found = -1;
        for (size_t L = 0; L <= nLog; ++L) {
            if (self.m_logToVis[L] == V) { found = static_cast<int>(L); break; }
        }
        self.m_visToLog[V] = (found < 0)
            ? static_cast<uint32_t>(nLog)
            : static_cast<uint32_t>(found);
    }

    // -----------------------------------------------------------------
    // Map X9-removed logical indices to their nearest visual neighbor.
    // -----------------------------------------------------------------
    {
        size_t lastVis = 0;
        for (size_t L = 0; L < nLog; ++L) {
            bool present = false;
            for (uint32_t v : r.visualToLogical) {
                if (v == L) { present = true; break; }
            }
            if (present) {
                lastVis = self.m_logToVis[L];
            }
            else {
                self.m_logToVis[L] = static_cast<uint32_t>(lastVis);
            }
        }
    }

    // -----------------------------------------------------------------
    // x positions.
    // -----------------------------------------------------------------
    self.m_xPos.assign(nVis + 1, 0.0f);
    float x = 0.0f;
    for (size_t v = 0; v < nVis; ++v) {
        float w = 1.0f;
        if (self.m_measureWidth) {
            std::u32string one(1, self.m_visual[v]);
            w = self.m_measureWidth(one);
        }
        x += w;
        self.m_xPos[v + 1] = x;
    }
    self.m_totalWidth = x;

    self.m_metricsDirty = false;
}

// =====================================================================
// Navigation
// =====================================================================

void InputTextModel::moveVisual(int delta, bool select) {
    ensureMetrics();

    const int nLog = static_cast<int>(m_shaped.textViewUtf32().size());
    if (nLog == 0) return;

    // If there's a selection and this is a plain arrow key, collapse
    // to the visual side matching the arrow direction.
    if (hasSelection() && !select) {
        size_t s = std::min(m_selStart, m_selEnd);
        size_t e = std::max(m_selStart, m_selEnd);
        size_t vS = logicalToVisual(s);
        size_t vE = logicalToVisual(e);
        size_t leftVis = std::min(vS, vE);
        size_t rightVis = std::max(vS, vE);
        if (delta < 0) {
            m_cursor = visualToLogical(leftVis);
        }
        else {
            m_cursor = visualToLogical(rightVis);
        }
        m_selStart = m_selEnd = m_cursor;
        return;
    }

    const bool rtl = (paragraphLevel() & 1) != 0;
    int logicalDelta = rtl ? -delta : delta;

    int c = static_cast<int>(m_cursor) + logicalDelta;
    if (c < 0) c = 0;
    if (c > nLog) c = nLog;

    size_t newCursor = static_cast<size_t>(c);
    if (select) {
        if (m_selStart == m_selEnd) m_selStart = m_cursor;
        m_selEnd = newCursor;
    }
    else {
        m_selStart = m_selEnd = newCursor;
    }
    m_cursor = newCursor;
}

void InputTextModel::moveWord(int delta, bool select) {
    auto txt = m_shaped.textViewUtf32();
    const int nLog = static_cast<int>(txt.size());
    if (nLog == 0) return;

    const bool rtl = (paragraphLevel() & 1) != 0;
    const int logicalDir = rtl ? -delta : delta;

    int i = static_cast<int>(m_cursor);

    while (i + logicalDir >= 0 && i + logicalDir <= nLog &&
        wordClassOf(txt, static_cast<size_t>(i + logicalDir)) == 0)
        i += logicalDir;
    int cls = wordClassOf(txt, static_cast<size_t>(i + logicalDir));
    if (cls != 0) {
        while (i + logicalDir >= 0 && i + logicalDir <= nLog &&
            wordClassOf(txt, static_cast<size_t>(i + logicalDir)) == cls)
            i += logicalDir;
        if (logicalDir > 0)
            i += logicalDir;
    }

    size_t newCursor = static_cast<size_t>(i);
    if (select) {
        if (m_selStart == m_selEnd) m_selStart = m_cursor;
        m_selEnd = newCursor;
    }
    else {
        m_selStart = m_selEnd = newCursor;
    }
    m_cursor = newCursor;
}

void InputTextModel::moveHome(bool select) {
    size_t newLogical = 0;
    if (select) {
        if (m_selStart == m_selEnd) m_selStart = m_cursor;
        m_selEnd = newLogical;
    }
    else {
        m_selStart = m_selEnd = newLogical;
    }
    m_cursor = newLogical;
}

void InputTextModel::moveEnd(bool select) {
    size_t newLogical = m_shaped.textViewUtf32().size();
    if (select) {
        if (m_selStart == m_selEnd) m_selStart = m_cursor;
        m_selEnd = newLogical;
    }
    else {
        m_selStart = m_selEnd = newLogical;
    }
    m_cursor = newLogical;
}

// =====================================================================
// Editing
// =====================================================================

void InputTextModel::insert(std::u32string_view text) {
    if (hasSelection()) deleteSelection();
    pushUndoState();
    m_shaped.insert(m_cursor, text);
    m_cursor += text.size();
    m_selStart = m_selEnd = m_cursor;
    m_metricsDirty = true;
    m_linesDirty = true;
}

void InputTextModel::insert(std::string_view utf8) {
    if (hasSelection()) deleteSelection();
    pushUndoState();
    size_t before = m_shaped.textViewUtf32().size();
    m_shaped.insert(m_cursor, utf8);
    size_t after = m_shaped.textViewUtf32().size();
    m_cursor += (after - before);
    m_selStart = m_selEnd = m_cursor;
    m_metricsDirty = true;
    m_linesDirty = true;
}

void InputTextModel::backspace() {
    if (hasSelection()) { deleteSelection(); return; }
    if (m_cursor == 0) return;
    pushUndoState();
    size_t prev = m_cursor - 1;
    m_shaped.erase(prev, 1);
    m_cursor = prev;
    m_selStart = m_selEnd = m_cursor;
    m_metricsDirty = true;
    m_linesDirty = true;
}

void InputTextModel::deleteForward() {
    if (hasSelection()) { deleteSelection(); return; }
    const size_t len = m_shaped.textViewUtf32().size();
    if (m_cursor >= len) return;
    pushUndoState();
    m_shaped.erase(m_cursor, 1);
    m_selStart = m_selEnd = m_cursor;
    m_metricsDirty = true;
    m_linesDirty = true;
}

void InputTextModel::deleteSelection() {
    if (!hasSelection()) return;
    pushUndoState();
    size_t s = std::min(m_selStart, m_selEnd);
    size_t e = std::max(m_selStart, m_selEnd);
    m_shaped.erase(s, e - s);
    m_cursor = s;
    m_selStart = m_selEnd = s;
    m_metricsDirty = true;
    m_linesDirty = true;
}

void InputTextModel::backspaceWord() {
    if (hasSelection()) { deleteSelection(); return; }
    if (m_cursor == 0) return;

    auto txt = m_shaped.textViewUtf32();
    pushUndoState();

    size_t i = m_cursor;
    while (i > 0 && wordClassOf(txt, i - 1) == 0) --i;
    int cls = (i > 0) ? wordClassOf(txt, i - 1) : 0;
    while (i > 0 && wordClassOf(txt, i - 1) == cls) --i;

    m_shaped.erase(i, m_cursor - i);
    m_cursor = i;
    m_selStart = m_selEnd = i;
    m_metricsDirty = true;
    m_linesDirty = true;
}

void InputTextModel::deleteWordForward() {
    if (hasSelection()) { deleteSelection(); return; }
    auto txt = m_shaped.textViewUtf32();
    const size_t nLog = txt.size();
    if (m_cursor >= nLog) return;

    pushUndoState();

    size_t i = m_cursor;
    while (i < nLog && wordClassOf(txt, i) == 0) ++i;
    int cls = (i < nLog) ? wordClassOf(txt, i) : 0;
    while (i < nLog && wordClassOf(txt, i) == cls) ++i;

    m_shaped.erase(m_cursor, i - m_cursor);
    m_selStart = m_selEnd = m_cursor;
    m_metricsDirty = true;
    m_linesDirty = true;
}

// =====================================================================
// Undo / redo
// =====================================================================

void InputTextModel::pushUndoState() {
    UndoState s;
    s.text = m_shaped.textViewUtf32();
    s.cursor = m_cursor;
    s.selStart = m_selStart;
    s.selEnd = m_selEnd;
    m_undoStack.push_back(std::move(s));
    m_redoStack.clear();
    if (m_undoLimit > 0 && m_undoStack.size() > m_undoLimit) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void InputTextModel::undo() {
    if (m_undoStack.empty()) return;

    UndoState current;
    current.text = m_shaped.textViewUtf32();
    current.cursor = m_cursor;
    current.selStart = m_selStart;
    current.selEnd = m_selEnd;
    m_redoStack.push_back(std::move(current));

    UndoState s = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    m_shaped.setText(s.text);
    m_cursor = s.cursor;
    m_selStart = s.selStart;
    m_selEnd = s.selEnd;
    m_metricsDirty = true;
}

void InputTextModel::redo() {
    if (m_redoStack.empty()) return;

    UndoState current;
    current.text = m_shaped.textViewUtf32();
    current.cursor = m_cursor;
    current.selStart = m_selStart;
    current.selEnd = m_selEnd;
    m_undoStack.push_back(std::move(current));

    UndoState s = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    m_shaped.setText(s.text);
    m_cursor = s.cursor;
    m_selStart = s.selStart;
    m_selEnd = s.selEnd;
    m_metricsDirty = true;
}

void InputTextModel::clearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

// =====================================================================
// Selection rectangles
// =====================================================================

std::vector<std::pair<float, float>>
    InputTextModel::computeSelectionRects(float originX) const {
    std::vector<std::pair<float, float>> rects;
    if (!hasSelection()) return rects;

    ensureMetrics();

    size_t logStart = std::min(m_selStart, m_selEnd);
    size_t logEnd = std::max(m_selStart, m_selEnd);

    // Use bidi's raw visualToLogical (consistent with m_visual).
    const auto& bidiVisToLog = m_shaped.bidi().result().visualToLogical;
    if (bidiVisToLog.empty()) return rects;

    const size_t nVis = visualCount();

    size_t i = 0;
    while (i < bidiVisToLog.size()) {
        size_t logical = bidiVisToLog[i];
        bool inSelection = (logical >= logStart && logical < logEnd);
        if (!inSelection) { ++i; continue; }

        size_t runStart = i;
        ++i;
        while (i < bidiVisToLog.size()) {
            size_t l = bidiVisToLog[i];
            if (l < logStart || l >= logEnd) break;
            ++i;
        }
        size_t runEnd = i;
        if (runStart > nVis) runStart = nVis;
        if (runEnd > nVis) runEnd = nVis;
        rects.emplace_back(originX + xPos(runStart),
            originX + xPos(runEnd));
    }
    return rects;
}

// =====================================================================
// Diagnostics
// =====================================================================

std::string InputTextModel::formatState() const {
    ensureMetrics();

    std::string out;
    out += "cursor=";
    out += std::to_string(m_cursor);
    out += " sel=";
    out += std::to_string(m_selStart);
    out += "..";
    out += std::to_string(m_selEnd);
    out += "\n";
    out += "text=\"";
    out += textUtf8();
    out += "\"\n";
    out += "vis=\"";
    out += m_visualUtf8;
    out += "\"\n";
    out += "cursorV=";
    out += std::to_string(logicalToVisual(m_cursor));
    out += "\n";
    out += "undo=";
    out += std::to_string(m_undoStack.size());
    out += " redo=";
    out += std::to_string(m_redoStack.size());
    out += "\n";

    return out;
}

void InputTextModel::clampCursor() {
    size_t n = m_shaped.textViewUtf32().size();
    if (m_cursor > n) m_cursor = n;
    if (m_selStart > n) m_selStart = n;
    if (m_selEnd > n) m_selEnd = n;
}

// =====================================================================
// Multiline support
// =====================================================================

void InputTextModel::setMultiline(bool on) {
    if (m_multiline == on) return;
    m_multiline = on;
    m_linesDirty = true;

    // Switching to single-line: replace \n with space.
    if (!on) {
        std::u32string text = m_shaped.textUtf32();
        bool changed = false;
        for (auto& c : text) {
            if (c == U'\n') { c = U' '; changed = true; }
        }
        if (changed) {
            size_t cur = m_cursor, s = m_selStart, e = m_selEnd;
            m_shaped.setText(text);
            m_cursor = std::min(cur, text.size());
            m_selStart = std::min(s, text.size());
            m_selEnd = std::min(e, text.size());
            m_metricsDirty = true;
        }
    }
}

size_t InputTextModel::lineCount() const {
    ensureLines();
    return m_lines.size();
}

const InputTextModel::LineInfo&
    InputTextModel::lineInfo(size_t i) const {
    ensureLines();
    return m_lines[i];
}

void InputTextModel::ensureLines() const {
    ensureMetrics();
    if (m_linesDirty) rebuildLines();
}

void InputTextModel::rebuildLines() const {
    auto& self = const_cast<InputTextModel&>(*this);
    self.m_lines.clear();

    const auto& r = m_shaped.bidi().result();
    const size_t nPara = (r.paragraphStart.size() > 0)
        ? r.paragraphStart.size() - 1 : 0;

    if (nPara == 0) {
        LineInfo empty;
        empty.logicalStart = 0; empty.logicalEnd = 0;
        empty.visParaOffset = 0; empty.visStart = 0; empty.visEnd = 0;
        empty.y = 0.0f; empty.width = 0.0f; empty.paragraphLevel = 0;
        self.m_lines.push_back(empty);
        self.m_linesDirty = false;
        return;
    }

    auto text = m_shaped.textViewUtf32();
    float y = 0.0f;
    size_t visOffset = 0;

    for (size_t p = 0; p < nPara; ++p) {
        size_t logStart = r.paragraphStart[p];
        size_t logEnd = r.paragraphStart[p + 1];

        const auto& para = m_shaped.bidi().paragraphResult(p);
        size_t visCount = para.visualToLogical.size();

        LineInfo info;
        info.logicalStart = logStart;
        info.logicalEnd = logEnd;
        info.visParaOffset = visOffset;
        info.visStart = visOffset;
        info.visEnd = visOffset + visCount;
        info.y = y;
        info.paragraphLevel = para.paragraphLevel;

        // Exclude trailing \n from visible range.
        if (logEnd > logStart && logEnd - 1 < text.size() &&
            text[logEnd - 1] == U'\n' && visCount > 0) {
            size_t nlLocal = (logEnd - 1) - logStart;
            if (nlLocal < para.logicalToVisual.size()) {
                size_t nlCharVis = para.logicalToVisual[nlLocal];
                if (nlCharVis == 0) {
                    info.visStart++;          // RTL: \n at leftmost
                }
                else if (nlCharVis == visCount - 1) {
                    info.visEnd--;            // LTR: \n at rightmost
                }
                // else: unusual case, leave as-is
            }
        }

        // Width.
        if (info.visEnd > info.visStart &&
            info.visEnd < self.m_xPos.size() &&
            info.visStart < self.m_xPos.size()) {
            info.width = self.m_xPos[info.visEnd] -
                self.m_xPos[info.visStart];
        }
        else {
            info.width = 0.0f;
        }

        self.m_lines.push_back(info);
        y += m_lineHeight;
        visOffset += visCount;
    }

    // If the text ends with '\n', add an extra empty line for the cursor
    // position after the last newline. Without this, pressing Enter at
    // the end of the text doesn't create a new visual line.
    if (!text.empty() && text.back() == U'\n') {
        LineInfo extra;
        extra.logicalStart = text.size();
        extra.logicalEnd = text.size();
        extra.visParaOffset = visOffset;   // after all paragraph visuals
        extra.visStart = visOffset;
        extra.visEnd = visOffset;
        extra.y = y;
        extra.width = 0.0f;
        extra.paragraphLevel = self.m_lines.empty()
            ? 0 : self.m_lines.back().paragraphLevel;
        self.m_lines.push_back(extra);
        // y is not needed after this, but be tidy.
        y += m_lineHeight;
    }

    self.m_linesDirty = false;
}

size_t InputTextModel::lineOfLogical(size_t logicalPos) const {
    ensureLines();
    if (m_lines.empty()) return 0;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        const auto& ln = m_lines[i];
        if (logicalPos >= ln.logicalStart && logicalPos < ln.logicalEnd)
            return i;
        if (i + 1 == m_lines.size() && logicalPos <= ln.logicalEnd)
            return i;
    }
    return m_lines.size() - 1;
}

InputTextModel::CursorPix
    InputTextModel::cursorPix(float widgetWidth, float paddingX) const {
    ensureLines();
    CursorPix out{ 0, 0, 0 };
    if (m_lines.empty()) return out;

    size_t lineIdx = lineOfLogical(m_cursor);
    const auto& ln = m_lines[lineIdx];

    float lineX = paddingX;
    if (ln.paragraphLevel & 1) {
        lineX = widgetWidth - paddingX - ln.width;
    }

    size_t vGlobal = logicalToVisual(m_cursor);
    size_t pv = 0;
    if (vGlobal >= ln.visParaOffset) pv = vGlobal - ln.visParaOffset;
    size_t pvFirst = ln.visStart - ln.visParaOffset;
    size_t pvLast = ln.visEnd - ln.visParaOffset;
    if (pv < pvFirst) pv = pvFirst;
    if (pv > pvLast)  pv = pvLast;

    float xInLine = 0.0f;
    size_t gv = ln.visParaOffset + pv;
    if (gv < m_xPos.size() && ln.visStart < m_xPos.size()) {
        xInLine = m_xPos[gv] - m_xPos[ln.visStart];
    }

    out.x = lineX + xInLine;
    out.y = ln.y;
    out.lineIndex = lineIdx;
    return out;
}

std::vector<InputTextModel::SelectionRect>
    InputTextModel::computeSelectionRectsMulti(
        float widgetWidth, float paddingX) const {
    std::vector<SelectionRect> rects;
    if (!hasSelection()) return rects;

    ensureLines();
    ensureMetrics();

    size_t logStart = std::min(m_selStart, m_selEnd);
    size_t logEnd = std::max(m_selStart, m_selEnd);
    if (logStart >= logEnd) return rects;

    for (size_t i = 0; i < m_lines.size(); ++i) {
        const auto& ln = m_lines[i];

        size_t selS = std::max(logStart, ln.logicalStart);
        size_t selE = std::min(logEnd, ln.logicalEnd);
        if (selS >= selE) continue;

        const auto& bidiVisToLog =
            m_shaped.bidi().paragraphResult(i).visualToLogical;
        if (bidiVisToLog.empty()) continue;

        size_t pvFirst = ln.visStart - ln.visParaOffset;
        size_t pvLast = ln.visEnd - ln.visParaOffset;
        if (pvFirst >= pvLast) continue;

        float lineX = paddingX;
        if (ln.paragraphLevel & 1) {
            lineX = widgetWidth - paddingX - ln.width;
        }
        float lineY = ln.y;

        // Convert para-local logical to global.
        auto localToGlobal = [&](size_t pv) -> size_t {
            return ln.logicalStart + bidiVisToLog[pv];
            };

        // Walk para-local visual positions in [pvFirst, pvLast).
        size_t pv = pvFirst;
        while (pv < pvLast && pv < bidiVisToLog.size()) {
            size_t g = localToGlobal(pv);
            if (g < selS || g >= selE) { ++pv; continue; }

            size_t runStartPv = pv;
            ++pv;
            while (pv < pvLast && pv < bidiVisToLog.size()) {
                size_t g2 = localToGlobal(pv);
                if (g2 < selS || g2 >= selE) break;
                ++pv;
            }
            size_t runEndPv = pv;

            size_t gv1 = ln.visParaOffset + runStartPv;
            size_t gv2 = ln.visParaOffset + runEndPv;

            if (gv1 >= m_xPos.size() || gv2 >= m_xPos.size() ||
                ln.visStart >= m_xPos.size()) {
                continue;   // skip on inconsistent state
            }

            float x1 = lineX + (m_xPos[gv1] - m_xPos[ln.visStart]);
            float x2 = lineX + (m_xPos[gv2] - m_xPos[ln.visStart]);

            rects.push_back({ x1, lineY, x2, lineY + m_lineHeight });
        }
    }

    return rects;
}

void InputTextModel::moveUpLine(float widgetWidth, float paddingX,
    bool select) {
    ensureLines();
    size_t lineIdx = lineOfLogical(m_cursor);
    if (lineIdx == 0) return;

    const auto& ln = m_lines[lineIdx - 1];

    // If target line is empty (no visible chars), use its logicalStart.
    if (ln.visStart >= ln.visEnd) {
        size_t newCursor = ln.logicalStart;
        if (select) {
            if (m_selStart == m_selEnd) m_selStart = m_cursor;
            m_selEnd = newCursor;
        }
        else {
            m_selStart = m_selEnd = newCursor;
        }
        m_cursor = newCursor;
        return;
    }

    CursorPix cur = cursorPix(widgetWidth, paddingX);
    float targetX = cur.x;

    float lineX = paddingX;
    if (ln.paragraphLevel & 1)
        lineX = widgetWidth - paddingX - ln.width;

    float localX = targetX - lineX;
    if (localX < 0) localX = 0;
    if (localX > ln.width) localX = ln.width;

    size_t bestVis = ln.visStart;
    float bestDist = 1e9f;
    for (size_t v = ln.visStart; v <= ln.visEnd; ++v) {
        if (v >= m_xPos.size() || ln.visStart >= m_xPos.size()) break;
        float xInLine = m_xPos[v] - m_xPos[ln.visStart];
        float d = std::fabs(xInLine - localX);
        if (d < bestDist) { bestDist = d; bestVis = v; }
    }

    size_t newCursor = visualToLogical(bestVis);
    if (select) {
        if (m_selStart == m_selEnd) m_selStart = m_cursor;
        m_selEnd = newCursor;
    }
    else {
        m_selStart = m_selEnd = newCursor;
    }
    m_cursor = newCursor;
}

void InputTextModel::moveDownLine(float widgetWidth, float paddingX,
    bool select) {
    ensureLines();
    size_t lineIdx = lineOfLogical(m_cursor);
    if (lineIdx + 1 >= m_lines.size()) return;

    const auto& ln = m_lines[lineIdx + 1];

    // If target line is empty, use its logicalStart.
    if (ln.visStart >= ln.visEnd) {
        size_t newCursor = ln.logicalStart;
        if (select) {
            if (m_selStart == m_selEnd) m_selStart = m_cursor;
            m_selEnd = newCursor;
        }
        else {
            m_selStart = m_selEnd = newCursor;
        }
        m_cursor = newCursor;
        return;
    }

    CursorPix cur = cursorPix(widgetWidth, paddingX);
    float targetX = cur.x;

    float lineX = paddingX;
    if (ln.paragraphLevel & 1)
        lineX = widgetWidth - paddingX - ln.width;

    float localX = targetX - lineX;
    if (localX < 0) localX = 0;
    if (localX > ln.width) localX = ln.width;

    size_t bestVis = ln.visStart;
    float bestDist = 1e9f;
    for (size_t v = ln.visStart; v <= ln.visEnd; ++v) {
        if (v >= m_xPos.size() || ln.visStart >= m_xPos.size()) break;
        float xInLine = m_xPos[v] - m_xPos[ln.visStart];
        float d = std::fabs(xInLine - localX);
        if (d < bestDist) { bestDist = d; bestVis = v; }
    }

    size_t newCursor = visualToLogical(bestVis);
    if (select) {
        if (m_selStart == m_selEnd) m_selStart = m_cursor;
        m_selEnd = newCursor;
    }
    else {
        m_selStart = m_selEnd = newCursor;
    }
    m_cursor = newCursor;
}

} // namespace bidi
