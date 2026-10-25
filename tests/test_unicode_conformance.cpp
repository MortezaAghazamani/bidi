// tests/test_unicode_conformance.cpp
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "detail/bidi_algorithm.h"

using namespace bidi::detail;

namespace {

std::vector<char32_t> parseCodepoints(const std::string& s) {
    std::vector<char32_t> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(static_cast<char32_t>(
            std::stoul(tok, nullptr, 16)));
    }
    return out;
}

std::vector<int> parseInts(const std::string& s) {
    std::vector<int> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        if (tok == "x")
            out.push_back(-1);
        else
            out.push_back(std::stoi(tok));
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr,
            "Usage: %s BidiCharacterTest.txt\n", argv[0]);
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "Cannot open %s\n", argv[1]);
        return 1;
    }

    int total = 0;
    int passed = 0;
    int failed = 0;
    int failedLevelsOnly = 0;
    int failedOrderOnly = 0;
    int failedBoth = 0;
    int shown = 0;

    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> fields;
        std::istringstream iss(line);
        std::string f;
        while (std::getline(iss, f, ';'))
            fields.push_back(f);

        if (fields.size() < 5)
            continue;

        auto codepoints     = parseCodepoints(fields[0]);
        int  paraDir        = std::stoi(fields[1]);
        auto expectedLevels = parseInts(fields[3]);
        auto expectedOrder  = parseInts(fields[4]);

        int requestedBaseLevel = (paraDir == 2) ? -1 : paraDir;

        auto result = analyzeParagraph(codepoints, requestedBaseLevel);

        ++total;

        bool levelsOk = true;
        if (expectedLevels.size() != result.levels.size()) {
            levelsOk = false;
        }
        else {
            for (size_t i = 0; i < expectedLevels.size(); ++i) {
                if (expectedLevels[i] == -1)
                    continue;
                if (result.removed[i]) {
                    levelsOk = false;
                    break;
                }
                if (expectedLevels[i] != result.levels[i]) {
                    levelsOk = false;
                    break;
                }
            }
        }

        bool orderOk = true;
        {
            std::vector<uint32_t> filteredExpected;
            for (int idx : expectedOrder) {
                if (idx < 0) continue;
                filteredExpected.push_back(static_cast<uint32_t>(idx));
            }

            std::vector<uint32_t> actualOrder;
            for (uint32_t v : result.visualOrder.visualToLogical) {
                if (result.removed[v]) continue;
                actualOrder.push_back(v);
            }

            if (filteredExpected != actualOrder)
                orderOk = false;
        }

        if (levelsOk && orderOk) {
            ++passed;
        }
        else {
            ++failed;
            if (!levelsOk && !orderOk)      ++failedBoth;
            else if (!levelsOk)             ++failedLevelsOnly;
            else                            ++failedOrderOnly;

            if (shown < 10) {
                ++shown;
                std::printf("[FAIL] line %d\n", lineNo);
                std::printf("  input:           %s\n", fields[0].c_str());
                std::printf("  paraDir:         %d\n", paraDir);

                std::printf("  expected levels:");
                for (int l : expectedLevels) std::printf(" %d", l);
                std::printf("\n  got levels:     ");
                for (size_t i = 0; i < result.levels.size(); ++i) {
                    if (result.removed[i])
                        std::printf(" -1");
                    else
                        std::printf(" %d", result.levels[i]);
                }

                std::printf("\n  expected order: ");
                for (int o : expectedOrder) std::printf(" %d", o);
                std::printf("\n  got order:      ");
                for (uint32_t v : result.visualOrder.visualToLogical) {
                    if (result.removed[v])
                        std::printf(" x");
                    else
                        std::printf(" %u", v);
                }
                std::printf("\n\n");
            }
        }
    }

    std::printf("\n");
    std::printf("========================================\n");
    std::printf("Results: %d / %d passed, %d failed\n",
                passed, total, failed);
    std::printf("  Failed levels only: %d\n", failedLevelsOnly);
    std::printf("  Failed order only:  %d\n", failedOrderOnly);
    std::printf("  Failed both:        %d\n", failedBoth);
    std::printf("========================================\n");

    return (failed == 0) ? 0 : 1;
}
