#pragma once
// The cache of the small layouts a numbered code fence draws: one line number per numbered line, one language badge per
// fence (plan item W7.6).
//
// A number layout is a layout of a few digits; the same digits at the same gutter format, reading direction and DPI are
// the same layout in every fence and in every generation of the geometry, so one table serves them all. A badge is keyed
// by its language the same way. The cache is shared by the published geometry and the one being prepared: a cached layout
// is immutable (it is handed out as `shared_ptr<const PreparedLayout>` and nothing prepares it again), so both geometries
// draw it at the same time.
//
// Bound: an entry is kept while the generation that last used it is the one being prepared or the one published. Each
// publication removes every entry the published generation did not use (the previous one's leftovers), so after a
// publication the cache holds what that geometry draws, and while the next one is prepared it holds that plus the layouts
// that preparation adds. At most MaximumNumbers number entries and MaximumBadges badge entries exist at any time: a number
// layout that finds the table full first drops the entries the preparation in progress has not used and, if none is left,
// is used but not kept. MaximumNumbers equals the number layouts one geometry holds at most.
//
// The key holds everything the layout is made from: the text and the layout options (width, fonts, size, line height, tab
// stop, locale, reading direction, alignment, wrap and DPI; options that only the semantic display reads do not matter to a
// plain layout and are not compared). The factory is not in the key: the cache holds the one factory its layouts were made
// by (Bind) and drops every entry when it is bound to another, so the address of a released factory can never name a
// layout. A theme changes only colors, which no layout holds; a font, size, DPI, gutter-width or reading-direction change
// is a different key.
//
// Insert allocates and does not catch: an allocation failure leaves the table as it was (the emplace gives the strong
// guarantee) and propagates to the owner turn's `catch (const std::bad_alloc&)`, which enters the fatal allocation path
// (Core_FailureHandling.md: broad catch-and-continue handlers are prohibited, a checked allocation that cannot report
// failure goes to the host's fatal diagnostics).
#include "DxUi.RichLayoutOptions.h"
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <wil/com.h>

namespace DxUi::RichText::Detail
{
class CodeLayoutCache
{
public:
    static constexpr std::size_t MaximumNumbers = 4096u;
    static constexpr std::size_t MaximumBadges  = 256u;
    enum class Kind : std::uint8_t
    {
        Number,
        Badge
    };
    // The cache holds `factory` and serves layouts made by it only: bound to another factory it drops everything first.
    void Bind(IDWriteFactory* factory)
    {
        if (factory_.get() == factory)
            return;
        Clear();
        factory_ = factory;
    }
    // The cached layout of `text` with `options`, or null. A hit marks the entry as used by the generation in preparation.
    [[nodiscard]] std::shared_ptr<const PreparedLayout> Find(Kind kind, std::wstring_view text, const LayoutOptions& options)
    {
        auto& table              = Table(kind);
        const auto [first, last] = table.equal_range(Hash(text, options));
        for (auto entry = first; entry != last; ++entry)
            if (entry->second.text == text && SameLayoutOptions(entry->second.options, options, LayoutOptionsUse::Text))
            {
                entry->second.epoch = epoch_;
                return entry->second.layout;
            }
        return {};
    }
    // Keeps `layout` for `text` and `options` if the bound allows; it is then marked as used by the generation in
    // preparation.
    void Insert(Kind kind, std::wstring_view text, const LayoutOptions& options, std::shared_ptr<const PreparedLayout> layout)
    {
        auto& table = Table(kind);
        if (table.size() >= Limit(kind))
        {
            Trim(table);
            if (table.size() >= Limit(kind))
                return;
        }
        table.emplace(Hash(text, options), Entry{std::wstring(text), options, std::move(layout), epoch_});
    }
    // The generation in preparation was published: the entries it did not use are dropped, and the next preparation
    // counts its own use.
    void Publish() noexcept
    {
        Trim(numbers_);
        Trim(badges_);
        ++epoch_;
    }
    void Clear() noexcept
    {
        numbers_.clear();
        badges_.clear();
    }
    [[nodiscard]] std::size_t Entries() const noexcept
    {
        return numbers_.size() + badges_.size();
    }
    // Resident bytes as RetainedCacheCount defines them: the tables, the keys' strings and options; the layouts' storage
    // is DirectWrite's and is not included.
    [[nodiscard]] std::uint64_t ResidentBytes() const noexcept
    {
        return TableBytes(numbers_) + TableBytes(badges_);
    }

private:
    struct Entry
    {
        std::wstring text;
        LayoutOptions options;
        std::shared_ptr<const PreparedLayout> layout;
        std::uint64_t epoch{};
    };
    using Map = std::unordered_multimap<std::size_t, Entry>;
    [[nodiscard]] Map& Table(Kind kind) noexcept
    {
        return kind == Kind::Number ? numbers_ : badges_;
    }
    [[nodiscard]] static std::size_t Limit(Kind kind) noexcept
    {
        return kind == Kind::Number ? MaximumNumbers : MaximumBadges;
    }
    void Trim(Map& table) const noexcept
    {
        for (auto entry = table.begin(); entry != table.end();)
            entry = entry->second.epoch == epoch_ ? std::next(entry) : table.erase(entry);
    }
    [[nodiscard]] static std::size_t Hash(std::wstring_view text, const LayoutOptions& options) noexcept
    {
        std::size_t hash = std::hash<std::wstring_view>{}(text);
        const auto mix   = [&](std::size_t value) noexcept { hash = (hash ^ value) * 1099511628211ull; };
        mix(std::bit_cast<std::uint32_t>(options.widthDip));
        mix(std::bit_cast<std::uint32_t>(options.fontSizeDip));
        mix(std::bit_cast<std::uint32_t>(options.dpi));
        mix(static_cast<std::size_t>(options.readingDirection) * 4u + static_cast<std::size_t>(options.textAlignment));
        return hash;
    }
    [[nodiscard]] static std::uint64_t StringBytes(const std::wstring& text) noexcept
    {
        constexpr std::size_t inlineCapacity = 7u;
        return text.capacity() > inlineCapacity ? std::uint64_t(text.capacity() + 1u) * sizeof(wchar_t) : 0u;
    }
    [[nodiscard]] static std::uint64_t TableBytes(const Map& table) noexcept
    {
        constexpr std::uint64_t node = 2u * sizeof(void*) + sizeof(Map::value_type);
        std::uint64_t bytes          = (std::uint64_t(table.size()) + 1u) * node + std::uint64_t(table.bucket_count()) * 2u * sizeof(void*);
        for (const auto& entry : table)
            bytes += StringBytes(entry.second.text) + StringBytes(entry.second.options.fontFamily) + StringBytes(entry.second.options.codeFontFamily) +
                     StringBytes(entry.second.options.locale) + StringBytes(entry.second.options.mathFontFamily);
        return bytes;
    }
    wil::com_ptr<IDWriteFactory> factory_;
    Map numbers_, badges_;
    std::uint64_t epoch_{1u};
};
} // namespace DxUi::RichText::Detail
