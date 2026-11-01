#pragma once
#include "Controls/DxUi.MathFontTable.h"
#include <array>
#include <cstdint>
#include <span>

namespace MathFontTableTests
{
template <typename Check> void Run(Check&& check)
{
    std::array<std::uint8_t, 80u> bytes{};
    const auto set = [&](std::size_t offset, std::uint16_t value)
    {
        bytes[offset]      = static_cast<std::uint8_t>(value >> 8u);
        bytes[offset + 1u] = static_cast<std::uint8_t>(value & 0xffu);
    };
    set(0u, 1u);
    set(6u, 10u);
    set(8u, 40u);
    set(10u, 8u); // MathGlyphInfo -> italic corrections
    set(18u, 12u);
    set(20u, 2u);
    set(22u, 125u);
    set(26u, 0xffceu);
    set(30u, 1u);
    set(32u, 2u);
    set(34u, 5u);
    set(36u, 9u); // format 1 coverage
    set(42u, 14u);
    set(46u, 1u);
    set(50u, 22u); // vertical coverage and construction
    set(54u, 1u);
    set(56u, 1u);
    set(58u, 5u);
    set(64u, 2u);
    set(66u, 11u);
    set(68u, 1000u);
    set(70u, 12u);
    set(72u, 2000u);
    const DxUi::RichText::Detail::MathFontTable table(bytes);
    check(table.ItalicCorrection(5u).value_or(0) == 125 && table.ItalicCorrection(9u).value_or(0) == -50 && ! table.ItalicCorrection(6u),
          "MATH italic correction follows coverage order and signed design units");
    const auto first = table.VerticalVariant(5u, 800u), second = table.VerticalVariant(5u, 1500u);
    check(first && first.value().glyph == 11u && first.value().advance == 1000u && second && second.value().glyph == 12u && second.value().advance == 2000u &&
              ! table.VerticalVariant(5u, 2001u) && ! table.VerticalVariant(9u, 1u),
          "MATH chooses the smallest adequate vertical variant and falls back when no variant covers the request");
    for (std::size_t length = 0u; length < 30u; ++length)
        check(! DxUi::RichText::Detail::MathFontTable(std::span(bytes.data(), length)).ItalicCorrection(5u),
              "truncated MATH italic tables are rejected without reading beyond the buffer");
    for (std::size_t length = 0u; length < 74u; ++length)
        check(! DxUi::RichText::Detail::MathFontTable(std::span(bytes.data(), length)).VerticalVariant(5u, 1500u),
              "truncated MATH variant tables fall back without reading beyond the buffer");
    set(30u, 2u);
    set(32u, 1u);
    set(34u, 5u);
    set(36u, 6u);
    set(38u, 0u); // format 2 range coverage
    const DxUi::RichText::Detail::MathFontTable ranged(bytes);
    check(ranged.ItalicCorrection(5u).value_or(0) == 125 && ranged.ItalicCorrection(6u).value_or(0) == -50 && ! ranged.ItalicCorrection(7u),
          "MATH range coverage resolves glyph offsets correctly");
    set(36u, 4u);
    check(! ranged.ItalicCorrection(5u), "invalid coverage ranges are rejected");
}
} // namespace MathFontTableTests
