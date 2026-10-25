// src/detail/utf.h
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace bidi::detail {

// UTF-8 -> UTF-32. Invalid sequences become U+FFFD.
// Uses a table-based decoder (2-3x faster than naive scalar).
std::vector<char32_t> utf8ToUtf32(std::string_view text);

// UTF-32 -> UTF-8.
std::string utf32ToUtf8(const std::vector<char32_t>& codepoints);
std::string utf32ToUtf8(std::u32string_view text);

} // namespace bidi::detail
