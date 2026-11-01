#pragma once
#include <algorithm>
#include <array>
#include <cmath>
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
    struct ValueRecord
    {
        std::int16_t value{};
        std::size_t device{}; // Absolute table offset, zero when no device correction exists.
    };
    [[nodiscard]] std::optional<ValueRecord> Value(std::size_t parent, std::size_t offset) const noexcept
    {
        if (offset > bytes.size() || parent > bytes.size() - offset)
            return std::nullopt;
        if (bytes.size() - (parent + offset) < 4u)
            return std::nullopt;
        const auto value = U16(parent + offset), device = U16(parent + offset + 2u);
        if (! value || ! device)
            return std::nullopt;
        const auto absolute = device.value() ? (device.value() > bytes.size() - parent ? bytes.size() : parent + device.value()) : 0u;
        return ValueRecord{static_cast<std::int16_t>(value.value()), absolute};
    }
    [[nodiscard]] std::optional<ValueRecord> MathConstant(std::size_t offset) const noexcept
    {
        const auto parent = HeaderOffset(4u);
        return parent ? Value(parent.value(), offset) : std::nullopt;
    }
    [[nodiscard]] std::optional<std::int16_t> DeviceDelta(std::size_t offset, std::uint32_t ppem) const noexcept
    {
        if (! offset)
            return std::int16_t{0};
        if (offset > bytes.size() || bytes.size() - offset < 6u)
            return std::nullopt;
        const auto first = U16(offset), last = U16(offset + 2u), format = U16(offset + 4u);
        if (! first || ! last || ! format || first.value() > last.value() || format.value() < 1u || format.value() > 3u)
            return std::nullopt; // Includes VariationIndex; it needs a separate variable-font store.
        const auto bits    = 1u << format.value();
        const auto perWord = 16u / bits;
        const auto count   = static_cast<std::size_t>(last.value() - first.value()) + 1u;
        const auto words   = (count + perWord - 1u) / perWord;
        if (words > (bytes.size() - offset - 6u) / 2u)
            return std::nullopt;
        if (ppem < first.value() || ppem > last.value())
            return std::int16_t{0};
        const auto index = ppem - first.value();
        const auto word  = U16(offset + 6u + static_cast<std::size_t>(index / perWord) * 2u);
        if (! word)
            return std::nullopt;
        const auto shift = 16u - bits * (index % perWord + 1u);
        const auto raw   = (word.value() >> shift) & ((1u << bits) - 1u);
        return static_cast<std::int16_t>((raw & (1u << (bits - 1u))) ? static_cast<int>(raw) - static_cast<int>(1u << bits) : static_cast<int>(raw));
    }
    [[nodiscard]] std::optional<float> AdjustedValue(ValueRecord record, float fontSizeDip, float dpi, std::uint16_t unitsPerEm) const noexcept
    {
        if (! std::isfinite(fontSizeDip) || fontSizeDip <= 0.f || fontSizeDip > 4096.f || ! std::isfinite(dpi) || dpi <= 0.f || dpi > 4096.f || ! unitsPerEm)
            return std::nullopt;
        const auto ppem  = static_cast<std::uint32_t>(std::round(fontSizeDip * dpi / 96.f));
        auto value       = fontSizeDip * static_cast<float>(record.value) / static_cast<float>(unitsPerEm);
        const auto delta = DeviceDelta(record.device, ppem);
        if (delta)
            value += static_cast<float>(delta.value()) * 96.f / dpi;
        return std::isfinite(value) ? std::optional<float>(value) : std::nullopt;
    }
    [[nodiscard]] std::optional<ValueRecord> ItalicCorrectionRecord(std::uint16_t glyph) const noexcept
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
        return Value(table, 4u + index.value() * 4u);
    }
    [[nodiscard]] std::optional<std::int16_t> ItalicCorrection(std::uint16_t glyph) const noexcept
    {
        const auto record = ItalicCorrectionRecord(glyph);
        return record ? std::optional<std::int16_t>(record->value) : std::nullopt;
    }
    struct Variant
    {
        std::uint16_t glyph{}, advance{};
    };
    struct AssemblyPart
    {
        std::uint16_t glyph{}, startConnector{}, endConnector{}, advance{};
        bool extender{};
    };
    struct AssemblyPlacement
    {
        AssemblyPart part;
        std::uint32_t offset{}; // From the bottom (vertical) or left (horizontal), in design units.
    };
    struct AssemblyPlan
    {
        std::array<AssemblyPlacement, 64u> placements{};
        std::size_t count{};
        std::uint32_t advance{};
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
    [[nodiscard]] std::optional<std::size_t> HorizontalConstruction(std::uint16_t glyph) const noexcept
    {
        const auto table = HeaderOffset(8u);
        if (! table)
            return std::nullopt;
        const auto coverage = U16(table.value() + 4u), verticalCount = U16(table.value() + 6u), horizontalCount = U16(table.value() + 8u);
        if (! coverage || ! verticalCount || ! horizontalCount || coverage.value() == 0u)
            return std::nullopt;
        const auto index = Coverage(table.value() + coverage.value(), glyph);
        if (! index || index.value() >= horizontalCount.value())
            return std::nullopt;
        const auto relative = U16(table.value() + 10u + static_cast<std::size_t>(verticalCount.value()) * 2u + index.value() * 2u);
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
    [[nodiscard]] std::optional<Variant> HorizontalVariantAt(std::size_t construction, std::uint16_t minimum) const noexcept
    {
        return VerticalVariantAt(construction, minimum);
    }
    [[nodiscard]] std::optional<AssemblyPlan> VerticalAssemblyAt(std::size_t construction, std::uint32_t minimum) const noexcept
    {
        const auto variants = HeaderOffset(8u);
        const auto relative = U16(construction);
        if (! variants || ! relative || relative.value() == 0u || minimum == 0u)
            return std::nullopt;
        const auto overlap  = U16(variants.value());
        const auto assembly = construction + relative.value();
        const auto count    = U16(assembly + 4u);
        if (! overlap || ! count || count.value() == 0u || count.value() > 32u || assembly > bytes.size() || bytes.size() - assembly < 6u ||
            count.value() > (bytes.size() - assembly - 6u) / 10u)
            return std::nullopt;
        std::array<AssemblyPart, 32u> parts{};
        bool hasExtender{};
        for (std::size_t index = 0u; index < count.value(); ++index)
        {
            const auto base  = assembly + 6u + index * 10u;
            const auto glyph = U16(base), start = U16(base + 2u), end = U16(base + 4u), advance = U16(base + 6u), flags = U16(base + 8u);
            if (! glyph || ! start || ! end || ! advance || ! flags || glyph.value() == 0u || advance.value() == 0u || start.value() > advance.value() ||
                end.value() > advance.value() || (flags.value() & 0xfffeu) != 0u)
                return std::nullopt;
            parts[index] = {glyph.value(), start.value(), end.value(), advance.value(), (flags.value() & 1u) != 0u};
            hasExtender  = hasExtender || parts[index].extender;
        }
        for (std::size_t repeat = 0u; repeat <= 64u; ++repeat)
        {
            AssemblyPlan plan;
            for (std::size_t index = 0u; index < count.value(); ++index)
            {
                const auto copies = parts[index].extender ? repeat : 1u;
                if (copies > plan.placements.size() - plan.count)
                    return std::nullopt;
                for (std::size_t copy = 0u; copy < copies; ++copy)
                    plan.placements[plan.count++].part = parts[index];
            }
            if (! plan.count)
                continue;
            std::array<std::uint32_t, 63u> overlaps{};
            std::uint32_t least{}, greatest{};
            bool compatible = true;
            for (std::size_t index = 0u; index < plan.count; ++index)
            {
                least += plan.placements[index].part.advance;
                greatest += plan.placements[index].part.advance;
                if (index)
                {
                    const auto connection = std::min(plan.placements[index - 1u].part.endConnector, plan.placements[index].part.startConnector);
                    if (connection < overlap.value())
                    {
                        compatible = false;
                        break;
                    }
                    overlaps[index - 1u] = connection;
                    least -= connection;
                    greatest -= overlap.value();
                }
            }
            if (! compatible)
            {
                if (! hasExtender)
                    return std::nullopt;
                continue;
            }
            if (greatest < minimum)
            {
                if (! hasExtender)
                    return std::nullopt;
                continue;
            }
            auto extra = std::min(minimum, greatest) > least ? std::min(minimum, greatest) - least : 0u;
            while (extra)
            {
                std::size_t active{};
                for (std::size_t index = 0u; index + 1u < plan.count; ++index)
                    active += overlaps[index] > overlap.value() ? 1u : 0u;
                if (! active)
                    return std::nullopt;
                const auto share = std::max<std::uint32_t>(1u, extra / static_cast<std::uint32_t>(active));
                for (std::size_t index = 0u; index + 1u < plan.count && extra; ++index)
                {
                    const auto change = std::min({share, extra, overlaps[index] - overlap.value()});
                    overlaps[index] -= change;
                    extra -= change;
                }
            }
            std::uint32_t offset{};
            for (std::size_t index = 0u; index < plan.count; ++index)
            {
                plan.placements[index].offset = offset;
                offset += plan.placements[index].part.advance;
                if (index + 1u < plan.count)
                    offset -= overlaps[index];
            }
            plan.advance = offset;
            return plan;
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<AssemblyPlan> HorizontalAssemblyAt(std::size_t construction, std::uint32_t minimum) const noexcept
    {
        return VerticalAssemblyAt(construction, minimum);
    }
};
} // namespace DxUi::RichText::Detail
