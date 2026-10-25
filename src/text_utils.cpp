// bidi/src/text_utils.cpp
#include "bidi/bidi.h"
#include "bidi/shaped_text.h"
#include "bidi/text_utils.h"
#include "detail/utf.h"
#include <algorithm>
#include <iterator>


namespace bidi {

namespace {

struct ReverseEntry {
    char32_t presentation;
    char32_t base1;
    char32_t base2;   // second base (for ligatures like lam-alef)
};

// Reverse mapping: presentation form -> base character(s).
// Generated from ArabicPresentationForms-A/B + UnicodeData.txt.
// Sorted by `presentation` for binary search.
//
// For ligatures (lam-alef, etc.), base2 != 0.
static constexpr ReverseEntry kReverse[] = {
    // --- Core Arabic (U+FE80..U+FEF4) ---
    { 0xFE80, 0x0621, 0 },  // hamza isolated
    { 0xFE81, 0x0622, 0 },  // alef-madda isolated
    { 0xFE82, 0x0622, 0 },  // alef-madda final
    { 0xFE83, 0x0623, 0 },  // alef-hamza-above isolated
    { 0xFE84, 0x0623, 0 },  // alef-hamza-above final
    { 0xFE85, 0x0624, 0 },  // waw-hamza isolated
    { 0xFE86, 0x0624, 0 },  // waw-hamza final
    { 0xFE87, 0x0625, 0 },  // alef-hamza-below isolated
    { 0xFE88, 0x0625, 0 },  // alef-hamza-below final
    { 0xFE89, 0x0626, 0 },  // yeh-hamza isolated
    { 0xFE8A, 0x0626, 0 },  // yeh-hamza final
    { 0xFE8B, 0x0626, 0 },  // yeh-hamza initial
    { 0xFE8C, 0x0626, 0 },  // yeh-hamza medial
    { 0xFE8D, 0x0627, 0 },  // alef isolated
    { 0xFE8E, 0x0627, 0 },  // alef final
    { 0xFE8F, 0x0628, 0 },  // beh isolated
    { 0xFE90, 0x0628, 0 },  // beh final
    { 0xFE91, 0x0628, 0 },  // beh initial
    { 0xFE92, 0x0628, 0 },  // beh medial
    { 0xFE93, 0x0629, 0 },  // teh-marbuta isolated
    { 0xFE94, 0x0629, 0 },  // teh-marbuta final
    { 0xFE95, 0x062A, 0 },  // teh isolated
    { 0xFE96, 0x062A, 0 },  // teh final
    { 0xFE97, 0x062A, 0 },  // teh initial
    { 0xFE98, 0x062A, 0 },  // teh medial
    { 0xFE99, 0x062B, 0 },  // theh isolated
    { 0xFE9A, 0x062B, 0 },  // theh final
    { 0xFE9B, 0x062B, 0 },  // theh initial
    { 0xFE9C, 0x062B, 0 },  // theh medial
    { 0xFE9D, 0x062C, 0 },  // jeem isolated
    { 0xFE9E, 0x062C, 0 },  // jeem final
    { 0xFE9F, 0x062C, 0 },  // jeem initial
    { 0xFEA0, 0x062C, 0 },  // jeem medial
    { 0xFEA1, 0x062D, 0 },  // hah isolated
    { 0xFEA2, 0x062D, 0 },  // hah final
    { 0xFEA3, 0x062D, 0 },  // hah initial
    { 0xFEA4, 0x062D, 0 },  // hah medial
    { 0xFEA5, 0x062E, 0 },  // khah isolated
    { 0xFEA6, 0x062E, 0 },  // khah final
    { 0xFEA7, 0x062E, 0 },  // khah initial
    { 0xFEA8, 0x062E, 0 },  // khah medial
    { 0xFEA9, 0x062F, 0 },  // dal isolated
    { 0xFEAA, 0x062F, 0 },  // dal final
    { 0xFEAB, 0x0630, 0 },  // thal isolated
    { 0xFEAC, 0x0630, 0 },  // thal final
    { 0xFEAD, 0x0631, 0 },  // reh isolated
    { 0xFEAE, 0x0631, 0 },  // reh final
    { 0xFEAF, 0x0632, 0 },  // zain isolated
    { 0xFEB0, 0x0632, 0 },  // zain final
    { 0xFEB1, 0x0633, 0 },  // seen isolated
    { 0xFEB2, 0x0633, 0 },  // seen final
    { 0xFEB3, 0x0633, 0 },  // seen initial
    { 0xFEB4, 0x0633, 0 },  // seen medial
    { 0xFEB5, 0x0634, 0 },  // sheen isolated
    { 0xFEB6, 0x0634, 0 },  // sheen final
    { 0xFEB7, 0x0634, 0 },  // sheen initial
    { 0xFEB8, 0x0634, 0 },  // sheen medial
    { 0xFEB9, 0x0635, 0 },  // sad isolated
    { 0xFEBA, 0x0635, 0 },  // sad final
    { 0xFEBB, 0x0635, 0 },  // sad initial
    { 0xFEBC, 0x0635, 0 },  // sad medial
    { 0xFEBD, 0x0636, 0 },  // dad isolated
    { 0xFEBE, 0x0636, 0 },  // dad final
    { 0xFEBF, 0x0636, 0 },  // dad initial
    { 0xFEC0, 0x0636, 0 },  // dad medial
    { 0xFEC1, 0x0637, 0 },  // tah isolated
    { 0xFEC2, 0x0637, 0 },  // tah final
    { 0xFEC3, 0x0637, 0 },  // tah initial
    { 0xFEC4, 0x0637, 0 },  // tah medial
    { 0xFEC5, 0x0638, 0 },  // zah isolated
    { 0xFEC6, 0x0638, 0 },  // zah final
    { 0xFEC7, 0x0638, 0 },  // zah initial
    { 0xFEC8, 0x0638, 0 },  // zah medial
    { 0xFEC9, 0x0639, 0 },  // ain isolated
    { 0xFECA, 0x0639, 0 },  // ain final
    { 0xFECB, 0x0639, 0 },  // ain initial
    { 0xFECC, 0x0639, 0 },  // ain medial
    { 0xFECD, 0x063A, 0 },  // ghain isolated
    { 0xFECE, 0x063A, 0 },  // ghain final
    { 0xFECF, 0x063A, 0 },  // ghain initial
    { 0xFED0, 0x063A, 0 },  // ghain medial
    { 0xFED1, 0x0641, 0 },  // feh isolated
    { 0xFED2, 0x0641, 0 },  // feh final
    { 0xFED3, 0x0641, 0 },  // feh initial
    { 0xFED4, 0x0641, 0 },  // feh medial
    { 0xFED5, 0x0642, 0 },  // qaf isolated
    { 0xFED6, 0x0642, 0 },  // qaf final
    { 0xFED7, 0x0642, 0 },  // qaf initial
    { 0xFED8, 0x0642, 0 },  // qaf medial
    { 0xFED9, 0x0643, 0 },  // kaf isolated
    { 0xFEDA, 0x0643, 0 },  // kaf final
    { 0xFEDB, 0x0643, 0 },  // kaf initial
    { 0xFEDC, 0x0643, 0 },  // kaf medial
    { 0xFEDD, 0x0644, 0 },  // lam isolated
    { 0xFEDE, 0x0644, 0 },  // lam final
    { 0xFEDF, 0x0644, 0 },  // lam initial
    { 0xFEE0, 0x0644, 0 },  // lam medial
    { 0xFEE1, 0x0645, 0 },  // meem isolated
    { 0xFEE2, 0x0645, 0 },  // meem final
    { 0xFEE3, 0x0645, 0 },  // meem initial
    { 0xFEE4, 0x0645, 0 },  // meem medial
    { 0xFEE5, 0x0646, 0 },  // noon isolated
    { 0xFEE6, 0x0646, 0 },  // noon final
    { 0xFEE7, 0x0646, 0 },  // noon initial
    { 0xFEE8, 0x0646, 0 },  // noon medial
    { 0xFEE9, 0x0647, 0 },  // heh isolated
    { 0xFEEA, 0x0647, 0 },  // heh final
    { 0xFEEB, 0x0647, 0 },  // heh initial
    { 0xFEEC, 0x0647, 0 },  // heh medial
    { 0xFEED, 0x0648, 0 },  // waw isolated
    { 0xFEEE, 0x0648, 0 },  // waw final
    { 0xFEEF, 0x0649, 0 },  // alef-maksura isolated
    { 0xFEF0, 0x0649, 0 },  // alef-maksura final
    { 0xFEF1, 0x064A, 0 },  // yeh isolated
    { 0xFEF2, 0x064A, 0 },  // yeh final
    { 0xFEF3, 0x064A, 0 },  // yeh initial
    { 0xFEF4, 0x064A, 0 },  // yeh medial

    // --- Lam-Alef ligatures (U+FEF5..U+FEFC) ---
    { 0xFEF5, 0x0644, 0x0622 },  // lam + alef-madda isolated
    { 0xFEF6, 0x0644, 0x0622 },  // lam + alef-madda final
    { 0xFEF7, 0x0644, 0x0623 },  // lam + alef-hamza-above isolated
    { 0xFEF8, 0x0644, 0x0623 },  // lam + alef-hamza-above final
    { 0xFEF9, 0x0644, 0x0625 },  // lam + alef-hamza-below isolated
    { 0xFEFA, 0x0644, 0x0625 },  // lam + alef-hamza-below final
    { 0xFEFB, 0x0644, 0x0627 },  // lam + alef isolated
    { 0xFEFC, 0x0644, 0x0627 },  // lam + alef final

    // --- Persian (U+FB56..U+FBFF) ---
    { 0xFB56, 0x067E, 0 },  // peh isolated
    { 0xFB57, 0x067E, 0 },  // peh final
    { 0xFB58, 0x067E, 0 },  // peh initial
    { 0xFB59, 0x067E, 0 },  // peh medial
    { 0xFB7A, 0x0686, 0 },  // tcheh isolated
    { 0xFB7B, 0x0686, 0 },  // tcheh final
    { 0xFB7C, 0x0686, 0 },  // tcheh initial
    { 0xFB7D, 0x0686, 0 },  // tcheh medial
    { 0xFB8A, 0x0698, 0 },  // jeh isolated
    { 0xFB8B, 0x0698, 0 },  // jeh final
    { 0xFB8E, 0x06A9, 0 },  // keheh isolated
    { 0xFB8F, 0x06A9, 0 },  // keheh final
    { 0xFB90, 0x06A9, 0 },  // keheh initial
    { 0xFB91, 0x06A9, 0 },  // keheh medial
    { 0xFB92, 0x06AF, 0 },  // gaf isolated
    { 0xFB93, 0x06AF, 0 },  // gaf final
    { 0xFB94, 0x06AF, 0 },  // gaf initial
    { 0xFB95, 0x06AF, 0 },  // gaf medial
    { 0xFBFC, 0x06CC, 0 },  // farsi-yeh isolated
    { 0xFBFD, 0x06CC, 0 },  // farsi-yeh final
    { 0xFBFE, 0x06CC, 0 },  // farsi-yeh initial
    { 0xFBFF, 0x06CC, 0 },  // farsi-yeh medial
};

const ReverseEntry* findReverse(char32_t cp) {
    auto it = std::lower_bound(
        std::begin(kReverse), std::end(kReverse), cp,
        [](const ReverseEntry& e, char32_t v) {
            return e.presentation < v;
        });
    if (it != std::end(kReverse) && it->presentation == cp)
        return &*it;
    return nullptr;
}

} // namespace

std::u32string unshapeToLogical(std::u32string_view visual) {
    std::u32string out;
    out.reserve(visual.size());

    for (char32_t cp : visual) {
        const ReverseEntry* e = findReverse(cp);
        if (!e) {
            // Not a presentation form (or not in our table).
            out.push_back(cp);
            continue;
        }
        out.push_back(e->base1);
        if (e->base2 != 0) {
            out.push_back(e->base2);
        }
    }
    return out;
}

std::string unshapeToLogical(std::string_view visualUtf8) {
    auto vec = detail::utf8ToUtf32(visualUtf8);
    std::u32string u32(vec.begin(), vec.end());

    std::u32string result = unshapeToLogical(u32);
    return detail::utf32ToUtf8(result);
}

std::string shapeToVisual(std::string_view logicalUtf8,
    int base_level) {
    ShapedText s;
    s.setBaseLevel(base_level);
    s.setText(logicalUtf8);
    return s.toShapedVisualString();
}

std::u32string shapeToVisual(std::u32string_view logicalUtf32,
    int base_level) {
    ShapedText s;
    s.setBaseLevel(base_level);
    s.setText(logicalUtf32);
    return s.toShapedVisualStringUtf32();
}

} // namespace bidi
