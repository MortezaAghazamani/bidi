// src/detail/utf.cpp
#include "utf.h"

namespace bidi::detail {

namespace {

constexpr char32_t kReplacement = 0xFFFD;

// ------------------------------------------------------------------
// UTF-8 -> UTF-32 (table-based)
// ------------------------------------------------------------------
struct Utf8Info {
    uint8_t length;   // 0 = invalid, 1..4 = bytes
    uint32_t mask;    // mask for the first byte
    uint32_t minCp;   // minimum valid code point
};

struct Utf8TableBuilder {
    Utf8Info data[256]{};
    constexpr Utf8TableBuilder() {
        for (int i = 0; i < 0x80; ++i)
            data[i] = {1, 0x7F, 0x00};
        for (int i = 0x80; i < 0xC0; ++i)
            data[i] = {0, 0, 0};
        for (int i = 0xC0; i < 0xE0; ++i)
            data[i] = {2, 0x1F, 0x80};
        for (int i = 0xE0; i < 0xF0; ++i)
            data[i] = {3, 0x0F, 0x800};
        for (int i = 0xF0; i < 0xF8; ++i)
            data[i] = {4, 0x07, 0x10000};
        for (int i = 0xF8; i < 0x100; ++i)
            data[i] = {0, 0, 0};
    }
};

constexpr Utf8TableBuilder kUtf8Builder;
constexpr const Utf8Info* kUtf8 = kUtf8Builder.data;

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            // Surrogate: emit replacement.
            out.push_back(static_cast<char>(0xEF));
            out.push_back(static_cast<char>(0xBF));
            out.push_back(static_cast<char>(0xBD));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

std::vector<char32_t> utf8ToUtf32(std::string_view text) {
    std::vector<char32_t> out;
    out.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        uint8_t b = static_cast<uint8_t>(text[i]);
        const Utf8Info& info = kUtf8[b];

        if (info.length == 0) {
            out.push_back(kReplacement);
            ++i;
            continue;
        }

        if (i + info.length > text.size()) {
            out.push_back(kReplacement);
            break;
        }

        char32_t cp = b & info.mask;
        bool valid = true;
        for (uint8_t k = 1; k < info.length; ++k) {
            uint8_t cont = static_cast<uint8_t>(text[i + k]);
            if ((cont & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (cont & 0x3F);
        }

        if (!valid || cp < info.minCp ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
        {
            out.push_back(kReplacement);
        } else {
            out.push_back(cp);
        }
        i += info.length;
    }

    return out;
}

std::string utf32ToUtf8(const std::vector<char32_t>& codepoints) {
    std::string out;
    out.reserve(codepoints.size() * 2);
    for (char32_t cp : codepoints)
        appendUtf8(out, cp);
    return out;
}

std::string utf32ToUtf8(std::u32string_view text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (char32_t cp : text)
        appendUtf8(out, cp);
    return out;
}

} // namespace bidi::detail
