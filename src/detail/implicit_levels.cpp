// src/detail/implicit_levels.cpp
#include "implicit_levels.h"
#include <cassert>

namespace bidi::detail {

void resolveImplicitLevels(
    const std::vector<BidiClass>& types,
    std::vector<int>& levels)
{
    const size_t n = types.size();
    assert(levels.size() == n);

#ifdef _DEBUG
    // After W and N rules, every active character must have one of these
    // four types. If not, there is a bug in the earlier stages.
    for (size_t i = 0; i < n; ++i) {
        BidiClass t = types[i];
        assert(t == BidiClass::L || t == BidiClass::R ||
               t == BidiClass::EN || t == BidiClass::AN);
    }
#endif

    for (size_t i = 0; i < n; ++i) {
        int level = levels[i];
        BidiClass t = types[i];

        if ((level & 1) == 0) {
            // I1: even (LTR) embedding level.
            switch (t) {
                case BidiClass::R:
                    levels[i] = level + 1;
                    break;
                case BidiClass::EN:
                case BidiClass::AN:
                    levels[i] = level + 2;
                    break;
                default:
                    // L: unchanged.
                    break;
            }
        }
        else {
            // I2: odd (RTL) embedding level.
            switch (t) {
                case BidiClass::L:
                case BidiClass::EN:
                case BidiClass::AN:
                    levels[i] = level + 1;
                    break;
                default:
                    // R: unchanged.
                    break;
            }
        }
    }
}

} // namespace bidi::detail
