// src/detail/bidi_algorithm.h
#pragma once
#include <string_view>
#include <vector>
#include <cstdint>
#include "bidi_class_table.h"
#include "explicit_levels.h"
#include "reorder.h"

namespace bidi::detail {

struct ParagraphAnalysis {
    std::vector<char32_t> codepoints;
    std::vector<BidiClass> originalTypes;
    std::vector<BidiClass> types;
    std::vector<int> levels;
    std::vector<uint8_t> removed;
    VisualOrder visualOrder;
    int paragraphLevel = 0;
    BidiClass sos = BidiClass::L;
    BidiClass eos = BidiClass::L;
};

ParagraphAnalysis analyzeParagraph(
    const std::vector<char32_t>& codepoints,
    int requestedBaseLevel
);

// Multi-paragraph analysis result.
struct TextAnalysis {
    // Per-character levels (same size as input).
    std::vector<int> levels;

    // Per-character paragraph level (same size as input).
    std::vector<int> paragraphLevels;

    // Per-character removed flag (same size as input).
    std::vector<uint8_t> removed;

    // Visual order across the whole text.
    VisualOrder visualOrder;

    // Per-character paragraph index (same size as input).
    std::vector<uint32_t> paragraphIndex;

    // Paragraph boundaries: paragraphStart[i] is the logical index of
    // the first character of paragraph i. The last entry is the total
    // length (one-past-the-end).
    std::vector<uint32_t> paragraphStart;
};

// Analyzes a text that may contain multiple paragraphs.
// `requestedBaseLevel` applies to all paragraphs (-1 = auto per paragraph).
TextAnalysis analyzeText(
    const std::vector<char32_t>& codepoints,
    int requestedBaseLevel
);

} // namespace bidi::detail
