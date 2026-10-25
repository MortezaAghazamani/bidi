// src/bidi.cpp
#include "bidi/bidi.h"
#include "detail/utf.h"
#include "detail/bidi_algorithm.h"
#include "detail/bidi_class_table.h"
#include "detail/bidi_mirroring_table.h"
#include <algorithm>
#include <optional>
#include <unordered_map>

namespace bidi {

namespace {

Direction directionOfLevel(int level) {
    return (level & 1) ? Direction::RTL : Direction::LTR;
}

std::vector<Run> buildRuns(
    const std::vector<uint8_t>& levels,
    const std::vector<uint32_t>& visualToLogical)
{
    std::vector<Run> runs;
    if (visualToLogical.empty())
        return runs;

    size_t i = 0;
    while (i < visualToLogical.size()) {
        uint32_t firstIdx = visualToLogical[i];
        int level = levels[firstIdx];

        int step = 0;
        if (i + 1 < visualToLogical.size()) {
            uint32_t secondIdx = visualToLogical[i + 1];
            if (levels[secondIdx] == level) {
                if (secondIdx == firstIdx + 1) step = 1;
                else if (secondIdx + 1 == firstIdx) step = -1;
            }
        }

        size_t j = i + 1;
        uint32_t lastIdx = firstIdx;

        if (step != 0) {
            while (j < visualToLogical.size()) {
                uint32_t nextIdx = visualToLogical[j];
                if (levels[nextIdx] != level) break;
                if (nextIdx != static_cast<uint32_t>(
                        static_cast<int64_t>(lastIdx) + step))
                    break;
                lastIdx = nextIdx;
                ++j;
            }
        }

        Run run;
        run.level = level;
        run.direction = directionOfLevel(level);
        run.containsInvisible = false;

        if (step >= 0) {
            run.logicalStart = firstIdx;
            run.logicalLength = (lastIdx - firstIdx) + 1;
        } else {
            run.logicalStart = lastIdx;
            run.logicalLength = (firstIdx - lastIdx) + 1;
        }

        runs.push_back(run);
        i = j;
    }

    return runs;
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            out.push_back(static_cast<char>(0xEF));
            out.push_back(static_cast<char>(0xBF));
            out.push_back(static_cast<char>(0xBD));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

AnalysisResult buildParagraphResult(
    const detail::ParagraphAnalysis& analysis)
{
    AnalysisResult out;
    size_t n = analysis.levels.size();

    out.levels.resize(n);
    out.classes.resize(n);
    out.directions.resize(n);
    out.mirrored.resize(n);

    for (size_t i = 0; i < n; ++i) {
        out.levels[i] = static_cast<uint8_t>(analysis.levels[i]);
        out.classes[i] = static_cast<uint8_t>(analysis.originalTypes[i]);
        out.directions[i] = directionOfLevel(analysis.levels[i]);
        out.mirrored[i] = 0;
    }

    for (size_t i = 0; i < n; ++i) {
        if ((analysis.levels[i] & 1) &&
            i < analysis.codepoints.size() &&
            detail::hasMirror(analysis.codepoints[i]))
        {
            out.mirrored[i] = 1;
        }
    }

    for (size_t i = 0; i < analysis.removed.size(); ++i) {
        if (analysis.removed[i])
            out.invisibleIndices.push_back(static_cast<uint32_t>(i));
    }

    out.visualToLogical = analysis.visualOrder.visualToLogical;
    out.logicalToVisual = analysis.visualOrder.logicalToVisual;
    out.paragraphLevel = analysis.paragraphLevel;
    out.paragraphLevels.assign(n, analysis.paragraphLevel);
    out.paragraphStart.push_back(0);
    out.paragraphStart.push_back(static_cast<uint32_t>(n));
    out.runs = buildRuns(out.levels, out.visualToLogical);
    return out;
}

} // namespace

struct BidiObject::Impl {
    struct ParagraphCache {
        std::u32string text;
        size_t startInGlobal = 0;
        detail::ParagraphAnalysis analysis;
        bool valid = false;
        mutable std::optional<AnalysisResult> cachedResult;
    };

    std::u32string text;
    int baseLevel = -1;
    std::vector<ParagraphCache> paragraphs;

    std::vector<uint32_t> paragraphStart;

    mutable std::optional<AnalysisResult> cachedGlobal;
    mutable bool globalValid = false;

    mutable std::string textUtf8Cache;
    mutable bool textUtf8Valid = false;
    mutable std::vector<uint32_t> utf8ParagraphStart;

