// include/bidi/shaped_text.h
#pragma once

#include "bidi/bidi.h"

namespace bidi {

/**
 * @brief Shaped text: visual order + Arabic/Persian presentation forms.
 *
 * Wraps a BidiObject. The visual string is produced in the correct
 * visual order and with Arabic and Persian characters replaced by
 * their presentation forms (isolated/initial/medial/final), so that
 * renderers without OpenType shaping (such as ImGui's default text
 * renderer) display the text correctly.
 *
 * Arabic/Persian shaping is based on the Joining_Type property from
 * ArabicShaping.txt. See UAX #9 L3 and the Unicode Standard,
 * Chapter 9 (Middle East-I).
 *
 * This class is optional. It is only available when the library is
 * built with BIDI_BUILD_SHAPING=ON (the default).
 */
class ShapedText {
public:
    ShapedText();
    ~ShapedText();
    ShapedText(ShapedText&&) noexcept;
    ShapedText& operator=(ShapedText&&) noexcept;
    ShapedText(const ShapedText&) = delete;
    ShapedText& operator=(const ShapedText&) = delete;

    // -----------------------------------------------------------------
    // Text management (delegates to BidiObject)
    // -----------------------------------------------------------------

    void setText(std::string_view utf8);
    void setText(std::u32string_view utf32);

    void insert(size_t pos, std::string_view utf8);
    void insert(size_t pos, std::u32string_view utf32);
    void erase(size_t pos, size_t length);
    void replace(size_t pos, size_t length, std::string_view utf8);
    void replace(size_t pos, size_t length, std::u32string_view utf32);

    void setBaseLevel(int level);

    // -----------------------------------------------------------------
    // Text access
    // -----------------------------------------------------------------

    std::string_view textView() const;
    std::u32string_view textViewUtf32() const;
    std::string text() const;
    std::u32string textUtf32() const;
    int baseLevel() const;

    // -----------------------------------------------------------------
    // Shaped output
    // -----------------------------------------------------------------

    /**
     * @brief Returns the shaped visual string (UTF-8).
     *
     * The string is in visual order and has Arabic/Persian
     * presentation forms applied. Mirroring is applied to brackets.
     * Computed lazily and cached.
     */
    std::string toShapedVisualString() const;

    /**
     * @brief Returns the shaped visual string (UTF-32).
     */
    std::u32string toShapedVisualStringUtf32() const;

    /**
     * @brief Returns a zero-copy view of the shaped visual string
     *        (UTF-8).
     *
     * @warning Invalidated by any text modification.
     */
    std::string_view shapedVisualView() const;

    /**
     * @brief Returns a zero-copy view of the shaped visual string
     *        (UTF-32).
     *
     * @warning Invalidated by any text modification.
     */
    std::u32string_view shapedVisualViewUtf32() const;

    // -----------------------------------------------------------------
    // Access underlying BidiObject
    // -----------------------------------------------------------------

    const BidiObject& bidi() const;

    // -----------------------------------------------------------------
    // Cache management
    // -----------------------------------------------------------------

    void invalidateCache();
    void clearCaches();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace bidi
