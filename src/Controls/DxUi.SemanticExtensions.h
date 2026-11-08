#pragma once
#include "DxUi.NativeFlowchart.h"
#include "DxUi.RichSemanticValues.h"
#include <DxUi/RichMath.h>
#include <algorithm>

namespace DxUi::RichText::Detail
{
// Shared authored-text admission for first-strong direction and semantic words.
// Generated chrome and object alternatives never contribute language characters.
inline bool IsAuthoredSemanticSpan(DisplaySpanKind kind) noexcept
{
    return kind != DisplaySpanKind::Generated && kind != DisplaySpanKind::Object;
}
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
// A native rejection is presentation state for an exact source owner. Keep its
// checked cause through rebuilding; capacity is not unsupported notation.
enum class NativeFallbackReason
{
    Unsupported,
    Capacity
};
struct NativeSourceFallback
{
    MarkdownRange source;
    NativeFallbackReason reason{};
    bool operator==(const NativeSourceFallback&) const = default;
};
struct SemanticExtensions
{
    static constexpr std::size_t MaximumObjects = 128u, MaximumRecords = MathMaximumDocumentNodes;
    SemanticExtensions() : diagrams(0u), maths(0u), diagnostics(0u), images(0u), sourceObjects(0u), disclosures(0u), abbreviationNodes(0u)
    {
    }
    std::vector<SemanticDiagram> diagrams;
    struct Formula
    {
        std::size_t element{};
        std::shared_ptr<const MathExpression> expression;
        bool display{};
    };
    std::vector<Formula> maths;
    std::vector<SemanticDiagnostic> diagnostics;
    std::vector<std::size_t> images;
    std::vector<NativeSourceFallback> sourceObjects;
    std::vector<DisclosureOverride> disclosures;
    std::vector<std::size_t> abbreviationNodes;
    std::size_t candidates{}, records{};
    bool CanAdmitObject() const noexcept
    {
        return images.size() + maths.size() + diagrams.size() < MaximumObjects;
    }
    bool CanAdmitRecords(std::size_t count) const noexcept
    {
        return count <= MaximumRecords - records;
    }
};
struct SemanticExtensionAccess
{
    static std::span<const NativeSourceFallback> SourceObjects(const SemanticDocument& document) noexcept
    {
        return document.extensions_ ? std::span<const NativeSourceFallback>(document.extensions_->sourceObjects) : std::span<const NativeSourceFallback>{};
    }
    static std::span<const std::size_t> AbbreviationNodes(const SemanticDocument& document) noexcept
    {
        return document.extensions_ ? std::span<const std::size_t>(document.extensions_->abbreviationNodes) : std::span<const std::size_t>{};
    }
    static std::span<const DisclosureOverride> Disclosures(const SemanticDocument& document) noexcept
    {
        return document.extensions_ ? std::span<const DisclosureOverride>(document.extensions_->disclosures) : std::span<const DisclosureOverride>{};
    }
    static void BeginWithNativeFallbacks(SemanticBuilder& builder, std::shared_ptr<const MarkdownSyntax> syntax, std::span<const MarkdownRange> sourceObjects)
    {
        const auto sourceBlock = syntax ? syntax->SourceRegionNode() : MarkdownNoNode;
        const auto fallbacks   = CapacityFallbacks(sourceObjects);
        builder.BeginWithNativeFallbacks(std::move(syntax), sourceBlock, fallbacks);
    }
    static void BeginWithPresentation(SemanticBuilder& builder,
                                      std::shared_ptr<const MarkdownSyntax> syntax,
                                      std::span<const MarkdownRange> sourceObjects,
                                      std::span<const DisclosureOverride> disclosures)
    {
        const auto sourceBlock = syntax ? syntax->SourceRegionNode() : MarkdownNoNode;
        const auto fallbacks   = CapacityFallbacks(sourceObjects);
        builder.BeginWithNativeFallbacks(std::move(syntax), sourceBlock, fallbacks, disclosures);
    }
    [[nodiscard]] static SourceResult TryBeginWithPresentation(SemanticBuilder& builder,
                                                               std::shared_ptr<const MarkdownSyntax> syntax,
                                                               std::span<const MarkdownRange> sourceObjects,
                                                               std::span<const DisclosureOverride> disclosures)
    {
        const auto sourceBlock = syntax ? syntax->SourceRegionNode() : MarkdownNoNode;
        if (sourceObjects.size() > SemanticExtensions::MaximumObjects)
            return SourceResult::CapacityExceeded;
        const auto fallbacks = CapacityFallbacks(sourceObjects);
        return builder.TryBeginWithNativeFallbacks(std::move(syntax), sourceBlock, fallbacks, disclosures);
    }
    [[nodiscard]] static SourceResult TryBeginWithTypedPresentation(SemanticBuilder& builder,
                                                                    std::shared_ptr<const MarkdownSyntax> syntax,
                                                                    std::span<const NativeSourceFallback> sourceObjects,
                                                                    std::span<const DisclosureOverride> disclosures)
    {
        const auto sourceBlock = syntax ? syntax->SourceRegionNode() : MarkdownNoNode;
        return builder.TryBeginWithNativeFallbacks(std::move(syntax), sourceBlock, sourceObjects, disclosures);
    }
    static std::span<const SemanticExtensions::Formula> Maths(const SemanticDocument& document) noexcept
    {
        return document.extensions_ ? std::span<const SemanticExtensions::Formula>(document.extensions_->maths)
                                    : std::span<const SemanticExtensions::Formula>{};
    }
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

private:
    static std::vector<NativeSourceFallback> CapacityFallbacks(std::span<const MarkdownRange> ranges)
    {
        // Legacy internal fixtures supply capacity-only ranges. The editor
        // supplies typed records through TryBeginWithTypedPresentation.
        std::vector<NativeSourceFallback> result(0u);
        result.reserve(std::min(ranges.size(), SemanticExtensions::MaximumObjects + 1u));
        for (const auto range : ranges.first(std::min(ranges.size(), SemanticExtensions::MaximumObjects + 1u)))
            result.push_back({range, NativeFallbackReason::Capacity});
        return result;
    }
};
} // namespace DxUi::RichText::Detail