    void invalidateTextUtf8() {
        textUtf8Valid = false;
        textUtf8Cache.clear();
        utf8ParagraphStart.clear();
    }

    void rebuildParagraphs() {
        // Use a list of caches per text, so that duplicate paragraphs
        // (same text) each get their own entry in the cache.
        std::unordered_map<std::u32string,
            std::vector<ParagraphCache>> oldByText;
        oldByText.reserve(paragraphs.size());
        for (auto& pc : paragraphs) {
            if (pc.valid || pc.cachedResult) {
                // Copy text as key (cheap; paragraph text is short).
                oldByText[pc.text].push_back(std::move(pc));
            }
        }

        std::vector<ParagraphCache> newParagraphs;
        size_t start = 0;

        for (size_t i = 0; i < text.size(); ++i) {
            if (detail::bidiClassOf(text[i]) ==
                detail::BidiClass::B)
            {
                ParagraphCache pc;
                pc.text = text.substr(start, i + 1 - start);
                pc.startInGlobal = start;
                newParagraphs.push_back(std::move(pc));
                start = i + 1;
            }
        }
        if (start < text.size() || newParagraphs.empty()) {
            ParagraphCache pc;
            pc.text = text.substr(start);
            pc.startInGlobal = start;
            newParagraphs.push_back(std::move(pc));
        }

        for (auto& newPc : newParagraphs) {
            auto it = oldByText.find(newPc.text);
            if (it != oldByText.end() && !it->second.empty()) {
                // Pop the first cached entry for this text.
                ParagraphCache& pc = it->second.front();
                newPc.analysis = std::move(pc.analysis);
                newPc.cachedResult = std::move(pc.cachedResult);
                newPc.valid = pc.valid;
                it->second.erase(it->second.begin());
            }
        }

        paragraphs = std::move(newParagraphs);

        paragraphStart.clear();
        paragraphStart.push_back(0);
        for (const auto& pc : paragraphs) {
            paragraphStart.push_back(static_cast<uint32_t>(
                pc.startInGlobal + pc.text.size()));
        }

        globalValid = false;
        cachedGlobal.reset();
        invalidateTextUtf8();
    }

    void computeParagraph(size_t idx) const {
        auto& pc = const_cast<ParagraphCache&>(paragraphs[idx]);
        if (pc.valid) return;
        std::vector<char32_t> cps(pc.text.begin(), pc.text.end());
        pc.analysis = detail::analyzeParagraph(cps, baseLevel);
        pc.valid = true;
    }

    const AnalysisResult& paragraphResult(size_t idx) const {
        computeParagraph(idx);
        auto& pc = const_cast<ParagraphCache&>(paragraphs[idx]);
        if (!pc.cachedResult)
            pc.cachedResult = buildParagraphResult(pc.analysis);
        return *pc.cachedResult;
    }

    void buildTextUtf8() const {
        auto& cache = const_cast<std::string&>(textUtf8Cache);
        auto& starts =
            const_cast<std::vector<uint32_t>&>(utf8ParagraphStart);

        cache.clear();
        starts.clear();
        starts.push_back(0);

        for (const auto& pc : paragraphs) {
            for (char32_t cp : pc.text)
                appendUtf8(cache, cp);
            starts.push_back(static_cast<uint32_t>(cache.size()));
        }
        textUtf8Valid = true;
    }

    AnalysisResult buildGlobal() const {
        AnalysisResult out;

        if (text.empty()) {
            out.paragraphStart.push_back(0);
            out.paragraphLevel = (baseLevel & 1) ? 1 : 0;
            return out;
        }

        for (size_t i = 0; i < paragraphs.size(); ++i)
            computeParagraph(i);

        size_t total = text.size();

        out.levels.resize(total);
        out.classes.resize(total);
        out.directions.resize(total);
        out.mirrored.resize(total);
        out.paragraphLevels.resize(total);
        out.paragraphStart.clear();
        out.paragraphStart.push_back(0);
        out.visualToLogical.clear();
        out.logicalToVisual.assign(total, 0);

        for (size_t p = 0; p < paragraphs.size(); ++p) {
            const auto& pc = paragraphs[p];
            size_t offset = pc.startInGlobal;
            size_t len = pc.text.size();

            for (size_t i = 0; i < len && i < pc.analysis.levels.size(); ++i) {
                out.levels[offset + i] =
                    static_cast<uint8_t>(pc.analysis.levels[i]);
                out.classes[offset + i] =
                    static_cast<uint8_t>(pc.analysis.originalTypes[i]);
                out.directions[offset + i] =
                    directionOfLevel(pc.analysis.levels[i]);
                out.paragraphLevels[offset + i] =
                    pc.analysis.paragraphLevel;

                if ((pc.analysis.levels[i] & 1) &&
                    i < pc.analysis.codepoints.size() &&
                    detail::hasMirror(pc.analysis.codepoints[i]))
                {
                    out.mirrored[offset + i] = 1;
                }

                if (pc.analysis.removed[i])
                    out.invisibleIndices.push_back(
                        static_cast<uint32_t>(offset + i));
            }

            for (uint32_t localIdx :
                 pc.analysis.visualOrder.visualToLogical) {
                out.visualToLogical.push_back(
                    static_cast<uint32_t>(offset + localIdx));
            }

            out.paragraphStart.push_back(
                static_cast<uint32_t>(offset + len));
        }

        for (uint32_t v = 0; v < out.visualToLogical.size(); ++v)
            out.logicalToVisual[out.visualToLogical[v]] = v;

        if (!out.paragraphLevels.empty())
            out.paragraphLevel = out.paragraphLevels[0];
        else
            out.paragraphLevel = (baseLevel & 1) ? 1 : 0;

        out.runs = buildRuns(out.levels, out.visualToLogical);
        return out;
    }

