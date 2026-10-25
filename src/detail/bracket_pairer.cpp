// src/detail/bracket_pairer.cpp
#include "bracket_pairer.h"
#include "bidi_brackets_table.h"
#include <algorithm>

namespace bidi::detail {

namespace {

// Maximum nesting depth allowed by BD16.
constexpr size_t kMaxPairingDepth = 125;

struct StackEntry {
    uint32_t index;
    char32_t canonicalOpener;
};

} // namespace

std::vector<BracketPair> resolveBrackets(
    const std::vector<char32_t>& codepoints,
    const std::vector<BidiClass>& types,
    BidiClass /*sos*/,
    BidiClass /*eos*/)
{
    std::vector<BracketPair> pairs;
    std::vector<StackEntry> stack;

    for (uint32_t i = 0; i < codepoints.size(); ++i) {
        if (types[i] != BidiClass::ON)
            continue;

        BracketEntry entry = lookupBracket(codepoints[i]);
        if (entry.type == BracketType::None)
            continue;

        if (entry.type == BracketType::Open) {
            if (stack.size() >= kMaxPairingDepth)
                continue;

            stack.push_back({ i, entry.pairedCanonical });
        }
        else {
            size_t found = stack.size();
            for (size_t s = stack.size(); s > 0; --s) {
                if (stack[s - 1].canonicalOpener == entry.pairedCanonical) {
                    found = s - 1;
                    break;
                }
            }

            if (found == stack.size())
                continue;

            pairs.push_back({ stack[found].index, i });
            stack.erase(stack.begin() + found, stack.end());
        }
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const BracketPair& a, const BracketPair& b) {
                  return a.opener < b.opener;
              });

    return pairs;
}

} // namespace bidi::detail
