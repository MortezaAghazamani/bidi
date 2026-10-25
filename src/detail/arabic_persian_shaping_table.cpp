// src/detail/arabic_persian_shaping_table.cpp
#include "arabic_persian_shaping_table.h"
#include <algorithm>
#include <iterator>

namespace bidi::detail {

namespace {

struct RawEntry {
    char32_t cp;
    char jt;  // Joining type as ASCII: U, R, L, D, C, T
};

// Generated from ArabicShaping.txt by
// tools/gen_arabic_persian_shaping.py.
static constexpr RawEntry kRaw[] = {
#include "generated/arabic_persian_shaping_ranges.inc"
};

inline JoiningType fromChar(char c) {
    switch (c) {
        case 'R': return JoiningType::R;
        case 'L': return JoiningType::L;
        case 'D': return JoiningType::D;
        case 'C': return JoiningType::C;
        case 'T': return JoiningType::T;
        default:  return JoiningType::U;
    }
}

struct PresentationEntry {
    char32_t cp;
    char32_t isolated;
    char32_t final;
    char32_t initial;
    char32_t medial;
};

static constexpr PresentationEntry kArabicPersianPresentation[] = {
    // --- Core Arabic block (U+0621..U+064A) ---
    { 0x0621, 0xFE80, 0xFE80, 0x0000, 0x0000 },  // hamza
    { 0x0622, 0xFE81, 0xFE82, 0x0000, 0x0000 },  // alef with madda
    { 0x0623, 0xFE83, 0xFE84, 0x0000, 0x0000 },  // alef with hamza above
    { 0x0624, 0xFE85, 0xFE86, 0x0000, 0x0000 },  // waw with hamza above
    { 0x0625, 0xFE87, 0xFE88, 0x0000, 0x0000 },  // alef with hamza below
    { 0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C },  // yeh with hamza above
    { 0x0627, 0xFE8D, 0xFE8E, 0x0000, 0x0000 },  // alef
    { 0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92 },  // beh
    { 0x0629, 0xFE93, 0xFE94, 0x0000, 0x0000 },  // teh marbuta
    { 0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98 },  // teh
    { 0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C },  // theh
    { 0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0 },  // jeem
    { 0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4 },  // hah
    { 0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8 },  // khah
    { 0x062F, 0xFEA9, 0xFEAA, 0x0000, 0x0000 },  // dal
    { 0x0630, 0xFEAB, 0xFEAC, 0x0000, 0x0000 },  // thal
    { 0x0631, 0xFEAD, 0xFEAE, 0x0000, 0x0000 },  // reh
    { 0x0632, 0xFEAF, 0xFEB0, 0x0000, 0x0000 },  // zain
    { 0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4 },  // seen
    { 0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8 },  // sheen
    { 0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC },  // sad
    { 0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0 },  // dad
    { 0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4 },  // tah
    { 0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8 },  // zah
    { 0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC },  // ain
    { 0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0 },  // ghain
    { 0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4 },  // feh
    { 0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8 },  // qaf
    { 0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC },  // kaf
    { 0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0 },  // lam
    { 0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4 },  // meem
    { 0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8 },  // noon
    { 0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC },  // heh
    { 0x0648, 0xFEED, 0xFEEE, 0x0000, 0x0000 },  // waw
    { 0x0649, 0xFEEF, 0xFEF0, 0x0000, 0x0000 },  // alef maksura
    { 0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4 },  // yeh

    // --- Persian letters (U+067E..U+06CC) ---
    { 0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59 },  // peh
    { 0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D },  // tcheh
    { 0x0698, 0xFB8A, 0xFB8B, 0x0000, 0x0000 },  // jeh
    { 0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91 },  // keheh
    { 0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95 },  // gaf
    { 0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF },  // farsi yeh

    // ... (full table generated from UnicodeData.txt +
    //      ArabicPresentationForms-A/B)
};

inline const PresentationEntry* findPresentation(char32_t cp) {
    auto it = std::lower_bound(
        std::begin(kArabicPersianPresentation),
        std::end(kArabicPersianPresentation), cp,
        [](const PresentationEntry& e, char32_t v) {
            return e.cp < v;
        });
    if (it != std::end(kArabicPersianPresentation) && it->cp == cp)
        return &*it;
    return nullptr;
}

} // namespace

JoiningType joiningTypeOf(char32_t cp) {
    // Arabic combining marks (Transparent for joining).
    // U+0610..U+061A, U+064B..U+065F, U+0670, U+06D6..U+06ED, U+06EF..U+06F9.
    if ((cp >= 0x0610 && cp <= 0x061A) ||
        (cp >= 0x064B && cp <= 0x065F) ||
        cp == 0x0670 ||
        (cp >= 0x06D6 && cp <= 0x06ED) ||
        (cp >= 0x06EF && cp <= 0x06F9)) {
        return JoiningType::T;
    }

    auto it = std::lower_bound(
        std::begin(kRaw), std::end(kRaw), cp,
        [](const RawEntry& e, char32_t v) { return e.cp < v; });
    if (it != std::end(kRaw) && it->cp == cp)
        return fromChar(it->jt);
    return JoiningType::U;
}

char32_t arabicPersianPresentationForm(char32_t cp,
                                       JoiningType prevType,
                                       JoiningType nextType)
{
    JoiningType self = joiningTypeOf(cp);
    if (self == JoiningType::U || self == JoiningType::T)
        return cp;

    const PresentationEntry* entry = findPresentation(cp);
    if (!entry)
        return cp;

    // Does `cp` join to the previous character?
    // cp must be R, D, or C.
    // prev must be L, D, or C.
    bool joinPrev = (self == JoiningType::R ||
                     self == JoiningType::D ||
                     self == JoiningType::C) &&
                    (prevType == JoiningType::L ||
                     prevType == JoiningType::D ||
                     prevType == JoiningType::C);

    // Does `cp` join to the next character?
    // cp must be L, D, or C.
    // next must be R, D, or C.
    bool joinNext = (self == JoiningType::L ||
                     self == JoiningType::D ||
                     self == JoiningType::C) &&
                    (nextType == JoiningType::R ||
                     nextType == JoiningType::D ||
                     nextType == JoiningType::C);

    if (joinPrev && joinNext && entry->medial != 0)
        return entry->medial;
    if (joinPrev && entry->final != 0)
        return entry->final;
    if (joinNext && entry->initial != 0)
        return entry->initial;
    return entry->isolated;
}

} // namespace bidi::detail
