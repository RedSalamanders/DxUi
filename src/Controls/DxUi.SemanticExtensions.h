#pragma once
#include "DxUi.NativeFlowchart.h"
#include "DxUi.RichSemanticValues.h"
#include <algorithm>

namespace DxUi::RichText::Detail
{
struct SemanticDiagram
{
    SemanticDiagram() : bodyUnits(0u)
    {
    }
    SemanticDiagram(const SemanticDiagram&)            = default;
    SemanticDiagram& operator=(const SemanticDiagram&) = default;
    std::size_t element{};
    MarkdownRange display;
    std::shared_ptr<const FlowchartModel> graph;
    // Logical code bodies omit container prefixes and normalize EOLs. Keep
    // provenance to translate private graph ranges back to original source.
    std::vector<SemanticValueUnit> bodyUnits;
    std::size_t sourceEnd{};
    std::optional<MarkdownRange> SourceRange(MarkdownRange body) const noexcept
    {
        if (body.first > body.last || body.last > bodyUnits.size())
            return std::nullopt;
        if (body.first == body.last)
        {
            const auto at = body.first < bodyUnits.size() ? bodyUnits[body.first].source.first : sourceEnd;
            return MarkdownRange{at, at};
        }
        return MarkdownRange{bodyUnits[body.first].source.first, bodyUnits[body.last - 1u].source.last};
    }
};
struct SemanticExtensions
{
    static constexpr std::size_t MaximumObjects = 128u, MaximumRecords = 32768u;
    SemanticExtensions() : diagrams(0u), diagnostics(0u), images(0u)
    {
    }
    std::vector<SemanticDiagram> diagrams;
    std::vector<SemanticDiagnostic> diagnostics;
    std::vector<std::size_t> images;
    std::size_t candidates{}, records{};
};
struct SemanticExtensionAccess
{
    static std::span<const std::size_t> Images(const SemanticDocument& document) noexcept
    {
        return document.extensions_ ? std::span<const std::size_t>(document.extensions_->images) : std::span<const std::size_t>{};
    }
    static std::span<const SemanticDiagram> Diagrams(const SemanticDocument& document) noexcept
    {
        return document.extensions_ ? std::span<const SemanticDiagram>(document.extensions_->diagrams) : std::span<const SemanticDiagram>{};
    }
    static const SemanticDiagram* Diagram(const SemanticDocument& document, std::size_t element) noexcept
    {
        if (! document.extensions_)
            return nullptr;
        const auto& diagrams = document.extensions_->diagrams;
        const auto found = std::lower_bound(diagrams.begin(), diagrams.end(), element, [](const auto& diagram, auto index) { return diagram.element < index; });
        return found != diagrams.end() && found->element == element ? &*found : nullptr;
    }
};
} // namespace DxUi::RichText::Detail
