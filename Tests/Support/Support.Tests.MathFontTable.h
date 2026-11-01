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

    std::array<std::uint8_t, 80u> assemblyBytes{};
    const auto put = [&](std::size_t offset, std::uint16_t value)
    {
        assemblyBytes[offset]      = static_cast<std::uint8_t>(value >> 8u);
        assemblyBytes[offset + 1u] = static_cast<std::uint8_t>(value & 0xffu);
    };
    put(0u, 1u);
    put(8u, 10u);  // MATH variants
    put(10u, 10u); // minimum overlap
    put(20u, 8u);  // construction -> assembly
    put(32u, 3u);  // three bottom-to-top parts
    put(34u, 11u); // bottom part
    put(38u, 20u);
    put(40u, 100u);
    put(44u, 12u); // repeatable middle part
    put(46u, 20u);
    put(48u, 20u);
    put(50u, 80u);
    put(52u, 1u);
    put(54u, 13u); // top part
    put(56u, 20u);
    put(60u, 100u);
    const DxUi::RichText::Detail::MathFontTable assembly(assemblyBytes);
    const auto compact = assembly.VerticalAssemblyAt(20u, 185u), stretched = assembly.VerticalAssemblyAt(20u, 320u);
    check(compact && compact->count == 2u && compact->advance == 185u && compact->placements[1u].offset == 85u,
          "vertical MATH assembly can omit extenders and distribute connector overlap");
    check(stretched && stretched->count == 4u && stretched->advance == 320u && stretched->placements[0u].part.glyph == 11u &&
              stretched->placements[1u].part.glyph == 12u && stretched->placements[2u].part.glyph == 12u && stretched->placements[3u].part.glyph == 13u &&
              stretched->placements[1u].offset == 87u && stretched->placements[2u].offset == 154u && stretched->placements[3u].offset == 220u,
          "vertical MATH assembly repeats extenders symmetrically and computes bounded offsets");
    for (std::size_t length = 0u; length < 64u; ++length)
        check(! DxUi::RichText::Detail::MathFontTable(std::span(assemblyBytes.data(), length)).VerticalAssemblyAt(20u, 320u),
              "truncated glyph assemblies fall back without reading beyond the table");
    check(! assembly.VerticalAssemblyAt(20u, 1000000u), "glyph assemblies cap the number of repeated parts");
    const auto horizontalAssembly = assembly.HorizontalAssemblyAt(20u, 320u);
    check(horizontalAssembly && horizontalAssembly->count == 4u && horizontalAssembly->advance == 320u &&
              horizontalAssembly->placements[1u].offset == 87u && horizontalAssembly->placements[3u].offset == 220u,
          "horizontal assemblies use the independently expected left-to-right connector placements");
    put(52u, 2u);
    check(! assembly.VerticalAssemblyAt(20u, 320u), "unknown glyph-part flags are rejected");
    put(52u, 1u);
    put(38u, 5u);
    check(! assembly.VerticalAssemblyAt(20u, 320u), "incompatible connector overlap is rejected");
    std::array<std::uint8_t, 64u> horizontalBytes{};
    const auto setHorizontal = [&](std::size_t offset, std::uint16_t value)
    {
        horizontalBytes[offset] = static_cast<std::uint8_t>(value >> 8u);
        horizontalBytes[offset + 1u] = static_cast<std::uint8_t>(value & 0xffu);
    };
    setHorizontal(0u, 1u);
    setHorizontal(8u, 10u);  // variants table
    setHorizontal(14u, 16u); // horizontal coverage at 26
    setHorizontal(16u, 1u);  // one vertical offset precedes horizontal offsets
    setHorizontal(18u, 1u);  // one horizontal construction
    setHorizontal(22u, 30u); // construction at 40
    setHorizontal(26u, 1u);
    setHorizontal(28u, 1u);
    setHorizontal(30u, 5u);
    setHorizontal(42u, 2u);
    setHorizontal(44u, 11u);
    setHorizontal(46u, 1000u);
    setHorizontal(48u, 12u);
    setHorizontal(50u, 2000u);
    const DxUi::RichText::Detail::MathFontTable horizontal(horizontalBytes);
    const auto construction = horizontal.HorizontalConstruction(5u);
    const auto selected = construction ? horizontal.HorizontalVariantAt(construction.value(), 1500u) : std::nullopt;
    check(construction && construction.value() == 40u && selected && selected->glyph == 12u && selected->advance == 2000u &&
              ! horizontal.HorizontalConstruction(6u),
          "horizontal coverage follows the vertical construction array and chooses the smallest adequate variant");
    for (std::size_t length = 0u; length < 52u; ++length)
    {
        const DxUi::RichText::Detail::MathFontTable truncated(std::span(horizontalBytes.data(), length));
        const auto partial = truncated.HorizontalConstruction(5u);
        check(! partial || ! truncated.HorizontalVariantAt(partial.value(), 1500u),
              "truncated horizontal variants never read beyond the supplied table");
    }
}
} // namespace MathFontTableTests
