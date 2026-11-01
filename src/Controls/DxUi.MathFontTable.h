#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace DxUi::RichText::Detail
{
// OpenType MATH offsets are untrusted font bytes. These helpers never allocate or read outside the supplied table.
class MathFontTable final
{
    std::span<const std::uint8_t> bytes;

    [[nodiscard]] std::optional<std::uint16_t> U16(std::size_t offset) const noexcept
    {
        if (offset > bytes.size() || bytes.size() - offset < 2u)
            return std::nullopt;
        return static_cast<std::uint16_t>((static_cast<unsigned>(bytes[offset]) << 8u) | bytes[offset + 1u]);
    }
    [[nodiscard]] std::optional<std::size_t> Coverage(std::size_t offset, std::uint16_t glyph) const noexcept
    {
        const auto format = U16(offset), count = U16(offset + 2u);
        if (! format || ! count)
            return std::nullopt;
        const std::size_t stride = format.value() == 1u ? 2u : format.value() == 2u ? 6u : 0u;
        if (stride == 0u || offset > bytes.size() || bytes.size() - offset < 4u || count.value() > (bytes.size() - offset - 4u) / stride)
            return std::nullopt;
        // OpenType coverage is sorted. Binary search bounds work even for a very large or malformed table.
        std::size_t first = 0u, last = count.value();
        while (first < last)
        {
            const auto middle = first + (last - first) / 2u;
            const auto start  = U16(offset + 4u + middle * stride);
            const auto end    = format.value() == 1u ? start : U16(offset + 6u + middle * stride);
            if (! start || ! end || start.value() > end.value())
                return std::nullopt;
            if (glyph < start.value())
                last = middle;
            else if (glyph > end.value())
                first = middle + 1u;
            else if (format.value() == 1u)
                return middle;
            else
            {
                const auto index = U16(offset + 8u + middle * stride);
                return index ? std::optional<std::size_t>(index.value() + static_cast<std::size_t>(glyph - start.value())) : std::nullopt;
            }
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::size_t> HeaderOffset(std::size_t field) const noexcept
    {
        const auto major = U16(0u), minor = U16(2u), offset = U16(field);
        if (! major || ! minor || ! offset || major.value() != 1u || minor.value() != 0u || offset.value() == 0u || offset.value() >= bytes.size())
            return std::nullopt;
        return offset.value();
    }

public:
    explicit MathFontTable(std::span<const std::uint8_t> source) noexcept : bytes(source)
    {
    }
    [[nodiscard]] std::optional<std::int16_t> ItalicCorrection(std::uint16_t glyph) const noexcept
    {
        const auto info = HeaderOffset(6u);
        if (! info)
            return std::nullopt;
        const auto relative = U16(info.value());
        if (! relative || relative.value() == 0u)
            return std::nullopt;
        const auto table    = info.value() + relative.value();
        const auto coverage = U16(table), count = U16(table + 2u);
        if (! coverage || ! count || coverage.value() == 0u)
            return std::nullopt;
        const auto index = Coverage(table + coverage.value(), glyph);
        if (! index || index.value() >= count.value())
            return std::nullopt;
        const auto value  = U16(table + 4u + index.value() * 4u);
        const auto device = U16(table + 6u + index.value() * 4u);
        if (! value || ! device)
            return std::nullopt;
        return static_cast<std::int16_t>(value.value());
    }
    struct Variant
    {
        std::uint16_t glyph{}, advance{};
    };
    [[nodiscard]] std::optional<std::size_t> VerticalConstruction(std::uint16_t glyph) const noexcept
    {
        const auto table = HeaderOffset(8u);
        if (! table)
            return std::nullopt;
        const auto coverage = U16(table.value() + 2u), count = U16(table.value() + 6u);
        if (! coverage || ! count || coverage.value() == 0u)
            return std::nullopt;
        const auto index = Coverage(table.value() + coverage.value(), glyph);
        if (! index || index.value() >= count.value())
            return std::nullopt;
        const auto relative = U16(table.value() + 10u + index.value() * 2u);
        if (! relative || relative.value() == 0u)
            return std::nullopt;
        return table.value() + relative.value();
    }
    [[nodiscard]] std::optional<Variant> VerticalVariantAt(std::size_t construction, std::uint16_t minimum) const noexcept
    {
        const auto variants = U16(construction + 2u);
        if (! variants)
            return std::nullopt;
        if (construction > bytes.size() || bytes.size() - construction < 4u || variants.value() > (bytes.size() - construction - 4u) / 4u)
            return std::nullopt;
        std::size_t first = 0u, last = variants.value();
        while (first < last)
        {
            const auto middle  = first + (last - first) / 2u;
            const auto advance = U16(construction + 6u + middle * 4u);
            if (! advance)
                return std::nullopt;
            if (advance.value() < minimum)
                first = middle + 1u;
            else
                last = middle;
        }
        if (first >= variants.value())
            return std::nullopt;
        const auto candidate = U16(construction + 4u + first * 4u), advance = U16(construction + 6u + first * 4u);
        if (! candidate || ! advance || candidate.value() == 0u)
            return std::nullopt;
        return Variant{candidate.value(), advance.value()};
    }
    [[nodiscard]] std::optional<Variant> VerticalVariant(std::uint16_t glyph, std::uint16_t minimum) const noexcept
    {
        const auto construction = VerticalConstruction(glyph);
        return construction ? VerticalVariantAt(construction.value(), minimum) : std::nullopt;
    }
};
} // namespace DxUi::RichText::Detail
