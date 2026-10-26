// src/detail/bidi_algorithm.cpp
#include "bidi_algorithm.h"
#include "weak_types.h"
#include "neutral_types.h"
#include "implicit_levels.h"
#include "bracket_pairer.h"
#include <algorithm>

namespace bidi::detail {

namespace {

int determineParagraphLevel(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& originalTypes,
    int requestedBaseLevel)
{
    if (requestedBaseLevel == 0 || requestedBaseLevel == 1)
        return requestedBaseLevel;

    size_t n = codepoints.size();
    size_t i = 0;
    std::vector<uint32_t> isolateStack;

    while (i < n) {
        BidiClass t = originalTypes[i];

        if (isIsolateInitiator(t)) {
            isolateStack.push_back(static_cast<uint32_t>(i));
            ++i;
            continue;
        }
        if (t == BidiClass::PDI) {
            if (!isolateStack.empty())
                isolateStack.pop_back();
            ++i;
            continue;
        }

        if (!isolateStack.empty()) {
            ++i;
            continue;
        }

        if (t == BidiClass::L)
            return 0;
        if (t == BidiClass::R || t == BidiClass::AL)
            return 1;

        ++i;
    }
    return 0;
}

// Computes sos/eos for a sequence boundary. Uses the type of the
// adjacent character if strong L/R/AL; otherwise uses the higher of
// the fallback level and the sequence level.
BidiClass computeSosEos(
    const std::vector<BidiClass>& originalTypes,
    int32_t idx,
    int fallbackLevel,
    int seqLevel)
{
    if (idx >= 0 && idx < static_cast<int32_t>(originalTypes.size())) {
        BidiClass t = originalTypes[idx];
        if (t == BidiClass::L) return BidiClass::L;
        if (t == BidiClass::R || t == BidiClass::AL) return BidiClass::R;
    }
    int maxLevel = std::max(fallbackLevel, seqLevel);
    return (maxLevel & 1) ? BidiClass::R : BidiClass::L;
}

} // namespace

ParagraphAnalysis analyzeParagraph(
    const std::vector<char32_t>& codepoints,
    int requestedBaseLevel)
{
    const size_t n = codepoints.size();

    ParagraphAnalysis result;
    result.codepoints = codepoints;
    result.originalTypes.resize(n);
    for (size_t i = 0; i < n; ++i)
        result.originalTypes[i] = bidiClassOf(codepoints[i]);

    result.paragraphLevel = determineParagraphLevel(
        codepoints, result.originalTypes, requestedBaseLevel);
    result.sos = (result.paragraphLevel & 1) ? BidiClass::R : BidiClass::L;
    result.eos = result.sos;

    ExplicitLevelsResult expl = computeExplicitLevels(
        codepoints, result.originalTypes, result.paragraphLevel);

    result.types = expl.types;
    result.levels = expl.levels;
    result.removed = expl.removed;

    struct LevelRun {
        std::vector<uint32_t> indices;
        int level;
    };
    std::vector<LevelRun> runs;

    {
        LevelRun current;
        bool haveCurrent = false;

        for (uint32_t i = 0; i < n; ++i) {
            if (expl.removed[i])
                continue;

            int level = expl.levels[i];
            if (!haveCurrent) {
                current.indices.push_back(i);
                current.level = level;
                haveCurrent = true;
            }
            else if (level == current.level) {
                current.indices.push_back(i);
            }
            else {
                runs.push_back(std::move(current));
                current = LevelRun{};
                current.indices.push_back(i);
                current.level = level;
                haveCurrent = true;
            }
        }
        if (haveCurrent)
            runs.push_back(std::move(current));
    }

    struct Sequence {
        std::vector<uint32_t> indices;
        int level;
    };
    std::vector<Sequence> sequences;

    {
        std::vector<bool> merged(runs.size(), false);

        for (size_t r = 0; r < runs.size(); ++r) {
            if (merged[r])
                continue;

            Sequence seq;
            seq.level = runs[r].level;
            for (uint32_t idx : runs[r].indices)
                seq.indices.push_back(idx);
            merged[r] = true;

            bool keepMerging = true;
            while (keepMerging) {
                keepMerging = false;
                if (seq.indices.empty())
                    break;

                uint32_t lastIdx = seq.indices.back();
                BidiClass lastType = result.originalTypes[lastIdx];
                if (!isIsolateInitiator(lastType))
                    break;

                int32_t pdiIdx = expl.matchingPDI[lastIdx];
                if (pdiIdx < 0)
                    break;

                for (size_t r2 = r + 1; r2 < runs.size(); ++r2) {
                    if (merged[r2]) continue;
                    if (runs[r2].indices.front() !=
                        static_cast<uint32_t>(pdiIdx))
                        continue;
                    if (runs[r2].level != seq.level)
                        continue;

                    for (uint32_t idx : runs[r2].indices)
                        seq.indices.push_back(idx);
                    merged[r2] = true;
                    keepMerging = true;
                    break;
                }
            }

            sequences.push_back(std::move(seq));
        }
    }

    for (Sequence& seq : sequences) {
        const size_t m = seq.indices.size();

        int prevLevel = result.paragraphLevel;
        int nextLevel = result.paragraphLevel;

        uint32_t firstIdx = seq.indices.front();
        for (int32_t i = static_cast<int32_t>(firstIdx) - 1; i >= 0; --i) {
            if (!expl.removed[i]) {
                prevLevel = expl.levels[i];
                break;
            }
        }

        uint32_t lastIdx = seq.indices.back();
        for (uint32_t i = lastIdx + 1; i < n; ++i) {
            if (!expl.removed[i]) {
                nextLevel = expl.levels[i];
                break;
            }
        }

        int32_t prevCharIdx = static_cast<int32_t>(firstIdx) - 1;
        int32_t nextCharIdx = static_cast<int32_t>(lastIdx) + 1;

        BidiClass sos = computeSosEos(
            result.originalTypes, prevCharIdx, prevLevel, seq.level);
        BidiClass eos = computeSosEos(
            result.originalTypes, nextCharIdx, nextLevel, seq.level);

        std::vector<BidiClass> seqTypes(m);
        std::vector<BidiClass> seqOriginal(m);
        std::vector<char32_t> seqCps(m);
        std::vector<int> seqLevels(m);
        for (size_t k = 0; k < m; ++k) {
            seqTypes[k] = result.types[seq.indices[k]];
            seqOriginal[k] = result.originalTypes[seq.indices[k]];
            seqCps[k] = codepoints[seq.indices[k]];
            seqLevels[k] = result.levels[seq.indices[k]];
        }

        resolveWeakTypes(seqTypes, sos, eos);

        std::vector<BracketPair> pairs = resolveBrackets(
            seqCps, seqTypes, sos, eos);
        applyN0(seqCps, seqOriginal, seqTypes, seqLevels, pairs, sos, eos);

        resolveNeutralTypes(seqOriginal, seqTypes, seqLevels, sos, eos);

        resolveImplicitLevels(seqTypes, seqLevels);

        for (size_t k = 0; k < m; ++k) {
            result.types[seq.indices[k]] = seqTypes[k];
            result.levels[seq.indices[k]] = seqLevels[k];
        }
    }

    applyL1(result.originalTypes, result.levels, result.paragraphLevel);

    result.visualOrder = applyL2(result.levels, result.removed);

    return result;
}

TextAnalysis analyzeText(
    const std::vector<char32_t>& codepoints,
    int requestedBaseLevel)
{
    const size_t n = codepoints.size();

    TextAnalysis out;
    out.levels.assign(n, 0);
    out.paragraphLevels.assign(n, 0);
    out.removed.assign(n, 0);
    out.paragraphIndex.assign(n, 0);

    if (n == 0) {
        out.visualOrder.visualToLogical.clear();
        out.visualOrder.logicalToVisual.clear();
        out.paragraphStart.push_back(0);
        return out;
    }

    // Split into paragraphs at B (paragraph separator).
    // B is included at the end of each paragraph.
    std::vector<std::pair<size_t, size_t>> paragraphRanges;

    size_t start = 0;
    for (size_t i = 0; i < n; ++i) {
        BidiClass cls = bidiClassOf(codepoints[i]);
        if (cls == BidiClass::B) {
            paragraphRanges.push_back({ start, i + 1 });
            start = i + 1;
        }
    }
    if (start < n) {
        paragraphRanges.push_back({ start, n });
    }

    // Analyze each paragraph.
    std::vector<ParagraphAnalysis> analyses;
    analyses.reserve(paragraphRanges.size());

    for (size_t p = 0; p < paragraphRanges.size(); ++p) {
        size_t pStart = paragraphRanges[p].first;
        size_t pEnd = paragraphRanges[p].second;

        std::vector<char32_t> paraCodepoints(
            codepoints.begin() + pStart,
            codepoints.begin() + pEnd);

        ParagraphAnalysis analysis = analyzeParagraph(
            paraCodepoints, requestedBaseLevel);

        // Safety: ensure the analysis covers the same number of characters.
        if (analysis.levels.size() != paraCodepoints.size()) {
            // Pad or truncate (should never happen).
            analysis.levels.resize(paraCodepoints.size(), 0);
            analysis.removed.resize(paraCodepoints.size(), 0);
        }

        analyses.push_back(std::move(analysis));

        // Fill per-character arrays.
        for (size_t k = 0; k < pEnd - pStart; ++k) {
            out.levels[pStart + k] = analyses.back().levels[k];
            out.paragraphLevels[pStart + k] =
                analyses.back().paragraphLevel;
            out.removed[pStart + k] = analyses.back().removed[k];
            out.paragraphIndex[pStart + k] = static_cast<uint32_t>(p);
        }

        out.paragraphStart.push_back(static_cast<uint32_t>(pStart));
    }
    out.paragraphStart.push_back(static_cast<uint32_t>(n));

    // Build the global visual order.
    out.visualOrder.visualToLogical.clear();
    out.visualOrder.logicalToVisual.assign(n, 0);

    for (size_t p = 0; p < paragraphRanges.size(); ++p) {
        size_t pStart = paragraphRanges[p].first;
        const auto& vis = analyses[p].visualOrder.visualToLogical;

        for (uint32_t localIdx : vis) {
            uint32_t globalIdx = static_cast<uint32_t>(pStart + localIdx);
            out.visualOrder.visualToLogical.push_back(globalIdx);
        }
    }

    for (uint32_t v = 0;
        v < out.visualOrder.visualToLogical.size(); ++v) {
        out.visualOrder.logicalToVisual[
            out.visualOrder.visualToLogical[v]] = v;
    }

    return out;
}

} // namespace bidi::detail
