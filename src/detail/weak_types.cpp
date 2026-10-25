// src/detail/weak_types.cpp
#include "weak_types.h"

namespace bidi::detail {

void resolveWeakTypes(
    std::vector<BidiClass>& types,
    BidiClass sos,
    BidiClass /*eos*/)
{
    const size_t n = types.size();
    if (n == 0) return;

    // W1: NSM takes the type of the previous character.
    // If the previous character is an isolate initiator or PDI,
    // the NSM becomes ON. At the start of the sequence, it takes sos.
    for (size_t i = 0; i < n; ++i) {
        if (types[i] != BidiClass::NSM)
            continue;

        BidiClass prev;
        if (i == 0) {
            prev = sos;
        }
        else {
            BidiClass p = types[i - 1];
            if (isIsolateInitiator(p) || p == BidiClass::PDI)
                prev = BidiClass::ON;
            else
                prev = p;
        }
        types[i] = prev;
    }

    // W2: EN with a prior strong AL becomes AN.
    {
        BidiClass lastStrong = sos;
        for (size_t i = 0; i < n; ++i) {
            BidiClass t = types[i];
            if (isStrong(t))
                lastStrong = t;
            else if (t == BidiClass::EN && lastStrong == BidiClass::AL)
                types[i] = BidiClass::AN;
        }
    }

    // W3: AL becomes R.
    for (size_t i = 0; i < n; ++i) {
        if (types[i] == BidiClass::AL)
            types[i] = BidiClass::R;
    }

    // W4: A single ES between two EN becomes EN.
    //     A single CS between two EN becomes EN; between two AN becomes AN.
    for (size_t i = 1; i + 1 < n; ++i) {
        BidiClass t = types[i];
        if (t != BidiClass::ES && t != BidiClass::CS)
            continue;

        BidiClass prev = types[i - 1];
        BidiClass next = types[i + 1];

        if (t == BidiClass::ES) {
            if (prev == BidiClass::EN && next == BidiClass::EN)
                types[i] = BidiClass::EN;
        }
        else { // CS
            if (prev == BidiClass::EN && next == BidiClass::EN)
                types[i] = BidiClass::EN;
            else if (prev == BidiClass::AN && next == BidiClass::AN)
                types[i] = BidiClass::AN;
        }
    }

    // W5: A sequence of ETs adjacent to EN becomes EN.
    // "Adjacent" means immediately before or after an EN, after W4.
    {
        size_t i = 0;
        while (i < n) {
            if (types[i] != BidiClass::ET) { ++i; continue; }

            size_t start = i;
            size_t end = i;
            while (end < n && types[end] == BidiClass::ET)
                ++end;

            bool adjacentToEN = false;
            if (start > 0 && types[start - 1] == BidiClass::EN)
                adjacentToEN = true;
            if (end < n && types[end] == BidiClass::EN)
                adjacentToEN = true;

            if (adjacentToEN) {
                for (size_t k = start; k < end; ++k)
                    types[k] = BidiClass::EN;
            }

            i = end;
        }
    }

    // W6: Remaining ES, ET, CS become ON.
    for (size_t i = 0; i < n; ++i) {
        BidiClass t = types[i];
        if (t == BidiClass::ES || t == BidiClass::ET || t == BidiClass::CS)
            types[i] = BidiClass::ON;
    }

    // W7: EN with a prior strong L becomes L.
    {
        BidiClass lastStrong = sos;
        for (size_t i = 0; i < n; ++i) {
            BidiClass t = types[i];
            if (t == BidiClass::L || t == BidiClass::R)
                lastStrong = t;
            else if (t == BidiClass::EN && lastStrong == BidiClass::L)
                types[i] = BidiClass::L;
        }
    }
}

} // namespace bidi::detail
