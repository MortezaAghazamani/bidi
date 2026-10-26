// src/detail/neutral_types.cpp
#include "neutral_types.h"
#include "bracket_pairer.h"
#include <algorithm>

namespace bidi::detail {

namespace {

inline bool isStrongForN(BidiClass t) {
    return t == BidiClass::L || t == BidiClass::R;
}

inline bool isStrongForN0(BidiClass t) {
    return t == BidiClass::L || t == BidiClass::R ||
           t == BidiClass::EN || t == BidiClass::AN;
}

inline BidiClass dirOf(BidiClass t) {
    return (t == BidiClass::L) ? BidiClass::L : BidiClass::R;
}

inline bool isNumberForN(BidiClass t) {
    return t == BidiClass::EN || t == BidiClass::AN;
}

inline BidiClass embeddingDir(int level) {
    return (level & 1) ? BidiClass::R : BidiClass::L;
}

inline BidiClass otherDir(BidiClass d) {
    return (d == BidiClass::L) ? BidiClass::R : BidiClass::L;
}

} // namespace

void applyN0(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& originalTypes,
    std::vector<BidiClass>& types,
    std::vector<int>& levels,
    const std::vector<BracketPair>& pairs,
    BidiClass sos,
    BidiClass eos)
{
    const bool overflowCase = pairs.size() > 63;

    for (const BracketPair& pair : pairs) {
        BidiClass embedding = embeddingDir(levels[pair.opener]);

        bool foundEmbedding = false;
        bool foundOpposite = false;

        for (uint32_t i = pair.opener + 1; i < pair.closer; ++i) {
            BidiClass t = types[i];
            if (!isStrongForN0(t)) continue;
            BidiClass dir = dirOf(t);
            if (dir == embedding) foundEmbedding = true;
            else foundOpposite = true;
        }

        if (!foundEmbedding && !foundOpposite)
            continue;

        if (foundEmbedding) {
            types[pair.opener] = embedding;
            types[pair.closer] = embedding;
            continue;
        }

        BidiClass opposite = otherDir(embedding);
        BidiClass contextDir = sos;

        // Prior only.
        for (int32_t i = static_cast<int32_t>(pair.opener) - 1; i >= 0; --i) {
            BidiClass t = types[i];
            if (isStrongForN0(t)) {
                contextDir = dirOf(t);
                break;
            }
        }

        if (contextDir == opposite) {
            types[pair.opener] = opposite;
            types[pair.closer] = opposite;
        }
        else {
            types[pair.opener] = embedding;
            types[pair.closer] = embedding;
        }

        if (overflowCase) {
            types[pair.closer] = sos;
        }
    }
}

void resolveNeutralTypes(
    const std::vector<BidiClass>& originalTypes,
    std::vector<BidiClass>& types,
    const std::vector<int>& levels,
    BidiClass sos,
    BidiClass eos)
{
    const size_t n = types.size();
    if (n == 0) return;

    size_t i = 0;
    while (i < n) {
        if (!isNeutralOrIsolate(types[i])) { ++i; continue; }

        size_t start = i;
        size_t end = i;
        while (end < n && isNeutralOrIsolate(types[end]))
            ++end;

        BidiClass before;
        if (start == 0) before = sos;
        else {
            BidiClass t = types[start - 1];
            if (isStrongForN(t)) before = t;
            else if (isNumberForN(t)) before = BidiClass::R;
            else before = BidiClass::ON;
        }

        BidiClass after;
        if (end == n) {
            bool hasOriginalNSM = false;
            for (size_t k = start; k < end; ++k) {
                if (originalTypes[k] == BidiClass::NSM) {
                    hasOriginalNSM = true; break;
                }
            }
            if (hasOriginalNSM &&
                (before == BidiClass::L || before == BidiClass::R))
                after = before;
            else
                after = sos;
        }
        else {
            BidiClass t = types[end];
            if (isStrongForN(t)) after = t;
            else if (isNumberForN(t)) after = BidiClass::R;
            else after = BidiClass::ON;
        }

        if (before == after) {
            for (size_t k = start; k < end; ++k)
                types[k] = before;
        }

        i = end;
    }

    for (size_t i = 0; i < n; ++i) {
        if (isNeutralOrIsolate(types[i]))
            types[i] = embeddingDir(levels[i]);
    }
}

} // namespace bidi::detail