    const AnalysisResult& getResult() const {
        if (!globalValid) {
            cachedGlobal = buildGlobal();
            globalValid = true;
        }
        return *cachedGlobal;
    }

    void setTextUtf32(std::u32string_view newText) {
        text.assign(newText);
        paragraphs.clear();
        rebuildParagraphs();
    }

    void insert(size_t pos, std::u32string_view newText) {
        if (pos > text.size()) pos = text.size();
        text.insert(pos, newText);
        rebuildParagraphs();
    }

    void erase(size_t pos, size_t length) {
        if (pos > text.size()) return;
        if (pos + length > text.size())
            length = text.size() - pos;
        text.erase(pos, length);
        rebuildParagraphs();
    }

    void replaceUtf32(size_t pos, size_t length,
                      std::u32string_view newText) {
        if (pos > text.size()) pos = text.size();
        if (pos + length > text.size())
            length = text.size() - pos;
        text.replace(pos, length, newText);
        rebuildParagraphs();
    }

    size_t paragraphContaining(size_t pos) const {
        if (pos >= text.size())
            return paragraphs.empty() ? 0 : paragraphs.size() - 1;

        auto it = std::upper_bound(
            paragraphStart.begin(), paragraphStart.end(), pos);
        if (it == paragraphStart.begin())
            return 0;
        size_t idx = static_cast<size_t>(
            it - paragraphStart.begin()) - 1;
        if (idx >= paragraphs.size())
            return paragraphs.size() - 1;
        return idx;
    }
};

// =====================================================================
// BidiObject
// =====================================================================

BidiObject::BidiObject() : m_impl(new Impl()) {}
BidiObject::~BidiObject() = default;
BidiObject::BidiObject(BidiObject&&) noexcept = default;
BidiObject& BidiObject::operator=(BidiObject&&) noexcept = default;

void BidiObject::setText(std::string_view utf8) {
    auto cps = detail::utf8ToUtf32(utf8);
    m_impl->setTextUtf32(std::u32string_view(
        reinterpret_cast<const char32_t*>(cps.data()), cps.size()));
}

void BidiObject::setText(std::u32string_view utf32) {
    m_impl->setTextUtf32(utf32);
}

void BidiObject::insert(size_t pos, std::string_view utf8) {
    auto cps = detail::utf8ToUtf32(utf8);
    m_impl->insert(pos, std::u32string_view(
        reinterpret_cast<const char32_t*>(cps.data()), cps.size()));
}

void BidiObject::insert(size_t pos, std::u32string_view utf32) {
    m_impl->insert(pos, utf32);
}

void BidiObject::erase(size_t pos, size_t length) {
    m_impl->erase(pos, length);
}

void BidiObject::replace(size_t pos, size_t length, std::string_view utf8) {
    auto cps = detail::utf8ToUtf32(utf8);
    m_impl->replaceUtf32(pos, length, std::u32string_view(
        reinterpret_cast<const char32_t*>(cps.data()), cps.size()));
}

void BidiObject::replace(size_t pos, size_t length,
                         std::u32string_view utf32) {
    m_impl->replaceUtf32(pos, length, utf32);
}

void BidiObject::setBaseLevel(int level) {
    if (level < -1 || level > 1) level = -1;
    m_impl->baseLevel = level;
    for (auto& pc : m_impl->paragraphs) {
        pc.valid = false;
        pc.cachedResult.reset();
    }
    m_impl->globalValid = false;
    m_impl->cachedGlobal.reset();
}

std::string_view BidiObject::textView() const {
    if (!m_impl->textUtf8Valid)
        m_impl->buildTextUtf8();
    return m_impl->textUtf8Cache;
}

std::u32string_view BidiObject::textViewUtf32() const {
    return m_impl->text;
}

std::string BidiObject::text() const {
    if (!m_impl->textUtf8Valid)
        m_impl->buildTextUtf8();
    return m_impl->textUtf8Cache;
}

std::u32string BidiObject::textUtf32() const {
    return m_impl->text;
}

int BidiObject::baseLevel() const {
    return m_impl->baseLevel;
}

const AnalysisResult& BidiObject::result() const {
    return m_impl->getResult();
}

std::string BidiObject::toVisualString() const {
    auto& r = result();
    std::string out;
    out.reserve(m_impl->text.size() * 2);

    std::vector<uint8_t> invisible(m_impl->text.size(), 0);
    for (uint32_t idx : r.invisibleIndices) {
        if (idx < invisible.size())
            invisible[idx] = 1;
    }

    for (uint32_t logicalIdx : r.visualToLogical) {
        if (logicalIdx >= m_impl->text.size()) continue;
        if (invisible[logicalIdx]) continue;
        char32_t cp = m_impl->text[logicalIdx];
        if (logicalIdx < r.mirrored.size() && r.mirrored[logicalIdx])
            cp = detail::getMirror(cp);
        appendUtf8(out, cp);
    }
    return out;
}

std::u32string BidiObject::toVisualStringUtf32() const {
    auto& r = result();
    std::u32string out;
    out.reserve(r.visualToLogical.size());

    std::vector<uint8_t> invisible(m_impl->text.size(), 0);
    for (uint32_t idx : r.invisibleIndices) {
        if (idx < invisible.size())
            invisible[idx] = 1;
    }

    for (uint32_t logicalIdx : r.visualToLogical) {
        if (logicalIdx >= m_impl->text.size()) continue;
        if (invisible[logicalIdx]) continue;
        char32_t cp = m_impl->text[logicalIdx];
        if (logicalIdx < r.mirrored.size() && r.mirrored[logicalIdx])
            cp = detail::getMirror(cp);
        out.push_back(cp);
    }
    return out;
}

size_t BidiObject::paragraphCount() const {
    return m_impl->paragraphs.size();
}

std::string_view BidiObject::paragraphTextView(size_t index) const {
    if (index >= m_impl->paragraphs.size()) return {};
    if (!m_impl->textUtf8Valid)
        m_impl->buildTextUtf8();

    size_t start = m_impl->utf8ParagraphStart[index];
    size_t end = m_impl->utf8ParagraphStart[index + 1];
    return std::string_view(m_impl->textUtf8Cache)
        .substr(start, end - start);
}

std::u32string_view BidiObject::paragraphTextViewUtf32(size_t index) const {
    if (index >= m_impl->paragraphs.size()) return {};
    return m_impl->paragraphs[index].text;
}

std::string BidiObject::paragraphText(size_t index) const {
    return std::string(paragraphTextView(index));
}

std::u32string BidiObject::paragraphTextUtf32(size_t index) const {
    if (index >= m_impl->paragraphs.size()) return {};
    return m_impl->paragraphs[index].text;
}

const AnalysisResult& BidiObject::paragraphResult(size_t index) const {
    return m_impl->paragraphResult(index);
}

int BidiObject::paragraphLevel(size_t index) const {
    if (index >= m_impl->paragraphs.size()) return 0;
    m_impl->computeParagraph(index);
    return m_impl->paragraphs[index].analysis.paragraphLevel;
}

size_t BidiObject::paragraphContaining(size_t pos) const {
    return m_impl->paragraphContaining(pos);
}

Direction BidiObject::directionAt(size_t pos) const {
    if (pos >= m_impl->text.size()) return Direction::Neutral;
    size_t paraIdx = m_impl->paragraphContaining(pos);
    size_t localPos = pos - m_impl->paragraphs[paraIdx].startInGlobal;
    auto& pr = m_impl->paragraphResult(paraIdx);
    if (localPos < pr.directions.size())
        return pr.directions[localPos];
    return Direction::Neutral;
}

int BidiObject::levelAt(size_t pos) const {
    if (pos >= m_impl->text.size()) return 0;
    size_t paraIdx = m_impl->paragraphContaining(pos);
    size_t localPos = pos - m_impl->paragraphs[paraIdx].startInGlobal;
    auto& pr = m_impl->paragraphResult(paraIdx);
    if (localPos < pr.levels.size())
        return pr.levels[localPos];
    return 0;
}

bool BidiObject::isMirrored(size_t pos) const {
    if (pos >= m_impl->text.size()) return false;
    size_t paraIdx = m_impl->paragraphContaining(pos);
    size_t localPos = pos - m_impl->paragraphs[paraIdx].startInGlobal;
    auto& pr = m_impl->paragraphResult(paraIdx);
    if (localPos < pr.mirrored.size())
        return pr.mirrored[localPos] != 0;
    return false;
}

const Run* BidiObject::runAt(size_t pos) const {
    if (pos >= m_impl->text.size()) return nullptr;
    size_t paraIdx = m_impl->paragraphContaining(pos);
    size_t localPos = pos - m_impl->paragraphs[paraIdx].startInGlobal;
    auto& pr = m_impl->paragraphResult(paraIdx);
    for (auto& run : pr.runs) {
        if (localPos >= run.logicalStart &&
            localPos < run.logicalStart + run.logicalLength)
            return &run;
    }
    return nullptr;
}

size_t BidiObject::logicalToVisualAt(size_t logicalPos) const {
    if (logicalPos >= m_impl->text.size()) return 0;
    size_t paraIdx = m_impl->paragraphContaining(logicalPos);
    size_t localPos =
        logicalPos - m_impl->paragraphs[paraIdx].startInGlobal;
    auto& pr = m_impl->paragraphResult(paraIdx);
    if (localPos < pr.logicalToVisual.size()) {
        uint32_t localVisual = pr.logicalToVisual[localPos];
        uint32_t globalOffset = 0;
        for (size_t i = 0; i < paraIdx; ++i) {
            auto& prev = m_impl->paragraphResult(i);
            globalOffset += static_cast<uint32_t>(
                prev.visualToLogical.size());
        }
        return globalOffset + localVisual;
    }
    return 0;
}

size_t BidiObject::visualToLogicalAt(size_t visualPos) const {
    auto& r = result();
    if (visualPos < r.visualToLogical.size())
        return r.visualToLogical[visualPos];
    return 0;
}

void BidiObject::invalidateCache() {
    m_impl->globalValid = false;
    m_impl->cachedGlobal.reset();
}

void BidiObject::clearCaches() {
    m_impl->cachedGlobal.reset();
    m_impl->globalValid = false;
    m_impl->invalidateTextUtf8();
    for (auto& pc : m_impl->paragraphs) {
        pc.valid = false;
        pc.cachedResult.reset();
    }
}

void BidiObject::reserve(size_t textSize) {
    m_impl->text.reserve(textSize);
    m_impl->paragraphs.reserve(textSize / 80 + 1);
}

std::string AnalysisResult::toVisualString(
    std::string_view logicalText,
    bool applyMirroring) const
{
    auto codepoints = detail::utf8ToUtf32(logicalText);
    std::string out;
    out.reserve(codepoints.size() * 2);

    std::vector<uint8_t> invisible(codepoints.size(), 0);
    for (uint32_t idx : invisibleIndices) {
        if (idx < invisible.size())
            invisible[idx] = 1;
    }

    for (uint32_t logicalIdx : visualToLogical) {
        if (logicalIdx >= codepoints.size()) continue;
        if (invisible[logicalIdx]) continue;
        char32_t cp = codepoints[logicalIdx];
        if (applyMirroring && logicalIdx < mirrored.size() &&
            mirrored[logicalIdx])
            cp = detail::getMirror(cp);
        appendUtf8(out, cp);
    }
    return out;
}

std::u32string AnalysisResult::toVisualStringUtf32(
    std::u32string_view logicalText,
    bool applyMirroring) const
{
    std::u32string out;
    out.reserve(visualToLogical.size());

    std::vector<uint8_t> invisible(logicalText.size(), 0);
    for (uint32_t idx : invisibleIndices) {
        if (idx < invisible.size())
            invisible[idx] = 1;
    }

    for (uint32_t logicalIdx : visualToLogical) {
        if (logicalIdx >= logicalText.size()) continue;
        if (invisible[logicalIdx]) continue;
        char32_t cp = logicalText[logicalIdx];
        if (applyMirroring && logicalIdx < mirrored.size() &&
            mirrored[logicalIdx])
            cp = detail::getMirror(cp);
        out.push_back(cp);
    }
    return out;
}

} // namespace bidi
