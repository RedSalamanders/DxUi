#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace DxUi
{
// A complete immutable semantic source prepared by the model's owner. Text is borrowed from this source;
// each published UIA snapshot retains its shared owner through every provider read and replacement.
// GetItem/FindItem are bounded, allocation-free reads and never touch the live model or controls. The consumer
// admits preparation and arranges final source destruction on its appropriate lane, including foreign readers.
// Source identity is a semantic epoch: retain the same owner for unchanged rows and publish a new owner whenever
// row sequence, IDs, text, depth, child presence or expansion change. Selection is independent. Snapshot diffs invalidate Tree children on
// an epoch change, including replacements with the same count and IDs; they do not traverse the source to diff it.
struct TreeAccessibilityItemView
{
    std::size_t visibleIndex = 0u;
    std::uint64_t itemId     = 0u;
    std::wstring_view text;
    std::size_t depth = 0u;
    bool hasChildren  = false;
    bool expanded     = false;
};

class IPreparedTreeAccessibilityRows
{
public:
    virtual ~IPreparedTreeAccessibilityRows()                                                                       = default;
    [[nodiscard]] virtual std::size_t GetCount() const noexcept                                                     = 0;
    [[nodiscard]] virtual std::optional<TreeAccessibilityItemView> GetItem(std::size_t visibleIndex) const noexcept = 0;
    [[nodiscard]] virtual std::optional<TreeAccessibilityItemView> FindItem(std::uint64_t itemId) const noexcept    = 0;
};

} // namespace DxUi
