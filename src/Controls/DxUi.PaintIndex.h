#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace DxUi::RichText::Detail
{
// An immutable interval tree retains original paint order even when semantic
// bounds overlap. It is staged a node at a time and queried without allocation.
struct PaintIndex
{
    struct Node
    {
        float top = std::numeric_limits<float>::infinity(), bottom = -std::numeric_limits<float>::infinity();
    };
    PaintIndex() : nodes(0u)
    {
    }
    std::vector<Node> nodes;
    std::size_t leaves{};
    void Swap(PaintIndex& other) noexcept
    {
        nodes.swap(other.nodes);
        std::swap(leaves, other.leaves);
    }
    template <typename Bounds> void Build(std::size_t count, Bounds&& bounds)
    {
        if (! count)
            return;
        leaves = 1u;
        while (leaves < count)
            leaves *= 2u;
        nodes.reserve(leaves * 2u);
        for (std::size_t i = 0u; i < leaves * 2u; ++i)
            nodes.emplace_back();
        for (std::size_t i = 0u; i < count; ++i)
        {
            const auto rect   = bounds(i);
            nodes[leaves + i] = {rect.top, rect.bottom};
        }
        for (auto i = leaves; i > 1u;)
        {
            --i;
            const auto& a = nodes[i * 2u];
            const auto& b = nodes[i * 2u + 1u];
            nodes[i]      = {std::min(a.top, b.top), std::max(a.bottom, b.bottom)};
        }
    }
    template <typename Visit> void ForEach(float top, float bottom, Visit&& visit) const
    {
        if (! leaves)
            return;
        const auto walk = [&](const auto& self, std::size_t index) -> void
        {
            const auto& node = nodes[index];
            if (node.bottom <= top || node.top >= bottom)
                return;
            if (index >= leaves)
            {
                visit(index - leaves);
                return;
            }
            self(self, index * 2u);
            self(self, index * 2u + 1u);
        };
        walk(walk, 1u);
    }
};
} // namespace DxUi::RichText::Detail
