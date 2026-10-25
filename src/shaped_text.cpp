// src/shaped_text.cpp
#include "bidi/shaped_text.h"
#include "detail/arabic_persian_shaping_table.h"
#include "detail/utf.h"
#include "detail/bidi_mirroring_table.h"

namespace bidi {

struct ShapedText::Impl {
    BidiObject bidi;

    mutable std::u32string shapedCache32;
    mutable std::string shapedCache8;
    mutable bool cacheValid = false;

    void invalidate() {
        cacheValid = false;
        shapedCache32.clear();
        shapedCache32.shrink_to_fit();
        shapedCache8.clear();
        shapedCache8.shrink_to_fit();
    }

    void build() const {
        if (cacheValid) return;

        auto& self = const_cast<Impl&>(*this);

        auto& r = bidi.result();
        auto text = bidi.textViewUtf32();

        self.shapedCache32.clear();
        self.shapedCache32.reserve(r.visualToLogical.size());

        const size_t n = text.size();

        // Precompute joining types.
        std::vector<detail::JoiningType> jt(n);
        for (size_t i = 0; i < n; ++i)
            jt[i] = detail::joiningTypeOf(text[i]);

        // Precompute prevNonT[i] = index of the previous non-Transparent
        // character before i, or -1 if none.
        std::vector<int32_t> prevNonT(n, -1);
        {
            int32_t last = -1;
            for (size_t i = 0; i < n; ++i) {
                prevNonT[i] = last;
                if (jt[i] != detail::JoiningType::T)
                    last = static_cast<int32_t>(i);
            }
        }

        // Precompute nextNonT[i] = index of the next non-Transparent
        // character after i, or -1 if none.
        std::vector<int32_t> nextNonT(n, -1);
        {
            int32_t last = -1;
            for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i) {
                nextNonT[i] = last;
                if (jt[i] != detail::JoiningType::T)
                    last = i;
            }
        }

        // Build shaped visual string.
        for (uint32_t logicalIdx : r.visualToLogical) {
            if (logicalIdx >= text.size()) continue;

            // Skip removed characters (X9).
            bool removed = false;
            for (uint32_t idx : r.invisibleIndices) {
                if (idx == logicalIdx) { removed = true; break; }
            }
            if (removed) continue;

            char32_t cp = text[logicalIdx];

            // Previous non-T joining type (O(1) lookup).
            detail::JoiningType prev = detail::JoiningType::U;
            int32_t p = prevNonT[logicalIdx];
            if (p >= 0) prev = jt[p];

            // Next non-T joining type (O(1) lookup).
            detail::JoiningType next = detail::JoiningType::U;
            int32_t nx = nextNonT[logicalIdx];
            if (nx >= 0) next = jt[nx];

            // Apply Arabic/Persian shaping.
            char32_t shaped = detail::arabicPersianPresentationForm(
                cp, prev, next);

            // Apply mirroring.
            if (logicalIdx < r.mirrored.size() && r.mirrored[logicalIdx])
                shaped = detail::getMirror(shaped);

            self.shapedCache32.push_back(shaped);
        }

        // Convert to UTF-8.
        std::vector<char32_t> cps(
            self.shapedCache32.begin(), self.shapedCache32.end());
        self.shapedCache8 = detail::utf32ToUtf8(cps);

        self.cacheValid = true;
    }
};

ShapedText::ShapedText() : m_impl(new Impl()) {}
ShapedText::~ShapedText() = default;
ShapedText::ShapedText(ShapedText&&) noexcept = default;
ShapedText& ShapedText::operator=(ShapedText&&) noexcept = default;

void ShapedText::setText(std::string_view utf8) {
    m_impl->bidi.setText(utf8);
    m_impl->invalidate();
}

void ShapedText::setText(std::u32string_view utf32) {
    m_impl->bidi.setText(utf32);
    m_impl->invalidate();
}

void ShapedText::insert(size_t pos, std::string_view utf8) {
    m_impl->bidi.insert(pos, utf8);
    m_impl->invalidate();
}

void ShapedText::insert(size_t pos, std::u32string_view utf32) {
    m_impl->bidi.insert(pos, utf32);
    m_impl->invalidate();
}

void ShapedText::erase(size_t pos, size_t length) {
    m_impl->bidi.erase(pos, length);
    m_impl->invalidate();
}

void ShapedText::replace(size_t pos, size_t length,
                         std::string_view utf8) {
    m_impl->bidi.replace(pos, length, utf8);
    m_impl->invalidate();
}

void ShapedText::replace(size_t pos, size_t length,
                         std::u32string_view utf32) {
    m_impl->bidi.replace(pos, length, utf32);
    m_impl->invalidate();
}

void ShapedText::setBaseLevel(int level) {
    m_impl->bidi.setBaseLevel(level);
    m_impl->invalidate();
}

std::string_view ShapedText::textView() const {
    return m_impl->bidi.textView();
}

std::u32string_view ShapedText::textViewUtf32() const {
    return m_impl->bidi.textViewUtf32();
}

std::string ShapedText::text() const {
    return m_impl->bidi.text();
}

std::u32string ShapedText::textUtf32() const {
    return m_impl->bidi.textUtf32();
}

int ShapedText::baseLevel() const {
    return m_impl->bidi.baseLevel();
}

std::string ShapedText::toShapedVisualString() const {
    m_impl->build();
    return m_impl->shapedCache8;
}

std::u32string ShapedText::toShapedVisualStringUtf32() const {
    m_impl->build();
    return m_impl->shapedCache32;
}

std::string_view ShapedText::shapedVisualView() const {
    m_impl->build();
    return m_impl->shapedCache8;
}

std::u32string_view ShapedText::shapedVisualViewUtf32() const {
    m_impl->build();
    return m_impl->shapedCache32;
}

const BidiObject& ShapedText::bidi() const {
    return m_impl->bidi;
}

void ShapedText::invalidateCache() {
    m_impl->invalidate();
}

void ShapedText::clearCaches() {
    m_impl->bidi.clearCaches();
    m_impl->invalidate();
}

} // namespace bidi
