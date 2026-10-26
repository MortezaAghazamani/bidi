// include/bidi/bidi.h
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file bidi.h
 * @brief C++17 Unicode Bidirectional Algorithm (UAX #9) library.
 *
 * 100% conformant with the official Unicode conformance test
 * (BidiCharacterTest.txt): 91,707 / 91,707 tests passed.
 *
 * @section encoding Encoding
 * Internally, the text is stored as UTF-32 (`std::u32string`).
 * Input and output are available as both UTF-8 and UTF-32.
 * UTF-16 is not supported.
 *
 * @section positions Positions
 * Positions (`pos` parameters) are always in **UTF-32 code units**
 * (i.e., one unit per Unicode code point). This is the same as
 * `textViewUtf32()`.
 *
 * @section view_copy Views vs. copies
 * Methods ending in `View` return zero-copy views. They are fast but
 * become invalid after any text modification.
 *
 * Methods without `View` return copies. They are safe to store but
 * allocate memory.
 */

namespace bidi {

/**
 * @brief Resolved direction of a character or run.
 */
enum class Direction : uint8_t {
    LTR,     ///< Left-to-right
    RTL,     ///< Right-to-left
    Neutral  ///< Neutral (should not appear in resolved runs)
};

/**
 * @brief A visual run: a maximal sequence of characters sharing the
 *        same embedding level, in display order.
 */
struct Run {
    size_t logicalStart;     ///< Start index in logical order (UTF-32).
    size_t logicalLength;    ///< Length in logical order (UTF-32).
    int level;               ///< Embedding level (even = LTR, odd = RTL).
    Direction direction;     ///< Resolved direction (LTR or RTL).
    bool containsInvisible;  ///< True if the run contains removed chars.
};

/**
 * @brief Result of bidirectional analysis.
 *
 * All per-character arrays have the same size as the input text
 * (in UTF-32 code units). For per-paragraph results, indices are
 * local to the paragraph.
 */
struct AnalysisResult {
    std::vector<uint8_t> levels;        ///< Per-character embedding level.
    std::vector<uint8_t> classes;       ///< Per-character Bidi_Class.
    std::vector<Direction> directions;  ///< Per-character direction.
    std::vector<uint8_t> mirrored;      ///< Per-character mirror flag.
    std::vector<uint32_t> invisibleIndices; ///< Indices removed by X9.
    std::vector<Run> runs;              ///< Visual runs in display order.
    std::vector<uint32_t> visualToLogical; ///< Visual → logical index.
    std::vector<uint32_t> logicalToVisual; ///< Logical → visual index.
    int paragraphLevel = 0;             ///< Paragraph level of first para.
    std::vector<int> paragraphLevels;   ///< Per-character para level.
    std::vector<uint32_t> paragraphStart; ///< Paragraph boundaries.

    /**
     * @brief Build the visual string (UTF-8) from the logical text.
     *
     * @param logicalText     The original logical text (UTF-8).
     * @param applyMirroring  Whether to apply mirroring (default true).
     * @return The visual string (UTF-8).
     */
    std::string toVisualString(std::string_view logicalText,
                               bool applyMirroring = true) const;

    /**
     * @brief Build the visual string (UTF-32) from the logical text.
     *
     * @param logicalText     The original logical text (UTF-32).
     * @param applyMirroring  Whether to apply mirroring (default true).
     * @return The visual string (UTF-32).
     */
    std::u32string toVisualStringUtf32(std::u32string_view logicalText,
                                       bool applyMirroring = true) const;
};

/**
 * @brief Main class for bidirectional text analysis.
 *
 * @section threading Thread safety
 * Not thread-safe. Use one object per thread.
 */
class BidiObject {
public:
    BidiObject();
    ~BidiObject();
    BidiObject(BidiObject&&) noexcept;
    BidiObject& operator=(BidiObject&&) noexcept;
    BidiObject(const BidiObject&) = delete;
    BidiObject& operator=(const BidiObject&) = delete;

    // -----------------------------------------------------------------
    // Text management
    // -----------------------------------------------------------------

    /**
     * @brief Set the text from UTF-8.
     *
     * Invalidates all caches.
     *
     * @param utf8  The new text (UTF-8).
     */
    void setText(std::string_view utf8);

    /**
     * @brief Set the text from UTF-32.
     *
     * Invalidates all caches.
     *
     * @param utf32  The new text (UTF-32).
     */
    void setText(std::u32string_view utf32);

    /**
     * @brief Insert UTF-8 text at the given position.
     *
     * Preserves caches for paragraphs whose text did not change.
     *
     * @param pos   Position in UTF-32 code units (0 = beginning).
     * @param utf8  Text to insert (UTF-8).
     */
    void insert(size_t pos, std::string_view utf8);

    /**
     * @brief Insert UTF-32 text at the given position.
     *
     * @param pos    Position in UTF-32 code units (0 = beginning).
     * @param utf32  Text to insert (UTF-32).
     */
    void insert(size_t pos, std::u32string_view utf32);

    /**
     * @brief Erase UTF-32 code units.
     *
     * @param pos     Start position in UTF-32 code units.
     * @param length  Number of code units to erase.
     */
    void erase(size_t pos, size_t length);

    /**
     * @brief Replace UTF-32 code units with new UTF-8 text.
     *
     * @param pos     Start position in UTF-32 code units.
     * @param length  Number of code units to replace.
     * @param utf8    Replacement text (UTF-8).
     */
    void replace(size_t pos, size_t length, std::string_view utf8);

    /**
     * @brief Replace UTF-32 code units with new UTF-32 text.
     *
     * @param pos     Start position in UTF-32 code units.
     * @param length  Number of code units to replace.
     * @param utf32   Replacement text (UTF-32).
     */
    void replace(size_t pos, size_t length, std::u32string_view utf32);

    /**
     * @brief Set the base level for all paragraphs.
     *
     * @param level  -1 = auto (P2/P3), 0 = LTR, 1 = RTL.
     *
     * Invalidates all caches.
     */
    void setBaseLevel(int level);

    // -----------------------------------------------------------------
    // Whole-text access
    // -----------------------------------------------------------------

    /**
     * @brief Returns a zero-copy view of the current text (UTF-8).
     *
     * The UTF-8 representation is computed lazily and cached. The
     * view is valid only until the text is modified.
     *
     * For a safe copy, use text().
     *
     * @warning Invalidated by any text modification.
     */
    std::string_view textView() const;

    /**
     * @brief Returns a zero-copy view of the current text (UTF-32).
     *
     * The view is valid only until the text is modified.
     *
     * For a safe copy, use textUtf32().
     *
     * @warning Invalidated by any text modification.
     */
    std::u32string_view textViewUtf32() const;

    /**
     * @brief Returns a copy of the current text (UTF-8).
     *
     * Safe to store.
     */
    std::string text() const;

    /**
     * @brief Returns a copy of the current text (UTF-32).
     *
     * Safe to store.
     */
    std::u32string textUtf32() const;

    /**
     * @brief Returns the current base level (-1, 0, or 1).
     */
    int baseLevel() const;

    // -----------------------------------------------------------------
    // Analysis (full text)
    // -----------------------------------------------------------------

    /**
     * @brief Get the full-text analysis result (cached).
     */
    const AnalysisResult& result() const;

    /**
     * @brief Get the visual string (UTF-8) with mirroring.
     */
    std::string toVisualString() const;

    /**
     * @brief Get the visual string (UTF-32) with mirroring.
     */
    std::u32string toVisualStringUtf32() const;

    // -----------------------------------------------------------------
    // Per-paragraph analysis
    // -----------------------------------------------------------------

    /**
     * @brief Get the number of paragraphs.
     */
    size_t paragraphCount() const;

    /**
     * @brief Returns a zero-copy view of the paragraph text (UTF-8).
     *
     * @param index  Paragraph index (0-based).
     * @warning Invalidated by any text modification.
     */
    std::string_view paragraphTextView(size_t index) const;

    /**
     * @brief Returns a zero-copy view of the paragraph text (UTF-32).
     *
     * @param index  Paragraph index (0-based).
     * @warning Invalidated by any text modification.
     */
    std::u32string_view paragraphTextViewUtf32(size_t index) const;

    /**
     * @brief Returns a copy of the paragraph text (UTF-8).
     *
     * @param index  Paragraph index (0-based).
     */
    std::string paragraphText(size_t index) const;

    /**
     * @brief Returns a copy of the paragraph text (UTF-32).
     *
     * @param index  Paragraph index (0-based).
     */
    std::u32string paragraphTextUtf32(size_t index) const;

    /**
     * @brief Get the analysis result of the given paragraph (cached).
     *
     * @param index  Paragraph index (0-based).
     */
    const AnalysisResult& paragraphResult(size_t index) const;

    /**
     * @brief Get the paragraph level of the given paragraph.
     *
     * @param index  Paragraph index (0-based).
     * @return 0 for LTR, 1 for RTL, or 0 if out of range.
     */
    int paragraphLevel(size_t index) const;

    /**
     * @brief Get the paragraph index containing the given position.
     *
     * @param pos  Position in UTF-32 code units.
     */
    size_t paragraphContaining(size_t pos) const;

    // -----------------------------------------------------------------
    // Per-character queries (positions in UTF-32 code units)
    // -----------------------------------------------------------------

    Direction directionAt(size_t pos) const;
    int levelAt(size_t pos) const;
    bool isMirrored(size_t pos) const;
    const Run* runAt(size_t pos) const;
    size_t logicalToVisualAt(size_t logicalPos) const;
    size_t visualToLogicalAt(size_t visualPos) const;

    // -----------------------------------------------------------------
    // Cache management
    // -----------------------------------------------------------------

    void invalidateCache();
    void clearCaches();
    void reserve(size_t textSize);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace bidi
