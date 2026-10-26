// tests/test_bracket_pairer.cpp
#include <cassert>
#include <cstdio>
#include <vector>
#include "detail/bracket_pairer.h"
#include "detail/bidi_class_table.h"

using namespace bidi::detail;

static std::vector<char32_t> cps(std::initializer_list<char32_t> list) {
    return std::vector<char32_t>(list);
}

static std::vector<BidiClass> allON(size_t n) {
    return std::vector<BidiClass>(n, BidiClass::ON);
}

static void checkPairs(
    const char* name,
    const std::vector<char32_t>& input,
    const std::vector<BracketPair>& expected)
{
    auto types = allON(input.size());
    auto pairs = resolveBrackets(input, types, BidiClass::L, BidiClass::L);

    bool ok = pairs.size() == expected.size();
    if (ok) {
        for (size_t i = 0; i < pairs.size(); ++i) {
            if (pairs[i].opener != expected[i].opener ||
                pairs[i].closer != expected[i].closer) {
                ok = false;
                break;
            }
        }
    }

    if (ok) {
        std::printf("[PASS] %s\n", name);
    } else {
        std::printf("[FAIL] %s\n", name);
        std::printf("  expected %zu pairs, got %zu\n",
                    expected.size(), pairs.size());
        for (auto& p : pairs)
            std::printf("    (%u, %u)\n", p.opener, p.closer);
    }
}

int main() {
    // ASCII brackets.
    checkPairs("empty", cps({}), {});
    checkPairs("single opener", cps({0x0028}), {});
    checkPairs("single closer", cps({0x0029}), {});
    checkPairs("( )", cps({0x0028, 0x0029}), {{0, 1}});
    checkPairs("([)]", cps({0x0028, 0x005B, 0x0029, 0x005D}), {{0, 2}});
    checkPairs("((()))", cps({0x0028, 0x0028, 0x0028,
                              0x0029, 0x0029, 0x0029}),
               {{0, 5}, {1, 4}, {2, 3}});
    checkPairs("(())", cps({0x0028, 0x0028, 0x0029, 0x0029}),
               {{0, 3}, {1, 2}});
    checkPairs("()()", cps({0x0028, 0x0029, 0x0028, 0x0029}),
               {{0, 1}, {2, 3}});
    checkPairs("mismatched", cps({0x0028, 0x005D}), {});
    checkPairs("nested with text", cps({0x0028, 0x0041, 0x005B, 0x0042,
                                        0x005D, 0x0043, 0x0029}),
               {{0, 6}, {2, 4}});

    // Canonical equivalence: U+2329 (〈) and U+3009 (〉).
    checkPairs("canonical pair 1",
               cps({0x2329, 0x232A}), {{0, 1}});
    checkPairs("canonical pair 2",
               cps({0x2329, 0x3009}), {{0, 1}});
    checkPairs("canonical pair 3",
               cps({0x3008, 0x232A}), {{0, 1}});

    // Non-ON types should be ignored.
    {
        std::vector<char32_t> input = {0x0028, 0x0029};
        std::vector<BidiClass> types = {BidiClass::L, BidiClass::L};
        auto pairs = resolveBrackets(input, types, BidiClass::L, BidiClass::L);
        std::printf("[%s] ignore non-ON\n",
                    pairs.empty() ? "PASS" : "FAIL");
    }

    return 0;
}
