// bidi/include/bidi/text_utils.h
//
// Reverse the Persian/Arabic presentation forms back to base characters.
//
#pragma once

#include <string>
#include <string_view>


namespace bidi {

/**
 * @brief Convert a string of Persian/Arabic presentation forms
 *        back to base codepoints.
 */
std::u32string unshapeToLogical(std::u32string_view visual);

/**
 * @brief Convenience overload for UTF-8 input/output.
 */
std::string unshapeToLogical(std::string_view visualUtf8);

/**
 * @brief Shape a UTF-8 logical string into a UTF-8 visual string.
 */
std::string shapeToVisual(std::string_view logicalUtf8,
    int base_level = -1);

/**
 * @brief Shape a UTF-32 logical string into a UTF-32 visual string.
 */
std::u32string shapeToVisual(std::u32string_view logicalUtf32,
    int base_level = -1);

} // namespace bidi
