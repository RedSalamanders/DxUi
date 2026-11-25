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
// Block-incremental projection (Specs/UI/UI_ControlsAndLayout.md, "Block-incremental
// semantic projection"). One cut per top-level syntax block, in sibling order,
// plus a terminal cut: the builder's state where that block's output starts.
//
// Reuse invariants, checked by SemanticBuilder before it copies a block:
// - A top-level block's output (elements, paragraphs, display text, spans, runs,
//   headings and extension records) depends only on its own syntax subtree, on
//   document-wide inputs and on the boundary state recorded here. Document-wide
//   inputs (footnotes, link reference definitions, front matter, abbreviation
//   definitions, admission bounds, presentation overrides) force a full build
//   when they could change; that full build is counted by cause.
// - Every source coordinate a block emits lies inside its own block range, so a
//   reused block shifts uniformly by the edit's source delta; its display,
//   element, paragraph and heading indices shift by the difference of the cuts.
// - Element and paragraph syntax indices move by the difference of the paired
//   top-level node indices; each is checked against the new syntax (kind, range
//   and identity) while copying, and a mismatch rebuilds fully.
// - Spans and runs merge across block edges exactly as SemanticOutput::Emit
//   merges them, so clipped copies re-merged with that rule equal a fresh build.
// Reused values are copies: a publication never shares mutable storage with
// the previous one, which stays immutable and may be released independently.
struct SemanticBlockCut
{
    std::size_t node = MarkdownNoNode; // Top-level syntax node; MarkdownNoNode for the terminal cut.
    MarkdownRange source;
    MarkdownBlockKind kind{};
    std::size_t display{}, element{}, paragraph{}, heading{};
    std::size_t maths{}, images{}, diagrams{}, diagnostics{}, records{}, candidates{}, callouts{};
    std::size_t lastTop    = MarkdownNoNode; // Last top-level element before this cut.
    std::size_t reusedFrom = MarkdownNoNode; // Block index in the previous publication, when copied from it.
    bool htmlPending{};                      // A native HTML empty insertion still waits at this display offset.
};
struct SemanticProjectionIndex
{
    SemanticProjectionIndex() : blocks(0u)
    {
    }
    std::vector<SemanticBlockCut> blocks;
    // Identity of the publication this one copied blocks from (0 for a full
    // build). With SemanticBlockCut::reusedFrom this is the old-to-new block
    // pairing later preparation stages may follow instead of an identity map.
    std::uint64_t previousIdentity{};
    bool admissionRefused{}; // Some object, record or diagram admission was refused.
};
struct SemanticExtensionAccess
{
    static const SemanticProjectionIndex* ProjectionIndex(const SemanticDocument& document) noexcept
    {
        return document._projection.get();
    }
    static std::span<const NativeSourceFallback> SourceObjects(const SemanticDocument& document) noexcept
    {
        return document._extensions ? std::span<const NativeSourceFallback>(document._extensions->sourceObjects) : std::span<const NativeSourceFallback>{};
    }
    static std::span<const std::size_t> AbbreviationNodes(const SemanticDocument& document) noexcept
    {
        return document._extensions ? std::span<const std::size_t>(document._extensions->abbreviationNodes) : std::span<const std::size_t>{};
    }
    static std::span<const DisclosureOverride> Disclosures(const SemanticDocument& document) noexcept
    {
        return document._extensions ? std::span<const DisclosureOverride>(document._extensions->disclosures) : std::span<const DisclosureOverride>{};
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
        return document._extensions ? std::span<const SemanticExtensions::Formula>(document._extensions->maths)
                                    : std::span<const SemanticExtensions::Formula>{};
    }
    static std::span<const std::size_t> Images(const SemanticDocument& document) noexcept
    {
        return document._extensions ? std::span<const std::size_t>(document._extensions->images) : std::span<const std::size_t>{};
    }
    static std::span<const SemanticDiagram> Diagrams(const SemanticDocument& document) noexcept
    {
        return document._extensions ? std::span<const SemanticDiagram>(document._extensions->diagrams) : std::span<const SemanticDiagram>{};
    }
    static const SemanticDiagram* Diagram(const SemanticDocument& document, std::size_t element) noexcept
    {
        if (! document._extensions)
            return nullptr;
        const auto& diagrams = document._extensions->diagrams;
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
