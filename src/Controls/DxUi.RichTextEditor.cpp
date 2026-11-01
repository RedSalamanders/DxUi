#include "DxUi.Internal.h"
#include "DxUi.MarkdownLabels.h"
#include "DxUi.PaintIndex.h"
#include "DxUi.RichAccessibilityEpoch.h"
#include "DxUi.RichCodeInk.h"
#include "DxUi.RichEditorLayout.h"
#include "DxUi.RichFileReferenceContext.h"
#include "DxUi.RichImagePaths.h"
#include "DxUi.RichImageResources.h"
#include "DxUi.RichSemanticGeometry.h"
#include "DxUi.RichSemanticValues.h"
#include "DxUi.RichSourceGuides.h"
#include "DxUi.RichSourceInk.h"
#include "DxUi.RichSourceWindow.h"
#include "DxUi.SemanticExtensions.h"
#include "DxUi.TextElements.h"
#include "Support/Diagnostics.h"
#include "TextClipboard.h"
#include <DxUi/AccessibilityTextUnits.h>
#include <DxUi/RichTextLayout.h>
#include <UIAutomation.h>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <wil/result.h>

namespace DxUi
{
namespace
{
bool MarkdownCommandInputBlocked(const ControlHost* host, const Control* control) noexcept
{
    if (! host || host->GetFocusControl() != control)
        return false;
    const auto composition = host->GetNativeTextCompositionActive(control);
    return (host->HasActiveNativeTextInputSession() && ! composition) || composition.value_or(false);
}
std::optional<RichText::MarkdownListKind> UniformListKind(const RichText::MarkdownSyntax& syntax,
                                                          std::size_t first,
                                                          std::size_t last,
                                                          std::size_t& ancestorSteps) noexcept
{
    using namespace RichText;
    const auto& nodes = syntax.Nodes();
    // Common depths use a small fixed stack. Deeper valid syntax restores the
    // parent item from syntax links, preserving exactness without heap storage.
    std::array<std::size_t, 64u> items;
    std::size_t depth = 0u, cursor = first;
    std::size_t active = MarkdownNoNode;
    std::optional<MarkdownListKind> result;
    bool mixed           = false;
    const auto kindAtTop = [&]() noexcept
    {
        if (! depth)
            return MarkdownListKind::None;
        const auto& item = nodes[active];
        if (item.task != MarkdownTask::None)
            return MarkdownListKind::Task;
        const auto marker = nodes[item.parent].listMarker;
        return marker == L'.' || marker == L')' ? MarkdownListKind::Ordered : MarkdownListKind::Unordered;
    };
    const auto consume = [&](std::size_t boundary) noexcept
    {
        const auto end = std::min(boundary, last);
        if (end <= cursor)
            return;
        const auto kind = kindAtTop();
        if (result && result.value() != kind)
            mixed = true;
        else if (! result)
            result = kind;
        cursor = end;
    };
    std::size_t node = 0u;
    bool entering    = true;
    while (node != MarkdownNoNode && ! mixed && cursor < last)
    {
        const auto& current = nodes[node];
        if (entering)
        {
            if (current.block.kind == MarkdownBlockKind::ListItem)
            {
                consume(current.block.source.first);
                if (depth < items.size())
                    items[depth] = node;
                ++depth;
                active = node;
            }
            if (current.firstChild != MarkdownNoNode)
            {
                node = current.firstChild;
                continue;
            }
        }
        if (current.block.kind == MarkdownBlockKind::ListItem)
        {
            consume(current.block.source.last);
            --depth;
            if (! depth)
                active = MarkdownNoNode;
            else if (depth <= items.size())
                active = items[depth - 1u];
            else
            {
                active = current.parent;
                while (active != MarkdownNoNode && nodes[active].block.kind != MarkdownBlockKind::ListItem)
                {
                    ++ancestorSteps;
                    active = nodes[active].parent;
                }
            }
        }
        if (current.nextSibling != MarkdownNoNode)
        {
            node     = current.nextSibling;
            entering = true;
        }
        else
        {
            node     = current.parent;
            entering = false;
        }
    }
    consume(last);
    if (mixed)
        return std::nullopt;
    return result.value_or(MarkdownListKind::None);
}
std::optional<RichText::MarkdownRange> HeadingFragment(const RichText::SemanticDocument& document, std::wstring_view destination)
{
    if (! destination.starts_with(L'#'))
        return std::nullopt;
    if (destination.size() == 1u)
        return RichText::MarkdownRange{};
    std::wstring wanted(0u, L' ');
    try
    {
        const auto encoded = RichText::EncodeUtf8(destination.substr(1u));
        std::string decoded(0u, '\0');
        decoded.reserve(encoded.size());
        const auto hex = [](char c) noexcept -> unsigned
        {
            if (c >= '0' && c <= '9')
                return static_cast<unsigned>(c - '0');
            if (c >= 'a' && c <= 'f')
                return static_cast<unsigned>(c - 'a') + 10u;
            if (c >= 'A' && c <= 'F')
                return static_cast<unsigned>(c - 'A') + 10u;
            return 16u;
        };
        for (size_t index = 0u; index < encoded.size(); ++index)
        {
            auto value = encoded[index];
            if (value == '%')
            {
                if (index + 2u >= encoded.size() || hex(encoded[index + 1u]) > 15u || hex(encoded[index + 2u]) > 15u)
                    return std::nullopt;
                value = static_cast<char>((hex(encoded[index + 1u]) << 4u) | hex(encoded[index + 2u]));
                index += 2u;
            }
            if (! value)
                return std::nullopt;
            decoded.push_back(value);
        }
        wanted = RichText::DecodeUtf8(decoded);
    }
    catch (const std::invalid_argument&)
    {
        return std::nullopt; // A malformed URI fragment never navigates a partial prefix.
    }
    catch (const std::length_error&)
    {
        return std::nullopt;
    }
    std::set<std::wstring> used;
    for (const auto& element : document.Elements())
    {
        if (element.role != RichText::SemanticRole::Heading)
            continue;
        const auto folded = RichText::Detail::NormalizeMarkdownLabel(element.name, document.Syntax()->Profile());
        std::wstring base(0u, L' ');
        for (const auto unit : folded)
        {
            if (unit == L' ')
                base.push_back(L'-');
            else if (unit != L'\ufffc' && (unit >= 128u || (unit >= L'a' && unit <= L'z') || (unit >= L'0' && unit <= L'9') || unit == L'_' || unit == L'-'))
                base.push_back(unit);
        }
        if (base.empty())
            base = L"section";
        std::wstring id(base);
        size_t suffix = 0u;
        while (used.contains(id))
            id = base + L"-" + std::to_wstring(++suffix);
        used.insert(id);
        if (id == wanted)
            return element.syntaxNode < document.Syntax()->Nodes().size() ? document.Syntax()->Nodes()[element.syntaxNode].block.content : element.source;
    }
    return std::nullopt;
}
wchar_t AutoPairClosing(wchar_t character) noexcept
{
    switch (character)
    {
        case L'(': return L')';
        case L'[': return L']';
        case L'{': return L'}';
        case L'"':
        case L'\'':
        case L'*':
        case L'_': return character;
        default: return 0;
    }
}
enum class AutoFormatMatch
{
    None,
    Prefix,
    Marker,
    Fence,
    Rule
};
AutoFormatMatch MatchAutoFormat(std::wstring_view value) noexcept
{
    if (value.empty())
        return AutoFormatMatch::None;
    const auto complete = value.back() == L' ';
    const auto marker   = complete ? value.substr(0u, value.size() - 1u) : value;
    if (marker.empty())
        return AutoFormatMatch::None;
    if (marker.front() == L'#' && marker.size() <= 6u && marker.find_first_not_of(L'#') == marker.npos)
        return complete ? AutoFormatMatch::Marker : AutoFormatMatch::Prefix;
    if (marker == L"-" || marker == L"+" || marker == L"*" || marker == L">")
        return complete ? AutoFormatMatch::Marker : AutoFormatMatch::Prefix;
    if (! complete && marker == L"--")
        return AutoFormatMatch::Prefix;
    if (! complete && marker == L"---")
        return AutoFormatMatch::Rule;
    if (! complete && (marker.front() == L'`' || marker.front() == L'~') && marker.size() <= 3u && marker.find_first_not_of(marker.front()) == marker.npos)
        return marker.size() == 3u ? AutoFormatMatch::Fence : AutoFormatMatch::Prefix;
    auto digits = size_t{};
    while (digits < marker.size() && digits < 9u && marker[digits] >= L'0' && marker[digits] <= L'9')
        ++digits;
    if (! digits)
        return AutoFormatMatch::None;
    if (digits == marker.size())
        return complete ? AutoFormatMatch::None : AutoFormatMatch::Prefix;
    if (digits + 1u == marker.size() && (marker[digits] == L'.' || marker[digits] == L')'))
        return complete ? AutoFormatMatch::Marker : AutoFormatMatch::Prefix;
    return AutoFormatMatch::None;
}
// Group complete UTF-16 text elements without splitting supplementary scalars
// or combining sequences. Punctuation and whitespace still terminate a run.
bool IsHistoryWordElement(std::wstring_view text) noexcept
{
    if (text.empty())
        return false;
    for (size_t i = 0u; i < text.size(); ++i)
    {
        const auto c = text[i];
        if (IsUtf16LeadSurrogate(c))
        {
            if (++i == text.size() || ! IsUtf16TrailSurrogate(text[i]))
                return false;
            continue;
        }
        if (IsUtf16TrailSurrogate(c))
            return false;
        WORD type1 = 0u, type3 = 0u;
        if (c != L'_' && c != L'\u200d' &&
            (! GetStringTypeW(CT_CTYPE1, &c, 1, &type1) ||
             ((type1 & (C1_ALPHA | C1_DIGIT)) == 0u &&
              (! GetStringTypeW(CT_CTYPE3, &c, 1, &type3) || (type3 & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) == 0u))))
            return false;
    }
    return true;
}
std::wstring CodeCopyText(const RichText::SemanticDocument& document, const RichText::SemanticElement& element)
{
    const auto& syntax = *document.Syntax();
    if (element.syntaxNode >= syntax.Nodes().size())
        return std::wstring(0u, L' ');
    RichText::Detail::SemanticValue value;
    RichText::Detail::NormalizeSemanticLeaf(syntax, element.syntaxNode, value, true);
    return std::wstring(value.text);
}
D2D1_COLOR_F ResolveMarkdownQuoteFill(const ThemePalette& theme, const TextFieldVisualStyle& style) noexcept
{
    if (theme.highContrast)
        return style.fill;
    // Document adapters can share input/card colors. Blend neutral ink so the
    // quote remains visible over either a light or dark document surface.
    auto ink = style.text;
    ink.a *= .08f;
    return CompositeOverBackground(ink, style.fill);
}
D2D1_COLOR_F DecorationColor(const ThemePalette& theme, RichTextDecorationRole role) noexcept
{
    if (theme.highContrast)
        return theme.text;
    switch (role)
    {
        case RichTextDecorationRole::Accent: return theme.accent;
        case RichTextDecorationRole::Subdued: return theme.subduedText;
        case RichTextDecorationRole::Warning: return theme.warningText;
        case RichTextDecorationRole::Error: return theme.errorText;
    }
    return theme.text;
}
// Preparation-only source coverage: unlike raw coordinate-range merging this
// preserves the active-highlight role, which takes priority over ordinary ink.
void UnionHighlights(std::span<const RichTextSourceHighlight> input,
                     std::span<const RichText::MarkdownRange> atomic,
                     std::vector<RichTextSourceHighlight>& output)
{
    struct Edge
    {
        size_t at{};
        int normal{}, current{};
    };
    std::vector<Edge> edges(0u);
    edges.reserve(input.size() * 2u);
    const auto snap = [&](size_t position, bool trailing)
    {
        const auto next = std::upper_bound(atomic.begin(), atomic.end(), position, [](size_t value, const auto& range) { return value < range.first; });
        if (next != atomic.begin())
        {
            const auto& range = *std::prev(next);
            if (range.first < position && position < range.last)
                return trailing ? range.last : range.first;
        }
        return position;
    };
    for (const auto& range : input)
        if (range.length)
        {
            edges.push_back({snap(range.start, false), range.current ? 0 : 1, range.current ? 1 : 0});
            edges.push_back({snap(range.start + range.length, true), range.current ? 0 : -1, range.current ? -1 : 0});
        }
    std::ranges::sort(edges, {}, &Edge::at);
    size_t previous = 0u, at = 0u;
    int normal = 0, current = 0;
    while (at < edges.size())
    {
        const auto position = edges[at].at;
        if (position > previous && (normal > 0 || current > 0))
        {
            const bool active = current > 0;
            if (! output.empty() && output.back().current == active && output.back().start + output.back().length == previous)
                output.back().length += position - previous;
            else
                output.push_back({previous, position - previous, active});
        }
        do
        {
            normal += edges[at].normal;
            current += edges[at].current;
            ++at;
        } while (at < edges.size() && edges[at].at == position);
        previous = position;
    }
}
} // namespace
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
struct RichTextEditor::State
{
    std::size_t lastListStateAncestorSteps{};
    explicit State(std::wstring text) : document(std::move(text)), searchQuery(0u, L' '), documentFilePath(0u, L' ')
    {
    }
    RichText::Document document;
    struct TypingRun
    {
        enum class Kind
        {
            Insert,
            Backspace,
            Delete
        } kind = Kind::Insert;
        uint64_t revision{}, selectionEpoch{}, lastTick{};
        size_t caret{}, units{};
    };
    std::optional<TypingRun> typingRun;
    RichTextEditorAppearance appearance;
    RichTextLinkUnderlinePolicy linkUnderlinePolicy = RichTextLinkUnderlinePolicy::Always;
    std::optional<RichText::MarkdownRange> hoveredLink;
    std::optional<RichText::MarkdownRange> caretLink;
    std::shared_ptr<const std::function<void(const RichTextLinkActivation&)>> onLinkActivated;
    std::weak_ptr<const RichText::SemanticDocument> keyboardNavigationDocument;
    size_t keyboardNavigationElement = RichText::MarkdownNoNode;
    std::vector<RichTextSourceHighlight> highlights;
    std::vector<RichTextDecoration> decorations;
    std::shared_ptr<const RichTextContextMenuLabels> contextMenuLabels;
    std::wstring searchQuery;
    RichText::SourceSearchOptions searchOptions;
    bool searchShortcutsEnabled = false;
    std::shared_ptr<const std::function<void()>> onFindRequested;
    bool markdownShortcutsEnabled  = false;
    bool autoPairEnabled           = false;
    bool smartTypographyEnabled    = false;
    bool markdownAutoFormatEnabled = false;
    struct AutoFormatRun
    {
        uint64_t revision{}, selectionEpoch{}, lastTick{};
        size_t caret{}, lineStart{};
        bool nested{};
        std::shared_ptr<const std::wstring> continuationPrefix;
    };
    std::optional<AutoFormatRun> autoFormatRun;
    struct SmartTypingSite
    {
        uint64_t revision{};
        size_t caret{};
    };
    std::optional<SmartTypingSite> smartTypingSite;
    std::shared_ptr<const std::function<void()>> onLinkEditRequested;
    struct HighlightRectangle
    {
        D2D1_RECT_F rectangle{};
        bool current{};
    };
    struct DecorationRectangle
    {
        D2D1_RECT_F rectangle{};
        RichTextDecorationKind kind{};
        RichTextDecorationRole role{};
    };
    std::vector<D2D1_RECT_F> selectionRectangles{0u};
    std::vector<HighlightRectangle> highlightRectangles{0u};
    std::vector<float> highlightBottoms{0u};
    std::vector<DecorationRectangle> decorationRectangles{0u};
    RichText::Detail::PaintIndex decorationPaintIndex;
    RichTextDecorationStatus decorationStatus = RichTextDecorationStatus::None;
    size_t decorationRectangleLimit           = 4096u;
    std::vector<D2D1_RECT_F> compositionRectangles{0u}, conversionRectangles{0u};
    wil::com_ptr<ID2D1PathGeometry> selectionMask;
    wil::com_ptr<ID2D1Layer> selectionLayer;
    float selectionLayerWidth{}, selectionLayerHeight{};
    uint64_t selectionLayerGeneration{};
    bool overlaySelectionDirty = true, overlayHighlightsDirty = true;
    bool focusMode = false;
    std::wstring documentFilePath;
    uint64_t imageLocationRevision = 1u;
    std::optional<RichText::SourceSnapshot> imageSource;
    RichText::Detail::RichImageResources images;
    RichText::Detail::RichEditorLayout layout;
    RichText::MarkdownParser markdown;
    RichText::SemanticBuilder semantic;
    RichText::Detail::RichSourceInk sourceInk;
    RichText::Detail::RichSourceGuides sourceGuides;
    RichText::LayoutOptions sourceGuideOptions;
    std::shared_ptr<const RichText::CodeTokenizer> codeTokenizer;
    std::shared_ptr<const std::function<void(std::wstring_view)>> onCodeCopyRequested;
    std::shared_ptr<const std::function<std::optional<std::wstring>(std::wstring_view)>> htmlPasteConverter;
    std::wstring codeCopyAccessibleName;
    std::shared_ptr<const RichText::SemanticDocument> codeInkDocument;
    std::vector<RichText::TextInkRange> codeInk, codeInkStaged;
    size_t codeInkCursor{};
    bool codeInkComplete{}, codeCallbackActive{};
    struct SourceEditingSession
    {
        RichText::MarkdownSourceRegion region;
        RichText::SourceSnapshot snapshot;
    };
    std::optional<SourceEditingSession> sourceEditing;
    uint64_t nextSourceEditingSession = 1u;
    struct PreparationRejection
    {
        RichText::SourceSnapshot source;
        std::optional<RichText::MarkdownProfile> profile;
        std::optional<RichText::MarkdownSourceRegion> region;
        RichText::ViewMode mode{};
        float width{}, dpi{};
        uint64_t typography{}, resources{}, binding{}, appearance{};
        bool rtl{};
    };
    std::optional<PreparationRejection> preparationRejected;
    bool RejectsPreparation(float width, float dpi, bool rtl, uint64_t typography, uint64_t resources) const noexcept
    {
        return preparationRejected && markdownRendering && preparationRejected->source.IsSameRevision(document.Snapshot()) &&
               preparationRejected->profile == markdownProfile && preparationRejected->mode == mode && preparationRejected->width == width &&
               preparationRejected->dpi == dpi && preparationRejected->rtl == rtl && preparationRejected->typography == typography &&
               preparationRejected->resources == resources && preparationRejected->binding == bindingRevision &&
               preparationRejected->appearance == appearanceRevision &&
               preparationRejected->region == (sourceEditing ? std::optional(sourceEditing->region) : std::nullopt);
    }
    std::shared_ptr<const RichText::MarkdownSyntax> semanticRejected;
    struct NativeGeometryRejection
    {
        std::shared_ptr<const RichText::SemanticDocument> document;
        float width{}, dpi{};
        uint64_t typography{}, resources{}, binding{}, appearance{};
        bool rtl{};
    };
    std::optional<NativeGeometryRejection> nativeGeometryRejected;
    struct NativeObjectRejections
    {
        NativeObjectRejections() : objects(0u)
        {
        }
        std::shared_ptr<const RichText::MarkdownSyntax> syntax;
        RichText::SourceSnapshot source;
        std::vector<RichText::MarkdownRange> objects;
        float width{}, dpi{};
        uint64_t typography{}, resources{}, binding{}, appearance{};
        bool rtl{};
        bool Matches(const std::shared_ptr<const RichText::MarkdownSyntax>& value,
                     float nextWidth,
                     float nextDpi,
                     bool nextRtl,
                     uint64_t nextTypography,
                     uint64_t nextResources,
                     uint64_t nextBinding,
                     uint64_t nextAppearance) const noexcept
        {
            return (! value || syntax == value) && width == nextWidth && dpi == nextDpi && rtl == nextRtl && typography == nextTypography &&
                   resources == nextResources && binding == nextBinding && appearance == nextAppearance;
        }
    };
    std::optional<NativeObjectRejections> nativeObjectRejections;
    bool RejectsNativeGeometry(const std::shared_ptr<const RichText::SemanticDocument>& value,
                               float width,
                               float dpi,
                               bool rtl,
                               uint64_t typography,
                               uint64_t resources) const noexcept
    {
        return nativeGeometryRejected && nativeGeometryRejected->document == value && nativeGeometryRejected->width == width &&
               nativeGeometryRejected->dpi == dpi && nativeGeometryRejected->rtl == rtl && nativeGeometryRejected->typography == typography &&
               nativeGeometryRejected->resources == resources && nativeGeometryRejected->binding == bindingRevision &&
               nativeGeometryRejected->appearance == appearanceRevision;
    }
    struct MarkdownCapacityRejection
    {
        RichText::SourceSnapshot source;
        uint64_t revision{};
        RichText::MarkdownProfile profile{};
    };
    std::optional<MarkdownCapacityRejection> markdownCapacityRejected;
    bool HasCurrentMarkdownCapacityRejection() const noexcept
    {
        return markdownCapacityRejected && markdownProfile && markdownCapacityRejected->revision == document.Revision() &&
               markdownCapacityRejected->profile == *markdownProfile && markdownCapacityRejected->source.IsSameRevision(document.Snapshot());
    }
    std::shared_ptr<RichText::Detail::AccessibilityEpoch> accessibilityEpoch;
    bool markdownRendering  = false;
    size_t semanticAdvances = 0, lastAdvanceSemanticWork = 0;
    std::optional<RichText::MarkdownProfile> markdownProfile;
    uint64_t markdownRequestRevision = 0;
    bool markdownRequestActive       = false;
    size_t markdownAdvances = 0, lastAdvanceMarkdownWork = 0;
    RichText::ViewMode mode = RichText::ViewMode::Rich;
    std::shared_ptr<const std::function<void()>> onTextChanged;
    std::shared_ptr<const std::function<void()>> onSelectionChanged;
    std::shared_ptr<const std::function<void(float)>> onScrollChanged;
    float publishedScrollDip = 0.0f;
    std::shared_ptr<const std::function<void(float)>> onHorizontalScrollChanged;
    float horizontalScrollDip{}, publishedHorizontalScrollDip{};
    RichText::Selection publishedSelection{};
    bool publishedSelectionGesture      = false;
    uint64_t publishedSelectionRevision = 0u;
    uint64_t typographyRevision         = 0;
    uint64_t appearanceRevision         = 0;
    uint64_t bindingRevision            = 0;
    uint64_t fileDropEpoch              = 0;
    uint64_t resourceGeneration         = 0;
    struct LayoutKey
    {
        uint64_t revision{}, binding{}, typography{}, resources{};
        IDWriteFactory* factory{}; // Identity only; staged layouts retain the factory through WIL.
        float width{};
        RichText::ViewMode mode                 = RichText::ViewMode::Rich;
        bool operator==(const LayoutKey&) const = default;
    };
    LayoutKey requestKey;
    bool requestActive         = false;
    size_t preparationAdvances = 0, cancelledPreparations = 0, lastAdvanceLayouts = 0, lastAdvanceBlockSteps = 0;
    float layoutWidth      = 0;
    float scrollDip        = 0;
    bool revealCaret       = true;
    bool readOnly          = false;
    bool dragging          = false;
    bool scrollbarDragging = false;
    float scrollbarDragY = 0.0f, scrollbarDragOffset = 0.0f;
    bool caretVisible            = true;
    wchar_t pendingLeadSurrogate = 0;
    std::optional<uint64_t> blinkEpoch;
    std::optional<float> preferredCaretX;
    // Native visual affinity only. Several padded cells share a real source ACP;
    // this identity never enters Document selection, TSF text or undo records.
    size_t sourceCaretSite = RichText::MarkdownNoNode;
    std::shared_ptr<const RichText::MarkdownCommandPlan> taskPressPlan;
    D2D1_RECT_F taskPressRect{};
    std::shared_ptr<const RichText::SemanticDocument> codePressDocument;
    size_t codePressElement = RichText::MarkdownNoNode;
    D2D1_RECT_F codePressRect{};
    bool codePressGesture{};
};
#pragma warning(pop)

RichTextEditor::RichTextEditor(std::wstring text) : _state(std::make_unique<State>(std::move(text)))
{
    SetFocusable(true);
    EnableContentPreparation();
}
RichTextEditor::~RichTextEditor() noexcept
{
    RetireSemanticAccessibility();
}
void RichTextEditor::SetText(std::wstring text)
{
    _state->document.Reset(std::move(text));
    _state->highlights.clear();
    _state->decorations.clear();
    _state->decorationStatus = RichTextDecorationStatus::None;
    _state->images.Cancel();
    _state->sourceEditing.reset();
    _state->markdownCapacityRejected.reset();
    _state->nativeGeometryRejected.reset();
    RetireSemanticAccessibility();
    _state->sourceCaretSite     = RichText::MarkdownNoNode;
    _state->scrollDip           = 0;
    _state->horizontalScrollDip = 0;
    RetireLayout();
    SelectionChanged();
}
std::wstring RichTextEditor::GetText() const
{
    return _state->document.Text();
}
RichText::SourceSnapshot RichTextEditor::Snapshot() const noexcept
{
    return _state->document.Snapshot();
}
bool RichTextEditor::SynchronizeReadOnlySourceSnapshot(const RichText::SourceSnapshot& source, RichText::Selection selection)
{
    if (! IsReadOnly() || GetViewMode() != RichText::ViewMode::Source)
        return false;

    const bool sourceChanged     = ! Snapshot().IsSameRevision(source);
    const auto previousSelection = GetSelection();
    if (! _state->document.AdoptReadOnlySourceSnapshot(source, selection))
        return false;
    if (! sourceChanged && previousSelection == selection)
        return true;

    if (sourceChanged)
    {
        _state->highlights.clear();
        _state->decorations.clear();
        _state->decorationStatus = RichTextDecorationStatus::None;
        _state->images.Cancel();
        _state->sourceEditing.reset();
        _state->markdownCapacityRejected.reset();
        _state->nativeGeometryRejected.reset();
        RetireSemanticAccessibility();
        _state->sourceInk.Clear();
        _state->hoveredLink.reset();
        _state->caretLink.reset();
        _state->taskPressPlan.reset();
        _state->codePressDocument.reset();
        ++_state->fileDropEpoch;
        if (_state->layout.HasPendingPreparation())
            ++_state->cancelledPreparations;
        _state->layout.Invalidate();
        _state->markdown.Cancel();
        _state->semantic.Cancel();
        _state->semanticRejected.reset();
        _state->markdownRequestActive = false;
        _state->requestActive         = false;
        RequestContentPreparation(false);
    }
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    // Match SetText/SetSelection notification ordering, including TSF synchronization and UIA publication.
    // Keep this as the final operation because owner callbacks may re-enter or retire the control.
    SelectionChanged(true, false, true, false);
    return true;
}
HRESULT RichTextEditor::TryAdoptReadOnlyMarkdownSyntax(const RichText::SourceSnapshot& source,
                                                       const std::shared_ptr<const RichText::MarkdownSyntax>& syntax) noexcept
{
    if (! IsReadOnly() || GetViewMode() != RichText::ViewMode::Source || ! _state->markdownProfile || ! Snapshot().IsSameRevision(source))
        return S_FALSE;

    const auto previous = _state->markdown.Published();
    try
    {
        if (! _state->markdown.TryAdoptParsedSnapshot(source, Revision(), syntax, *_state->markdownProfile))
            return S_FALSE;
    }
    catch (const std::bad_alloc&)
    {
        // Resource exhaustion crosses this noexcept preparation boundary as an
        // HRESULT. The parser stages its allocation before changing state.
        return E_OUTOFMEMORY;
    }
    if (_state->markdown.Published() == previous)
        return S_OK;

    // Source geometry depends on the unchanged snapshot, not the parse owner.
    // Rebuild only its colored ink; preserve selection, scroll and caret state.
    _state->sourceInk.Clear();
    _state->markdownRequestActive = false;
    _state->markdownCapacityRejected.reset();
    _state->semantic.Clear();
    _state->semanticRejected.reset();
    _state->nativeGeometryRejected.reset();
    _state->nativeObjectRejections.reset();
    // Scheduling may re-enter or retire the owner. No member access follows.
    RequestContentPreparation();
    return S_OK;
}
void RichTextEditor::SetDocumentFilePath(std::wstring path)
{
    auto normalized = RichText::Detail::NormalizeDocumentFilePath(path);
    if (normalized == _state->documentFilePath)
        return;
    _state->documentFilePath.swap(normalized);
    ++_state->imageLocationRevision;
    _state->images.Clear();
    RetireSemanticAccessibility();
    _state->nativeGeometryRejected.reset();
    RetireLayout();
    // Metadata invalidation also changes the embedded transport revision.
    // Do not synchronize/cancel an active composition or edit source/history.
    RequestInvalidate();
}
std::wstring_view RichTextEditor::GetDocumentFilePath() const noexcept
{
    return _state->documentFilePath;
}
void RichTextEditor::SetRemoteImagesEnabled(bool enabled)
{
    if (_state->images.RemoteImagesEnabled() == enabled)
        return;
    _state->images.SetRemoteImagesEnabled(enabled);
    RetireSemanticAccessibility();
    _state->nativeGeometryRejected.reset();
    RetireLayout();
    RequestInvalidate();
}
bool RichTextEditor::RemoteImagesEnabled() const noexcept
{
    return _state->images.RemoteImagesEnabled();
}
void RichTextEditor::TakePendingImageRequests(std::vector<RichText::RichImageRequest>& requests)
{
    const auto* host    = GetHost();
    const auto document = host ? GetAcknowledgedSemanticDocument(*host) : nullptr;
    if (! GetHost() || ! IsVisible() || ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich || ! document ||
        ! _state->images.Current(document, _state->imageLocationRevision, _state->bindingRevision))
    {
        requests.clear();
        return;
    }
    _state->images.Take(requests);
}
bool RichTextEditor::CompleteImageRequest(const RichText::RichImageRequest& request, std::shared_ptr<const RichText::RichImage> image)
{
    const auto document  = GetSemanticDocument();
    const bool suspended = _state->images.CanCompleteSuspended(_state->imageLocationRevision, _state->bindingRevision);
    if (! GetHost() || ! IsVisible() || ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich ||
        (! suspended && (! document || ! _state->images.CurrentForCompletion(document, _state->imageLocationRevision, _state->bindingRevision))))
        return false;
    const auto viewport = TextViewport();
    const auto* host    = GetHost();
    if (_state->RejectsPreparation(
            std::max(1.0f, viewport.right - viewport.left), host->GetDpi(), IsRightToLeft(), host->GetTypographyRevision(), host->GetResourceGeneration()))
        return false;
    if (document && _state->RejectsNativeGeometry(document,
                                                  std::max(1.0f, viewport.right - viewport.left),
                                                  host->GetDpi(),
                                                  IsRightToLeft(),
                                                  host->GetTypographyRevision(),
                                                  host->GetResourceGeneration()))
        return false;
    const bool alreadyDirty = _state->images.HasUnpublished();
    if (! _state->images.Complete(request, std::move(image)))
        return false;
    if (! suspended && ! alreadyDirty && _state->images.HasUnpublished())
    {
        RetireSemanticAccessibility();
        _state->hoveredLink.reset();
        _state->caretLink.reset();
        if (_state->layout.HasPendingPreparation())
            ++_state->cancelledPreparations;
        _state->layout.Invalidate();
        _state->requestActive = false;
        RequestContentPreparation(false);
        RequestInvalidate(); // May retire this control; no member access follows.
    }
    return true;
}
void RichTextEditor::ReloadImages()
{
    _state->images.Clear();
    RetireSemanticAccessibility();
    RetireLayout();
    RequestInvalidate();
}
RichText::RichImageResourceStatistics RichTextEditor::GetImageResourceStatistics() const noexcept
{
    return _state->images.Statistics();
}
bool RichTextEditor::CanPasteMarkdownFileReferences(RichText::Selection selection) const noexcept
{
    using namespace RichText;
    if (! Editable() || ! _state->markdownRendering || _state->mode != ViewMode::Rich || _state->sourceEditing)
        return false;
    const auto syntax = GetMarkdownSyntax();
    if (! syntax || syntax->SourceRegion())
        return false;
    const auto text = syntax->Source();
    for (const auto& token : syntax->Inlines())
        if (token.kind == MarkdownInlineKind::Code || token.kind == MarkdownInlineKind::Html || token.kind == MarkdownInlineKind::Image ||
            token.kind == MarkdownInlineKind::Link)
        {
            const bool overlaps = selection.Start() < token.source.last && selection.End() > token.source.first;
            const bool encloses = selection.Start() <= token.source.first && selection.End() >= token.source.last;
            if ((selection.Start() == selection.End() && selection.caret > token.source.first && selection.caret < token.source.last) ||
                (overlaps && ! encloses))
                return false;
        }
    for (std::size_t index = 1u; index < syntax->Nodes().size(); ++index)
    {
        const auto& node = syntax->Nodes()[index];
        const auto kind  = node.block.kind;
        bool literalNote{};
        if (kind == MarkdownBlockKind::FootnoteDefinition)
        {
            const auto note = std::ranges::find(syntax->Footnotes(), index, &MarkdownFootnote::definition);
            literalNote     = note == syntax->Footnotes().end() || ! note->ordinal;
        }
        if (kind != MarkdownBlockKind::FencedCode && kind != MarkdownBlockKind::IndentedCode && kind != MarkdownBlockKind::HtmlBlock &&
            kind != MarkdownBlockKind::FrontMatter && ! literalNote)
            continue;
        const auto range = node.block.source;
        if (selection.Start() != selection.End())
        {
            if (selection.Start() < range.last && selection.End() > range.first && ! (selection.Start() <= range.first && selection.End() >= range.last))
                return false;
        }
        else
        {
            const bool eofInside = range.last == text.size() && (kind == MarkdownBlockKind::HtmlBlock || literalNote || ! node.block.closed ||
                                                                 (! text.empty() && text.back() != L'\r' && text.back() != L'\n'));
            if (selection.caret >= range.first &&
                (selection.caret < range.last || (selection.caret == range.last && (eofInside || kind == MarkdownBlockKind::HtmlBlock || literalNote))))
                return false;
        }
    }
    return true;
}
bool RichTextEditor::TryPrepareClipboardPaste(const TextClipboardContent& content, std::wstring& replacement) const
{
    return TryPrepareClipboardPaste(content, replacement, nullptr);
}
bool RichTextEditor::TryPrepareClipboardPaste(const TextClipboardContent& content, std::wstring& replacement, bool* stale) const
{
    if (stale)
        *stale = false;
    const auto prepareFileReference = [&]()
    {
        try
        {
            return TryPrepareFileReferencesAt(content, GetSelection(), replacement);
        }
        catch (const std::invalid_argument&)
        {
            return false; // Invalid external file text never publishes a partial plan.
        }
        catch (const std::length_error&)
        {
            return false; // Capacity rejection retains the caller's result.
        }
    };
    if (! content.filePaths.empty())
        return prepareFileReference();
    const auto selection = GetSelection();
    if (Editable() && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich && content.hasHtmlFragment &&
        content.htmlFragment.size() <= RichText::Document::MaximumUnits && RichText::IsValidUtf16(content.htmlFragment) && _state->htmlPasteConverter)
    {
        const auto converter = _state->htmlPasteConverter;
        const auto lifetime  = GetLifetimeToken();
        const auto revision  = Revision();
        const auto binding   = _state->bindingRevision;
        const auto host      = GetHost();
        const auto focus     = host ? host->GetFocusControl() : nullptr;
        try
        {
            auto converted = (*converter)(content.htmlFragment);
            if (lifetime.expired() || GetHost() != host || (host && host->GetFocusControl() != focus) || Revision() != revision ||
                _state->bindingRevision != binding || GetSelection() != selection || _state->htmlPasteConverter != converter || ! Editable() ||
                ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich)
            {
                if (stale)
                    *stale = true;
                return false;
            }
            if (converted && converted->size() <= RichText::Document::MaximumUnits && RichText::IsValidUtf16(*converted))
            {
                replacement.swap(*converted);
                return true;
            }
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception&)
        {
            if (stale && (lifetime.expired() || GetHost() != host || (host && host->GetFocusControl() != focus) || Revision() != revision ||
                          _state->bindingRevision != binding || GetSelection() != selection || _state->htmlPasteConverter != converter || ! Editable() ||
                          ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich))
                *stale = true;
            // Application converter failure retains ordinary Unicode paste unless the target changed.
            return false;
        }
    }
    if (! Editable() || ! _state->appearance.pasteUrlOnSelection || ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich ||
        selection.Start() == selection.End())
        return prepareFileReference();
    const std::wstring_view url(content.text);
    const auto startsScheme = [&](std::wstring_view prefix) noexcept
    {
        if (url.size() <= prefix.size())
            return false;
        for (size_t index = 0u; index < prefix.size(); ++index)
        {
            auto character = url[index];
            if (character >= L'A' && character <= L'Z')
                character = static_cast<wchar_t>(character + (L'a' - L'A'));
            if (character != prefix[index])
                return false;
        }
        return true;
    };
    if (! (startsScheme(L"https://") || startsScheme(L"http://") || startsScheme(L"mailto:")))
        return prepareFileReference();
    const auto syntax = GetMarkdownSyntax();
    if (! syntax)
        return prepareFileReference();
    RichText::MarkdownCommandRequest request;
    request.command     = RichText::MarkdownCommand::SetLink;
    request.destination = url;
    try
    {
        const auto plan  = RichText::BuildMarkdownCommand(Snapshot(), Revision(), syntax, selection, request);
        const auto edits = plan.Edits();
        if (! plan.IsApplicable() || edits.size() != 1u || edits[0].source.first != selection.Start() || edits[0].source.last != selection.End())
            return prepareFileReference();
        replacement = edits[0].replacement;
        return true;
    }
    catch (const std::invalid_argument&)
    {
        return prepareFileReference(); // Ineligible destination retains ordinary paste.
    }
    catch (const std::length_error&)
    {
        return prepareFileReference();
    }
}
bool RichTextEditor::TryPrepareFileReferencesAt(const TextClipboardContent& content,
                                                RichText::Selection selection,
                                                std::wstring& replacement,
                                                size_t targetSite) const
{
    if (! Editable())
        return false;
    if (content.filePaths.size() > TextClipboardContent::MaximumFiles)
        throw std::length_error("Clipboard file capacity exceeded");
    std::size_t totalFileUnits{};
    for (const auto& file : content.filePaths)
    {
        if (file.empty() || ! RichText::IsValidUtf16(file) || file.find(L'\0') != file.npos || file.find_first_of(L"\"\r\n") != file.npos ||
            file.size() > TextClipboardContent::MaximumFileUnits)
            throw std::invalid_argument("Invalid Unicode clipboard file path");
        if (file.size() > TextClipboardContent::MaximumTotalFileUnits - totalFileUnits)
            throw std::length_error("Clipboard file capacity exceeded");
        totalFileUnits += file.size();
    }
    const bool references = CanPasteMarkdownFileReferences(selection);
    const auto syntax     = GetMarkdownSyntax();
    std::wstring_view eol = L"\r\n";
    std::wstring sourceStorage(0u, L' ');
    if (! syntax)
        sourceStorage = Snapshot().Text();
    const std::wstring_view source = syntax ? syntax->Source() : std::wstring_view(sourceStorage);
    if (const auto at = source.find_first_of(L"\r\n"); at != source.npos)
        eol = source.substr(at, source[at] == L'\r' && at + 1u < source.size() && source[at + 1u] == L'\n' ? 2u : 1u);
    std::optional<RichText::Detail::FileReferenceContext> context;
    if (content.filePaths.size() > 1u && syntax && ! syntax->SourceRegion() && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich &&
        ! _state->sourceEditing)
    {
        auto* host           = GetHost();
        const auto* geometry = _state->layout.SemanticGeometry();
        const auto site      = targetSite == RichText::MarkdownNoNode ? _state->sourceCaretSite : targetSite;
        if ((targetSite != RichText::MarkdownNoNode || selection == GetSelection()) && selection.Start() == selection.End() && host && LayoutIsCurrent(*host) &&
            geometry && geometry->IsCellSourceSite(selection.caret, site))
        {
            context.emplace();
            context->table = true;
        }
        else
            context = RichText::Detail::FileReferenceInsertionContext(Snapshot(), Revision(), syntax, selection);
        if (! context)
            return false;
    }
    std::wstring referenceSeparator(0u, L' '), literalSeparator(0u, L' ');
    const bool inlineOpaque = context && ! references && syntax &&
                              std::ranges::any_of(syntax->Inlines(),
                                                  [&](const auto& token)
    {
        const bool opaque = token.kind == RichText::MarkdownInlineKind::Code || token.kind == RichText::MarkdownInlineKind::Html ||
                            token.kind == RichText::MarkdownInlineKind::Image || token.kind == RichText::MarkdownInlineKind::Link;
        return opaque && ((selection.Start() < token.source.last && selection.End() > token.source.first) ||
                          (selection.Start() == selection.End() && selection.caret > token.source.first && selection.caret < token.source.last));
    });
    if (context && (context->table || inlineOpaque))
        referenceSeparator = literalSeparator = L" ";
    else
    {
        literalSeparator.append(eol);
        if (context)
            literalSeparator.append(context->continuation);
        referenceSeparator = literalSeparator;
        referenceSeparator.append(literalSeparator);
    }
    std::wstring staged(0u, L' ');
    if (content.filePaths.empty())
    {
        if (! references)
            return false;
        const auto markdown = RichText::Detail::BuildMarkdownFileReference(_state->documentFilePath, content.text);
        if (! markdown)
            return false;
        staged.assign(*markdown);
    }
    else
    {
        bool allReferences = references;
        for (const auto& file : content.filePaths)
        {
            const auto markdown = allReferences ? RichText::Detail::BuildMarkdownFileReference(_state->documentFilePath, file) : std::nullopt;
            if (! markdown)
            {
                allReferences = false;
                break;
            }
            const auto separator = staged.empty() ? 0u : referenceSeparator.size();
            if (separator > RichText::Document::MaximumUnits - staged.size() || markdown->size() > RichText::Document::MaximumUnits - staged.size() - separator)
                throw std::length_error("File reference paste source capacity exceeded");
            if (separator)
                staged.append(referenceSeparator);
            staged.append(*markdown);
        }
        if (! allReferences)
        {
            // A raw/code context or unrepresentable file path keeps every
            // original spelling as quoted source; never drop an unsupported tail.
            staged.clear();
            for (const auto& file : content.filePaths)
            {
                if (! references && context && context->table && file.find(L'|') != std::wstring::npos)
                    return false;
                const auto literal   = references ? RichText::Detail::EscapeFileReferenceLiteral(file) : std::wstring(file);
                const auto separator = staged.empty() ? 0u : literalSeparator.size();
                if (separator + 2u > RichText::Document::MaximumUnits - staged.size() ||
                    literal.size() > RichText::Document::MaximumUnits - staged.size() - separator - 2u)
                    throw std::length_error("Clipboard file source capacity exceeded");
                if (! staged.empty())
                    staged.append(literalSeparator);
                staged.push_back(L'"');
                staged.append(literal);
                staged.push_back(L'"');
                if (staged.size() > RichText::Document::MaximumUnits)
                    throw std::length_error("Clipboard file source capacity exceeded");
            }
        }
    }
    replacement.swap(staged);
    return true;
}
bool RichTextEditor::ApplyClipboardFilePaste(const TextClipboardContent& content)
{
    if (content.filePaths.empty())
        return false;
    std::wstring replacement(0u, L' ');
    if (! TryPrepareClipboardPaste(content, replacement))
        return false;
    const auto selection = GetSelection();
    if (_state->sourceCaretSite != RichText::MarkdownNoNode && (! GetHost() || ! LayoutIsCurrent(*GetHost())))
        return false; // Never discard a pending source site's identity.
    if (selection.Start() == selection.End())
        if (auto* host = GetHost(); host && LayoutIsCurrent(*host))
            if (const auto* geometry = _state->layout.SemanticGeometry(); geometry && geometry->IsCellSourceSite(selection.caret, _state->sourceCaretSite))
            {
                RichText::MarkdownCommandRequest request;
                request.command    = RichText::MarkdownCommand::SetTableCell;
                request.targetCell = geometry->SourceSyntaxNode(selection.caret, _state->sourceCaretSite);
                request.text       = replacement;
                return ExecuteMarkdownCommand(request) == RichText::MarkdownCommandStatus::Changed;
            }
    return ReplaceSelection(std::move(replacement));
}
std::optional<RichTextEditor::FileDropPosition> RichTextEditor::FileDropCaret(const ControlHost& host, D2D1_POINT_2F point) const
{
    if (GetHost() != &host || ! Editable() || ! LayoutIsCurrent(host) ||
        (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich && ! GetMarkdownSyntax()) || ! std::isfinite(point.x) ||
        ! std::isfinite(point.y))
        return std::nullopt;
    const auto viewport = TextViewport();
    if (point.x < viewport.left || point.x >= viewport.right || point.y < viewport.top || point.y >= viewport.bottom)
        return std::nullopt;
    const auto x = point.x - viewport.left - HorizontalTranslation(), y = point.y - viewport.top + _state->scrollDip;
    const auto hit = _state->layout.HitTestSource(x, y);
    if (const auto* geometry = _state->layout.SemanticGeometry())
    {
        const auto syntax = GetMarkdownSyntax();
        if (! syntax)
            return std::nullopt;
        const auto& document = geometry->Document();
        const auto visual    = geometry->HitTest(x, y);
        if (visual.inside)
        {
            const auto position = visual.trailing && visual.position ? visual.position - 1u : visual.position;
            const auto spans    = document.Map().Spans();
            const auto span =
                std::upper_bound(spans.begin(), spans.end(), position, [](size_t offset, const auto& value) { return offset < value.display.last; });
            if (span != spans.end() && span->kind == RichText::DisplaySpanKind::Object && position >= span->display.first && position < span->display.last)
                return std::nullopt;
        }
    }
    const auto source = Snapshot();
    if (! source.IsBoundary(hit.position))
        return std::nullopt;
    // PreparedLayout::HitTest snaps to its retained Unicode grapheme stops;
    // semantic mapping additionally snaps atomic source spans. Explicit caret
    // sites are parser-owned line/cell boundaries, never interior scalar ACPs.
    return FileDropPosition{hit.position, hit.site};
}
uint64_t RichTextEditor::FileDropEpoch() const noexcept
{
    return _state->fileDropEpoch;
}
bool RichTextEditor::CommitFileDrop(FileDropPosition position, const TextClipboardContent& content)
{
    if (content.filePaths.empty())
        return false;
    for (const auto& path : content.filePaths)
        if (path.find_first_of(L"\"\r\n") != std::wstring::npos)
            return false;
    const auto source = Snapshot();
    const auto caret  = position.caret;
    if (! source.IsBoundary(caret))
        return false;
    std::wstring replacement(0u, L' ');
    if (! TryPrepareFileReferencesAt(content, {caret, caret}, replacement, position.site) || replacement.empty())
        return false;
    if (auto* host = GetHost(); host && LayoutIsCurrent(*host))
        if (const auto* geometry = _state->layout.SemanticGeometry(); geometry && geometry->IsCellSourceSite(caret, position.site))
        {
            RichText::MarkdownCommandRequest request;
            request.command     = RichText::MarkdownCommand::SetTableCell;
            request.targetCell  = geometry->SourceSyntaxNode(caret, position.site);
            request.text        = replacement;
            const auto revision = Revision();
            const auto plan     = RichText::BuildMarkdownCommand(source, revision, GetMarkdownSyntax(), {caret, caret}, request);
            if (! plan.IsApplicable())
                return false;
            std::vector<RichText::SourceEdit> edits(0u);
            edits.reserve(plan.Edits().size());
            for (const auto& edit : plan.Edits())
                edits.push_back({edit.source, edit.replacement});
            return ApplySourceEdits(source, revision, edits, plan.After());
        }
    const RichText::SourceEdit edit{{caret, caret}, replacement};
    const auto after = caret + replacement.size();
    return ApplySourceEdits(source, Revision(), std::span(&edit, 1u), {after, after});
}
uint64_t RichTextEditor::Revision() const noexcept
{
    return _state->document.Revision();
}
void RichTextEditor::SetSelection(RichText::Selection selection)
{
    _state->document.SetSelection(selection);
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    SelectionChanged();
}
RichText::Selection RichTextEditor::GetSelection() const noexcept
{
    return _state->document.GetSelection();
}
std::optional<RichText::SourceLocation> RichTextEditor::GetSourceCaretLocation() const noexcept
{
    return Snapshot().LocationAt(GetSelection().caret);
}
void RichTextEditor::SetViewMode(RichText::ViewMode mode)
{
    if (mode != RichText::ViewMode::Source && mode != RichText::ViewMode::Rich)
        throw std::invalid_argument("Invalid rich-text view mode");
    if (_state->mode == mode)
        return;
    _state->autoFormatRun.reset();
    _state->smartTypingSite.reset();
    RetireSemanticAccessibility();
    _state->mode = mode;
    _state->images.Cancel();
    _state->sourceEditing.reset();
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    RetireLayout();
    SelectionChanged();
}
RichText::ViewMode RichTextEditor::GetViewMode() const noexcept
{
    return _state->mode;
}
void RichTextEditor::SetAppearance(const RichTextEditorAppearance& appearance)
{
    if (! std::isfinite(appearance.zoom) || appearance.zoom < .25f || appearance.zoom > 4.0f || ! std::isfinite(appearance.tabStopDip) ||
        appearance.tabStopDip < 0.0f || appearance.tabStopDip > 4096.0f || (appearance.tabStopDip > 0.0f && appearance.tabStopDip < 1.0f))
        throw std::invalid_argument("Invalid rich editor zoom or tab stop");
    const auto validFont = [](std::wstring_view value) noexcept
    { return value.size() <= 255u && value.find(L'\0') == value.npos && RichText::IsValidUtf16(value); };
    const auto validSize    = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 512.0f)); };
    const auto validHeight  = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 4.0f)); };
    const auto validSpacing = [](const std::optional<float>& value, float maximum) noexcept
    { return ! value || (std::isfinite(value.value()) && value.value() >= 0.0f && value.value() <= maximum); };
    if (appearance.sourceTabSpaces > 16u || ! std::isfinite(appearance.maximumTextWidthDip) || appearance.maximumTextWidthDip < 0.0f ||
        appearance.maximumTextWidthDip > 1048576.0f || ! std::isfinite(appearance.paddingDip) ||
        (appearance.paddingDip != -1.0f && (appearance.paddingDip < 0.0f || appearance.paddingDip > 256.0f)) || ! validSize(appearance.bodyFontSizeDip) ||
        ! validSize(appearance.codeFontSizeDip) || ! std::isfinite(appearance.lineHeightMultiplier) ||
        (appearance.lineHeightMultiplier != 0.0f && (appearance.lineHeightMultiplier < 1.0f || appearance.lineHeightMultiplier > 4.0f)) ||
        ! std::isfinite(appearance.headingScaleStep) || appearance.headingScaleStep < 0.0f || appearance.headingScaleStep > 1.0f ||
        ! validFont(appearance.bodyFontFamily) || ! validFont(appearance.codeFontFamily) || ! std::ranges::all_of(appearance.headingFontSizeDip, validSize) ||
        ! std::ranges::all_of(appearance.headingLineHeightMultiplier, validHeight) || ! validSpacing(appearance.paragraphSpacingAfterLines, 4.0f) ||
        ! validSpacing(appearance.listItemSpacingDip, 256.0f) || (appearance.semanticSpacing && ! appearance.semanticSpacing->IsValid()) ||
        (appearance.markdownPresentation != RichText::MarkdownPresentationStyle::Default &&
         appearance.markdownPresentation != RichText::MarkdownPresentationStyle::Subtle))
        throw std::invalid_argument("Invalid rich editor appearance");
    if (_state->appearance == appearance)
        return;
    auto next            = appearance; // Stage all owned strings before publishing any option.
    const auto& previous = _state->appearance;
    const bool geometryChanged =
        previous.tableOverflow != next.tableOverflow || previous.zoom != next.zoom || previous.tabStopDip != next.tabStopDip ||
        previous.sourceMonospace != next.sourceMonospace || previous.sourceWrap != next.sourceWrap || previous.sourceLineNumbers != next.sourceLineNumbers ||
        previous.sourceWhitespaceMarks != next.sourceWhitespaceMarks || previous.showVerticalScrollbar != next.showVerticalScrollbar ||
        previous.maximumTextWidthDip != next.maximumTextWidthDip || previous.paddingDip != next.paddingDip ||
        previous.bodyFontSizeDip != next.bodyFontSizeDip || previous.codeFontSizeDip != next.codeFontSizeDip ||

        previous.lineHeightMultiplier != next.lineHeightMultiplier || previous.headingScaleStep != next.headingScaleStep ||
        previous.bodyFontFamily != next.bodyFontFamily || previous.codeFontFamily != next.codeFontFamily ||
        previous.headingFontSizeDip != next.headingFontSizeDip || previous.headingLineHeightMultiplier != next.headingLineHeightMultiplier ||
        previous.paragraphSpacingAfterLines != next.paragraphSpacingAfterLines || previous.listItemSpacingDip != next.listItemSpacingDip ||
        previous.semanticSpacing != next.semanticSpacing || previous.markdownPresentation != next.markdownPresentation;
    const bool sourceInkChanged = previous.colorizeMarkdownSource != next.colorizeMarkdownSource;
    // Staged strings publish without allocator-backed aggregate moves; Debug
    // STL proxies can allocate during those moves.
    auto& value                     = _state->appearance;
    value.markdownPresentation      = next.markdownPresentation;
    value.drawBorder                = next.drawBorder;
    value.drawFocusIndicator        = next.drawFocusIndicator;
    value.blendWithWindowBackground = next.blendWithWindowBackground;
    value.useAccentLinkColor        = next.useAccentLinkColor;
    value.colorizeMarkdownSource    = next.colorizeMarkdownSource;
    value.directSourceInkPreview    = next.directSourceInkPreview;
    value.showVerticalScrollbar     = next.showVerticalScrollbar;
    value.sourceMonospace           = next.sourceMonospace;
    value.tableOverflow             = next.tableOverflow;
    value.sourceWrap                = next.sourceWrap;
    value.sourceLineNumbers         = next.sourceLineNumbers;
    value.sourceWhitespaceMarks     = next.sourceWhitespaceMarks;
    value.pasteUrlOnSelection       = next.pasteUrlOnSelection;

    value.zoom                        = next.zoom;
    value.tabStopDip                  = next.tabStopDip;
    value.sourceTabSpaces             = next.sourceTabSpaces;
    value.sourceTabUsesTabs           = next.sourceTabUsesTabs;
    value.maximumTextWidthDip         = next.maximumTextWidthDip;
    value.paddingDip                  = next.paddingDip;
    value.bodyFontSizeDip             = next.bodyFontSizeDip;
    value.codeFontSizeDip             = next.codeFontSizeDip;
    value.lineHeightMultiplier        = next.lineHeightMultiplier;
    value.headingScaleStep            = next.headingScaleStep;
    value.headingFontSizeDip          = next.headingFontSizeDip;
    value.headingLineHeightMultiplier = next.headingLineHeightMultiplier;
    value.paragraphSpacingAfterLines  = next.paragraphSpacingAfterLines;
    value.listItemSpacingDip          = next.listItemSpacingDip;
    value.semanticSpacing             = next.semanticSpacing;
    value.bodyFontFamily.swap(next.bodyFontFamily);
    value.codeFontFamily.swap(next.codeFontFamily);
    const auto lifetime = GetLifetimeToken();
    if (geometryChanged)
    {
        ++_state->appearanceRevision;
        _state->nativeGeometryRejected.reset();
        RetireLayout();
    }
    else if (sourceInkChanged)
    {
        _state->sourceInk.Clear();
        RequestContentPreparation(false);
    }
    if (! lifetime.expired())
        RequestInvalidate();
}
const RichTextEditorAppearance& RichTextEditor::GetAppearance() const noexcept
{
    return _state->appearance;
}
void RichTextEditor::SetZoom(float zoom)
{
    auto appearance = GetAppearance();
    appearance.zoom = zoom;
    SetAppearance(appearance);
}
float RichTextEditor::GetZoom() const noexcept
{
    return _state->appearance.zoom;
}
void RichTextEditor::SetLinkUnderlinePolicy(RichTextLinkUnderlinePolicy policy)
{
    if (policy != RichTextLinkUnderlinePolicy::Always && policy != RichTextLinkUnderlinePolicy::HoverOrCaret)
        throw std::invalid_argument("Invalid rich editor link underline policy");
    if (_state->linkUnderlinePolicy == policy)
        return;
    _state->linkUnderlinePolicy = policy;
    RequestInvalidate();
}
RichTextLinkUnderlinePolicy RichTextEditor::GetLinkUnderlinePolicy() const noexcept
{
    return _state->linkUnderlinePolicy;
}
std::optional<RichText::MarkdownRange> RichTextEditor::CaretLinkDisplayRange() const noexcept
{
    if (! HasFocus() || ! IsEnabled() || ! IsVisible())
        return std::nullopt;
    return _state->caretLink;
}
void RichTextEditor::UpdateCaretLinkDisplayRange() noexcept
{
    _state->caretLink.reset();
    const auto* geometry = _state->layout.SemanticGeometry();
    if (! geometry || _state->mode != RichText::ViewMode::Rich || _state->layout.Revision() != Revision())
        return;
    const auto caret = GetSelection().caret;
    for (const auto& element : geometry->Document().Elements())
        if ((element.role == RichText::SemanticRole::Link || element.role == RichText::SemanticRole::FootnoteReference) && element.source.first <= caret &&
            caret < element.source.last)
        {
            _state->caretLink = element.display;
            return;
        }
}
bool RichTextEditor::IsLinkUnderlineVisible(size_t sourceOffset) const noexcept
{
    const auto* host    = GetHost();
    const auto document = host ? GetAcknowledgedSemanticDocument(*host) : nullptr;
    if (! document || sourceOffset >= document->Map().Source().size())
        return false;
    const auto runs = document->SourceRuns();
    const auto run =
        std::lower_bound(runs.begin(), runs.end(), sourceOffset, [](const auto& value, size_t position) { return value.start + value.length <= position; });
    if (run == runs.end() || sourceOffset < run->start)
        return false;
    if (! (run->flags & RichText::Link))
        return run != runs.end() && (run->flags & RichText::Underline);
    if ((run->flags & RichText::Underline) || host->GetTheme().highContrast || _state->linkUnderlinePolicy == RichTextLinkUnderlinePolicy::Always)
        return true;
    const auto display = document->Map().ToDisplay(sourceOffset);
    const auto caret   = CaretLinkDisplayRange();
    return (_state->hoveredLink && _state->hoveredLink->first <= display && display < _state->hoveredLink->last) ||
           (caret && caret->first <= display && display < caret->last);
}
void RichTextEditor::SetSourceHighlights(std::span<const RichTextSourceHighlight> highlights)
{
    if (highlights.size() > 4096u)
        throw std::length_error("Too many rich source highlights");
    const auto source = Snapshot();
    for (const auto& highlight : highlights)
        if (highlight.start > source.Length() || highlight.length > source.Length() - highlight.start || ! source.IsBoundary(highlight.start) ||
            ! source.IsBoundary(highlight.start + highlight.length))
            throw std::invalid_argument("Invalid rich source highlight");
    if (std::ranges::equal(_state->highlights, highlights))
        return;
    std::vector<RichTextSourceHighlight> next(highlights.begin(), highlights.end());
    _state->highlights.swap(next);
    _state->overlayHighlightsDirty = true;
    const auto lifetime            = GetLifetimeToken();
    RequestContentPreparation(false);
    if (! lifetime.expired())
        RequestInvalidate();
}
void RichTextEditor::SetSearchMatches(const RichText::SourceSnapshot& expectedSource,
                                      std::span<const RichText::MarkdownRange> matches,
                                      std::optional<size_t> activeIndex)
{
    if (! Snapshot().IsSameRevision(expectedSource))
        throw std::invalid_argument("Stale search matches");
    if (matches.size() > 4096u)
        throw std::length_error("Too many search matches");
    if (activeIndex && activeIndex.value() >= matches.size())
        throw std::out_of_range("Invalid active search match");
    std::vector<RichTextSourceHighlight> highlights(0u);
    highlights.reserve(matches.size());
    size_t previousEnd = 0u;
    for (size_t i = 0u; i < matches.size(); ++i)
    {
        const auto range = matches[i];
        if (range.first >= range.last || range.first < previousEnd || ! expectedSource.IsBoundary(range.first) || ! expectedSource.IsBoundary(range.last))
            throw std::invalid_argument("Invalid search match range");
        highlights.push_back({range.first, range.last - range.first, activeIndex && activeIndex.value() == i});
        previousEnd = range.last;
    }
    SetSourceHighlights(highlights);
}
void RichTextEditor::ClearSourceHighlights() noexcept
{
    if (_state->highlights.empty())
        return;
    _state->highlights.clear();
    _state->overlayHighlightsDirty = true;
    const auto lifetime            = GetLifetimeToken();
    RequestContentPreparation(false);
    if (! lifetime.expired())
        RequestInvalidate();
}
std::span<const RichTextSourceHighlight> RichTextEditor::GetSourceHighlights() const noexcept
{
    return _state->highlights;
}
void RichTextEditor::SetDecorations(const RichText::SourceSnapshot& expectedSource, std::span<const RichTextDecoration> decorations)
{
    if (! Snapshot().IsSameRevision(expectedSource))
        throw std::invalid_argument("Stale rich decoration source");
    if (decorations.size() > 256u)
        throw std::length_error("Too many rich decorations");
    for (const auto& item : decorations)
        if (item.start > expectedSource.Length() || item.length > expectedSource.Length() - item.start || ! expectedSource.IsBoundary(item.start) ||
            ! expectedSource.IsBoundary(item.start + item.length) ||
            static_cast<unsigned>(item.kind) > static_cast<unsigned>(RichTextDecorationKind::Foreground) ||
            static_cast<unsigned>(item.role) > static_cast<unsigned>(RichTextDecorationRole::Error))
            throw std::invalid_argument("Invalid rich decoration");
    if (std::ranges::equal(_state->decorations, decorations))
        return;
    if (! decorations.empty())
    {
        const auto* host = GetHost();
        if (host && LayoutIsCurrent(*host))
        {
            size_t rectangleCount = 0u;
            for (const auto& item : decorations)
            {
                const auto rectangles = _state->layout.SourceRange(item.start, item.start + item.length);
                if (rectangles.size() > _state->decorationRectangleLimit - rectangleCount)
                    throw std::length_error("Rich decoration rectangle capacity exceeded");
                rectangleCount += rectangles.size();
            }
        }
    }
    std::vector<RichTextDecoration> staged(decorations.begin(), decorations.end());
    _state->decorations.swap(staged);
    _state->decorationStatus       = _state->decorations.empty() ? RichTextDecorationStatus::None : RichTextDecorationStatus::Pending;
    _state->overlayHighlightsDirty = true;
    const auto lifetime            = GetLifetimeToken();
    RequestContentPreparation(false);
    if (! lifetime.expired())
        RequestInvalidate();
}
std::span<const RichTextDecoration> RichTextEditor::GetDecorations() const noexcept
{
    return _state->decorations;
}
RichTextDecorationStatus RichTextEditor::GetDecorationStatus() const noexcept
{
    if (! _state->decorations.empty())
    {
        const auto* host = GetHost();
        if (_state->overlayHighlightsDirty || ! host || ! LayoutIsCurrent(*host))
            return RichTextDecorationStatus::Pending;
    }
    return _state->decorationStatus;
}
#if DXUI_ENABLE_DIAGNOSTICS
void RichTextEditor::DebugSetDecorationRectangleLimitForTest(size_t limit) noexcept
{
    limit = std::min(limit, size_t{4096u});
    if (_state->decorationRectangleLimit == limit)
        return;
    _state->decorationRectangleLimit = limit;
    _state->overlayHighlightsDirty   = true;
    if (! _state->decorations.empty())
        _state->decorationStatus = RichTextDecorationStatus::Pending;
    const auto lifetime = GetLifetimeToken();
    RequestContentPreparation(false);
    if (! lifetime.expired())
        RequestInvalidate();
}
#endif
void RichTextEditor::SetDefaultContextMenuLabels(const RichTextContextMenuLabels* labels)
{
    if (! labels)
    {
        _state->contextMenuLabels.reset();
        return;
    }
    for (const auto* text : {&labels->undo, &labels->redo, &labels->cut, &labels->copy, &labels->paste, &labels->selectAll, &labels->bold, &labels->italic})
        if (text->empty() || text->size() > 255u || ! RichText::IsValidUtf16(*text))
            throw std::invalid_argument("Invalid rich context menu label");
    auto next = std::make_shared<const RichTextContextMenuLabels>(*labels);
    _state->contextMenuLabels.swap(next);
}
void RichTextEditor::BuildDefaultContextMenuItems(std::vector<MenuFlyoutItem>& out) const
{
    std::vector<MenuFlyoutItem> items(0u);
    const auto labels = _state->contextMenuLabels;
    if (! labels)
    {
        out.swap(items);
        return;
    }
    const auto selection = GetSelection();
    items.reserve(8u);
    const auto add = [&](const std::wstring& label, int id, bool enabled)
    {
        items.emplace_back();
        auto& item     = items.back();
        item.text      = label;
        item.commandId = id;
        item.enabled   = enabled;
    };
    add(labels->undo, 1, Editable() && CanUndo());
    add(labels->redo, 2, Editable() && CanRedo());
    add(labels->cut, 3, Editable() && selection.Start() != selection.End());
    add(labels->copy, 4, IsEnabled() && selection.Start() != selection.End());
    add(labels->paste, 5, Editable());
    add(labels->selectAll, 6, IsEnabled() && Snapshot().Length() != 0u);
    add(labels->bold, 7, Editable());
    add(labels->italic, 8, Editable());
    out.swap(items);
}
bool RichTextEditor::InvokeDefaultContextMenuCommand(ControlHost& host, int commandId) noexcept
{
    if (GetHost() != &host || ! IsEnabled())
        return false;
    switch (commandId)
    {
        case 1: return OnKeyDown(host, 'Z', MK_CONTROL);
        case 2: return OnKeyDown(host, 'Y', MK_CONTROL);
        case 3: return OnKeyDown(host, 'X', MK_CONTROL);
        case 4: return OnCopy(host);
        case 5: return OnKeyDown(host, 'V', MK_CONTROL);
        case 6: return OnSelectAll(host);
        case 7: return OnKeyDown(host, 'B', MK_CONTROL);
        case 8: return OnKeyDown(host, 'I', MK_CONTROL);
        default: return false;
    }
}
void RichTextEditor::SetFocusModeEnabled(bool enabled) noexcept
{
    if (_state->focusMode == enabled)
        return;
    _state->focusMode = enabled;
    RequestInvalidate();
}
bool RichTextEditor::IsFocusModeEnabled() const noexcept
{
    return _state->focusMode;
}
void RichTextEditor::SetMarkdownAnalysisProfile(std::optional<RichText::MarkdownProfile> profile)
{
    if (profile && *profile != RichText::MarkdownProfile::CommonMark0312 && *profile != RichText::MarkdownProfile::Gfm029 &&
        *profile != RichText::MarkdownProfile::RichMarkdown)
        throw std::invalid_argument("Invalid Markdown analysis profile");
    if (_state->markdownProfile == profile)
        return;
    _state->autoFormatRun.reset();
    _state->smartTypingSite.reset();
    RetireSemanticAccessibility();
    _state->markdown.Clear();
    _state->images.Cancel();
    _state->sourceEditing.reset();
    _state->markdownCapacityRejected.reset();
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    _state->semantic.Clear();
    _state->semanticRejected.reset();
    _state->nativeGeometryRejected.reset();
    if (! profile)
        _state->markdownRendering = false;
    _state->markdownProfile = profile;
    RetireLayout();
    SelectionChanged();
}
std::optional<RichText::MarkdownProfile> RichTextEditor::GetMarkdownAnalysisProfile() const noexcept
{
    return _state->markdownProfile;
}
void RichTextEditor::SetAccessibilityLabels(std::shared_ptr<const RichText::AccessibilityLabels> labels) noexcept
{
    auto previous = _state->semantic.AccessibilityLabelSet();
    if (previous == labels)
        return;
    RetireSemanticAccessibility();
    ++_state->appearanceRevision;
    // Label changes rebuild semantic identity without changing the source.
    // Retain eligible jobs and staged pixels privately until new semantics
    // rebinds them; source edits still use the same occurrence guard.
    if (_state->imageSource)
        _state->images.SuspendForEdit(Snapshot(), *_state->imageSource);
    else
        _state->images.Cancel();
    _state->semantic.SetAccessibilityLabels(std::move(labels));
    _state->semanticRejected.reset();
    _state->nativeGeometryRejected.reset();
    RetireLayout(); // Marks preparation pending without calling the application.
    RequestInvalidate(); // Last owner access: scheduling can retire this control.
}
std::shared_ptr<const RichText::AccessibilityLabels> RichTextEditor::GetAccessibilityLabels() const noexcept
{
    return _state->semantic.AccessibilityLabelSet();
}
std::shared_ptr<const RichText::MarkdownSyntax> RichTextEditor::GetMarkdownSyntax() const noexcept
{
    const auto syntax       = _state->markdown.Published();
    const auto region       = syntax ? syntax->SourceRegion() : std::nullopt;
    const bool scopeMatches = _state->sourceEditing ? region && region->session == _state->sourceEditing->region.session : ! region;
    return syntax && scopeMatches && _state->markdownProfile && syntax->Revision() == Revision() && syntax->Profile() == *_state->markdownProfile ? syntax
                                                                                                                                                  : nullptr;
}
void RichTextEditor::SetMarkdownRenderingEnabled(bool enabled)
{
    if (enabled && ! _state->markdownProfile)
        throw std::invalid_argument("Semantic rendering requires a Markdown analysis profile");
    if (_state->markdownRendering == enabled)
        return;
    // Allocate only for the semantic opt-in, before changing any live state.
    // Retirement itself is one atomic increment and must survive allocation
    // failure or a delayed/failed accessibility snapshot publication.
    if (enabled && ! _state->accessibilityEpoch)
        _state->accessibilityEpoch = std::make_shared<RichText::Detail::AccessibilityEpoch>();
    RetireSemanticAccessibility();
    _state->autoFormatRun.reset();
    _state->smartTypingSite.reset();
    _state->markdownRendering = enabled;
    _state->images.Cancel();
    _state->sourceEditing.reset();
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    _state->semantic.Clear();
    _state->semanticRejected.reset();
    _state->nativeGeometryRejected.reset();
    RetireLayout();
    SelectionChanged();
}
bool RichTextEditor::IsMarkdownRenderingEnabled() const noexcept
{
    return _state->markdownRendering;
}
std::shared_ptr<const RichText::SemanticDocument> RichTextEditor::GetSemanticDocument() const noexcept
{
    const auto syntax   = GetMarkdownSyntax();
    const auto document = _state->semantic.Published();
    return _state->markdownRendering && syntax && document && document->Syntax() == syntax ? document : nullptr;
}
MarkdownRenderingStatus RichTextEditor::GetMarkdownRenderingStatus() const noexcept
{
    if (! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich)
        return MarkdownRenderingStatus::Disabled;
    if (_state->HasCurrentMarkdownCapacityRejection())
        return MarkdownRenderingStatus::SourceFallback;
    if (const auto* host = GetHost())
    {
        const auto viewport = TextViewport();
        if (_state->RejectsPreparation(
                std::max(1.0f, viewport.right - viewport.left), host->GetDpi(), IsRightToLeft(), host->GetTypographyRevision(), host->GetResourceGeneration()))
            return MarkdownRenderingStatus::SourceFallback;
        if (_state->RejectsNativeGeometry(GetSemanticDocument(),
                                          std::max(1.0f, viewport.right - viewport.left),
                                          host->GetDpi(),
                                          IsRightToLeft(),
                                          host->GetTypographyRevision(),
                                          host->GetResourceGeneration()))
            return MarkdownRenderingStatus::SourceFallback;
    }
    const auto syntax = GetMarkdownSyntax();
    if (syntax && _state->semanticRejected == syntax)
        return MarkdownRenderingStatus::SourceFallback;
    return GetSemanticDocument() && _state->layout.Ready() && _state->layout.Semantic() ? MarkdownRenderingStatus::Semantic : MarkdownRenderingStatus::Pending;
}
std::shared_ptr<const RichText::SemanticDocument> RichTextEditor::GetAcknowledgedSemanticDocument(const ControlHost& host) const noexcept
{
    const auto document  = GetSemanticDocument();
    const auto* geometry = _state->layout.SemanticGeometry();
    // Another retained editor may be preparing a preview in the same host. The
    // exact owner/source/width/typography/resource key acknowledges this editor;
    // unrelated work must not revoke its coherent semantic geometry/actions.
    return document && LayoutIsCurrent(host) && _state->mode == RichText::ViewMode::Rich && geometry && &geometry->Document() == document.get() ? document
                                                                                                                                                : nullptr;
}
std::shared_ptr<const RichText::Detail::AccessibilityEpoch> RichTextEditor::AccessibilitySemanticEpoch() const noexcept
{
    return _state->accessibilityEpoch;
}
void RichTextEditor::RetireSemanticAccessibility() const noexcept
{
    if (_state->accessibilityEpoch)
        _state->accessibilityEpoch->revision.fetch_add(1u, std::memory_order_release);
}
std::optional<D2D1_RECT_F> RichTextEditor::GetSemanticElementBounds(const ControlHost& host, size_t element) const noexcept
{
    const auto document = GetAcknowledgedSemanticDocument(host);
    if (! document || element >= document->Elements().size())
        return std::nullopt;
    auto bounds = _state->layout.SemanticGeometry()->ElementBounds(element);
    if (bounds)
    {
        const auto viewport = TextViewport();
        bounds->left        = std::max(viewport.left, bounds->left + viewport.left + HorizontalTranslation());
        bounds->right       = std::min(viewport.right, bounds->right + viewport.left + HorizontalTranslation());
        bounds->top         = std::max(viewport.top, bounds->top + viewport.top - _state->scrollDip);
        bounds->bottom      = std::min(viewport.bottom, bounds->bottom + viewport.top - _state->scrollDip);
        if (bounds->left >= bounds->right || bounds->top >= bounds->bottom)
            bounds.reset();
    }
    return bounds;
}
bool RichTextEditor::ToggleSemanticTask(ControlHost& host, const std::shared_ptr<const RichText::SemanticDocument>& expected, size_t element)
{
    using namespace RichText;
    if (! expected || GetAcknowledgedSemanticDocument(host) != expected || ! Editable() || element >= expected->Elements().size())
        return false;
    const auto& task = expected->Elements()[element];
    if (task.role != SemanticRole::Task || task.taskMarker.last - task.taskMarker.first != 3u)
        return false;
    if (host.GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host.TryReadNativeTextInputState(this, input);
        if ((host.HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    const wchar_t value = task.task == MarkdownTask::Checked ? L' ' : L'x';
    const SourceEdit edit{{task.taskMarker.first + 1u, task.taskMarker.first + 2u}, std::wstring_view(&value, 1u)};
    return ApplySourceEdits(Snapshot(), Revision(), std::span(&edit, 1u), GetSelection());
}
bool RichTextEditor::SelectMarkdownDiagnostic(ControlHost& host, bool backwards)
{
    if (GetHost() != &host || ! IsEnabled() || ! IsVisible() || _state->sourceEditing || ! LayoutIsCurrent(host))
        return false;
    if (host.GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host.TryReadNativeTextInputState(this, input);
        if ((host.HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    const auto syntax = GetMarkdownSyntax();
    if (! syntax || syntax->SourceRegion())
        return false;
    const auto semantic  = GetAcknowledgedSemanticDocument(host);
    const auto selection = GetSelection();
    const auto source    = Snapshot();
    std::optional<RichText::MarkdownRange> nearest, wrapped;
    const auto consider = [&](RichText::MarkdownRange range)
    {
        if (range.first > range.last || range.last > source.Length() || ! source.IsBoundary(range.first) || ! source.IsBoundary(range.last))
            return;
        const auto key      = std::pair{range.first, range.last};
        const auto selected = std::pair{selection.Start(), selection.End()};
        const auto earlier  = [](auto left, auto right) { return std::pair{left.first, left.last} < std::pair{right.first, right.last}; };
        if (! wrapped || (backwards ? earlier(*wrapped, range) : earlier(range, *wrapped)))
            wrapped = range;
        if ((backwards ? key < selected : key > selected) && (! nearest || (backwards ? earlier(*nearest, range) : earlier(range, *nearest))))
            nearest = range;
    };
    for (const auto& diagnostic : syntax->Diagnostics())
        consider(diagnostic.source);
    if (semantic && semantic->Syntax() == syntax)
        for (const auto& diagnostic : semantic->Diagnostics())
            consider(diagnostic.source);
    const auto target = nearest ? nearest : wrapped;
    if (! target)
        return false;
    const auto lifetime = GetLifetimeToken();
    const auto revision = Revision();
    const auto binding  = _state->bindingRevision;
    if (! ScrollSourceRangeIntoView(host, target->first, target->last, true))
        return false;
    if (lifetime.expired() || GetHost() != &host || Revision() != revision || _state->bindingRevision != binding || GetMarkdownSyntax() != syntax ||
        GetSelection() != selection || ! Snapshot().IsSameRevision(source))
        return false;
    if (host.GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host.TryReadNativeTextInputState(this, input);
        if ((host.HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    if (lifetime.expired() || GetHost() != &host || Revision() != revision || _state->bindingRevision != binding || GetMarkdownSyntax() != syntax ||
        GetSelection() != selection || ! Snapshot().IsSameRevision(source) || (semantic && GetAcknowledgedSemanticDocument(host) != semantic))
        return false;
    SetSelection({target->first, target->last});
    return true;
}
bool RichTextEditor::NavigateSemanticElement(ControlHost& host, const std::shared_ptr<const RichText::SemanticDocument>& expected, size_t element)
{
    const auto document = expected; // The callback may reset the caller's shared_ptr.
    if (! document || element >= document->Elements().size() || GetHost() != &host || ! IsEnabled() || ! IsVisible() ||
        GetAcknowledgedSemanticDocument(host) != document)
        return false;
    const auto lifetime = GetLifetimeToken();
    auto target         = document->NavigationTarget(element);
    if (host.GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host.TryReadNativeTextInputState(this, input);
        if ((host.HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    if (lifetime.expired() || GetHost() != &host || GetAcknowledgedSemanticDocument(host) != document)
        return false;
    const auto& link = document->Elements()[element];
    if (! target && link.role == RichText::SemanticRole::Link)
    {
        if (link.destination.starts_with(L'#'))
            target = HeadingFragment(*document, link.destination);
        else
        {
            const auto callback = _state->onLinkActivated;
            if (! callback)
                return false;
            const RichTextLinkActivation activation{link.source, link.destination, link.title};
            try
            {
                (*callback)(activation);
            }
            catch (const std::bad_alloc&)
            {
                std::terminate(); // Same application-notification contract as text and selection callbacks.
            }
            catch (const std::exception&)
            {
                Debug::Warning(L"DxUi::RichTextEditor: link notification failed; source retained.");
            }
            return true; // A dispatched intent remains handled if the callback retires its owner.
        }
    }
    if (! target)
        return false;
    if (! ScrollSourceRangeIntoView(host, target->first, target->last, true))
        return false;
    if (lifetime.expired() || GetHost() != &host || GetAcknowledgedSemanticDocument(host) != document)
        return false;
    // Selection synchronization can re-enter or destroy the owner. Complete
    // scrolling first and never access this control after SetSelection returns.
    SetSelection({target->first, target->last});
    return true;
}
void RichTextEditor::SetOnLinkActivated(std::function<void(const RichTextLinkActivation&)> callback)
{
    auto next                      = callback ? std::make_shared<const std::function<void(const RichTextLinkActivation&)>>(std::move(callback)) : nullptr;
    const bool availabilityChanged = bool(next) != bool(_state->onLinkActivated);
    _state->onLinkActivated.swap(next);
    if (availabilityChanged)
    {
        RetireSemanticAccessibility();
        RequestInvalidate(); // A scheduling callback may retire this owner.
    }
}
bool RichTextEditor::HasLinkActivationHandler() const noexcept
{
    return bool(_state->onLinkActivated);
}
bool RichTextEditor::CanChangeSemanticSourcePresentation(ControlHost& host) const
{
    if (GetHost() != &host || ! IsEnabled() || ! IsVisible() || ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich)
        return false;
    if (host.GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host.TryReadNativeTextInputState(this, input);
        if ((host.HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    return true;
}
bool RichTextEditor::BeginSemanticSourceEdit(ControlHost& host, const std::shared_ptr<const RichText::SemanticDocument>& expected, size_t element)
{
    if (! expected || GetAcknowledgedSemanticDocument(host) != expected || ! CanChangeSemanticSourcePresentation(host))
        return false;
    const auto source = expected->SourceEditingRange(element);
    if (! source || _state->sourceEditing)
        return false;
    const bool inlineMath = expected->Elements()[element].role == RichText::SemanticRole::Math;
    const auto node       = expected->SourceEditingNode(element);
    const auto caret      = inlineMath ? source->first : expected->Syntax()->Nodes()[*node].block.content.first;
    auto session          = _state->nextSourceEditingSession++;
    if (! session)
        session = _state->nextSourceEditingSession++;
    _state->sourceEditing.emplace(State::SourceEditingSession{
        {*source, session, inlineMath ? RichText::MarkdownSourceRegionKind::Inline : RichText::MarkdownSourceRegionKind::Block}, Snapshot()});
    _state->document.SetSelection({caret, caret});
    SourcePresentationChanged(host);
    return true;
}
bool RichTextEditor::EndSemanticSourceEdit(ControlHost& host)
{
    if (! _state->sourceEditing || ! CanChangeSemanticSourcePresentation(host))
        return false;
    _state->sourceEditing.reset();
    SourcePresentationChanged(host);
    return true;
}
void RichTextEditor::SourcePresentationChanged(ControlHost& host)
{
    _state->autoFormatRun.reset();
    _state->smartTypingSite.reset();
    _state->images.Cancel();
    RetireSemanticAccessibility();
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    _state->dragging        = false;
    _state->markdownCapacityRejected.reset();
    RetireLayout();
    const auto lifetime = GetLifetimeToken();
    const auto binding = _state->bindingRevision, revision = Revision();
    const auto region = GetSemanticSourceRegion();
    // A presentation transition cancels its old gesture. Native ReleaseCapture
    // can re-enter the owner, so finish mutation before crossing that boundary.
    if (host.GetCapturedControl() == this)
        host.ReleaseMouseCapture();
    if (lifetime.expired() || GetHost() != &host || _state->bindingRevision != binding || Revision() != revision || GetSemanticSourceRegion() != region)
        return;
    SelectionChanged();
}
std::optional<RichText::MarkdownSourceRegion> RichTextEditor::GetSemanticSourceRegion() const noexcept
{
    return _state->sourceEditing ? std::optional(_state->sourceEditing->region) : std::nullopt;
}
void RichTextEditor::UpdateSemanticSourceRegion() noexcept
{
    if (! _state->sourceEditing)
        return;
    auto& editing      = *_state->sourceEditing;
    const auto current = Snapshot();
    const auto change  = current.ChangedRangeFrom(editing.snapshot);
    if (change.kind == RichText::SourceChangeKind::Unchanged)
        return;
    // Local recorded edits, previews, cancellation, undo and redo share the
    // document's bounded scalar/CRLF-safe change metadata. A foreign/outside
    // replacement retires presentation rather than guessing another owner.
    auto& range = editing.region.source;
    if (change.begin < range.first || change.previousEnd > range.last)
    {
        _state->sourceEditing.reset();
        return;
    }
    range.last       = range.last - (change.previousEnd - change.begin) + (change.currentEnd - change.begin);
    editing.snapshot = current;
}
void RichTextEditor::SetReadOnly(bool readOnly) noexcept
{
    _state->autoFormatRun.reset();
    _state->smartTypingSite.reset();
    _state->readOnly = readOnly;
    SelectionChanged();
}
bool RichTextEditor::IsReadOnly() const noexcept
{
    return _state->readOnly;
}
bool RichTextEditor::Editable() const noexcept
{
    return IsEnabled() && ! _state->readOnly;
}
bool RichTextEditor::ReplaceSelection(std::wstring text)
{
    if (! Editable())
        return false;
    const bool changed = _state->document.ReplaceSelection(std::move(text));
    Changed(changed);
    return changed;
}
bool RichTextEditor::ToggleFormatting(std::wstring_view marker)
{
    if (! Editable())
        return false;
    const bool changed = RichText::ToggleMarker(_state->document, marker);
    Changed(changed);
    return changed;
}
bool RichTextEditor::ApplySourceEdits(const RichText::SourceSnapshot& expectedSource,
                                      uint64_t expectedRevision,
                                      std::span<const RichText::SourceEdit> edits,
                                      RichText::Selection after)
{
    if (! Editable())
        return false;
    const bool changed      = RichText::ApplySourceEdits(_state->document, expectedSource, expectedRevision, edits, after);
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    Changed(changed);
    return changed;
}
void RichTextEditor::SetSearchQuery(std::wstring_view query, RichText::SourceSearchOptions options)
{
    if (query.size() > RichText::Document::MaximumUnits || ! RichText::IsValidSourceUtf16(query))
        throw std::invalid_argument("Invalid search query");
    std::wstring staged(query);
    _state->searchQuery.swap(staged);
    _state->searchOptions = options;
}
void RichTextEditor::SetSearchShortcutsEnabled(bool enabled) noexcept
{
    _state->searchShortcutsEnabled = enabled;
}
void RichTextEditor::SetOnFindRequested(std::function<void()> callback)
{
    auto next = callback ? std::make_shared<const std::function<void()>>(std::move(callback)) : nullptr;
    _state->onFindRequested.swap(next);
}
void RichTextEditor::SetMarkdownShortcutsEnabled(bool enabled) noexcept
{
    _state->markdownShortcutsEnabled = enabled;
}
void RichTextEditor::SetAutoPairEnabled(bool enabled) noexcept
{
    _state->autoFormatRun.reset();
    _state->smartTypingSite.reset();
    _state->autoPairEnabled = enabled;
}
bool RichTextEditor::IsAutoPairEnabled() const noexcept
{
    return _state->autoPairEnabled;
}
void RichTextEditor::SetSmartTypographyEnabled(bool enabled) noexcept
{
    _state->smartTypingSite.reset();
    _state->smartTypographyEnabled = enabled;
}
bool RichTextEditor::IsSmartTypographyEnabled() const noexcept
{
    return _state->smartTypographyEnabled;
}
void RichTextEditor::SetMarkdownAutoFormatEnabled(bool enabled) noexcept
{
    _state->markdownAutoFormatEnabled = enabled;
    _state->autoFormatRun.reset();
}
bool RichTextEditor::IsMarkdownAutoFormatEnabled() const noexcept
{
    return _state->markdownAutoFormatEnabled;
}
void RichTextEditor::SetOnLinkEditRequested(std::function<void()> callback)
{
    auto next = callback ? std::make_shared<const std::function<void()>>(std::move(callback)) : nullptr;
    _state->onLinkEditRequested.swap(next);
}
bool RichTextEditor::SelectSourceMatch(std::wstring_view query, bool backwards, RichText::SourceSearchOptions options)
{
    if (! IsEnabled())
        return false;
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host->TryReadNativeTextInputState(this, input);
        if ((host->HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    const auto matches = RichText::FindInSource(Snapshot(), query, options);
    if (matches.empty())
        return false;
    const auto selection = GetSelection();
    auto index           = backwards ? matches.size() - 1u : 0u;
    if (backwards)
    {
        for (size_t i = matches.size(); i > 0u; --i)
            if (matches[i - 1u].last <= selection.Start())
            {
                index = i - 1u;
                break;
            }
    }
    else
        for (size_t i = 0u; i < matches.size(); ++i)
            if (matches[i].first >= selection.End())
            {
                index = i;
                break;
            }
    SetSelection({matches[index].first, matches[index].last});
    return true;
}
size_t RichTextEditor::ReplaceAllSourceMatches(std::wstring_view query, std::wstring_view replacement, RichText::SourceSearchOptions options)
{
    if (! Editable())
        return 0u;
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host->TryReadNativeTextInputState(this, input);
        if ((host->HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return 0u;
    }
    if (replacement.size() > RichText::Document::MaximumUnits)
        throw std::length_error("Search replacement exceeds source capacity");
    const auto source   = Snapshot();
    const auto revision = Revision();
    const auto matches  = RichText::FindInSource(source, query, options);
    if (matches.empty())
        return 0u;
    std::vector<RichText::SourceEdit> edits(0u);
    edits.reserve(matches.size());
    for (const auto range : matches)
        edits.push_back({range, replacement});
    const auto end = matches.front().first + replacement.size();
    return ApplySourceEdits(source, revision, edits, {end, end}) ? matches.size() : 0u;
}
bool RichTextEditor::ReplaceSelectedSourceMatch(std::wstring_view query, std::wstring_view replacement, RichText::SourceSearchOptions options)
{
    if (! Editable())
        return false;
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        const auto composition = host->GetNativeTextCompositionActive(this);
        if ((host->HasActiveNativeTextInputSession() && ! composition) || composition.value_or(false))
            return false;
    }
    if (replacement.size() > RichText::Document::MaximumUnits)
        throw std::length_error("Search replacement exceeds source capacity");
    const auto source    = Snapshot();
    const auto revision  = Revision();
    const auto selection = GetSelection();
    const auto matches   = RichText::FindInSource(source, query, options);
    const auto found     = std::ranges::find(matches, RichText::MarkdownRange{selection.Start(), selection.End()});
    if (found == matches.end())
        return false;
    const RichText::SourceEdit edit{*found, replacement};
    const auto end = found->first + replacement.size();
    return ApplySourceEdits(source, revision, std::span(&edit, 1u), {end, end});
}
RichText::MarkdownCommandStatus RichTextEditor::ApplyMarkdownCommand(const RichText::MarkdownCommandPlan& plan)
{
    using namespace RichText;
    if (! Editable() || ! _state->markdownRendering || _state->mode != ViewMode::Rich || _state->sourceEditing)
        return MarkdownCommandStatus::Unavailable;
    const auto syntax = GetMarkdownSyntax();
    if (! syntax)
        return MarkdownCommandStatus::Unavailable;
    if (syntax != plan.Syntax())
        throw std::invalid_argument("Stale Markdown command syntax publication");
    if (MarkdownCommandInputBlocked(GetHost(), this))
        return MarkdownCommandStatus::Unavailable;
    auto site = MarkdownNoNode;
    if (plan.AfterCell() != MarkdownNoNode)
    {
        auto* host           = GetHost();
        const auto* geometry = _state->layout.SemanticGeometry();
        if (! host || ! LayoutIsCurrent(*host) || ! geometry)
            return MarkdownCommandStatus::Unavailable;
        site = geometry->SourceSiteForSyntaxNode(plan.AfterCell());
    }
    const bool changed = RichText::ApplyMarkdownCommand(_state->document, plan);
    if (! plan.IsApplicable())
        return MarkdownCommandStatus::Unavailable;
    _state->sourceCaretSite = changed ? MarkdownNoNode : site;
    Changed(changed);
    // Changed may have retired this owner or undone the transaction. No member
    // access follows it; the result describes this committed attempt.
    return changed ? MarkdownCommandStatus::Changed : MarkdownCommandStatus::Unchanged;
}
RichText::MarkdownCommandStatus RichTextEditor::ExecuteMarkdownCommand(const RichText::MarkdownCommandRequest& request)
{
    using namespace RichText;
    if (! Editable() || ! _state->markdownRendering || _state->mode != ViewMode::Rich || _state->sourceEditing)
        return MarkdownCommandStatus::Unavailable;
    const auto syntax = GetMarkdownSyntax();
    if (! syntax)
        return MarkdownCommandStatus::Unavailable;
    auto resolved = request;
    std::wstring imageDestination(0u, L' ');
    if (request.command == MarkdownCommand::InsertImage)
        if (const auto reference = RichText::Detail::BuildMarkdownFileReference(_state->documentFilePath, request.destination))
        {
            const auto marker = reference->rfind(L"](");
            if (marker == reference->npos || reference->empty() || reference->back() != L')')
                throw std::logic_error("Invalid generated file reference");
            imageDestination.assign(*reference, marker + 2u, reference->size() - marker - 3u);
            resolved.destination = imageDestination;
        }
    if (resolved.targetCell == MarkdownNoNode && request.command >= MarkdownCommand::InsertTableRow && request.command <= MarkdownCommand::PreviousTableCell)
        if (auto* host = GetHost(); host && LayoutIsCurrent(*host))
            if (const auto* geometry = _state->layout.SemanticGeometry())
            {
                const auto node = geometry->SourceSyntaxNode(GetSelection().caret, _state->sourceCaretSite);
                if (node < syntax->Nodes().size() && syntax->Nodes()[node].block.kind == MarkdownBlockKind::TableCell)
                    resolved.targetCell = node;
            }
    const auto plan = BuildMarkdownCommand(Snapshot(), Revision(), syntax, GetSelection(), resolved);
    return ApplyMarkdownCommand(plan);
}
std::optional<RichText::MarkdownCommandState> RichTextEditor::QueryMarkdownCommandState(const RichText::MarkdownCommandRequest& request) const noexcept
{
    using namespace RichText;
    _state->lastListStateAncestorSteps = 0u;
    if (static_cast<unsigned>(request.command) > static_cast<unsigned>(MarkdownCommand::InsertEscapedText))
        return std::nullopt;
    MarkdownCommandState result;
    if (! Editable() || ! _state->markdownRendering || _state->mode != ViewMode::Rich || _state->sourceEditing)
    {
        result.available = false;
        return result;
    }
    const auto document = GetSemanticDocument();
    if (! document)
        return std::nullopt;
    const auto selection    = GetSelection();
    const bool inputBlocked = MarkdownCommandInputBlocked(GetHost(), this);
    if (request.command == MarkdownCommand::JoinBackward || request.command == MarkdownCommand::JoinForward)
    {
        bool simpleApplicable = false;
        const auto leaf       = FindJoinLeaf(*document->Syntax(), selection, request.command == MarkdownCommand::JoinBackward, &simpleApplicable);
        if (! leaf)
            result.available = false;
        else if (simpleApplicable)
            result.available = ! inputBlocked;
    }
    if (request.command == MarkdownCommand::ExitCodeBlock || request.command == MarkdownCommand::SetCodeLanguage ||
        request.command == MarkdownCommand::InsertMath || request.command == MarkdownCommand::InsertThematicBreak ||
        request.command == MarkdownCommand::InsertEscapedText)
    {
        const bool applicable =
            request.command == MarkdownCommand::ExitCodeBlock       ? FindExitCodeBlock(*document->Syntax(), selection).has_value()
            : request.command == MarkdownCommand::SetCodeLanguage   ? FindCodeLanguageTarget(*document->Syntax(), selection, request.text).has_value()
            : request.command == MarkdownCommand::InsertMath        ? CanInsertMath(*document->Syntax(), selection, request.text, request.mathDisplay)
            : request.command == MarkdownCommand::InsertEscapedText ? CanInsertEscapedText(*document->Syntax(), selection, request.text)
                                                                    : CanInsertThematicBreak(*document->Syntax(), selection);
        result.available = ! inputBlocked && applicable;
    }
    auto first = selection.Start(), last = selection.End();
    if (first == last && first != 0u)
        --first;
    if (first == last)
        ++last;
    unsigned flag = 0u;
    if (request.command == MarkdownCommand::ToggleStyle)
        switch (request.style)
        {
            case MarkdownTextStyle::Strong: flag = Bold; break;
            case MarkdownTextStyle::Emphasis: flag = Italic; break;
            case MarkdownTextStyle::Strikethrough: flag = Strike; break;
            case MarkdownTextStyle::Code: flag = Code; break;
            default: return std::nullopt;
        }
    bool seen = false, yes = false, no = false;
    unsigned heading  = 0u;
    bool mixedHeading = false;
    for (const auto& run : document->SourceRuns())
    {
        if (run.start >= last)
            break;
        if (run.start + run.length <= first)
            continue;
        if (! seen)
            heading = run.heading;
        else if (heading != run.heading)
            mixedHeading = true;
        seen = true;
        if (flag && (run.flags & flag))
            yes = true;
        else if (flag)
            no = true;
    }
    result.headingLevel = mixedHeading ? 0u : heading;
    result.listKind = UniformListKind(*document->Syntax(), first, last, _state->lastListStateAncestorSteps);
    if (request.command == MarkdownCommand::SetHeading)
    {
        result.active = seen && ! mixedHeading && heading == request.headingLevel;
        result.mixed  = mixedHeading;
    }
    else if (flag)
    {
        result.active = yes && ! no;
        result.mixed  = yes && no;
    }
    else if (request.command == MarkdownCommand::SetList)
    {
        result.active = result.listKind && result.listKind.value() == request.listKind;
        result.mixed  = ! result.listKind;
    }
    if (inputBlocked || ((request.command == MarkdownCommand::InsertParagraph || request.command == MarkdownCommand::InsertHardBreak) &&
                         ! IsSingleLineCommandSelection(*document->Syntax(), selection)) ||
        ((request.command != MarkdownCommand::SetList || ! request.listStart || request.listStart.value() <= 999999999u) &&
                         ! IsCommandFeatureEnabled(*document->Syntax(), request.command, request.style, request.listKind)))
        result.available = false;
    return result;
}
bool RichTextEditor::Undo()
{
    if (! Editable() || ! _state->document.Undo())
        return false;
    Changed(true);
    return true;
}
bool RichTextEditor::Redo()
{
    if (! Editable() || ! _state->document.Redo())
        return false;
    Changed(true);
    return true;
}
bool RichTextEditor::CanUndo() const noexcept
{
    return _state->document.CanUndo();
}
bool RichTextEditor::CanRedo() const noexcept
{
    return _state->document.CanRedo();
}
bool RichText::TokenizeBuiltinCode(std::wstring_view language, std::wstring_view text, std::vector<CodeToken>& tokens)
{
    return Detail::TokenizeCode(language, text, tokens);
}
void RichTextEditor::SetCodeTokenizer(RichText::CodeTokenizer tokenizer)
{
    auto next = tokenizer ? std::make_shared<const RichText::CodeTokenizer>(std::move(tokenizer)) : nullptr;
    _state->codeTokenizer.swap(next);
    _state->codeInkDocument.reset();
    _state->codeInk.clear();
    _state->codeInkStaged.clear();
    _state->codeInkCursor   = 0u;
    _state->codeInkComplete = false;
    RequestContentPreparation();
}
void RichTextEditor::SetHtmlPasteConverter(std::function<std::optional<std::wstring>(std::wstring_view)> converter)
{
    auto next = converter ? std::make_shared<const std::function<std::optional<std::wstring>(std::wstring_view)>>(std::move(converter)) : nullptr;
    _state->htmlPasteConverter.swap(next);
}
bool RichTextEditor::HasHtmlPasteConverter() const noexcept
{
    return bool(_state->htmlPasteConverter);
}
void RichTextEditor::SetOnCodeCopyRequested(std::function<void(std::wstring_view)> callback, std::wstring accessibleName)
{
    if (callback && (accessibleName.empty() || accessibleName.size() > 1024u || ! RichText::IsValidUtf16(accessibleName)))
        throw std::invalid_argument("Invalid code copy accessible name");
    auto next                    = callback ? std::make_shared<const std::function<void(std::wstring_view)>>(std::move(callback)) : nullptr;
    const bool visibilityChanged = bool(next) != bool(_state->onCodeCopyRequested);
    const bool labelChanged      = _state->codeCopyAccessibleName != accessibleName;
    _state->onCodeCopyRequested.swap(next);
    _state->codeCopyAccessibleName.swap(accessibleName);
    _state->codePressDocument.reset();
    if (visibilityChanged || labelChanged)
    {
        RetireSemanticAccessibility();
        if (visibilityChanged)
            RetireLayout();
        RequestInvalidate(); // Scheduling may retire the owner; no member access follows.
    }
}
std::wstring_view RichTextEditor::GetCodeCopyAccessibleName() const noexcept
{
    return _state->onCodeCopyRequested ? std::wstring_view(_state->codeCopyAccessibleName) : std::wstring_view{};
}
std::optional<D2D1_RECT_F> RichTextEditor::GetSemanticCodeCopyBounds(const ControlHost& host, size_t element) const noexcept
{
    const auto document = GetAcknowledgedSemanticDocument(host);
    if (! document || ! _state->onCodeCopyRequested || _state->appearance.markdownPresentation != RichText::MarkdownPresentationStyle::Subtle ||
        document->Syntax()->SourceRegion() || element >= document->Elements().size())
        return std::nullopt;
    const auto& value = document->Elements()[element];
    const auto& nodes = document->Syntax()->Nodes();
    if (value.role != RichText::SemanticRole::Code || value.syntaxNode >= nodes.size() ||
        (nodes[value.syntaxNode].block.kind != RichText::MarkdownBlockKind::FencedCode &&
         nodes[value.syntaxNode].block.kind != RichText::MarkdownBlockKind::IndentedCode))
        return std::nullopt;
    const auto frame = _state->layout.SemanticGeometry()->ElementBounds(element);
    auto bounds      = frame ? RichText::Detail::CodeCopyBounds(*frame, IsRightToLeft()) : std::nullopt;
    if (! bounds)
        return std::nullopt;
    const auto viewport = TextViewport();
    bounds->left += viewport.left + HorizontalTranslation();
    bounds->right += viewport.left + HorizontalTranslation();
    bounds->top += viewport.top - _state->scrollDip;
    bounds->bottom += viewport.top - _state->scrollDip;
    return bounds;
}
HRESULT RichTextEditor::InvokeSemanticCodeCopy(ControlHost& host, const std::shared_ptr<const RichText::SemanticDocument>& document, size_t element) noexcept
{
    try
    {
        if (! IsEnabled() || ! IsVisible() || ! _state->onCodeCopyRequested ||
            _state->appearance.markdownPresentation != RichText::MarkdownPresentationStyle::Subtle || ! document ||
            GetAcknowledgedSemanticDocument(host) != document || document->Syntax()->SourceRegion() || element >= document->Elements().size())
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& value = document->Elements()[element];
        const auto& nodes = document->Syntax()->Nodes();
        if (value.role != RichText::SemanticRole::Code || value.syntaxNode >= nodes.size() ||
            (nodes[value.syntaxNode].block.kind != RichText::MarkdownBlockKind::FencedCode &&
             nodes[value.syntaxNode].block.kind != RichText::MarkdownBlockKind::IndentedCode))
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto callback = _state->onCodeCopyRequested;
        (*callback)(CodeCopyText(*document, value));
        return S_OK; // The callback may retire this owner.
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        return E_FAIL; // Callback failure does not edit source.
    }
}
void RichTextEditor::SetOnTextChanged(std::function<void()> callback)
{
    _state->onTextChanged = callback ? std::make_shared<const std::function<void()>>(std::move(callback)) : nullptr;
}
void RichTextEditor::SetOnSelectionChanged(std::function<void()> callback)
{
    auto next                          = callback ? std::make_shared<const std::function<void()>>(std::move(callback)) : nullptr;
    _state->publishedSelection         = GetSelection();
    _state->publishedSelectionGesture  = _state->dragging;
    _state->publishedSelectionRevision = Revision();
    _state->onSelectionChanged.swap(next);
}
std::optional<RichTextScrollInfo> RichTextEditor::GetScrollInfo() const noexcept
{
    const auto* host = GetHost();
    if (! host || ! IsVisible() || ! LayoutIsCurrent(*host))
        return std::nullopt;
    const auto viewport = TextViewport();
    return RichTextScrollInfo{_state->scrollDip, std::max(0.0f, viewport.bottom - viewport.top), _state->layout.Height()};
}
void RichTextEditor::SetOnScrollChanged(std::function<void(float)> callback)
{
    auto next                  = callback ? std::make_shared<const std::function<void(float)>>(std::move(callback)) : nullptr;
    _state->publishedScrollDip = _state->scrollDip;
    _state->onScrollChanged.swap(next);
}
void RichTextEditor::PublishScrollChanged() noexcept
{
    const float offset = _state->scrollDip;
    if (_state->publishedScrollDip == offset)
        return;
    _state->publishedScrollDip = offset;
    const auto callback        = _state->onScrollChanged;
    if (! callback)
        return;
    try
    {
        (*callback)(offset);
    }
    catch (const std::bad_alloc&)
    {
        // As with committed text/selection observers, allocation failure is fatal.
        std::terminate();
    }
    catch (const std::exception&)
    {
        Debug::Warning(L"DxUi::RichTextEditor: scroll notification failed; committed offset retained.");
    }
    // The observer may detach or destroy this editor. No owner reads follow.
}
bool RichTextEditor::SetScrollOffsetDip(float offsetDip)
{
    auto* host      = GetHost();
    const auto info = GetScrollInfo();
    if (! info || ! IsEnabled() || ! std::isfinite(offsetDip))
        return false;
    const float offset  = std::clamp(offsetDip, 0.0f, std::max(0.0f, info->contentHeightDip - info->viewportHeightDip));
    _state->revealCaret = false;
    if (_state->scrollDip == offset)
        return true;
    _state->scrollDip = offset;
    _state->hoveredLink.reset();
    ++_state->fileDropEpoch;
    const auto lifetime = GetLifetimeToken();
    const auto binding  = _state->bindingRevision;
    host->NotifyTextInputLayoutChanged(this);
    if (lifetime.expired() || GetHost() != host || _state->bindingRevision != binding)
        return true;
    host->RefreshAccessibilitySnapshot();
    if (lifetime.expired() || GetHost() != host || _state->bindingRevision != binding)
        return true;
    RequestInvalidate();
    if (! lifetime.expired() && GetHost() == host && _state->bindingRevision == binding)
        PublishScrollChanged();
    return true;
}
std::optional<RichTextHorizontalScrollInfo> RichTextEditor::GetHorizontalScrollInfo() const noexcept
{
    const auto* host = GetHost();
    if (! host || ! LayoutIsCurrent(*host))
        return std::nullopt;
    const auto viewport = TextViewport();
    const float width   = std::max(0.0f, viewport.right - viewport.left);
    return RichTextHorizontalScrollInfo{_state->horizontalScrollDip, width, std::max(width, _state->layout.Width())};
}
void RichTextEditor::SetOnHorizontalScrollChanged(std::function<void(float)> callback)
{
    auto next                            = callback ? std::make_shared<const std::function<void(float)>>(std::move(callback)) : nullptr;
    _state->publishedHorizontalScrollDip = _state->horizontalScrollDip;
    _state->onHorizontalScrollChanged.swap(next);
}
void RichTextEditor::PublishHorizontalScrollChanged() noexcept
{
    const auto offset = _state->horizontalScrollDip;
    if (_state->publishedHorizontalScrollDip == offset)
        return;
    _state->publishedHorizontalScrollDip = offset;
    const auto callback                  = _state->onHorizontalScrollChanged;
    if (! callback)
        return;
    try
    {
        (*callback)(offset);
    }
    catch (const std::bad_alloc&)
    {
        std::terminate();
    }
    catch (const std::exception&)
    {
        Debug::Warning(L"DxUi::RichTextEditor: horizontal scroll notification failed; committed offset retained.");
    }
}
float RichTextEditor::SourceGutterWidth() const noexcept
{
    const auto& appearance = _state->appearance;
    auto size              = appearance.bodyFontSizeDip > 0.0f ? appearance.bodyFontSizeDip : 18.0f;
    if (appearance.sourceMonospace && appearance.codeFontSizeDip > 0.0f)
        size = appearance.codeFontSizeDip;
    size            = std::clamp(size * appearance.zoom, 1.0f, 512.0f);
    auto remaining  = Snapshot().Length() + 1u;
    unsigned digits = 1u;
    while (remaining >= 10u)
    {
        remaining /= 10u;
        ++digits;
    }
    return std::max(44.0f, static_cast<float>(digits) * size * 0.75f + 12.0f);
}
float RichTextEditor::HorizontalTranslation() const noexcept
{
    return _state->mode == RichText::ViewMode::Source && IsRightToLeft() ? _state->horizontalScrollDip : -_state->horizontalScrollDip;
}
bool RichTextEditor::SetHorizontalScrollOffsetDip(float offsetDip)
{
    auto* host      = GetHost();
    const auto info = GetHorizontalScrollInfo();
    if (! host || ! info || ! IsEnabled() || ! std::isfinite(offsetDip))
        return false;
    const auto offset   = std::clamp(offsetDip, 0.0f, std::max(0.0f, info->contentWidthDip - info->viewportWidthDip));
    _state->revealCaret = false;
    if (_state->horizontalScrollDip == offset)
        return true;
    _state->horizontalScrollDip = offset;
    _state->hoveredLink.reset();
    ++_state->fileDropEpoch;
    const auto lifetime = GetLifetimeToken();
    const auto binding  = _state->bindingRevision;
    const auto revision = Revision();
    host->NotifyTextInputLayoutChanged(this);
    if (lifetime.expired() || GetHost() != host || _state->bindingRevision != binding || Revision() != revision)
        return true;
    host->RefreshAccessibilitySnapshot();
    if (lifetime.expired() || GetHost() != host || _state->bindingRevision != binding || Revision() != revision)
        return true;
    RequestInvalidate();
    if (! lifetime.expired() && GetHost() == host && _state->bindingRevision == binding && Revision() == revision)
        PublishHorizontalScrollChanged();
    return true;
}
bool RichTextEditor::IsSelectionGestureActive() const noexcept
{
    return _state->dragging;
}
void RichTextEditor::PublishSelectionChanged() noexcept
{
    const auto selection = GetSelection();
    const bool gesture   = _state->dragging;
    const auto revision  = Revision();
    if (_state->publishedSelection == selection && _state->publishedSelectionGesture == gesture && _state->publishedSelectionRevision == revision)
        return;
    // Publish before invoking user code: nested selection changes must see their
    // actual predecessor, and self-replacement cannot invalidate this delegate.
    _state->publishedSelection         = selection;
    _state->publishedSelectionGesture  = gesture;
    _state->publishedSelectionRevision = revision;
    const auto callback                = _state->onSelectionChanged;
    if (! callback)
        return;
    try
    {
        (*callback)();
    }
    catch (const std::bad_alloc&)
    {
        // Allocation failure is fatal at the native notification boundary.
        std::terminate();
    }
    catch (const std::exception&)
    {
        // Selection is already committed; a failed notification cannot undo it.
        Debug::Warning(L"DxUi::RichTextEditor: selection notification failed; committed selection retained.");
    }
    // The callback may have destroyed or detached this control. No owner reads.
}
void RichTextEditor::Changed(bool sourceChanged, bool synchronizeTextInput)
{
    if (sourceChanged)
        _state->sourceInk.Clear();
    if (sourceChanged)
        _state->highlights.clear();
    if (sourceChanged)
        _state->decorations.clear();
    if (sourceChanged)
        _state->decorationStatus = RichTextDecorationStatus::None;
    if (sourceChanged)
    {
        if (_state->imageSource)
            _state->images.SuspendForEdit(Snapshot(), *_state->imageSource);
        else
            _state->images.Cancel();
        _state->hoveredLink.reset();
        _state->sourceGuides.Clear();
        _state->codeInkDocument.reset();
        _state->codeInk.clear();
        _state->codeInkStaged.clear();
        _state->codeInkComplete = false;
        UpdateSemanticSourceRegion();
        RetireSemanticAccessibility();
        _state->taskPressPlan.reset();
        _state->codePressDocument.reset();
        _state->sourceCaretSite = RichText::MarkdownNoNode;
        // Reject stale queries immediately, but retain bounded immutable chunks
        // for the next owner-thread preparation. Host/font/visibility retirement
        // still releases the complete graph through RetireLayout/Clear.
        if (_state->layout.HasPendingPreparation())
            ++_state->cancelledPreparations;
        _state->layout.Invalidate();
        _state->markdown.Cancel();
        _state->semantic.Cancel();
        _state->semanticRejected.reset();
        _state->nativeGeometryRejected.reset();
        _state->markdownCapacityRejected.reset();
        _state->markdownRequestActive = false;
        _state->requestActive         = false;
        RequestContentPreparation(false);
        _state->revealCaret = true;
    }
    const auto callback = sourceChanged ? _state->onTextChanged : nullptr;
    const auto lifetime = GetLifetimeToken();
    const auto revision = Revision();
    const auto binding  = _state->bindingRevision;
    const auto* host    = GetHost();
    // A committed source edit may have both callbacks registered. Keep native
    // text-input synchronization and internal selection state current now, but
    // let the consumer derive facts from the committed source before publishing
    // the corresponding selection notification.
    const bool deferSelectionPublication = sourceChanged && callback && _state->onSelectionChanged;
    SelectionChanged(synchronizeTextInput, false, ! deferSelectionPublication);
    // An embedded scheduling callback may itself edit, detach or retire this
    // control. Never publish a superseded notification or touch a retired owner.
    if (lifetime.expired() || GetHost() != host || Revision() != revision || _state->bindingRevision != binding ||
        (callback && _state->onTextChanged != callback))
        return;
    if (callback)
    {
        try
        {
            (*callback)();
        }
        catch (const std::bad_alloc&)
        {
            // Allocation failure is fatal at this native notification boundary.
            std::terminate();
        }
        catch (const std::exception&)
        {
            // The source edit is committed; notification failure cannot roll it back.
            Debug::Warning(L"DxUi::RichTextEditor: text notification failed; committed source retained.");
        }
    }
    // The text callback may replace its own registration; that must not suppress
    // this source revision's selection event. Source, attachment and owner still
    // have to be the transaction whose notification was deferred.
    if (deferSelectionPublication && ! lifetime.expired() && GetHost() == host && Revision() == revision && _state->bindingRevision == binding)
        PublishSelectionChanged();
}
void RichTextEditor::SelectionChanged(bool synchronizeTextInput, bool preservePreferredCaretX, bool publishSelection, bool revealCaret)
{
    _state->keyboardNavigationDocument.reset();
    _state->keyboardNavigationElement = RichText::MarkdownNoNode;
    ++_state->fileDropEpoch;
    if (_state->sourceEditing)
    {
        const auto range     = _state->sourceEditing->region.source;
        const auto selection = GetSelection();
        if (selection.Start() < range.first || selection.End() > range.last)
        {
            _state->sourceEditing.reset();
            RetireSemanticAccessibility();
            RetireLayout();
        }
    }
    _state->pendingLeadSurrogate  = 0;
    _state->overlaySelectionDirty = true;
    if (! preservePreferredCaretX)
        _state->preferredCaretX.reset();
    _state->revealCaret  = revealCaret;
    _state->caretVisible = true;
    _state->blinkEpoch.reset();
    UpdateCaretLinkDisplayRange();
    const auto lifetime = GetLifetimeToken();
    auto* host          = GetHost();
    // Imported previews are synchronized by the host after it publishes its
    // composition ownership marker. Public changes supersede that preview.
    if (synchronizeTextInput && host)
        host->SyncTextInput(this);
    if (lifetime.expired() || GetHost() != host)
        return;
    RequestContentPreparation(false);
    if (lifetime.expired() || GetHost() != host)
        return;
    RequestInvalidate();
    if (! lifetime.expired() && GetHost() == host && host && HasFocus() && Editable() && ! host->GetTheme().reducedMotion)
        host->RequestAnimation();
    if (publishSelection && ! lifetime.expired() && GetHost() == host)
        PublishSelectionChanged();
}
D2D1_RECT_F RichTextEditor::TextViewport() const noexcept
{
    const auto bounds      = GetBounds();
    const auto& appearance = _state->appearance;
    const float padding    = appearance.paddingDip < 0.0f ? (IsCompactDensity() ? 6.0f : 8.0f) : appearance.paddingDip;
    auto result            = D2D1::RectF(bounds.left + padding,
                                         bounds.top + padding,
                                         std::max(bounds.left + padding, bounds.right - padding),
                                         std::max(bounds.top + padding, bounds.bottom - padding));
    if (appearance.showVerticalScrollbar)
    {
        constexpr float lane = kScrollbarThicknessDip + 4.0f;
        if (IsRightToLeft())
            result.left = std::min(result.right, result.left + lane);
        else
            result.right = std::max(result.left, result.right - lane);
    }
    if (appearance.maximumTextWidthDip > 0.0f && result.right - result.left > appearance.maximumTextWidthDip)
    {
        result.left += (result.right - result.left - appearance.maximumTextWidthDip) * 0.5f;
        result.right = result.left + appearance.maximumTextWidthDip;
    }
    if (_state->mode == RichText::ViewMode::Source && appearance.sourceLineNumbers)
    {
        const float lane = std::min(SourceGutterWidth(), std::max(0.0f, (result.right - result.left - 1.0f) * 0.5f));
        if (IsRightToLeft())
            result.right -= lane;
        else
            result.left += lane;
    }
    return result;
}
D2D1_RECT_F RichTextEditor::ScrollbarTrack() const noexcept
{
    if (! _state->appearance.showVerticalScrollbar)
        return D2D1::RectF();
    const auto bounds   = GetBounds();
    const auto viewport = TextViewport();
    const auto padding  = _state->appearance.paddingDip < 0.0f ? (IsCompactDensity() ? 6.0f : 8.0f) : _state->appearance.paddingDip;
    const float left    = IsRightToLeft() ? bounds.left + padding : bounds.right - padding - kScrollbarThicknessDip;
    return D2D1::RectF(std::max(bounds.left, left), viewport.top, std::min(bounds.right, left + kScrollbarThicknessDip), viewport.bottom);
}
void RichTextEditor::RetireLayout(bool retainGeometry) const noexcept
{
    _state->selectionMask.reset();
    _state->selectionLayer.reset();
    _state->selectionLayerWidth = _state->selectionLayerHeight = 0.0f;
    ++_state->fileDropEpoch;
    _state->overlaySelectionDirty = _state->overlayHighlightsDirty = true;
    if (! _state->decorations.empty())
        _state->decorationStatus = RichTextDecorationStatus::Pending;
    _state->sourceInk.Clear();
    _state->sourceGuides.Clear();
    _state->codeInkDocument.reset();
    _state->codeInk.clear();
    _state->codeInkStaged.clear();
    _state->codeInkComplete = false;
    _state->hoveredLink.reset();
    _state->caretLink.reset();
    _state->keyboardNavigationDocument.reset();
    _state->keyboardNavigationElement = RichText::MarkdownNoNode;
    _state->taskPressPlan.reset();
    _state->codePressDocument.reset();
    if (_state->layout.HasPendingPreparation())
        ++_state->cancelledPreparations;
    if (retainGeometry)
        _state->layout.Invalidate();
    else
        _state->layout.Clear();
    _state->markdown.Cancel();
    _state->semantic.Cancel();
    _state->markdownRequestActive = false;
    _state->requestActive         = false;
    _state->revealCaret           = true;
    RequestContentPreparation(false);
}
bool RichTextEditor::LayoutIsCurrent(const ControlHost& host) const noexcept
{
    const auto& state   = *_state;
    const auto viewport = TextViewport();
    return GetHost() == &host && state.layout.Ready() && state.layout.Revision() == state.document.Revision() &&
           state.layoutWidth == std::max(1.0f, viewport.right - viewport.left) && state.typographyRevision == host.GetTypographyRevision() &&
           state.resourceGeneration == host.GetResourceGeneration() &&
           (! state.markdownProfile || GetMarkdownSyntax() || state.HasCurrentMarkdownCapacityRejection());
}
bool RichTextEditor::AcceptsTextInputDuringPreparation() const noexcept
{
    return true; // Source/selection/transactions are independent of glyph preparation.
}
void RichTextEditor::PrepareOverlayGeometry(ControlHost& host)
{
    auto& state        = *_state;
    const auto& layout = state.layout;
    if (state.appearance.showVerticalScrollbar)
        for (const bool dragging : {false, true})
        {
            const auto visuals = ResolveScrollbarVisuals(host.GetTheme(), false, false, dragging);
            static_cast<void>(host.GetSolidBrush(visuals.track));
            static_cast<void>(host.GetSolidBrush(visuals.thumb));
        }
    if (state.overlaySelectionDirty)
    {
        const auto selection = GetSelection();
        auto selected        = layout.SourceRange(selection.Start(), selection.End());
        wil::com_ptr<ID2D1PathGeometry> mask;
        wil::com_ptr<ID2D1Layer> layer = state.selectionLayer;
        const auto viewport            = TextViewport();
        const auto width = std::max(1.0f, viewport.right - viewport.left), height = std::max(1.0f, viewport.bottom - viewport.top);
        if (selected.size() > 1u)
        {
            auto* context = host.GetDeviceContext();
            THROW_HR_IF(E_UNEXPECTED, ! context);
            wil::com_ptr<ID2D1Factory> factory;
            context->GetFactory(factory.put());
            THROW_IF_FAILED(factory->CreatePathGeometry(mask.put()));
            wil::com_ptr<ID2D1GeometrySink> sink;
            THROW_IF_FAILED(mask->Open(sink.put()));
            sink->SetFillMode(D2D1_FILL_MODE_WINDING);
            for (const auto& rectangle : selected)
            {
                sink->BeginFigure(D2D1::Point2F(rectangle.left, rectangle.top), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(rectangle.right, rectangle.top));
                sink->AddLine(D2D1::Point2F(rectangle.right, rectangle.bottom));
                sink->AddLine(D2D1::Point2F(rectangle.left, rectangle.bottom));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
            THROW_IF_FAILED(sink->Close());
            if (! layer || state.selectionLayerWidth != width || state.selectionLayerHeight != height ||
                state.selectionLayerGeneration != host.GetResourceGeneration())
                THROW_IF_FAILED(context->CreateLayer(D2D1::SizeF(width, height), layer.put()));
        }
        std::vector<D2D1_RECT_F> composition(0u), conversion(0u);
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        if (host.TryReadNativeTextInputState(this, input) && layout.SourceMatches(input.text) && input.compositionStartIndex && input.compositionEndIndex)
        {
            auto staged = layout.SourceRange(*input.compositionStartIndex, *input.compositionEndIndex);
            composition.swap(staged);
            if (input.conversionTargetStartIndex && input.conversionTargetEndIndex)
            {
                auto target = layout.SourceRange(*input.conversionTargetStartIndex, *input.conversionTargetEndIndex);
                conversion.swap(target);
            }
        }
        state.selectionRectangles.swap(selected);
        state.selectionMask.swap(mask);
        state.selectionLayer.swap(layer);
        state.selectionLayerWidth      = width;
        state.selectionLayerHeight     = height;
        state.selectionLayerGeneration = host.GetResourceGeneration();
        state.compositionRectangles.swap(composition);
        state.conversionRectangles.swap(conversion);
        state.overlaySelectionDirty = false;
    }
    if (state.overlayHighlightsDirty)
    {
        std::vector<State::HighlightRectangle> rectangles(0u);
        std::vector<State::DecorationRectangle> decorations(0u);
        bool decorationCapacityExceeded = false;
        std::vector<RichTextSourceHighlight> normalized(0u);
        std::vector<RichText::MarkdownRange> atomic(0u);
        if (! state.highlights.empty())
            if (const auto* semantic = layout.SemanticGeometry())
            {
                for (const auto& span : semantic->Document().Map().Spans())
                    if (span.primary && (span.kind == RichText::DisplaySpanKind::Transformed || span.kind == RichText::DisplaySpanKind::Object))
                        atomic.push_back(span.source);
                std::ranges::sort(atomic, {}, &RichText::MarkdownRange::first);
            }
        UnionHighlights(state.highlights, atomic, normalized);
        for (const auto& highlight : normalized)
            for (const auto& rectangle : layout.SourceRange(highlight.start, highlight.start + highlight.length))
                rectangles.push_back({rectangle, highlight.current});
        std::ranges::sort(rectangles, [](const auto& a, const auto& b) {
            return a.rectangle.top < b.rectangle.top || (a.rectangle.top == b.rectangle.top && a.current < b.current);
        });
        std::vector<float> bottoms(0u);
        bottoms.reserve(rectangles.size());
        float bottom = 0.0f;
        for (const auto& rectangle : rectangles)
        {
            bottom = std::max(bottom, rectangle.rectangle.bottom);
            bottoms.push_back(bottom);
        }
        for (const auto& decoration : state.decorations)
        {
            const auto mapped = layout.SourceRange(decoration.start, decoration.start + decoration.length);
            if (mapped.size() > state.decorationRectangleLimit - decorations.size())
            {
                decorationCapacityExceeded = true;
                decorations.clear();
                break;
            }
            for (const auto& rectangle : mapped)
                decorations.push_back({rectangle, decoration.kind, decoration.role});
        }
        for (const auto role :
             {RichTextDecorationRole::Accent, RichTextDecorationRole::Subdued, RichTextDecorationRole::Warning, RichTextDecorationRole::Error})
        {
            auto color = DecorationColor(host.GetTheme(), role);
            static_cast<void>(host.GetSolidBrush(color));
            if (! host.GetTheme().highContrast)
            {
                color.a *= .18f;
                static_cast<void>(host.GetSolidBrush(color));
            }
        }
        for (const bool current : {false, true})
        {
            auto color = host.GetTheme().selectionFill;
            color.a *= host.GetTheme().highContrast ? 1.0f : current ? 0.38f : 0.18f;
            static_cast<void>(host.GetSolidBrush(color));
        }
        state.highlightRectangles.swap(rectangles);
        state.highlightBottoms.swap(bottoms);
        RichText::Detail::PaintIndex decorationIndex;
        decorationIndex.Build(decorations.size(), [&](size_t i) { return decorations[i].rectangle; });
        state.decorationPaintIndex.Swap(decorationIndex);
        state.decorationRectangles.swap(decorations);
        state.decorationStatus       = state.decorations.empty()    ? RichTextDecorationStatus::None
                                       : decorationCapacityExceeded ? RichTextDecorationStatus::CapacityExceeded
                                                                    : RichTextDecorationStatus::Ready;
        state.overlayHighlightsDirty = false;
    }
}
void RichTextEditor::CancelContentPreparation() noexcept
{
    _state->images.Cancel();
    RetireLayout();
}
HRESULT RichTextEditor::PrepareContent(ControlHost& host) noexcept
{
    auto& state                = *_state;
    bool richPreparationActive = false;
    if (state.codeCallbackActive)
        return S_FALSE;
    state.lastAdvanceLayouts = state.lastAdvanceBlockSteps = 0u;
    state.lastAdvanceMarkdownWork                          = 0u;
    state.lastAdvanceSemanticWork                          = 0u;
    const auto rejectNativeObjects                         = [&](std::span<const std::size_t> elements) -> HRESULT
    {
        const auto value = GetSemanticDocument();
        if (value && state.markdownRendering && state.mode == RichText::ViewMode::Rich)
        {
            try
            {
                std::vector<RichText::MarkdownRange> objects(0u);
                if (state.nativeObjectRejections)
                    objects = state.nativeObjectRejections->objects;
                bool added = false;
                for (const auto element : elements)
                {
                    if (element >= value->Elements().size() ||
                        (value->Elements()[element].role != RichText::SemanticRole::Math && value->Elements()[element].role != RichText::SemanticRole::Diagram))
                        break;
                    const auto source = value->Elements()[element].source;
                    if (std::ranges::find(objects, source) == objects.end())
                    {
                        objects.push_back(source);
                        added = true;
                    }
                }
                if (added && ! elements.empty() && objects.size() <= RichText::Detail::SemanticExtensions::MaximumObjects &&
                    std::ranges::all_of(elements,
                                        [&](const auto element)
                {
                    return element < value->Elements().size() && (value->Elements()[element].role == RichText::SemanticRole::Math ||
                                                                  value->Elements()[element].role == RichText::SemanticRole::Diagram);
                }))
                {
                    std::ranges::sort(objects, {}, &RichText::MarkdownRange::first);
                    if (! state.nativeObjectRejections)
                    {
                        auto& rejected      = state.nativeObjectRejections.emplace();
                        rejected.syntax     = value->Syntax();
                        rejected.source     = Snapshot();
                        rejected.width      = std::max(1.0f, TextViewport().right - TextViewport().left);
                        rejected.dpi        = host.GetDpi();
                        rejected.typography = host.GetTypographyRevision();
                        rejected.resources  = host.GetResourceGeneration();
                        rejected.binding    = state.bindingRevision;
                        rejected.appearance = state.appearanceRevision;
                        rejected.rtl        = IsRightToLeft();
                    }
                    state.nativeObjectRejections->objects.swap(objects);
                    RetireSemanticAccessibility();
                    state.semantic.Clear();
                    state.layout.Invalidate();
                    state.requestActive = false;
                    return S_FALSE;
                }
            }
            catch (const std::bad_alloc&)
            {
                // Do not publish a partial rejection set on allocation failure.
                state.layout.Invalidate();
                state.requestActive = false;
                return E_OUTOFMEMORY;
            }
            state.nativeGeometryRejected.emplace(value,
                                                 std::max(1.0f, TextViewport().right - TextViewport().left),
                                                 host.GetDpi(),
                                                 host.GetTypographyRevision(),
                                                 host.GetResourceGeneration(),
                                                 state.bindingRevision,
                                                 state.appearanceRevision,
                                                 IsRightToLeft());
            RetireSemanticAccessibility();
            state.layout.Invalidate();
            state.requestActive = false;
            return S_FALSE;
        }
        state.layout.Invalidate();
        state.requestActive = false;
        return E_FAIL;
    };
    try
    {
        const auto viewport = TextViewport();
        if (GetHost() != &host || ! IsVisible() || viewport.right <= viewport.left || viewport.bottom <= viewport.top)
        {
            state.images.Cancel();
            if (state.layout.HasPendingPreparation())
                ++state.cancelledPreparations;
            state.layout.Clear();
            state.markdown.Cancel();
            state.semantic.Clear();
            state.semanticRejected.reset();
            state.nativeGeometryRejected.reset();
            state.markdownRequestActive = false;
            state.requestActive         = false;
            return S_OK;
        }
        if (state.mode == RichText::ViewMode::Rich && state.appearance.markdownPresentation == RichText::MarkdownPresentationStyle::Subtle)
        {
            auto style = ResolveTextFieldVisualStyle(host.GetTheme(), IsEnabled(), IsHovered(), HasFocus(), HasFocus() && host.IsKeyboardFocusVisible());
            if (state.appearance.blendWithWindowBackground)
                style.fill = host.GetTheme().windowBackground;
            // Resolve additional document brushes during preparation, so a
            // quote-only first paint cannot allocate a new cache entry.
            static_cast<void>(host.GetSolidBrush(host.GetTheme().highContrast ? style.text : host.GetTheme().borderDefault));
            static_cast<void>(host.GetSolidBrush(ResolveMarkdownQuoteFill(host.GetTheme(), style)));
            auto opaque = style.fill;
            opaque.a    = 1.0f;
            static_cast<void>(host.GetSolidBrush(opaque));
            static_cast<void>(host.GetSolidBrush(CompositeOverBackground(style.selectionFill, opaque)));
        }
        if (state.mode == RichText::ViewMode::Rich && state.codeTokenizer && ! host.GetTheme().highContrast)
        {
            static_cast<void>(host.GetSolidBrush(host.GetTheme().subduedText));
            static_cast<void>(host.GetSolidBrush(host.GetTheme().accent));
        }
        const auto width      = std::max(1.0f, viewport.right - viewport.left);
        const auto typography = host.GetTypographyRevision(), resources = host.GetResourceGeneration();
        const bool rejectedPreparation = state.RejectsPreparation(width, host.GetDpi(), IsRightToLeft(), typography, resources);
        if (state.preparationRejected && ! rejectedPreparation)
        {
            state.preparationRejected.reset();
            RetireSemanticAccessibility();
            state.layout.Invalidate();
            state.requestActive = false;
        }
        if (LayoutIsCurrent(host))
        {
            UpdateCaretScroll();
            PrepareOverlayGeometry(host);
            const auto codeInk = PrepareCodeInk(state.mode == RichText::ViewMode::Rich && state.layout.Semantic() ? GetSemanticDocument() : nullptr);
            if (codeInk != S_OK)
                return codeInk;
            const auto result          = PrepareSourceInk() && PrepareSourceGuides(host) ? S_OK : S_FALSE;
            const auto lifetime        = GetLifetimeToken();
            const auto callbackBinding = state.bindingRevision, callbackRevision = Revision();
            PublishHorizontalScrollChanged();
            if (! lifetime.expired() && GetHost() == &host && _state->bindingRevision == callbackBinding && Revision() == callbackRevision)
                PublishScrollChanged();
            return result;
        }
        if (! rejectedPreparation && state.markdownProfile && ! GetMarkdownSyntax() && ! state.HasCurrentMarkdownCapacityRejection())
        {
            try
            {
                if (! state.markdownRequestActive || state.markdownRequestRevision != Revision() || ! state.markdown.HasPendingPreparation())
                {
                    // Exact bounded source admission/copy is still indivisible.
                    // Parsing resumes its own cursors, without DirectWrite or a
                    // per-control worker. Semantic preparation consumes only a
                    // completed current tree within the remaining owner-turn budget.
                    if (state.sourceEditing)
                        state.markdown.BeginSourceRegion(Snapshot(), Revision(), state.sourceEditing->region, *state.markdownProfile);
                    else
                        state.markdown.Begin(Snapshot(), Revision(), *state.markdownProfile);
                    state.markdownRequestRevision = Revision();
                    state.markdownRequestActive   = true;
                }
                ++state.markdownAdvances;
                const auto advance            = state.markdown.Advance();
                state.lastAdvanceMarkdownWork = advance.workUnits;
                if (advance.status == RichText::MarkdownPreparationStatus::Pending)
                    return S_FALSE;
                state.markdownRequestActive = false;
                if (state.sourceEditing)
                    if (const auto syntax = GetMarkdownSyntax())
                        state.sourceEditing->region = *syntax->SourceRegion();
            }
            catch (const std::length_error&)
            {
                // This exact request cannot fit the rich preview. Retain its
                // shared source identity without copying text or inventing a
                // partial tree. Memory failures remain separately retryable.
                state.markdown.Cancel();
                state.markdownRequestActive = false;
                state.semantic.Clear();
                state.semanticRejected.reset();
                state.nativeGeometryRejected.reset();
                state.markdownCapacityRejected.emplace(Snapshot(), Revision(), *state.markdownProfile);
            }
        }
        const auto currentSyntax = GetMarkdownSyntax();
        if (state.nativeObjectRejections &&
            ! state.nativeObjectRejections->Matches(
                nullptr, width, host.GetDpi(), IsRightToLeft(), typography, resources, state.bindingRevision, state.appearanceRevision))
        {
            state.nativeObjectRejections.reset();
            RetireSemanticAccessibility();
            state.semantic.Clear();
        }
        if (state.nativeObjectRejections && currentSyntax && state.nativeObjectRejections->syntax != currentSyntax)
        {
            auto& rejected    = *state.nativeObjectRejections;
            const auto source = Snapshot();
            const auto change = source.ChangedRangeFrom(rejected.source);
            std::vector<RichText::MarkdownRange> retained(0u);
            retained.reserve(rejected.objects.size());
            if (change.kind != RichText::SourceChangeKind::FullReplacement)
                for (auto range : rejected.objects)
                {
                    const auto previousRange = range;
                    if (range.last <= change.begin)
                    {
                        // Entire object precedes the tracked edit.
                    }
                    else if (range.first >= change.previousEnd)
                    {
                        range.first = change.currentEnd + (range.first - change.previousEnd);
                        range.last  = change.currentEnd + (range.last - change.previousEnd);
                    }
                    else
                        continue;
                    const auto& oldNodes      = rejected.syntax->Nodes();
                    const auto& newNodes      = currentSyntax->Nodes();
                    const auto containingLeaf = [](const auto& nodes, RichText::MarkdownRange object)
                    {
                        auto best = RichText::MarkdownNoNode;
                        for (std::size_t i = 1u; i < nodes.size(); ++i)
                        {
                            const auto kind = nodes[i].block.kind;
                            if (kind != RichText::MarkdownBlockKind::Paragraph && kind != RichText::MarkdownBlockKind::AtxHeading &&
                                kind != RichText::MarkdownBlockKind::SetextHeading && kind != RichText::MarkdownBlockKind::TableCell)
                                continue;
                            const auto source = nodes[i].block.source;
                            if (source.first <= object.first && object.last <= source.last &&
                                (best == RichText::MarkdownNoNode ||
                                 source.last - source.first < nodes[best].block.source.last - nodes[best].block.source.first))
                                best = i;
                        }
                        return best;
                    };
                    const auto sameContext = [&](std::size_t before, std::size_t after)
                    {
                        while (before != RichText::MarkdownNoNode && after != RichText::MarkdownNoNode)
                        {
                            const auto& left  = oldNodes[before];
                            const auto& right = newNodes[after];
                            if (left.block.kind != right.block.kind || left.block.heading != right.block.heading || left.contentIndent != right.contentIndent ||
                                left.listStart != right.listStart || left.listMarker != right.listMarker || left.alignment != right.alignment)
                                return false;
                            before = left.parent;
                            after  = right.parent;
                        }
                        return before == after;
                    };
                    bool math = std::ranges::any_of(currentSyntax->Inlines(),
                                                    [&](const auto& item) { return item.kind == RichText::MarkdownInlineKind::Math && item.source == range; });
                    if (math)
                    {
                        const auto oldLeaf = containingLeaf(oldNodes, previousRange);
                        const auto newLeaf = containingLeaf(newNodes, range);
                        math               = oldLeaf != RichText::MarkdownNoNode && newLeaf != RichText::MarkdownNoNode && sameContext(oldLeaf, newLeaf);
                        if (math)
                        {
                            const auto oldBlock = oldNodes[oldLeaf].block.source;
                            const auto newBlock = newNodes[newLeaf].block.source;
                            math                = rejected.syntax->Slice(oldBlock) == currentSyntax->Slice(newBlock);
                        }
                    }
                    const auto oldDiagram = std::ranges::find_if(oldNodes, [&](const auto& item) {
                        return item.block.kind == RichText::MarkdownBlockKind::FencedCode && item.block.source == previousRange;
                    });
                    const auto newDiagram = std::ranges::find_if(
                        newNodes, [&](const auto& item) { return item.block.kind == RichText::MarkdownBlockKind::FencedCode && item.block.source == range; });
                    bool diagram = oldDiagram != oldNodes.end() && newDiagram != newNodes.end();
                    if (diagram)
                    {
                        const auto info = currentSyntax->Slice(newDiagram->block.info);
                        diagram         = info.starts_with(L"mermaid") && (info.size() == 7u || info[7u] == L' ' || info[7u] == L'\t');
                        diagram         = diagram && sameContext(oldDiagram->parent, newDiagram->parent);
                    }
                    if (math || diagram)
                        retained.push_back(range);
                }
            rejected.objects.swap(retained);
            rejected.syntax = currentSyntax;
            rejected.source = source;
            RetireSemanticAccessibility();
            state.semantic.Clear();
        }
        std::shared_ptr<const RichText::SemanticDocument> semanticDocument;
        if (! rejectedPreparation && state.markdownRendering && state.mode == RichText::ViewMode::Rich)
        {
            const auto syntax = GetMarkdownSyntax();
            semanticDocument  = GetSemanticDocument();
            if (syntax && ! semanticDocument && state.semanticRejected != syntax)
            {
                try
                {
                    constexpr std::size_t sharedWorkBudget = 4096u;
                    if (state.lastAdvanceMarkdownWork >= sharedWorkBudget)
                        return S_FALSE;
                    if (! state.semantic.HasPendingPreparation())
                    {
                        richPreparationActive = true;
                        if (state.nativeObjectRejections)
                            RichText::Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(state.semantic, syntax, state.nativeObjectRejections->objects);
                        else
                            state.semantic.Begin(syntax);
                    }
                    ++state.semanticAdvances;
                    richPreparationActive = true;
#if DXUI_ENABLE_DIAGNOSTICS
                    if (RichText::Detail::testRichPreparationFailure == 1u)
                    {
                        ++RichText::Detail::testRichPreparationFailureAttempts;
                        throw std::logic_error("injected deterministic semantic staging failure");
                    }
#endif
                    const auto advance            = state.semantic.Advance(sharedWorkBudget - state.lastAdvanceMarkdownWork);
                    richPreparationActive         = false;
                    state.lastAdvanceSemanticWork = advance.workUnits;
                    if (advance.status == RichText::MarkdownPreparationStatus::Pending)
                        return S_FALSE;
                    semanticDocument = GetSemanticDocument();
                }
                catch (const std::length_error&)
                {
                    richPreparationActive = false;
                    // Admission is a preview bound, not a document failure.
                    // Remember this exact immutable syntax so idle preparation
                    // cannot repeatedly fail. Editing/profile changes retry.
                    state.semantic.Clear();
                    state.semanticRejected = syntax;
                }
            }
        }
        auto* factory = host.GetWriteFactory();
#if DXUI_ENABLE_DIAGNOSTICS
        if (RichText::Detail::testRichSemanticWriteFactory)
            factory = RichText::Detail::testRichSemanticWriteFactory;
#endif
        if (semanticDocument && state.RejectsNativeGeometry(semanticDocument, width, host.GetDpi(), IsRightToLeft(), typography, resources))
            semanticDocument.reset();
        if (semanticDocument)
        {
            if (! state.images.Begin(semanticDocument, state.documentFilePath, state.imageLocationRevision, state.bindingRevision) && ! state.images.Advance())
            {
                ++state.preparationAdvances;
                state.lastAdvanceBlockSteps = 4u;
                return S_FALSE;
            }
        }
        else
            state.images.Clear();
        const auto effectiveMode =
            state.markdownRendering && state.mode == RichText::ViewMode::Rich && ! semanticDocument ? RichText::ViewMode::Source : state.mode;
        const auto revision = Revision(), binding = state.bindingRevision;
        if (semanticDocument)
        {
            state.images.Publish();
            state.imageSource = Snapshot();
        }
        const State::LayoutKey requestKey{revision, binding, typography, resources, factory, width, state.mode};
        const bool sameRequest = state.requestActive && state.layout.HasPendingPreparation() && state.requestKey == requestKey;
        if (! sameRequest)
        {
            if (state.layout.HasPendingPreparation())
                ++state.cancelledPreparations;
            state.layout.Invalidate();
            state.requestActive = false;
            auto* format        = host.GetTextFormat(FontRole::Body);
            auto* codeFormat    = host.GetTextFormat(FontRole::Monospace);
            if (! factory || ! format || ! codeFormat)
                THROW_HR(E_UNEXPECTED);
            RichText::LayoutOptions options;
            options.dpi         = host.GetDpi();
            options.widthDip    = width;
            options.fontSizeDip = format->GetFontSize();
            options.fontFamily.resize(static_cast<size_t>(format->GetFontFamilyNameLength()) + 1u);
            THROW_IF_FAILED(format->GetFontFamilyName(options.fontFamily.data(), static_cast<UINT32>(options.fontFamily.size())));
            options.fontFamily.pop_back();
            options.codeFontFamily.resize(static_cast<size_t>(codeFormat->GetFontFamilyNameLength()) + 1u);
            THROW_IF_FAILED(codeFormat->GetFontFamilyName(options.codeFontFamily.data(), static_cast<UINT32>(options.codeFontFamily.size())));
            options.codeFontFamily.pop_back();
            options.locale.resize(static_cast<size_t>(format->GetLocaleNameLength()) + 1u);
            THROW_IF_FAILED(format->GetLocaleName(options.locale.data(), static_cast<UINT32>(options.locale.size())));
            options.locale.pop_back();
            const auto& appearance = state.appearance;
            if (appearance.bodyFontSizeDip > 0.0f)
                options.fontSizeDip = appearance.bodyFontSizeDip;
            options.codeFontSizeDip             = appearance.codeFontSizeDip;
            options.lineHeightMultiplier        = appearance.lineHeightMultiplier;
            options.headingScaleStep            = appearance.headingScaleStep;
            options.headingFontSizeDip          = appearance.headingFontSizeDip;
            options.headingLineHeightMultiplier = appearance.headingLineHeightMultiplier;
            options.paragraphSpacingAfterLines  = appearance.paragraphSpacingAfterLines;
            options.listItemSpacingDip          = appearance.listItemSpacingDip;
            options.semanticSpacing             = appearance.semanticSpacing;
            options.markdownPresentation        = appearance.markdownPresentation;
            options.codeCopyAction              = bool(state.onCodeCopyRequested);
            options.wrap                        = effectiveMode != RichText::ViewMode::Source || appearance.sourceWrap;

            if (! appearance.bodyFontFamily.empty())
                options.fontFamily = appearance.bodyFontFamily;
            if (! appearance.codeFontFamily.empty())
                options.codeFontFamily = appearance.codeFontFamily;
            if (effectiveMode == RichText::ViewMode::Source && appearance.sourceMonospace)
            {
                options.fontFamily = options.codeFontFamily;
                if (options.codeFontSizeDip > 0.0f)
                    options.fontSizeDip = options.codeFontSizeDip;
            }
            const auto scale    = [zoom = appearance.zoom](float size) noexcept { return std::clamp(size * zoom, 1.0f, 512.0f); };
            options.fontSizeDip = scale(options.fontSizeDip);
            if (options.codeFontSizeDip > 0.0f)
                options.codeFontSizeDip = scale(options.codeFontSizeDip);
            for (auto& headingSize : options.headingFontSizeDip)
                if (headingSize > 0.0f)
                    headingSize = scale(headingSize);
            options.tabStopDip       = appearance.tabStopDip;
            options.tableOverflow    = appearance.tableOverflow;
            options.readingDirection = IsRightToLeft() ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
            state.sourceGuideOptions = options;
            richPreparationActive    = semanticDocument != nullptr;
            state.layout.BeginPreparation(factory,
                                          state.document.Snapshot(),
                                          revision,
                                          effectiveMode,
                                          options,
                                          semanticDocument,
                                          nullptr,
                                          {typography, resources},
                                          state.images.Snapshot(),
                                          host.GetDeviceContext());
            richPreparationActive = false;
            state.requestKey      = requestKey;
            state.requestActive   = true;
        }
        if (state.layout.HasPendingPreparation())
        {
            ++state.preparationAdvances;
            richPreparationActive = semanticDocument != nullptr;
#if DXUI_ENABLE_DIAGNOSTICS
            if (richPreparationActive && RichText::Detail::testRichPreparationFailure == 2u)
            {
                ++RichText::Detail::testRichPreparationFailureAttempts;
                throw std::logic_error("injected deterministic native geometry failure");
            }
#endif
            const auto advance          = state.layout.AdvancePreparation();
            richPreparationActive       = false;
            state.lastAdvanceLayouts    = advance.layoutsPrepared;
            state.lastAdvanceBlockSteps = advance.blockSteps;
            if (advance.status == RichText::LayoutPreparationStatus::Pending)
                return S_FALSE;
        }
        // No application callback crosses staging. Recheck the complete key
        // before acknowledging geometry; host publication follows rasterization.
        if (GetHost() != &host || Revision() != revision || state.bindingRevision != binding || state.mode != state.requestKey.mode ||
            host.GetResourceGeneration() != resources || host.GetTypographyRevision() != typography ||
            std::max(1.0f, TextViewport().right - TextViewport().left) != width)
        {
            state.layout.Invalidate();
            state.requestActive = false;
            return S_FALSE;
        }
        state.layoutWidth           = width;
        state.typographyRevision    = typography;
        state.resourceGeneration    = resources;
        state.requestActive         = false;
        state.overlaySelectionDirty = state.overlayHighlightsDirty = true;
        UpdateCaretScroll();
        UpdateCaretLinkDisplayRange();
        PrepareOverlayGeometry(host);
        const auto codeInk = PrepareCodeInk(semanticDocument);
        if (codeInk != S_OK)
            return codeInk;
        const auto result          = PrepareSourceInk() && PrepareSourceGuides(host) ? S_OK : S_FALSE;
        const auto lifetime        = GetLifetimeToken();
        const auto callbackBinding = state.bindingRevision, callbackRevision = Revision();
        PublishHorizontalScrollChanged();
        if (! lifetime.expired() && GetHost() == &host && _state->bindingRevision == callbackBinding && Revision() == callbackRevision)
            PublishScrollChanged();
        return result;
    }
    catch (const RichText::Detail::NativeObjectsRejected& failure)
    {
        return rejectNativeObjects(failure.Elements());
    }
    catch (const RichText::Detail::NativeObjectUnsupported& failure)
    {
        if (failure.Error() != DWRITE_E_UNSUPPORTEDOPERATION)
            return failure.Error();
        const auto element = failure.Element();
        return rejectNativeObjects(std::span(&element, 1u));
    }
    catch (const RichText::Detail::NativeObjectCapacity& failure)
    {
        // Bounded geometry capacity and the explicitly classified font-local
        // unsupported operation both reject only the owning object/key.
        const auto element = failure.Element();
        return rejectNativeObjects(std::span(&element, 1u));
    }
    catch (const std::length_error&)
    {
        // Native caret scaffolding shares the paragraph/cell admission ceiling.
        // Reject this syntax once, then prepare exact source geometry next turn.
        if (state.markdownRendering && state.mode == RichText::ViewMode::Rich && state.semanticRejected != GetMarkdownSyntax())
        {
            state.semanticRejected = GetMarkdownSyntax();
            state.semantic.Clear();
            state.layout.Invalidate();
            state.requestActive = false;
            return S_FALSE;
        }
        state.layout.Invalidate();
        return E_FAIL;
    }
    catch (const std::bad_alloc&)
    {
        state.images.Cancel();
        state.layout.Invalidate();
        state.markdown.Cancel();
        state.semantic.Cancel();
        state.markdownRequestActive = false;
        state.requestActive         = false;
        return E_OUTOFMEMORY;
    }
    catch (const wil::ResultException& failure)
    {
        // Preserve native HRESULTs across this noexcept preparation boundary.
        // In particular, target loss remains visible to the host and memory
        // exhaustion never becomes an object-capacity rejection.
        state.images.Cancel();
        state.layout.Invalidate();
        state.markdown.Cancel();
        state.semantic.Cancel();
        state.markdownRequestActive = false;
        state.requestActive         = false;
        return failure.GetErrorCode();
    }
    catch (const std::logic_error&)
    {
        if (richPreparationActive && GetHost() == &host && state.markdownRendering && state.mode == RichText::ViewMode::Rich)
        {
            const auto viewport = TextViewport();
            auto& rejected      = state.preparationRejected.emplace();
            rejected.source     = Snapshot();
            rejected.profile    = state.markdownProfile;
            if (state.sourceEditing)
                rejected.region = state.sourceEditing->region;
            rejected.mode       = state.mode;
            rejected.width      = std::max(1.0f, viewport.right - viewport.left);
            rejected.dpi        = host.GetDpi();
            rejected.rtl        = IsRightToLeft();
            rejected.typography = host.GetTypographyRevision();
            rejected.resources  = host.GetResourceGeneration();
            rejected.binding    = state.bindingRevision;
            rejected.appearance = state.appearanceRevision;
            state.images.Cancel();
            state.semantic.Clear();
            RetireSemanticAccessibility();
            state.layout.Invalidate();
            state.requestActive = false;
            Debug::Warning(L"DxUi::RichTextEditor: deterministic rich preparation failed; exact source is shown for this view key.");
            return S_FALSE;
        }
        state.layout.Invalidate();
        state.semantic.Cancel();
        state.requestActive = false;
        return E_FAIL;
    }
    catch (const std::exception&)
    {
        state.layout.Invalidate();
        state.markdown.Cancel();
        state.semantic.Cancel();
        state.markdownRequestActive = false;
        state.requestActive         = false;
        return E_FAIL;
    }
}
void RichTextEditor::RequireCurrentLayout(const ControlHost& host) const
{
    // Geometry queries, input and painting never drain pending preparation.
    if (! LayoutIsCurrent(host))
        throw std::logic_error("Rich-text geometry is pending or unavailable");
}
void RichTextEditor::UpdateCaretScroll() const
{
    auto& state                   = *_state;
    const auto previousScroll     = state.scrollDip;
    const auto previousHorizontal = state.horizontalScrollDip;
    const auto viewport           = TextViewport();
    const float height            = std::max(0.0f, viewport.bottom - viewport.top);
    if (state.revealCaret)
    {
        const auto caret = state.layout.SourceCaret(state.document.GetSelection().caret, state.sourceCaretSite);
        if (caret.y < state.scrollDip)
            state.scrollDip = caret.y;
        if (caret.y + caret.height > state.scrollDip + height)
            state.scrollDip = caret.y + caret.height - height;
        const float width    = std::max(0.0f, viewport.right - viewport.left);
        const bool reverse   = state.mode == RichText::ViewMode::Source && IsRightToLeft();
        const float visibleX = caret.x + (reverse ? state.horizontalScrollDip : -state.horizontalScrollDip);
        if (visibleX < 0.0f)
            state.horizontalScrollDip += reverse ? -visibleX : visibleX;
        else if (visibleX > width - 1.0f)
            state.horizontalScrollDip += reverse ? width - 1.0f - visibleX : visibleX - width + 1.0f;
        state.revealCaret = false;
    }
    state.scrollDip = std::clamp(state.scrollDip, 0.0f, std::max(0.0f, state.layout.Height() - height));
    state.horizontalScrollDip =
        std::clamp(state.horizontalScrollDip, 0.0f, std::max(0.0f, state.layout.Width() - std::max(0.0f, viewport.right - viewport.left)));
    if (state.scrollDip != previousScroll || state.horizontalScrollDip != previousHorizontal)
    {
        state.hoveredLink.reset();
        ++state.fileDropEpoch;
    }
}
RichTextEditorDebugState RichTextEditor::DebugGetState() const noexcept
{
    const auto preparation = _state->layout.LastPreparation();
    return {_state->layout.Ready(),
            _state->layout.Generation(),
            _state->layout.Height(),
            _state->scrollDip,
            _state->dragging,
            _state->layout.ChunkCount(),
            preparation.preparedChunks,
            preparation.reusedChunks,
            preparation.preparedSourceUnits,
            _state->layout.UsesWholeGeometry(),
            _state->layout.UsesWholeRaster(),
            preparation.wholeLayoutsPrepared,
            preparation.wholeLayoutSourceUnits,
            _state->layout.HasPendingPreparation() || _state->markdown.HasPendingPreparation() || _state->semantic.HasPendingPreparation() ||
                _state->images.Pending(),
            _state->preparationAdvances,
            _state->cancelledPreparations,
            _state->lastAdvanceLayouts,
            _state->lastAdvanceBlockSteps,
            _state->markdown.HasPendingPreparation(),
            _state->markdownAdvances,
            _state->lastAdvanceMarkdownWork,
            _state->semantic.HasPendingPreparation(),
            _state->semanticAdvances,
            _state->lastAdvanceSemanticWork,
            _state->sourceInk.Pending(),
            _state->sourceInk.Ranges().size(),
            _state->selectionRectangles.size(),
            _state->highlightRectangles.size(),
            _state->decorationRectangles.size(),
            _state->overlaySelectionDirty || _state->overlayHighlightsDirty};
}
size_t RichTextEditor::DebugGetListStateAncestorSteps() const noexcept
{
    return _state->lastListStateAncestorSteps;
}
bool RichTextEditor::SupportsTextInput() const noexcept
{
    return true;
}
bool RichTextEditor::AcceptsTypedText() const noexcept
{
    return Editable();
}
std::optional<std::vector<RichText::MarkdownRange>> RichTextEditor::GetPreparedCaretWordSourceRanges(const ControlHost& host) const noexcept
{
    try
    {
        if (! LayoutIsCurrent(host))
            return std::nullopt;
        const auto& projection = _state->layout;
        const auto source = Snapshot();
        const auto displayLength = projection.DisplayLength();
        const auto displayCaret = projection.ToDisplay(GetSelection().caret);
        const auto word = RichText::Detail::PreparedDisplayWordRange(displayLength, displayCaret, [&](size_t index) { return projection.DisplayUnit(index); });
        if (word.first == word.last)
            return std::optional<std::vector<RichText::MarkdownRange>>(std::in_place, 0u);
        if (const auto* semantic = projection.SemanticGeometry())
        {
            const auto ranges = semantic->Document().Map().SourceRanges({word.first, word.last});
            for (const auto range : ranges)
                if (! source.IsBoundary(range.first) || ! source.IsBoundary(range.last))
                    return std::nullopt;
            return std::optional<std::vector<RichText::MarkdownRange>>(std::in_place, ranges.begin(), ranges.end());
        }
        std::vector<RichText::MarkdownRange> ranges(0u);
        for (size_t index = word.first; index < word.last; ++index)
        {
            const auto first = projection.ToSource(index);
            const auto last = projection.ToSource(index + 1u, true);
            if (first > last || (! ranges.empty() && first < ranges.back().first))
                return std::nullopt;
            if (! ranges.empty() && first <= ranges.back().last)
                ranges.back().last = std::max(ranges.back().last, last);
            else if (first != last)
                ranges.push_back({first, last});
        }
        for (const auto range : ranges)
            if (! source.IsBoundary(range.first) || ! source.IsBoundary(range.last))
                return std::nullopt;
        return std::optional<std::vector<RichText::MarkdownRange>>(std::in_place, ranges.begin(), ranges.end());
    }
    catch (const std::exception&)
    {
        // Prepared geometry can retire or temporary storage can fail; publish no range.
        return std::nullopt;
    }
}

std::optional<std::vector<std::pair<size_t, size_t>>> RichTextEditor::GetSourceVisualLineRanges(const ControlHost& host) const noexcept
{
    try
    {
        return SourceVisualLineRangesFallible(host);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
std::optional<std::vector<std::pair<size_t, size_t>>> RichTextEditor::SourceVisualLineRangesFallible(const ControlHost& host) const
{
    RequireCurrentLayout(host);
    const auto& projection = _state->layout;
    std::vector<std::pair<size_t, size_t>> ranges(0u);
    if (const auto* semantic = projection.SemanticGeometry())
    {
        const auto lines = semantic->SourceLines();
        return std::optional<std::vector<std::pair<size_t, size_t>>>(std::in_place, lines.begin(), lines.end());
    }
    size_t displayStart = 0;
    size_t sourceStart  = 0;
    do
    {
        size_t displayEnd = _state->layout.LineBoundary(displayStart, true);
        if (displayEnd < projection.DisplayLength() && projection.DisplayUnit(displayEnd) == L'\n')
            ++displayEnd;
        const size_t sourceEnd = displayEnd == projection.DisplayLength() ? projection.SourceLength() : projection.SourceBoundary(displayEnd);
        ranges.emplace_back(sourceStart, sourceEnd);
        if (displayEnd >= projection.DisplayLength() || displayEnd <= displayStart)
        {
            if (projection.DisplayLength() && projection.DisplayUnit(projection.DisplayLength() - 1u) == L'\n')
                ranges.emplace_back(projection.SourceLength(), projection.SourceLength());
            break;
        }
        sourceStart  = sourceEnd;
        displayStart = displayEnd;
    } while (true);
    return std::optional<std::vector<std::pair<size_t, size_t>>>(std::in_place, ranges);
}
bool RichTextEditor::ScrollSourceRangeIntoView(ControlHost& host, size_t sourceStart, size_t sourceEnd, bool alignToTop) noexcept
{
    try
    {
        const auto source = Snapshot();
        if (GetHost() != &host || ! IsEnabled() || sourceStart > sourceEnd || ! source.IsBoundary(sourceStart) || ! source.IsBoundary(sourceEnd))
            return false;
        // Scrolling is a geometry query too, but pending owner preparation is
        // ordinary control flow here. Do not throw/catch for the expected
        // not-current case; only a prepared current layout can reveal a range.
        if (! LayoutIsCurrent(host))
            return false;
        const auto& projection = _state->layout;
        const auto rectangles  = projection.SourceRange(sourceStart, sourceEnd);
        // Hidden-only syntax has no painted extent to reveal. Do not jump to
        // an offscreen marker's anchor or fabricate a visible rectangle.
        if (sourceStart != sourceEnd && rectangles.empty())
            return true;
        const auto caret = _state->layout.SourceCaret(sourceStart, sourceStart == GetSelection().caret ? _state->sourceCaretSite : RichText::MarkdownNoNode);
        float top        = caret.y;
        float bottom     = caret.y + caret.height;
        if (! rectangles.empty())
        {
            top    = rectangles.front().top;
            bottom = rectangles.front().bottom;
            for (const auto& rectangle : rectangles)
            {
                top    = std::min(top, rectangle.top);
                bottom = std::max(bottom, rectangle.bottom);
            }
        }
        const auto viewport = TextViewport();
        const float height  = std::max(0.0f, viewport.bottom - viewport.top);
        const float offset  = std::clamp(alignToTop ? top : bottom - height, 0.0f, std::max(0.0f, _state->layout.Height() - height));
        return SetScrollOffsetDip(offset);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
std::optional<std::pair<size_t, size_t>> RichTextEditor::GetVisibleSourceRange(const ControlHost& host) const noexcept
{
    if (GetHost() != &host || ! GetTextInputViewportRect())
        return std::nullopt;
    try
    {
        RequireCurrentLayout(host);
        const auto viewport = TextViewport();
        return _state->layout.VisibleSourceRange(_state->scrollDip, _state->scrollDip + viewport.bottom - viewport.top);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
std::optional<D2D1_RECT_F> RichTextEditor::GetTextInputViewportRect() const noexcept
{
    const auto viewport = TextViewport();
    if (! IsVisible() || viewport.right <= viewport.left || viewport.bottom <= viewport.top)
        return std::nullopt;
    return viewport;
}
std::optional<D2D1_RECT_F> RichTextEditor::GetTextInputCaretRect(const ControlHost& host, size_t sourceIndex) const noexcept
{
    if (GetHost() != &host || ! GetTextInputViewportRect() || ! Snapshot().IsBoundary(sourceIndex))
        return std::nullopt;
    try
    {
        RequireCurrentLayout(host);
        const auto caret    = _state->layout.SourceCaret(sourceIndex, sourceIndex == GetSelection().caret ? _state->sourceCaretSite : RichText::MarkdownNoNode);
        const auto viewport = TextViewport();
        // Keep the un-clipped caret for TSF's clipped flag; painting and native
        // screen transport clip it to the same prepared text viewport.
        return D2D1::RectF(viewport.left + HorizontalTranslation() + caret.x,
                           viewport.top + caret.y - _state->scrollDip,
                           viewport.left + HorizontalTranslation() + caret.x + 1,
                           viewport.top + caret.y + caret.height - _state->scrollDip);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
std::optional<std::vector<D2D1_RECT_F>> RichTextEditor::GetTextInputRangeRects(const ControlHost& host, size_t sourceStart, size_t sourceEnd) const
{
    if (GetHost() != &host || ! GetTextInputViewportRect() || sourceStart > sourceEnd || ! Snapshot().IsBoundary(sourceStart) ||
        ! Snapshot().IsBoundary(sourceEnd))
        return std::nullopt;
    try
    {
        RequireCurrentLayout(host);
        const auto& projection = _state->layout;
        const auto viewport    = TextViewport();
        auto rectangles        = projection.SourceRange(sourceStart, sourceEnd);
        for (auto& rectangle : rectangles)
        {
            rectangle.left += viewport.left + HorizontalTranslation();
            rectangle.right += viewport.left + HorizontalTranslation();
            rectangle.top += viewport.top - _state->scrollDip;
            rectangle.bottom += viewport.top - _state->scrollDip;
        }
        // Service transport needs the raw extents to report clipping truthfully.
        // UIA clips these separately and keeps hidden delimiters pixel-empty.
        // Construct the contained vector fallibly in place. Converting a vector
        // temporary into optional would invoke MSVC Debug's noexcept move.
        return std::optional<std::vector<D2D1_RECT_F>>(std::in_place, rectangles);
    }
    catch (const std::bad_alloc&)
    {
        // Fallible geometry propagates to its TSF/UIA boundary, which must
        // distinguish resource exhaustion from a stale/unavailable layout.
        throw;
    }
    catch (const std::exception&)
    {
        // Invalid or unavailable geometry remains a layout refusal.
        return std::nullopt;
    }
}
std::optional<size_t> RichTextEditor::HitTestTextInputPoint(const ControlHost& host, D2D1_POINT_2F point) const noexcept
{
    if (GetHost() != &host || ! GetTextInputViewportRect() || ! std::isfinite(point.x) || ! std::isfinite(point.y))
        return std::nullopt;
    try
    {
        RequireCurrentLayout(host);
        const auto viewport = TextViewport();
        return _state->layout.HitTestSource(point.x - viewport.left - HorizontalTranslation(), point.y - viewport.top + _state->scrollDip).position;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
bool RichTextEditor::ExportTextInputState(TextInputState& state) const
{
    state                = {};
    state.text           = GetText();
    const auto selection = GetSelection();
    state.caretIndex     = selection.caret;
    if (selection.anchor != selection.caret)
        state.selectionAnchorIndex = selection.anchor;
    state.readOnly  = ! Editable();
    state.multiline = true;
    return true;
}
bool RichTextEditor::ImportTextInputState(ControlHost& host, const TextInputState& state, bool notifyChange)
{
    if (GetHost() != &host || state.masked || ! state.multiline || (notifyChange && ! Editable()))
        return false;
    const RichText::Selection selection{state.selectionAnchorIndex.value_or(state.caretIndex), state.caretIndex};
    // Hosts restore entry source before a notifying commit. Preview updates,
    // cancellation and selection-only imports preserve both undo and redo.
    const auto previousSelection = GetSelection();
    bool changed;
    try
    {
        changed = notifyChange ? _state->document.ReplaceWholeText(state.text, selection) : _state->document.ReplaceTransient(state.text, selection);
    }
    catch (const std::logic_error&)
    {
        // Malformed service text, oversized input and split scalar/CRLF ACPs
        // are rejected atomically, without escaping a native noexcept handler.
        return false;
    }
    _state->dragging = false;
    if (changed || selection != previousSelection)
        _state->sourceCaretSite = RichText::MarkdownNoNode;
    if (notifyChange)
        Changed(changed, false);
    else
    {
        if (changed)
        {
            _state->highlights.clear();
            _state->decorations.clear();
            _state->decorationStatus = RichTextDecorationStatus::None;
            _state->images.Cancel();
            UpdateSemanticSourceRegion();
            RetireSemanticAccessibility();
            _state->nativeGeometryRejected.reset();
            RetireLayout();
        }
        SelectionChanged(false);
    }
    return true;
}
void RichTextEditor::Paint(ControlHost& host) const
{
    // Hosts can paint a concrete control directly as their root. A hidden root
    // must not recreate the layout retired by OnHidden or expose its text.
    if (! IsVisible())
        return;
    auto* context = host.GetDeviceContext();
    if (! context)
        THROW_HR(E_UNEXPECTED);
    auto style             = ResolveTextFieldVisualStyle(host.GetTheme(), IsEnabled(), IsHovered(), HasFocus(), HasFocus() && host.IsKeyboardFocusVisible());
    const auto& appearance = _state->appearance;
    if (appearance.blendWithWindowBackground)
        style.fill = host.GetTheme().windowBackground;
    if (appearance.drawBorder)
        DrawRoundedRect(host, GetBounds(), style.fill, style.border, 4.0f);
    else
        context->FillRectangle(GetBounds(), host.GetSolidBrush(style.fill));
    const bool showFocusIndicator = appearance.drawFocusIndicator || host.GetTheme().highContrast;
    if (showFocusIndicator && ! appearance.drawBorder && (style.showFocus || (host.GetTheme().highContrast && HasFocus())))
    {
        const auto bounds = GetBounds();
        context->DrawRectangle(D2D1::RectF(bounds.left + 1.0f, bounds.top + 1.0f, bounds.right - 1.0f, bounds.bottom - 1.0f),
                               host.GetSolidBrush(host.GetTheme().focusStroke),
                               1.0f);
    }
    if (showFocusIndicator && style.showFocus && appearance.drawBorder)
        context->DrawLine(D2D1::Point2F(GetBounds().left + 4, GetBounds().bottom - 1),
                          D2D1::Point2F(GetBounds().right - 4, GetBounds().bottom - 1),
                          host.GetSolidBrush(host.GetTheme().accent),
                          2.0f);
    const auto viewport = TextViewport();
    if (viewport.right <= viewport.left || viewport.bottom <= viewport.top)
        return;
    RequireCurrentLayout(host);
    auto& state               = *_state;
    auto* textBrush           = host.GetSolidBrush(style.text);
    auto* selectedTextBrush   = host.GetSolidBrush(style.selectionText);
    auto* selectionBrush      = host.GetSolidBrush(style.selectionFill);
    auto* caretBrush          = host.GetSolidBrush(style.caret);
    const bool subtleMarkdown = state.mode == RichText::ViewMode::Rich && appearance.markdownPresentation == RichText::MarkdownPresentationStyle::Subtle;
    auto* semanticDecoration  = subtleMarkdown ? host.GetSolidBrush(host.GetTheme().highContrast ? style.text : host.GetTheme().borderDefault) : caretBrush;
    auto* quoteFill           = subtleMarkdown ? host.GetSolidBrush(ResolveMarkdownQuoteFill(host.GetTheme(), style)) : nullptr;
    ID2D1Brush* diagramFill   = nullptr;
    ID2D1Brush* selectedDiagramFill = nullptr;
    if (const auto* semantic = state.layout.SemanticGeometry(); semantic && (semantic->HasNativeObjects() || subtleMarkdown || semantic->HasMarkedText()))
    {
        auto opaque = style.fill;
        opaque.a    = 1.0f;
        diagramFill = host.GetSolidBrush(opaque);
        // Diagram nodes and crossing-label backing are opaque. Resolve the
        // selection surface first so their final pass cannot cover the
        // selected background with the ordinary input surface.
        selectedDiagramFill = host.GetSolidBrush(CompositeOverBackground(style.selectionFill, opaque));
    }
    const auto selection   = state.document.GetSelection();
    const auto& projection = state.layout;
    const std::span<const D2D1_RECT_F> rectangles =
        state.overlaySelectionDirty ? std::span<const D2D1_RECT_F>{} : std::span<const D2D1_RECT_F>(state.selectionRectangles);
    const auto origin = D2D1::Point2F(viewport.left + HorizontalTranslation(), viewport.top - state.scrollDip);
    {
        context->PushAxisAlignedClip(viewport, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto restoreClip                                        = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
        const std::span<const State::DecorationRectangle> decorations = state.overlayHighlightsDirty
                                                                            ? std::span<const State::DecorationRectangle>{}
                                                                            : std::span<const State::DecorationRectangle>(state.decorationRectangles);
        const float visibleBottom                                     = state.scrollDip + viewport.bottom - viewport.top;
        if (! state.overlayHighlightsDirty)
            state.decorationPaintIndex.ForEach(state.scrollDip,
                                               visibleBottom,
                                               [&](size_t index)
            {
                const auto& decoration = decorations[index];
                if (decoration.kind != RichTextDecorationKind::Background)
                    return;
                auto color = DecorationColor(host.GetTheme(), decoration.role);
                if (! host.GetTheme().highContrast)
                    color.a *= .18f;
                auto* brush           = host.GetSolidBrush(color);
                const auto& rectangle = decoration.rectangle;
                const auto bounds = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
                if (host.GetTheme().highContrast)
                    context->DrawRectangle(bounds, brush);
                else
                    context->FillRectangle(bounds, brush);
            });
        const std::span<const State::HighlightRectangle> highlights =
            state.overlayHighlightsDirty ? std::span<const State::HighlightRectangle>{} : std::span<const State::HighlightRectangle>(state.highlightRectangles);
        const auto firstHighlight = state.overlayHighlightsDirty
                                        ? 0u
                                        : static_cast<size_t>(std::upper_bound(state.highlightBottoms.begin(), state.highlightBottoms.end(), state.scrollDip) -
                                                              state.highlightBottoms.begin());
        for (size_t highlightIndex = firstHighlight; highlightIndex < highlights.size(); ++highlightIndex)
        {
            const auto& highlight = highlights[highlightIndex];
            if (highlight.rectangle.top >= state.scrollDip + viewport.bottom - viewport.top)
                break;
            auto color = host.GetTheme().selectionFill;
            color.a *= host.GetTheme().highContrast ? 1.0f : highlight.current ? 0.38f : 0.18f;
            auto* brush           = host.GetSolidBrush(color);
            const auto& rectangle = highlight.rectangle;
            const auto bounds     = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
            if (host.GetTheme().highContrast)
                context->DrawRectangle(bounds, brush, highlight.current ? 2.0f : 1.0f);
            else
                context->FillRectangle(bounds, brush);
        }
        RichText::LinkDrawing links;
        if (const auto* semantic = state.layout.SemanticGeometry(); semantic && semantic->HasMarkedText())
        {
            links.markedTextFill    = host.GetSolidBrush(ResolveMarkdownQuoteFill(host.GetTheme(), style));
            links.outlineMarkedText = host.GetTheme().highContrast;
        }
        links.brush     = appearance.useAccentLinkColor && IsEnabled() ? host.GetSolidBrush(host.GetTheme().accent) : nullptr;
        links.underline = state.linkUnderlinePolicy == RichTextLinkUnderlinePolicy::Always || host.GetTheme().highContrast;
        links.hover     = state.hoveredLink;
        links.caret     = CaretLinkDisplayRange();
        if (state.mode == RichText::ViewMode::Source && appearance.colorizeMarkdownSource && ! host.GetTheme().highContrast)
        {
            links.inkRanges              = state.sourceInk.Ranges();
            links.directSourceInkPreview = appearance.directSourceInkPreview;
            // Keep the ordinary first pass beneath true selection. Translucent
            // selection fill and antialiased selected glyph edges must never retain
            // colored syntax ink from underneath their selected-text pass.
            if (selection.anchor != selection.caret)
                links.inkExclusion = RichText::MarkdownRange{projection.ToDisplay(selection.Start()), projection.ToDisplay(selection.End())};
            links.inkBrushes = {
                host.GetSolidBrush(host.GetTheme().subduedText), host.GetSolidBrush(host.GetTheme().accent), host.GetSolidBrush(host.GetTheme().accent)};
        }
        if (state.mode == RichText::ViewMode::Rich && state.codeInkComplete && state.codeInkDocument == GetSemanticDocument() && ! host.GetTheme().highContrast)
        {
            links.inkRanges = state.codeInk;
            if (selection.anchor != selection.caret)
                links.inkExclusion = RichText::MarkdownRange{projection.ToDisplay(selection.Start()), projection.ToDisplay(selection.End())};
            links.inkBrushes = {
                host.GetSolidBrush(host.GetTheme().subduedText), host.GetSolidBrush(host.GetTheme().accent), host.GetSolidBrush(host.GetTheme().accent)};
        }
        if (state.mode == RichText::ViewMode::Source && appearance.sourceWhitespaceMarks &&
            state.sourceGuides.Current(Revision(), state.layout.Generation(), appearance.sourceLineNumbers, true))
        {
            auto* guideBrush = host.GetSolidBrush(host.GetTheme().highContrast ? style.text : host.GetTheme().subduedText);
            const auto marks = state.sourceGuides.Marks();
            auto first       = std::lower_bound(marks.begin(), marks.end(), state.scrollDip, [](const auto& mark, float top) { return mark.y < top; });
            while (first != marks.begin() && (first - 1)->y + (first - 1)->height > state.scrollDip)
                --first;
            for (auto item = first; item != marks.end() && item->y < state.scrollDip + viewport.bottom - viewport.top; ++item)
            {
                const auto& mark = *item;
                const float x = origin.x + mark.x, y = origin.y + mark.y + mark.height * 0.55f;
                if (y < viewport.top || y >= viewport.bottom || x < viewport.left - 16.0f || x > viewport.right + 16.0f)
                    continue;
                if (mark.kind == L' ')
                    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + mark.width * 0.5f, y), 1.0f, 1.0f), guideBrush);
                else
                {
                    const float end = x + (mark.kind == L'\t' ? std::max(3.0f, mark.width - 2.0f) : 5.0f);
                    context->DrawLine(D2D1::Point2F(x + 1.0f, y), D2D1::Point2F(end, y), guideBrush);
                    context->DrawLine(D2D1::Point2F(end - 2.0f, y - 2.0f), D2D1::Point2F(end, y), guideBrush);
                    context->DrawLine(D2D1::Point2F(end - 2.0f, y + 2.0f), D2D1::Point2F(end, y), guideBrush);
                }
            }
        }
        const bool dimmed = state.focusMode && state.mode == RichText::ViewMode::Rich && ! host.GetTheme().highContrast && IsEnabled();
        auto* subdued     = dimmed ? host.GetSolidBrush(host.GetTheme().subduedText) : textBrush;
        if (dimmed)
        {
            links.emphasis          = ActiveParagraphDisplayRange();
            links.emphasisBrush     = textBrush;
            links.inactiveLinkBrush = subdued;
        }
        const bool decorated =
            links.markedTextFill || ! links.inkRanges.empty() || dimmed || (state.mode == RichText::ViewMode::Rich && (links.brush || ! links.underline));
        state.layout.Draw(context,
                          subdued,
                          dimmed ? subdued : semanticDecoration,
                          origin,
                          state.scrollDip,
                          state.scrollDip + viewport.bottom - viewport.top,
                          diagramFill,
                          1.0f,
                          decorated ? &links : nullptr,
                          quoteFill);
        if (! state.overlayHighlightsDirty)
            state.decorationPaintIndex.ForEach(state.scrollDip,
                                               visibleBottom,
                                               [&](size_t index)
            {
                const auto& decoration = decorations[index];
                if (decoration.kind == RichTextDecorationKind::Background)
                    return;
                const auto& rectangle = decoration.rectangle;
                const auto bounds = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
                if (bounds.bottom + 3.0f <= viewport.top || bounds.top - 3.0f >= viewport.bottom || bounds.right + 3.0f <= viewport.left ||
                    bounds.left - 3.0f >= viewport.right)
                    return;
                auto* brush = host.GetSolidBrush(DecorationColor(host.GetTheme(), decoration.role));
                if (decoration.kind == RichTextDecorationKind::Foreground)
                {
                    context->PushAxisAlignedClip(bounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    const auto restoreDecorationClip = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
                    state.layout.Draw(context, brush, brush, origin, rectangle.top, rectangle.bottom, diagramFill, 1.0f, nullptr, quoteFill);
                }
                else if (decoration.kind == RichTextDecorationKind::Underline)
                    context->DrawLine(D2D1::Point2F(bounds.left, bounds.bottom - 1.0f), D2D1::Point2F(bounds.right, bounds.bottom - 1.0f), brush, 1.0f);
                else
                {
                    const float first = std::max(bounds.left, viewport.left), last = std::min(bounds.right, viewport.right);
                    const float step = std::max(3.0f, (last - first) / 1024.0f);
                    for (unsigned i = 0u; i < 1024u; ++i)
                    {
                        const float x = first + static_cast<float>(i) * step;
                        if (x >= last)
                            break;
                        context->DrawLine(D2D1::Point2F(x, bounds.bottom - 1.0f), D2D1::Point2F(std::min(last, x + step), bounds.bottom - 3.0f), brush, 1.0f);
                    }
                }
            });
        auto selectedLinks              = links;
        selectedLinks.brush             = selectedTextBrush;
        selectedLinks.emphasisBrush     = selectedTextBrush;
        selectedLinks.inactiveLinkBrush = selectedTextBrush;
        selectedLinks.inkBrushes.fill(selectedTextBrush);
        selectedLinks.markedTextFill = selectedDiagramFill;
        bool selectionVisible        = false;
        for (const auto& rectangle : rectangles)
        {
            const auto translated = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
            if (translated.bottom <= viewport.top || translated.top >= viewport.bottom)
                continue;
            context->FillRectangle(translated, selectionBrush);
            selectionVisible = true;
            if (state.selectionMask)
                continue;
            context->PushAxisAlignedClip(translated, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            const auto restoreSelectionClip = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
            state.layout.Draw(context,
                              selectedTextBrush,
                              selectedTextBrush,
                              origin,
                              rectangle.top,
                              rectangle.bottom,
                              selectedDiagramFill,
                              0.65f,
                              decorated ? &selectedLinks : nullptr,
                              subtleMarkdown ? selectedDiagramFill : nullptr);
        }
        if (selectionVisible && state.selectionMask && state.selectionLayer)
        {
            const auto parameters = D2D1::LayerParameters1(viewport,
                                                           state.selectionMask.get(),
                                                           D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                           D2D1::Matrix3x2F::Translation(origin.x, origin.y),
                                                           1.0f,
                                                           nullptr,
                                                           D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND);
            context->PushLayer(parameters, state.selectionLayer.get());
            const auto restoreSelectionLayer = wil::scope_exit([context]() noexcept { context->PopLayer(); });
            state.layout.Draw(context,
                              selectedTextBrush,
                              selectedTextBrush,
                              origin,
                              state.scrollDip,
                              state.scrollDip + viewport.bottom - viewport.top,
                              selectedDiagramFill,
                              0.65f,
                              decorated ? &selectedLinks : nullptr,
                              subtleMarkdown ? selectedDiagramFill : nullptr);
        }
        if (! state.overlaySelectionDirty)
        {
            const auto underline = [&](std::span<const D2D1_RECT_F> ranges, float thickness)
            {
                for (const auto& rect : ranges)
                    context->DrawLine(D2D1::Point2F(rect.left + origin.x, rect.bottom + origin.y - 1),
                                      D2D1::Point2F(rect.right + origin.x, rect.bottom + origin.y - 1),
                                      caretBrush,
                                      thickness);
            };
            underline(state.compositionRectangles, 1.0f);
            underline(state.conversionRectangles, 2.0f);
        }
        if (HasFocus() && IsEnabled() && state.keyboardNavigationElement != RichText::MarkdownNoNode &&
            state.keyboardNavigationDocument.lock() == GetAcknowledgedSemanticDocument(host))
            if (const auto bounds = GetSemanticElementBounds(host, state.keyboardNavigationElement))
                context->DrawRectangle(*bounds, host.GetSolidBrush(host.GetTheme().focusStroke), 1.0f);
        if (HasFocus() && IsEnabled() && state.caretVisible && state.keyboardNavigationElement == RichText::MarkdownNoNode)
        {
            const auto caret = state.layout.SourceCaret(selection.caret, state.sourceCaretSite);
            const float x    = std::clamp(origin.x + caret.x, viewport.left, std::max(viewport.left, viewport.right - 1.0f));
            context->DrawLine(D2D1::Point2F(x, origin.y + caret.y), D2D1::Point2F(x, origin.y + caret.y + caret.height), caretBrush, 1.0f);
        }
    }
    if (state.mode == RichText::ViewMode::Source && appearance.sourceLineNumbers &&
        state.sourceGuides.Current(Revision(), state.layout.Generation(), true, appearance.sourceWhitespaceMarks))
    {
        const auto outer       = GetBounds();
        const auto gutterWidth = std::min(SourceGutterWidth(), std::max(0.0f, viewport.right - viewport.left - 1.0f));
        const auto gutter = IsRightToLeft() ? D2D1::RectF(viewport.right, viewport.top, std::min(outer.right, viewport.right + gutterWidth), viewport.bottom)
                                            : D2D1::RectF(std::max(outer.left, viewport.left - gutterWidth), viewport.top, viewport.left, viewport.bottom);
        context->PushAxisAlignedClip(gutter, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto restoreGutter = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
        auto* brush              = host.GetSolidBrush(host.GetTheme().highContrast ? style.text : host.GetTheme().subduedText);
        const auto numbers       = state.sourceGuides.Numbers();
        auto first = std::lower_bound(numbers.begin(), numbers.end(), state.scrollDip, [](const auto& number, float top) { return number.y < top; });
        while (first != numbers.begin() && (first - 1)->y + (first - 1)->layout->Height() > state.scrollDip)
            --first;
        for (auto item = first; item != numbers.end() && item->y < state.scrollDip + viewport.bottom - viewport.top; ++item)
            item->layout->Draw(context, brush, D2D1::Point2F(gutter.left + 2.0f, viewport.top + item->y - state.scrollDip));
    }
    const float height = std::max(0.0f, viewport.bottom - viewport.top);
    const float extent = std::max(0.0f, state.layout.Height() - height);
    if (appearance.showVerticalScrollbar && extent > 0.0f)
    {
        const auto track = ScrollbarTrack();
        const auto thumb = ComputeScrollbarThumbRect(track, ScrollbarOrientation::Vertical, height, state.layout.Height(), state.scrollDip, extent);
        PaintScrollbar(host, track, thumb, ResolveScrollbarVisuals(host.GetTheme(), false, false, state.scrollbarDragging));
    }
}
bool RichTextEditor::Tick(ControlHost& host, uint64_t nowTickMs)
{
    auto& state = *_state;
    if (! HasFocus() || ! IsVisible() || ! Editable() || host.GetTheme().reducedMotion)
    {
        const bool changed = ! state.caretVisible;
        state.caretVisible = true;
        state.blinkEpoch.reset();
        if (changed)
            RequestInvalidate();
        return false;
    }
    // The application supplies the embedded epoch, including zero. No uptime,
    // private timer, or thread is introduced by this control.
    if (! state.blinkEpoch || nowTickMs < *state.blinkEpoch)
        state.blinkEpoch = nowTickMs;
    const bool visible = ((nowTickMs - *state.blinkEpoch) / 500u) % 2u == 0;
    if (visible != state.caretVisible)
    {
        state.caretVisible = visible;
        RequestInvalidate();
    }
    return true;
}
size_t RichTextEditor::HitSource(ControlHost& host, D2D1_POINT_2F point) const
{
    RequireCurrentLayout(host);
    const auto viewport     = TextViewport();
    const auto hit          = _state->layout.HitTestSource(point.x - viewport.left - HorizontalTranslation(), point.y - viewport.top + _state->scrollDip);
    _state->sourceCaretSite = hit.site;
    return hit.position;
}
void RichTextEditor::MoveCaret(ControlHost& host, UINT key, bool extend, bool byWord)
{
    if (! LayoutIsCurrent(host))
    {
        // Logical commands remain usable during reflow. Semantic preparation
        // falls back to exact source graphemes until its current map exists;
        // it must never apply the older renderer's grammar to another dialect.
        const auto selection = GetSelection();
        if (! extend && selection.Start() != selection.End() && (key == VK_LEFT || key == VK_RIGHT))
        {
            const auto source = key == VK_LEFT ? selection.Start() : selection.End();
            SetSelection({source, source});
            return;
        }
        if (key != VK_LEFT && key != VK_RIGHT && ! (byWord && (key == VK_HOME || key == VK_END)))
            return; // Wrapped/vertical movement needs published visual geometry.
        if (_state->markdownRendering || _state->mode == RichText::ViewMode::Source)
        {
            const auto source = Snapshot();
            size_t next{};
            if (key == VK_HOME)
                next = 0u;
            else if (key == VK_END)
                next = source.Length();
            else
            {
                using Operation = RichText::Detail::SourceWindowOperation;
                const auto operation =
                    key == VK_LEFT ? (byWord ? Operation::PreviousWord : Operation::Previous) : (byWord ? Operation::NextWord : Operation::Next);
                const auto range = RichText::Detail::SourceWindowRange(source, selection.caret, operation);
                next             = key == VK_LEFT ? range.first : range.last;
            }
            SetSelection({extend ? selection.anchor : next, next});
            return;
        }
        using Operation      = RichText::Detail::SourceWindowOperation;
        const auto operation = key == VK_HOME   ? Operation::Home
                               : key == VK_END  ? Operation::End
                               : key == VK_LEFT ? (byWord ? Operation::PreviousWord : Operation::Previous)
                                                : (byWord ? Operation::NextWord : Operation::Next);
        const auto range     = RichText::Detail::RichSourceWindowRange(Snapshot(), selection.caret, operation);
        const auto source    = key == VK_LEFT || key == VK_HOME ? range.first : range.last;
        SetSelection({extend ? selection.anchor : source, source});
        return;
    }
    auto& state            = *_state;
    const auto selection   = GetSelection();
    const auto& projection = state.layout;
    const auto position    = projection.ToDisplay(selection.caret);
    size_t next            = position;
    bool trailing          = false;
    std::optional<float> preferredX;
    if (! extend && selection.Start() != selection.End() && (key == VK_LEFT || key == VK_RIGHT))
    {
        const auto source = key == VK_LEFT ? selection.Start() : selection.End();
        SetSelection({source, source});
        return;
    }
    if (const auto* geometry = state.layout.SemanticGeometry())
    {
        RichText::Detail::SourceCaretHit target{selection.caret, state.sourceCaretSite};
        if (byWord && (key == VK_HOME || key == VK_END))
            target = {key == VK_HOME ? 0u : state.document.Length()};
        else if (! byWord && (key == VK_LEFT || key == VK_RIGHT))
            target = geometry->StepSource(selection.caret, state.sourceCaretSite, key == VK_RIGHT);
        else if (key == VK_HOME || key == VK_END)
            target = geometry->SourceLineBoundary(selection.caret, state.sourceCaretSite, key == VK_END);
        else if (key != VK_LEFT && key != VK_RIGHT)
        {
            const auto caret     = geometry->SourceCaret(selection.caret, state.sourceCaretSite);
            preferredX           = state.preferredCaretX.value_or(caret.x);
            const bool down      = key == VK_DOWN || key == VK_NEXT;
            const bool page      = key == VK_NEXT || key == VK_PRIOR;
            const float distance = page ? std::max(caret.height, TextViewport().bottom - TextViewport().top) : caret.height;
            target               = geometry->HitTestSource(*preferredX, caret.y + caret.height * .5f + (down ? distance : -distance));
            // Ordinary wrapped lines keep their existing SDK hit path. Only
            // a same-row tie must cross an explicitly requested larger gap.
            if (! page && std::abs(geometry->SourceCaret(target.position, target.site).y - caret.y) < .01f)
            {
                if (state.appearance.paragraphSpacingAfterLines || state.appearance.listItemSpacingDip || state.appearance.semanticSpacing)
                    target = geometry->AdjacentSourceLine(selection.caret, state.sourceCaretSite, preferredX.value(), down);
                else
                    target = geometry->HitTestSource(*preferredX, caret.y + caret.height * .5f + (down ? distance + 12.0f : -distance - 12.0f));
            }
        }
        else
        {
            const auto display = byWord && key == VK_LEFT ? FindPreviousWordBoundary(projection.DisplayText(), position)
                                                          : FindNextWordBoundary(projection.DisplayText(), position);
            const auto raw     = projection.ToSource(display);
            target             = {key == VK_LEFT ? std::min(raw, selection.caret) : std::max(raw, selection.caret)};
        }
        state.document.SetSelection({extend ? selection.anchor : target.position, target.position});
        state.sourceCaretSite = target.site;
        state.preferredCaretX = preferredX;
        SelectionChanged(true, true);
        return;
    }
    switch (key)
    {
        case VK_LEFT: next = byWord ? FindPreviousWordBoundary(projection.DisplayText(), position) : state.layout.PreviousCluster(position); break;
        case VK_RIGHT:
            next = byWord ? FindNextWordBoundary(projection.DisplayText(), position) : state.layout.NextCluster(position);
            // At a semantic boundary, leading affinity belongs to the next
            // content span. Trailing would select a generated separator's old
            // source anchor and leave Right stuck at the preceding paragraph.
            trailing = ! projection.Semantic();
            break;
        case VK_HOME: next = byWord ? 0 : state.layout.LineBoundary(position, false); break;
        case VK_END:
            next     = byWord ? projection.DisplayLength() : state.layout.LineBoundary(position, true);
            trailing = true;
            break;
        default:
        {
            const auto caret     = state.layout.Caret(position);
            preferredX           = state.preferredCaretX.value_or(caret.x);
            const bool down      = key == VK_DOWN || key == VK_NEXT;
            const float distance = (key == VK_NEXT || key == VK_PRIOR) ? std::max(caret.height, TextViewport().bottom - TextViewport().top) : caret.height;
            const auto hit       = state.layout.HitTest(*preferredX, caret.y + caret.height * 0.5f + (down ? distance : -distance));
            next                 = hit.position;
            trailing             = hit.trailing;
            break;
        }
    }
    const auto source = byWord && key == VK_HOME ? 0u : byWord && key == VK_END ? state.document.Length() : projection.ToSource(next, trailing);
    state.document.SetSelection({extend ? selection.anchor : source, source});
    state.preferredCaretX = preferredX;
    SelectionChanged(true, true);
}
bool RichTextEditor::Erase(ControlHost& host, bool backwards, bool byWord, bool coalesceHistory)
{
    if (! Editable())
        return false;
    const auto selection = GetSelection();
    if (_state->sourceEditing || GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback)
    {
        // A view-capacity rejection can retain valid CPU semantics while the
        // visible layout is exact source. Never use that hidden atomic-object
        // map for Delete/Backspace in the source fallback presentation.
        const auto source = Snapshot();
        auto first = selection.Start(), last = selection.End();
        if (first == last)
        {
            const auto text = source.Text();
            if (byWord)
            {
                first = backwards ? FindPreviousWordBoundary(text, selection.caret) : selection.caret;
                last  = backwards ? selection.caret : FindNextWordBoundary(text, selection.caret);
                if (_state->sourceEditing)
                {
                    first = std::max(first, _state->sourceEditing->region.source.first);
                    last  = std::min(last, _state->sourceEditing->region.source.last);
                }
            }
            else
            {
                const auto range = GetTextElementDeletionRange(text, selection.caret, backwards);
                first            = range.first;
                last             = range.last;
            }
        }
        const bool changed = _state->document.ReplaceImpl(first, last, {}, {first, first}, true, coalesceHistory);
        Changed(changed);
        return changed;
    }
    if (selection.Start() != selection.End())
    {
        if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
            if (const auto syntax = GetMarkdownSyntax())
            {
                const std::array ranges{RichText::MarkdownRange{selection.Start(), selection.End()}};
                const auto plan = RichText::BuildMarkdownDeletion(Snapshot(), Revision(), syntax, selection, ranges);
                return ApplyMarkdownCommand(plan) == RichText::MarkdownCommandStatus::Changed;
            }
        return ReplaceSelection({});
    }
    if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
        if (const auto document = GetSemanticDocument())
        {
            const auto& map = document->Map();
            for (const auto& node : document->Syntax()->Nodes())
            {
                const bool heading =
                    node.block.kind == RichText::MarkdownBlockKind::AtxHeading || node.block.kind == RichText::MarkdownBlockKind::SetextHeading;
                if ((node.block.kind != RichText::MarkdownBlockKind::Paragraph && ! (backwards && heading)) || selection.caret < node.block.content.first ||
                    selection.caret > node.block.content.last)
                    continue;
                const auto edge    = backwards && node.firstInline != RichText::MarkdownNoNode ? document->Syntax()->Inlines()[node.firstInline].source.first
                                     : backwards                                               ? node.block.content.first
                                                                                               : node.block.content.last;
                const auto visible = map.ToSource(map.ToDisplay(edge), backwards ? RichText::MappingAffinity::Leading : RichText::MappingAffinity::Trailing);
                if (selection.caret != visible)
                    continue;
                RichText::MarkdownCommandRequest request;
                request.command = backwards ? RichText::MarkdownCommand::JoinBackward : RichText::MarkdownCommand::JoinForward;
                if (ExecuteMarkdownCommand(request) == RichText::MarkdownCommandStatus::Changed)
                    return true;
                break;
            }
        }
    if (LayoutIsCurrent(host))
        if (const auto* geometry = _state->layout.SemanticGeometry(); geometry && geometry->IsEmptySourceSite(selection.caret, _state->sourceCaretSite))
        {
            // A visual cell scaffold owns no raw token to delete. Structural
            // empty-cell erasure is not a column-deletion command. Empty physical lines
            // instead use exact raw graphemes, including indivisible CRLF.
            if (geometry->IsCellSourceSite(selection.caret, _state->sourceCaretSite))
                return false;
            const auto source = Snapshot();
            const auto text   = source.Text();
            const auto first =
                backwards ? (byWord ? FindPreviousWordBoundary(text, selection.caret) : StepToPreviousTextElement(text, selection.caret)) : selection.caret;
            const auto last =
                backwards ? selection.caret : (byWord ? FindNextWordBoundary(text, selection.caret) : StepToNextTextElement(text, selection.caret));
            const bool changed = _state->document.Replace(first, last, L"", {first, first});
            Changed(changed);
            return changed;
        }
    if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
    {
        if (const auto document = GetSemanticDocument())
        {
            const auto& map     = document->Map();
            const auto position = map.ToDisplay(selection.caret);
            std::pair<size_t, size_t> range;
            if (byWord)
                range = backwards ? std::pair{FindPreviousWordBoundary(map.Display(), position), position}
                                  : std::pair{position, FindNextWordBoundary(map.Display(), position)};
            else if (LayoutIsCurrent(host) && _state->layout.Semantic())
                range = _state->layout.DeletionRange(position, backwards);
            else
            {
                const auto element = GetTextElementDeletionRange(map.Display(), position, backwards);
                range              = {element.first, element.last};
            }
            // A raw source client may place its caret inside an entity/object.
            // Visual delete owns that complete token, in either direction.
            for (const auto& span : map.Spans())
                if (span.primary && (span.kind == RichText::DisplaySpanKind::Transformed || span.kind == RichText::DisplaySpanKind::Object) &&
                    span.source.first < selection.caret && selection.caret < span.source.last)
                {
                    range = {span.display.first, span.display.last};
                    break;
                }
            range.first       = map.SnapDisplayBoundary(range.first, RichText::MappingAffinity::Leading);
            range.second      = map.SnapDisplayBoundary(range.second, RichText::MappingAffinity::Trailing);
            const auto ranges = map.SourceRanges({range.first, range.second});
            if (ranges.empty())
                return false; // Generated separators need an explicit structural join command (M4e).
            const auto plan = RichText::BuildMarkdownDeletion(Snapshot(), Revision(), document->Syntax(), selection, ranges);
            // Only a single plain source erasure may join a typing run. Plans
            // that also remove markup or combine disjoint spans stay atomic.
            if (! byWord && plan.IsApplicable() && plan.Edits().size() == 1u && plan.Edits()[0].replacement.empty())
            {
                const auto& edit = plan.Edits()[0];
                const auto units = edit.source.last - edit.source.first;
                if (units <= 64u && IsHistoryWordElement(plan.ExpectedSource().Slice(edit.source.first, edit.source.last)))
                {
                    const auto composition = host.GetFocusControl() == this ? host.GetNativeTextCompositionActive(this) : std::optional<bool>{false};
                    if ((host.GetFocusControl() == this && host.HasActiveNativeTextInputSession() && ! composition) || composition.value_or(false))
                    {
                        _state->typingRun.reset();
                        return false;
                    }
                    const auto now      = GetTickCount64();
                    const auto previous = _state->typingRun;
                    const auto kind     = backwards ? State::TypingRun::Kind::Backspace : State::TypingRun::Kind::Delete;
                    const bool merge    = previous && previous->kind == kind && previous->revision == Revision() &&
                                          previous->selectionEpoch == _state->document.selectionEpoch_ && previous->caret == selection.caret &&
                                          now >= previous->lastTick && now - previous->lastTick <= 1000u && previous->units + units <= 64u &&
                                          _state->document.CanUndo();
                    const bool changed  = _state->document.ReplaceImpl(edit.source.first, edit.source.last, {}, plan.After(), true, merge);
                    if (changed)
                        _state->typingRun = State::TypingRun{
                            kind, Revision(), _state->document.selectionEpoch_, now, plan.After().caret, merge ? previous->units + units : units};
                    else
                        _state->typingRun.reset();
                    _state->sourceCaretSite = RichText::MarkdownNoNode;
                    Changed(changed);
                    return changed;
                }
            }
            _state->typingRun.reset();
            return ApplyMarkdownCommand(plan) == RichText::MarkdownCommandStatus::Changed;
        }
    }
    if (! LayoutIsCurrent(host))
    {
        if (_state->markdownRendering || _state->mode == RichText::ViewMode::Source)
        {
            using Operation = RichText::Detail::SourceWindowOperation;
            const auto operation =
                byWord ? (backwards ? Operation::PreviousWord : Operation::NextWord) : (backwards ? Operation::DeleteBackward : Operation::DeleteForward);
            const auto range = RichText::Detail::SourceWindowRange(Snapshot(), selection.caret, operation);
            if (range.first == range.last)
                return false;
            const bool changed = _state->document.ReplaceImpl(range.first, range.last, {}, {range.first, range.first}, true, coalesceHistory);
            Changed(changed);
            return changed;
        }
        using Operation = RichText::Detail::SourceWindowOperation;
        const auto operation =
            byWord ? (backwards ? Operation::PreviousWord : Operation::NextWord) : (backwards ? Operation::DeleteBackward : Operation::DeleteForward);
        const auto range = RichText::Detail::RichSourceWindowRange(Snapshot(), selection.caret, operation, true);
        if (range.first == range.last)
            return false;
        const bool changed = _state->document.ReplaceImpl(range.first, range.last, {}, {range.first, range.first}, true, coalesceHistory);
        Changed(changed);
        return changed;
    }
    auto& state              = *_state;
    const auto& projection   = state.layout;
    const auto position      = projection.ToDisplay(selection.caret);
    const auto [first, last] = byWord ? (backwards ? std::pair{FindPreviousWordBoundary(projection.DisplayText(), position), position}
                                                   : std::pair{position, FindNextWordBoundary(projection.DisplayText(), position)})
                                      : state.layout.DeletionRange(position, backwards);
    if (first == last)
        return false;
    const auto begin   = projection.ToSource(first);
    const auto end     = projection.ToSource(last, true);
    const bool changed = state.document.ReplaceImpl(begin, end, {}, {begin, begin}, true, coalesceHistory);
    Changed(changed);
    return changed;
}
bool RichTextEditor::InputFailureHandled(const std::weak_ptr<int>& lifetime, uint64_t revision, uint64_t epoch, RichText::Selection selection) const noexcept
{
    // Once publication/cancellation has crossed a callback boundary, a native
    // input remains handled even if its follow-up work failed. Precommit OOM
    // declines with the document's atomic transaction intact.
    return lifetime.expired() || Revision() != revision || _state->fileDropEpoch != epoch || GetSelection() != selection || _state->dragging ||
           _state->scrollbarDragging;
}
bool RichTextEditor::OnChar(ControlHost& host, wchar_t character, UINT modifiers) noexcept
{
    const auto inputLifetime  = GetLifetimeToken();
    const auto inputRevision  = Revision();
    const auto inputEpoch     = _state->fileDropEpoch;
    const auto inputSelection = GetSelection();
    const auto pendingLead    = _state->pendingLeadSurrogate;
    try
    {
        // AltGr arrives as Ctrl+Alt on Windows. Its Unicode WM_CHAR must remain
        // insertable, while an isolated command modifier must not type a shortcut.
        if (! Editable() || (ModifiersContainCtrl(modifiers) != ModifiersContainAlt(modifiers)))
        {
            _state->pendingLeadSurrogate = 0;
            return false;
        }
        if (IsUtf16LeadSurrogate(character))
        {
            _state->pendingLeadSurrogate = character;
            return true;
        }
        // MSVC Debug's noexcept empty-string constructor allocates an iterator
        // proxy. The size constructor lets this input boundary catch failure.
        std::wstring text(0u, L' ');
        if (IsUtf16TrailSurrogate(character))
        {
            const auto lead = std::exchange(_state->pendingLeadSurrogate, wchar_t{});
            if (! lead)
                return true;
            text = {lead, character};
        }
        else
        {
            _state->pendingLeadSurrogate = 0;
            if (character == L'\r')
            {
                if (! _state->sourceEditing && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
                {
                    RichText::MarkdownCommandRequest request;
                    request.command =
                        ModifiersContainShift(modifiers) ? RichText::MarkdownCommand::InsertHardBreak : RichText::MarkdownCommand::InsertParagraph;
                    if (! ModifiersContainShift(modifiers) && LayoutIsCurrent(host))
                        if (const auto* geometry = _state->layout.SemanticGeometry())
                        {
                            const auto node = geometry->SourceSyntaxNode(GetSelection().caret, _state->sourceCaretSite);
                            if (const auto syntax = GetMarkdownSyntax();
                                syntax && node < syntax->Nodes().size() && syntax->Nodes()[node].block.kind == RichText::MarkdownBlockKind::TableCell)
                            {
                                request.command    = RichText::MarkdownCommand::InsertTableRow;
                                request.targetCell = node;
                            }
                        }
                    if (ExecuteMarkdownCommand(request) != RichText::MarkdownCommandStatus::Unavailable)
                        return true;
                }
                const auto source = _state->document.Snapshot();
                const auto view   = source.Text();
                const auto ending = view.find_first_of(L"\r\n");
                text = ending == view.npos ? L"\r\n"
                                           : view.substr(ending, view[ending] == L'\r' && ending + 1u < view.size() && view[ending + 1u] == L'\n' ? 2u : 1u);
            }
            else if (character >= L' ' && character != L'\x7f')
                text.assign(1, character);
            else
                return false; // Tab is host traversal; control characters never enter the source.
        }
        if (_state->markdownAutoFormatEnabled && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich && ! _state->sourceEditing &&
            text.size() == 1u && GetSelection().Start() == GetSelection().End() && (_state->autoFormatRun || MatchAutoFormat(text) != AutoFormatMatch::None))
        {
            const auto selection  = GetSelection();
            const auto source     = Snapshot();
            const auto now        = GetTickCount64();
            const auto previous   = _state->autoFormatRun;
            const bool continuing = previous && previous->revision == Revision() && previous->selectionEpoch == _state->document.selectionEpoch_ &&
                                    previous->caret == selection.caret && now >= previous->lastTick && now - previous->lastTick <= 1000u &&
                                    _state->document.CanUndo();
            const auto start      = continuing ? previous->lineStart : selection.caret;
            auto candidate        = source.Slice(start, selection.caret);
            candidate += text;
            const auto match        = MatchAutoFormat(candidate);
            bool allowed            = match != AutoFormatMatch::None;
            bool nested             = continuing && previous->nested;
            auto continuationPrefix = continuing ? previous->continuationPrefix : nullptr;
            const bool focused      = host.GetFocusControl() == this;
            const auto composition  = focused ? host.GetNativeTextCompositionActive(this) : std::optional<bool>{false};
            allowed = allowed && (! focused || ! host.HasActiveNativeTextInputSession() || composition.has_value()) && ! composition.value_or(false);
            if (allowed && ! continuing)
            {
                bool lineStart = selection.caret == 0u;
                if (! lineStart && source.IsBoundary(selection.caret - 1u))
                {
                    const auto preceding = source.Slice(selection.caret - 1u, selection.caret);
                    lineStart            = preceding == L"\r" || preceding == L"\n";
                }
                else if (! lineStart && selection.caret >= 2u && source.IsBoundary(selection.caret - 2u))
                    lineStart = source.Slice(selection.caret - 2u, selection.caret) == L"\r\n";
                bool lineEnd = selection.caret == source.Length();
                if (! lineEnd && source.IsBoundary(selection.caret + 1u))
                {
                    const auto following = source.Slice(selection.caret, selection.caret + 1u);
                    lineEnd              = following == L"\r" || following == L"\n";
                }
                else if (! lineEnd && selection.caret + 2u <= source.Length() && source.IsBoundary(selection.caret + 2u))
                    lineEnd = source.Slice(selection.caret, selection.caret + 2u) == L"\r\n";
                const auto syntax = GetMarkdownSyntax();
                if (syntax)
                    for (const auto& empty : syntax->EmptyLines())
                        if (empty.insertion == selection.caret && empty.owner < syntax->Nodes().size())
                        {
                            const auto kind = syntax->Nodes()[empty.owner].block.kind;
                            if (kind == RichText::MarkdownBlockKind::Document || kind == RichText::MarkdownBlockKind::BlockQuote ||
                                kind == RichText::MarkdownBlockKind::ListItem)
                            {
                                lineStart = true;
                                nested    = kind != RichText::MarkdownBlockKind::Document;
                                if (nested)
                                {
                                    const auto raw = syntax->Source();
                                    auto line      = selection.caret;
                                    while (line && raw[line - 1u] != L'\r' && raw[line - 1u] != L'\n')
                                        --line;
                                    auto prefix = std::make_shared<std::wstring>(raw.substr(line, selection.caret - line));
                                    for (auto owner = empty.owner; owner != RichText::MarkdownNoNode; owner = syntax->Nodes()[owner].parent)
                                    {
                                        const auto& item = syntax->Nodes()[owner];
                                        if (item.block.kind != RichText::MarkdownBlockKind::ListItem || item.block.info.first < line ||
                                            item.block.info.last > selection.caret)
                                            continue;
                                        for (auto at = item.block.info.first; at < item.block.info.last; ++at)
                                            (*prefix)[at - line] = L' ';
                                    }
                                    continuationPrefix = std::move(prefix);
                                }
                            }
                        }
                allowed = lineStart && lineEnd && syntax != nullptr;
                if (syntax)
                    for (const auto& node : syntax->Nodes())
                        if (node.block.kind != RichText::MarkdownBlockKind::Document && node.block.kind != RichText::MarkdownBlockKind::BlankLine &&
                            node.block.kind != RichText::MarkdownBlockKind::BlockQuote && node.block.kind != RichText::MarkdownBlockKind::List &&
                            node.block.kind != RichText::MarkdownBlockKind::ListItem && node.block.source.first <= selection.caret &&
                            (selection.caret < node.block.source.last ||
                             (selection.caret == node.block.source.last && selection.caret == source.Length() &&
                              ((node.block.kind == RichText::MarkdownBlockKind::FencedCode && ! node.block.closed) ||
                               node.block.kind == RichText::MarkdownBlockKind::HtmlBlock))))
                            allowed = false;
            }
            if (allowed)
            {
                std::wstring insertion(text);
                auto caret = selection.caret + insertion.size();
                if (match == AutoFormatMatch::Fence)
                {
                    const auto all              = source.Text();
                    const auto newline          = all.find_first_of(L"\r\n");
                    const std::wstring_view eol = newline == all.npos ? std::wstring_view(L"\r\n")
                                                  : all[newline] == L'\r' && newline + 1u < all.size() && all[newline + 1u] == L'\n'
                                                      ? std::wstring_view(L"\r\n")
                                                  : all[newline] == L'\r' ? std::wstring_view(L"\r")
                                                                          : std::wstring_view(L"\n");
                    insertion += eol;
                    caret += eol.size();
                    if (continuationPrefix)
                    {
                        insertion += *continuationPrefix;
                        caret += continuationPrefix->size();
                    }
                    insertion += eol;
                    if (continuationPrefix)
                        insertion += *continuationPrefix;
                    insertion += candidate;
                }
                _state->typingRun.reset();
                const bool changed = _state->document.ReplaceImpl(selection.caret, selection.caret, insertion, {caret, caret}, true, continuing);
                if (changed && match == AutoFormatMatch::Prefix)
                    _state->autoFormatRun = State::AutoFormatRun{Revision(), _state->document.selectionEpoch_, now, caret, start, nested, continuationPrefix};
                else
                    _state->autoFormatRun.reset();
                Changed(changed);
                return true;
            }
            _state->autoFormatRun.reset();
        }
        if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich && LayoutIsCurrent(host))
            if (const auto* geometry = _state->layout.SemanticGeometry(); geometry && geometry->IsCellSourceSite(GetSelection().caret, _state->sourceCaretSite))
            {
                RichText::MarkdownCommandRequest request;
                request.command    = RichText::MarkdownCommand::SetTableCell;
                request.targetCell = geometry->SourceSyntaxNode(GetSelection().caret, _state->sourceCaretSite);
                request.text       = text;
                std::optional<std::wstring> pairedText;
                if (text == L"|")
                    request.text = L"\\|";
                else if (text == L"\\")
                    request.text = L"\\\\";
                else if (_state->autoPairEnabled && text.size() == 1u)
                {
                    const auto closing     = AutoPairClosing(text[0]);
                    const bool focused     = host.GetFocusControl() == this;
                    const auto composition = focused ? host.GetNativeTextCompositionActive(this) : std::optional<bool>{false};
                    if (closing && (! focused || ! host.HasActiveNativeTextInputSession() || composition.has_value()) && ! composition.value_or(false))
                    {
                        pairedText.emplace(text);
                        pairedText.value().push_back(closing);
                        request.text                 = pairedText.value();
                        request.tableCellCaretOffset = 1u;
                    }
                }
                if (ExecuteMarkdownCommand(request) != RichText::MarkdownCommandStatus::Unavailable)
                    return true;
            }
        const auto selection = GetSelection();
        if (_state->autoPairEnabled && text.size() == 1u)
        {
            const auto closing             = AutoPairClosing(text[0]);
            const bool focused             = host.GetFocusControl() == this;
            const auto composition         = focused ? host.GetNativeTextCompositionActive(this) : std::optional<bool>{false};
            const bool insideSourceSession = ! _state->sourceEditing || (selection.Start() >= _state->sourceEditing->region.source.first &&
                                                                         selection.End() <= _state->sourceEditing->region.source.last);
            if (closing && insideSourceSession && (! focused || ! host.HasActiveNativeTextInputSession() || composition.has_value()) &&
                ! composition.value_or(false))
            {
                std::wstring replacement(1u, text[0]);
                replacement += Snapshot().Slice(selection.Start(), selection.End());
                replacement += closing;
                const auto begin = selection.Start() + 1u, end = selection.End() + 1u;
                const RichText::Selection after = selection.anchor <= selection.caret ? RichText::Selection{begin, end} : RichText::Selection{end, begin};
                _state->typingRun.reset();
                const bool changed = _state->document.ReplaceImpl(selection.Start(), selection.End(), replacement, after, true);
                Changed(changed);
                return true;
            }
        }
        if (_state->smartTypographyEnabled && ! _state->sourceEditing && _state->mode == RichText::ViewMode::Rich && text.size() == 1u &&
            selection.Start() == selection.End() && (text[0] == L'.' || text[0] == L'-' || text[0] == L'\'' || text[0] == L'"'))
        {
            const bool focused     = host.GetFocusControl() == this;
            const auto composition = focused ? host.GetNativeTextCompositionActive(this) : std::optional<bool>{false};
            bool permitted         = (! focused || ! host.HasActiveNativeTextInputSession() || composition.has_value()) && ! composition.value_or(false);
            if (permitted && _state->markdownRendering)
            {
                const auto syntax = GetMarkdownSyntax();
                permitted = syntax != nullptr ||
                            (_state->smartTypingSite && _state->smartTypingSite->revision == Revision() && _state->smartTypingSite->caret == selection.caret);
                if (syntax)
                {
                    for (const auto& node : syntax->Nodes())
                        if ((node.block.kind == RichText::MarkdownBlockKind::FencedCode || node.block.kind == RichText::MarkdownBlockKind::IndentedCode ||
                             node.block.kind == RichText::MarkdownBlockKind::HtmlBlock || node.block.kind == RichText::MarkdownBlockKind::FrontMatter) &&
                            node.block.source.first <= selection.caret &&
                            (selection.caret < node.block.source.last ||
                             (selection.caret == node.block.source.last && selection.caret == syntax->Source().size() && ! node.block.closed)))
                            permitted = false;
                    for (const auto& node : syntax->Inlines())
                        if ((node.kind == RichText::MarkdownInlineKind::Code || node.kind == RichText::MarkdownInlineKind::Html ||
                             node.kind == RichText::MarkdownInlineKind::Math || node.kind == RichText::MarkdownInlineKind::Image ||
                             node.kind == RichText::MarkdownInlineKind::Link) &&
                            node.source.first < selection.caret && selection.caret < node.source.last)
                            permitted = false;
                }
            }
            if (permitted)
            {
                const auto source = Snapshot();
                auto first        = selection.caret;
                wchar_t replacement{};
                if (text[0] == L'.' && first >= 2u && source.IsBoundary(first - 2u) && source.Slice(first - 2u, first) == L"..")
                {
                    first -= 2u;
                    replacement = L'\u2026';
                }
                else if (text[0] == L'-' && first && source.IsBoundary(first - 1u))
                {
                    const auto previous = source.Slice(first - 1u, first);
                    if (previous == L"-" || previous == L"\u2013")
                    {
                        --first;
                        replacement = previous == L"-" ? L'\u2013' : L'\u2014';
                    }
                }
                else if (text[0] == L'\'' || text[0] == L'"')
                {
                    bool opening = first == 0u;
                    if (first && source.IsBoundary(first - 1u))
                    {
                        const auto previous = source.Slice(first - 1u, first);
                        WORD type{};
                        opening = previous[0] == L'(' || previous[0] == L'[' || previous[0] == L'{' ||
                                  (GetStringTypeW(CT_CTYPE1, previous.data(), 1, &type) && (type & C1_SPACE) != 0u);
                        if (previous[0] == L'\\')
                            permitted = false;
                    }
                    replacement = text[0] == L'\'' ? (opening ? L'\u2018' : L'\u2019') : (opening ? L'\u201c' : L'\u201d');
                }
                if (permitted && replacement)
                {
                    _state->typingRun.reset();
                    const auto end          = first + 1u;
                    const bool changed      = _state->document.ReplaceImpl(first, selection.caret, std::wstring_view(&replacement, 1u), {end, end}, true);
                    _state->smartTypingSite = changed ? std::optional{State::SmartTypingSite{Revision(), end}} : std::nullopt;
                    Changed(changed);
                    return true;
                }
                if (permitted)
                {
                    _state->typingRun.reset();
                    const auto end          = selection.caret + text.size();
                    const bool changed      = _state->document.ReplaceImpl(selection.caret, selection.caret, text, {end, end}, true);
                    _state->smartTypingSite = changed ? std::optional{State::SmartTypingSite{Revision(), end}} : std::nullopt;
                    Changed(changed);
                    return true;
                }
            }
        }
        const bool word = IsHistoryWordElement(text);
        if (word && selection.Start() == selection.End())
        {
            const auto now      = GetTickCount64();
            const auto previous = _state->typingRun;
            const bool merge    = previous && previous->kind == State::TypingRun::Kind::Insert && previous->revision == Revision() &&
                                  previous->selectionEpoch == _state->document.selectionEpoch_ && previous->caret == selection.caret &&
                                  now >= previous->lastTick && now - previous->lastTick <= 1000u && previous->units + text.size() <= 64u &&
                                  _state->document.CanUndo();
            const auto end      = selection.caret + text.size();
            const bool changed  = _state->document.ReplaceImpl(selection.caret, selection.caret, text, {end, end}, true, merge);
            if (changed)
                _state->typingRun = State::TypingRun{State::TypingRun::Kind::Insert,
                                                     Revision(),
                                                     _state->document.selectionEpoch_,
                                                     now,
                                                     end,
                                                     merge ? previous->units + text.size() : text.size()};
            else
                _state->typingRun.reset();
            Changed(changed);
        }
        else
        {
            _state->typingRun.reset();
            static_cast<void>(ReplaceSelection(std::move(text)));
        }
        return true;
    }
    catch (const std::bad_alloc&)
    {
        const bool handled = InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
        if (! handled)
            _state->pendingLeadSurrogate = pendingLead;
        return handled;
    }
    catch (const std::exception&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
}
bool RichTextEditor::OnKeyDown(ControlHost& host, UINT key, UINT modifiers) noexcept
{
    const auto inputLifetime  = GetLifetimeToken();
    const auto inputRevision  = Revision();
    const auto inputEpoch     = _state->fileDropEpoch;
    const auto inputSelection = GetSelection();
    try
    {
        if (! IsEnabled())
            return false;
        _state->pendingLeadSurrogate = 0;
        const bool control = ModifiersContainCtrl(modifiers), shift = ModifiersContainShift(modifiers);
        if (control || key == VK_ESCAPE || key == VK_RETURN || key == VK_TAB || key == VK_F2 || key == VK_LEFT || key == VK_RIGHT || key == VK_UP ||
            key == VK_DOWN || key == VK_HOME || key == VK_END || key == VK_PRIOR || key == VK_NEXT)
        {
            _state->typingRun.reset();
            _state->autoFormatRun.reset();
            _state->smartTypingSite.reset();
        }
        if (_state->markdownShortcutsEnabled && Editable() && control && ! ModifiersContainAlt(modifiers) && shift && key >= '0' && key <= '6' &&
            _state->markdownRendering && _state->mode == RichText::ViewMode::Rich && ! _state->sourceEditing)
        {
            RichText::MarkdownCommandRequest request;
            request.command      = RichText::MarkdownCommand::SetHeading;
            request.headingLevel = key - static_cast<UINT>('0');
            static_cast<void>(ExecuteMarkdownCommand(request));
            return true;
        }
        if (ModifiersContainAlt(modifiers))
            return false;
        if (control && ! shift && (key == VK_OEM_PLUS || key == VK_ADD || key == VK_OEM_MINUS || key == VK_SUBTRACT || key == L'0'))
        {
            const float next = key == L'0' ? 1.0f : std::clamp(GetZoom() * ((key == VK_OEM_PLUS || key == VK_ADD) ? 1.1f : 1.0f / 1.1f), .25f, 4.0f);
            SetZoom(next);
            return true;
        }
        if (key == VK_ESCAPE && ! _state->sourceEditing && (_state->scrollbarDragging || _state->dragging))
        {
            const auto release        = ReleaseUnchangedCaptureOnExit(host, *this);
            _state->scrollbarDragging = false;
            _state->dragging          = false;
            _state->codePressGesture  = false;
            _state->codePressDocument.reset();
            _state->taskPressPlan.reset();
            PublishSelectionChanged();
            return true;
        }
        if (_state->markdownShortcutsEnabled && Editable() && control && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich &&
            ! _state->sourceEditing)
        {
            if ((! shift && key == VK_OEM_3) || (shift && key == 'X'))
            {
                RichText::MarkdownCommandRequest request;
                request.command = RichText::MarkdownCommand::ToggleStyle;
                request.style   = shift ? RichText::MarkdownTextStyle::Strikethrough : RichText::MarkdownTextStyle::Code;
                static_cast<void>(ExecuteMarkdownCommand(request));
                return true;
            }
            if (! shift && key == 'K')
            {
                const auto composition = host.GetFocusControl() == this ? host.GetNativeTextCompositionActive(this) : std::optional<bool>{false};
                if ((host.GetFocusControl() == this && host.HasActiveNativeTextInputSession() && ! composition) || composition.value_or(false))
                    return true;
                const auto callback = _state->onLinkEditRequested;
                if (! callback)
                    return false;
                try
                {
                    (*callback)();
                }
                catch (const std::bad_alloc&)
                {
                    // Observer allocation failure remains fatal at the native input boundary.
                    std::terminate();
                }
                catch (const std::exception&)
                {
                    // The link-edit request was already delivered; do not replay it.
                    Debug::Warning(L"DxUi::RichTextEditor: link edit request callback failed.");
                }
                return true;
            }
        }
        if (_state->searchShortcutsEnabled)
        {
            if (control && ! shift && key == 'F')
            {
                const auto callback = _state->onFindRequested;
                if (! callback)
                    return false;
                try
                {
                    (*callback)();
                }
                catch (const std::bad_alloc&)
                {
                    // Observer allocation failure remains fatal at the native input boundary.
                    std::terminate();
                }
                catch (const std::exception&)
                {
                    // The find request was already delivered; do not replay it.
                    Debug::Warning(L"DxUi::RichTextEditor: find request callback failed.");
                }
                return true;
            }
            if (! control && key == VK_F3 && ! _state->searchQuery.empty())
            {
                static_cast<void>(SelectSourceMatch(_state->searchQuery, shift, _state->searchOptions));
                return true;
            }
        }
        if (control && shift && key == 'C' && _state->onCodeCopyRequested &&
            _state->appearance.markdownPresentation == RichText::MarkdownPresentationStyle::Subtle)
        {
            if (const auto document = GetAcknowledgedSemanticDocument(host); document && ! document->Syntax()->SourceRegion())
            {
                const auto caret  = GetSelection().caret;
                const auto& nodes = document->Syntax()->Nodes();
                for (const auto& element : document->Elements())
                    if (element.role == RichText::SemanticRole::Code && element.syntaxNode < nodes.size() &&
                        (nodes[element.syntaxNode].block.kind == RichText::MarkdownBlockKind::FencedCode ||
                         nodes[element.syntaxNode].block.kind == RichText::MarkdownBlockKind::IndentedCode) &&
                        element.source.first <= caret && caret < element.source.last)
                    {
                        const auto callback = _state->onCodeCopyRequested;
                        (*callback)(CodeCopyText(*document, element));
                        return true; // Callback may retire the owner.
                    }
            }
            return false;
        }
        if (_state->sourceEditing && (key == VK_ESCAPE || (control && key == VK_RETURN && ! shift) || (key == VK_F2 && ! control && ! shift)))
            return EndSemanticSourceEdit(host);
        if (key == VK_F8 && ! control)
            return SelectMarkdownDiagnostic(host, shift);
        if (key == VK_F2 && ! control && ! shift)
        {
            if (const auto document = GetAcknowledgedSemanticDocument(host))
            {
                const auto caret = GetSelection().caret;
                auto trailing    = RichText::MarkdownNoNode;
                for (std::size_t element = 0u; element < document->Elements().size(); ++element)
                    if (const auto range = document->SourceEditingRange(element))
                    {
                        const auto source = *range;
                        if (caret >= source.first && caret < source.last)
                            return BeginSemanticSourceEdit(host, document, element);
                        if (caret == source.last)
                            trailing = element;
                    }
                // A graphic exposes leading/trailing source carets. Its end
                // remains an entry site before following ordinary text. Prefer
                // a new object's leading site when two envelopes touch.
                if (trailing != RichText::MarkdownNoNode)
                    return BeginSemanticSourceEdit(host, document, trailing);
            }
            return false;
        }
        if (control)
        {
            if (key == VK_TAB && ! _state->sourceEditing && CanChangeSemanticSourcePresentation(host))
            {
                const auto document = GetAcknowledgedSemanticDocument(host);
                if (document)
                {
                    std::size_t first = RichText::MarkdownNoNode, last = RichText::MarkdownNoNode;
                    std::size_t next = RichText::MarkdownNoNode, previous = RichText::MarkdownNoNode;
                    const auto current = _state->keyboardNavigationDocument.lock() == document ? _state->keyboardNavigationElement : RichText::MarkdownNoNode;
                    bool pastCurrent   = false;
                    for (const auto& navigation : document->Navigation())
                    {
                        if (document->Elements()[navigation.element].role != RichText::SemanticRole::FootnoteBacklink)
                            continue;
                        if (first == RichText::MarkdownNoNode)
                            first = navigation.element;
                        if (pastCurrent && next == RichText::MarkdownNoNode)
                            next = navigation.element;
                        if (navigation.element == current)
                        {
                            previous    = last;
                            pastCurrent = true;
                        }
                        last = navigation.element;
                    }
                    const auto target = shift ? (previous != RichText::MarkdownNoNode ? previous : last) : (next != RichText::MarkdownNoNode ? next : first);
                    if (target != RichText::MarkdownNoNode)
                    {
                        const auto* geometry = _state->layout.SemanticGeometry();
                        const auto bounds    = geometry ? geometry->ElementBounds(target) : std::nullopt;
                        if (! bounds)
                            return false;
                        _state->keyboardNavigationDocument = document;
                        _state->keyboardNavigationElement  = target;
                        _state->caretLink                  = document->Elements()[target].display;
                        // Even a no-op scroll needs the new keyboard adornment painted.
                        const auto lifetime = GetLifetimeToken();
                        RequestInvalidate();
                        if (lifetime.expired() || GetHost() != &host)
                            return true;
                        return SetScrollOffsetDip(bounds->top);
                    }
                }
            }
            if (key == VK_RETURN && ! shift && _state->keyboardNavigationElement != RichText::MarkdownNoNode)
            {
                const auto document = _state->keyboardNavigationDocument.lock();
                const auto element  = _state->keyboardNavigationElement;
                if (document && GetAcknowledgedSemanticDocument(host) == document)
                    return NavigateSemanticElement(host, document, element);
            }
            if (key == VK_RETURN && ! shift)
                if (const auto document = GetAcknowledgedSemanticDocument(host))
                {
                    for (const auto& navigation : document->Navigation())
                    {
                        const auto& source = document->Elements()[navigation.element].source;
                        if (source.first < source.last && GetSelection().caret >= source.first && GetSelection().caret < source.last)
                            return NavigateSemanticElement(host, document, navigation.element);
                    }
                    for (size_t element = 0u; element < document->Elements().size(); ++element)
                    {
                        const auto& link = document->Elements()[element];
                        if (link.role == RichText::SemanticRole::Link && link.source.first <= GetSelection().caret && GetSelection().caret < link.source.last)
                            return NavigateSemanticElement(host, document, element);
                    }
                }
            switch (key)
            {
                case 'A': return OnSelectAll(host);
                case 'C': return OnCopy(host);
                case 'X':
                {
                    if (! Editable())
                        return true;
                    const auto lifetime  = GetLifetimeToken();
                    const auto revision  = Revision();
                    const auto binding   = _state->bindingRevision;
                    const auto selection = GetSelection();
                    if (OnCopy(host) && ! lifetime.expired() && GetHost() == &host && _state->bindingRevision == binding && Revision() == revision &&
                        GetSelection() == selection)
                        static_cast<void>(ReplaceSelection({}));
                    return true;
                }
                case 'V':
                {
                    if (! Editable())
                        return true;
                    if (host.GetFocusControl() == this)
                    {
                        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
                        const bool available = host.TryReadNativeTextInputState(this, input);
                        if ((host.HasActiveNativeTextInputSession() && ! available) ||
                            (available && (input.compositionStartIndex || input.compositionEndIndex)))
                            return true;
                    }
                    const auto lifetime  = GetLifetimeToken();
                    const auto revision  = Revision();
                    const auto binding   = _state->bindingRevision;
                    const auto location  = _state->imageLocationRevision;
                    const auto selection = GetSelection();
                    const auto focus     = host.GetFocusControl();
                    TextClipboardContent content;
                    const bool available = host.ReadClipboardContent(
                        content, bool(_state->htmlPasteConverter) && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich);
                    if (available && ! lifetime.expired() && GetHost() == &host && host.GetFocusControl() == focus && _state->bindingRevision == binding &&
                        Revision() == revision && _state->imageLocationRevision == location && GetSelection() == selection)
                    {
                        if (! content.filePaths.empty())
                            static_cast<void>(ApplyClipboardFilePaste(content));
                        else
                        {
                            std::wstring replacement(0u, L' ');
                            bool stalePaste        = false;
                            const bool transformed = TryPrepareClipboardPaste(content, replacement, &stalePaste);
                            if (stalePaste || lifetime.expired() || GetHost() != &host || host.GetFocusControl() != focus ||
                                _state->bindingRevision != binding || Revision() != revision || _state->imageLocationRevision != location ||
                                GetSelection() != selection)
                                return true;
                            if (transformed)
                                static_cast<void>(ReplaceSelection(std::move(replacement)));
                            else if (! content.text.empty())
                                static_cast<void>(ReplaceSelection(std::move(content.text)));
                        }
                    }
                    return true;
                }
                case 'Z': static_cast<void>(shift ? Redo() : Undo()); return true;
                case 'Y': static_cast<void>(Redo()); return true;
                case 'B':
                case 'I':
                    if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
                    {
                        RichText::MarkdownCommandRequest request;
                        request.command = RichText::MarkdownCommand::ToggleStyle;
                        request.style   = key == 'B' ? RichText::MarkdownTextStyle::Strong : RichText::MarkdownTextStyle::Emphasis;
                        static_cast<void>(ExecuteMarkdownCommand(request));
                    }
                    else
                        static_cast<void>(ToggleFormatting(key == 'B' ? L"**" : L"*"));
                    return true;
                case VK_RETURN:
                    if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
                    {
                        RichText::MarkdownCommandRequest request;
                        request.command = RichText::MarkdownCommand::ExitCodeBlock;
                        if (ExecuteMarkdownCommand(request) != RichText::MarkdownCommandStatus::Unavailable)
                            return true;
                        request.command = RichText::MarkdownCommand::ToggleTask;
                        static_cast<void>(ExecuteMarkdownCommand(request));
                        return true;
                    }
                    break;
                default: break;
            }
        }
        switch (key)
        {
            case VK_TAB:
                if (! control && Editable() && _state->appearance.sourceTabSpaces != 0u)
                {
                    const auto selection = GetSelection();
                    const auto snapshot  = Snapshot();
                    const auto text      = snapshot.Text();
                    RichText::MarkdownRange scope{0u, text.size()};
                    bool eligible = _state->mode == RichText::ViewMode::Source;
                    if (_state->sourceEditing)
                    {
                        scope    = _state->sourceEditing->region.source;
                        eligible = true;
                    }
                    else if (! eligible && _state->markdownRendering)
                    {
                        if (const auto syntax = GetMarkdownSyntax())
                            for (const auto& node : syntax->Nodes())
                                if ((node.block.kind == RichText::MarkdownBlockKind::FencedCode ||
                                     node.block.kind == RichText::MarkdownBlockKind::IndentedCode) &&
                                    selection.Start() >= node.block.content.first && selection.End() <= node.block.content.last)
                                {
                                    scope    = node.block.content;
                                    eligible = true;
                                    break;
                                }
                    }
                    if (eligible && selection.Start() >= scope.first && selection.End() <= scope.last)
                    {
                        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
                        if (host.GetFocusControl() == this)
                        {
                            const bool available = host.TryReadNativeTextInputState(this, input);
                            if ((host.HasActiveNativeTextInputSession() && ! available) ||
                                (available && (input.compositionStartIndex || input.compositionEndIndex)))
                                return true;
                        }
                        const std::wstring indent(_state->appearance.sourceTabUsesTabs ? 1u : _state->appearance.sourceTabSpaces,
                                                  _state->appearance.sourceTabUsesTabs ? L'\t' : L' ');
                        if (! shift && selection.Start() == selection.End())
                        {
                            const RichText::SourceEdit edit{{selection.Start(), selection.End()}, indent};
                            const auto end = selection.Start() + indent.size();
                            static_cast<void>(ApplySourceEdits(snapshot, Revision(), std::span(&edit, 1u), {end, end}));
                            return true;
                        }
                        auto line = selection.Start();
                        while (line > scope.first && text[line - 1u] != L'\n' && text[line - 1u] != L'\r')
                            --line;
                        std::vector<RichText::SourceEdit> edits(0u);
                        while (line <= selection.End() && line < scope.last)
                        {
                            if (line == selection.End() && selection.Start() != selection.End())
                                break;
                            size_t remove = 0u;
                            if (shift)
                            {
                                if (text[line] == L'\t')
                                    remove = 1u;
                                else
                                    while (remove < _state->appearance.sourceTabSpaces && line + remove < scope.last && text[line + remove] == L' ')
                                        ++remove;
                                if (remove != 0u)
                                    edits.push_back({{line, line + remove}, {}});
                            }
                            else
                                edits.push_back({{line, line}, indent});
                            while (line < scope.last && text[line] != L'\r' && text[line] != L'\n')
                                ++line;
                            if (line == scope.last)
                                break;
                            if (text[line++] == L'\r' && line < scope.last && text[line] == L'\n')
                                ++line;
                        }
                        if (! edits.empty())
                        {
                            const auto map = [&](size_t offset, bool leading)
                            {
                                size_t mapped = offset;
                                for (const auto& edit : edits)
                                {
                                    if (edit.source.first > offset || (leading && edit.source.first == offset))
                                        break;
                                    if (edit.source.first == edit.source.last)
                                        mapped += edit.replacement.size();
                                    else
                                        mapped -= (std::min)(offset, edit.source.last) - edit.source.first;
                                }
                                return mapped;
                            };
                            const auto start = map(selection.Start(), true);
                            const auto end   = map(selection.End(), false);
                            const RichText::Selection after =
                                selection.anchor <= selection.caret ? RichText::Selection{start, end} : RichText::Selection{end, start};
                            static_cast<void>(ApplySourceEdits(snapshot, Revision(), edits, after));
                        }
                        return true;
                    }
                }
                if (! control && ! _state->sourceEditing && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich)
                {
                    RichText::MarkdownCommandRequest request;
                    request.command = shift ? RichText::MarkdownCommand::PreviousTableCell : RichText::MarkdownCommand::NextTableCell;
                    if (ExecuteMarkdownCommand(request) != RichText::MarkdownCommandStatus::Unavailable)
                        return true;
                    request.command = shift ? RichText::MarkdownCommand::OutdentList : RichText::MarkdownCommand::IndentList;
                    return ExecuteMarkdownCommand(request) != RichText::MarkdownCommandStatus::Unavailable;
                }
                return false;
            case VK_BACK:
            case VK_DELETE:
            {
                const bool backwards = key == VK_BACK;
                const auto selection = GetSelection();
                bool merge           = false;
                if (! control && Editable() && selection.Start() == selection.End() &&
                    (_state->sourceEditing || _state->mode == RichText::ViewMode::Source ||
                     GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback))
                {
                    const auto source = Snapshot();
                    auto range        = TextElementRange{};
                    if (! _state->sourceEditing && _state->mode == RichText::ViewMode::Source && LayoutIsCurrent(host))
                    {
                        const auto projected = _state->layout.DeletionRange(selection.caret, backwards);
                        range                = {projected.first, projected.second};
                    }
                    else
                        range = GetTextElementDeletionRange(source.Text(), selection.caret, backwards);
                    const auto units = range.last - range.first;
                    const bool word  = units <= 64u && IsHistoryWordElement(source.Slice(range.first, range.last));
                    if (word)
                    {
                        const auto now      = GetTickCount64();
                        const auto previous = _state->typingRun;
                        const auto kind     = backwards ? State::TypingRun::Kind::Backspace : State::TypingRun::Kind::Delete;
                        merge = previous && previous->kind == kind && previous->revision == Revision() &&
                                previous->selectionEpoch == _state->document.selectionEpoch_ && previous->caret == selection.caret &&
                                now >= previous->lastTick && now - previous->lastTick <= 1000u && previous->units + units <= 64u && _state->document.CanUndo();
                        _state->typingRun = State::TypingRun{
                            kind, Revision() + 1u, _state->document.selectionEpoch_, now, range.first, merge ? previous->units + units : units};
                    }
                    else
                        _state->typingRun.reset();
                }
                else if (control || ! Editable() || selection.Start() != selection.End() || ! _state->markdownRendering ||
                         _state->mode != RichText::ViewMode::Rich)
                    _state->typingRun.reset();
                static_cast<void>(Erase(host, backwards, control, merge));
                return true;
            }
            case VK_LEFT:
            case VK_RIGHT:
            case VK_UP:
            case VK_DOWN:
            case VK_HOME:
            case VK_END:
            case VK_PRIOR:
            case VK_NEXT: MoveCaret(host, key, shift, control); return true;
            case VK_ESCAPE:
            {
                if (! _state->dragging)
                    return false;
                _state->dragging         = false;
                _state->codePressGesture = false;
                _state->codePressDocument.reset();
                _state->taskPressPlan.reset();
                const auto lifetime = GetLifetimeToken();
                host.ReleaseMouseCapture();
                if (! lifetime.expired() && GetHost() == &host)
                    PublishSelectionChanged();
                return true;
            }
            case VK_RETURN:
                if (! Editable())
                    return false;
                host.AllowTranslatedCharacter(); // WM_CHAR / correlated DispatchCharacter performs the one insertion.
                return true;
            default: return false;
        }
    }
    catch (const std::bad_alloc&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
    catch (const std::exception&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
}
bool RichTextEditor::OnContextMenu(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F point) noexcept
{
    const auto inputLifetime  = GetLifetimeToken();
    const auto inputRevision  = Revision();
    const auto inputEpoch     = _state->fileDropEpoch;
    const auto inputSelection = GetSelection();
    try
    {
        if (! IsEnabled() || ! IsVisible())
            return false;
        const auto lifetime = GetLifetimeToken();
        const auto geometry = host.GetInteractionRevision();
        host.SetFocusControl(this);
        if (lifetime.expired() || GetHost() != &host || host.GetInteractionRevision() != geometry || host.GetFocusControl() != this)
            return true;
        if (! keyboardInvocation && LayoutIsCurrent(host))
        {
            const auto previousSite = _state->sourceCaretSite;
            const auto source       = HitSource(host, point);
            const auto selection    = GetSelection();
            if (source < selection.Start() || source >= selection.End())
            {
                _state->document.SetSelection({source, source});
                const auto revision = Revision();
                SelectionChanged();
                if (lifetime.expired() || GetHost() != &host || host.GetInteractionRevision() != geometry || host.GetFocusControl() != this ||
                    Revision() != revision || GetSelection() != RichText::Selection{source, source})
                    return true;
            }
            else
                _state->sourceCaretSite = previousSite;
        }
        try
        {
            if (Control::OnContextMenu(host, keyboardInvocation, point))
                return true;
        }
        catch (const std::bad_alloc&)
        {
            // A consumer observer's allocation failure remains fatal.
            std::terminate();
        }
        catch (const std::exception&)
        {
            // The consumer has received the request; never open a second menu.
            Debug::Warning(L"DxUi::RichTextEditor: context menu callback failed.");
            return true;
        }
        const auto hwnd = host.GetHwnd();
        if (! _state->contextMenuLabels || ! hwnd)
            return false;
        std::vector<MenuFlyoutItem> items(0u);
        BuildDefaultContextMenuItems(items);
        const auto anchor = ResolveContextMenuAnchor(host, keyboardInvocation, point);
        if (lifetime.expired() || GetHost() != &host)
            return true;
        auto* owner        = &host;
        const auto binding = _state->bindingRevision;
        ContextMenuSessionCallbacks options;
        options.focusFirstNavigableItem = keyboardInvocation;
        return ContextMenu::ShowAsync(hwnd,
                                      anchor,
                                      items,
                                      host.GetTheme(),
                                      [this, lifetime, owner, binding](std::optional<int> command)
        {
            if (lifetime.expired() || GetHost() != owner || _state->bindingRevision != binding || ! command)
                return;
            static_cast<void>(InvokeDefaultContextMenuCommand(*owner, command.value()));
        },
                                      options);
    }
    catch (const std::bad_alloc&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
    catch (const std::exception&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
}
bool RichTextEditor::OnCopy(ControlHost& host) noexcept
{
    try
    {
        if (! IsEnabled())
            return false;
        const auto selection = GetSelection();
        if (selection.Start() == selection.End())
            return false;
        // Copy exact Markdown source, including delimiters and original EOLs.
        const auto text = Snapshot().Slice(selection.Start(), selection.End());
        return host.CopyTextToClipboard(text);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnSelectAll(ControlHost&) noexcept
{
    try
    {
        if (! IsEnabled())
            return false;
        SetSelection({0, _state->document.Length()});
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers) noexcept
{
    const auto inputLifetime  = GetLifetimeToken();
    const auto inputRevision  = Revision();
    const auto inputEpoch     = _state->fileDropEpoch;
    const auto inputSelection = GetSelection();
    try
    {
        if (! IsEnabled() || rightButton)
            return false;
        const auto lifetime = GetLifetimeToken();
        const auto geometry = host.GetInteractionRevision();
        host.SetFocusControl(this);
        if (lifetime.expired() || GetHost() != &host || host.GetInteractionRevision() != geometry || ! IsEnabled() || ! IsVisible() ||
            host.GetFocusControl() != this)
            return true;
        if (const auto info = GetScrollInfo(); info && info->contentHeightDip > info->viewportHeightDip)
        {
            const auto track = ScrollbarTrack();
            if (point.x >= track.left && point.x < track.right && point.y >= track.top && point.y < track.bottom)
            {
                const float extent = info->contentHeightDip - info->viewportHeightDip;
                const auto thumb   = ComputeScrollbarThumbHitRect(
                    track, ScrollbarOrientation::Vertical, info->viewportHeightDip, info->contentHeightDip, info->offsetDip, extent);
                if (point.y >= thumb.top && point.y < thumb.bottom)
                {
                    _state->scrollbarDragging   = true;
                    _state->scrollbarDragY      = point.y;
                    _state->scrollbarDragOffset = info->offsetDip;
                    host.CaptureMouse(this);
                    if (! lifetime.expired() && GetHost() == &host)
                        RequestInvalidate();
                    return true;
                }
                const auto step = ComputeScrollbarPageStepDip(track, ScrollbarOrientation::Vertical, info->viewportHeightDip, info->contentHeightDip);
                return SetScrollOffsetDip(info->offsetDip + (point.y < thumb.top ? -step : step));
            }
        }
        if (! ModifiersContainCtrl(modifiers) && ! ModifiersContainShift(modifiers) && ! ModifiersContainAlt(modifiers) && _state->onCodeCopyRequested &&
            _state->appearance.markdownPresentation == RichText::MarkdownPresentationStyle::Subtle)
            if (const auto document = GetAcknowledgedSemanticDocument(host); document && ! document->Syntax()->SourceRegion())
                for (std::size_t index = 0u; index < document->Elements().size(); ++index)
                {
                    const auto& element = document->Elements()[index];
                    const auto& nodes   = document->Syntax()->Nodes();
                    if (element.role != RichText::SemanticRole::Code || element.syntaxNode >= nodes.size() ||
                        (nodes[element.syntaxNode].block.kind != RichText::MarkdownBlockKind::FencedCode &&
                         nodes[element.syntaxNode].block.kind != RichText::MarkdownBlockKind::IndentedCode))
                        continue;
                    const auto copy     = GetSemanticCodeCopyBounds(host, index);
                    const auto viewport = TextViewport();
                    if (copy && point.x >= viewport.left && point.x < viewport.right && point.y >= viewport.top && point.y < viewport.bottom &&
                        point.x >= copy->left && point.x <= copy->right && point.y >= copy->top && point.y <= copy->bottom)
                    {
                        _state->codePressDocument = document;
                        _state->codePressElement  = index;
                        _state->codePressRect     = *copy;
                        _state->codePressGesture = _state->dragging = true;
                        host.CaptureMouse(this);
                        return true;
                    }
                }

        const auto source = HitSource(host, point);
        if (ModifiersContainCtrl(modifiers) && ! ModifiersContainShift(modifiers))
            if (const auto document = GetAcknowledgedSemanticDocument(host))
            {
                for (const auto& navigation : document->Navigation())
                    if (const auto bounds = GetSemanticElementBounds(host, navigation.element);
                        bounds && point.x >= bounds->left && point.x <= bounds->right && point.y >= bounds->top && point.y <= bounds->bottom)
                        return NavigateSemanticElement(host, document, navigation.element);
                for (size_t element = 0u; element < document->Elements().size(); ++element)
                    if (document->Elements()[element].role == RichText::SemanticRole::Link)
                        if (const auto bounds = GetSemanticElementBounds(host, element);
                            bounds && point.x >= bounds->left && point.x <= bounds->right && point.y >= bounds->top && point.y <= bounds->bottom)
                            return NavigateSemanticElement(host, document, element);
            }
        std::shared_ptr<const RichText::MarkdownCommandPlan> taskPlan;
        D2D1_RECT_F taskRect{};
        if (Editable() && ! ModifiersContainShift(modifiers))
            if (const auto* semantic = _state->layout.SemanticGeometry())
            {
                const auto& document = semantic->Document();
                const auto viewport  = TextViewport();
                for (std::size_t i = 0u; i < document.Elements().size(); ++i)
                {
                    const auto& element = document.Elements()[i];
                    if (element.role != RichText::SemanticRole::Task)
                        continue;
                    auto bounds = semantic->ElementBounds(i);
                    if (! bounds)
                        continue;
                    bounds->left += viewport.left + HorizontalTranslation();
                    bounds->right += viewport.left + HorizontalTranslation();
                    bounds->top += viewport.top - _state->scrollDip;
                    bounds->bottom += viewport.top - _state->scrollDip;
                    if (point.x < bounds->left || point.x > bounds->right || point.y < bounds->top || point.y > bounds->bottom)
                        continue;
                    RichText::MarkdownCommandRequest request;
                    request.command = RichText::MarkdownCommand::ToggleTask;
                    taskPlan        = std::make_shared<const RichText::MarkdownCommandPlan>(
                        RichText::BuildMarkdownCommand(Snapshot(), Revision(), document.Syntax(), RichText::Selection{source, source}, request));
                    taskRect = *bounds;
                    break;
                }
            }
        _state->dragging = true;
        _state->document.SetSelection({ModifiersContainShift(modifiers) ? GetSelection().anchor : source, source});
        _state->taskPressPlan = std::move(taskPlan);
        _state->taskPressRect = taskRect;
        host.CaptureMouse(this);
        if (! lifetime.expired() && GetHost() == &host)
            SelectionChanged();
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
    catch (const std::exception&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
}
bool RichTextEditor::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT) noexcept
{
    const auto inputLifetime  = GetLifetimeToken();
    const auto inputRevision  = Revision();
    const auto inputEpoch     = _state->fileDropEpoch;
    const auto inputSelection = GetSelection();
    try
    {
        if (! IsEnabled() || rightButton)
            return false;
        const auto source      = HitSource(host, point);
        const auto& projection = _state->layout;
        if (const auto* geometry = projection.SemanticGeometry(); geometry && geometry->IsEmptySourceSite(source, _state->sourceCaretSite))
        {
            // This line/cell has a source insertion site but no owned word.
            // Display-only lookup would select a neighboring paragraph or cell.
            // Keep the clicked affinity even when several cells share one ACP.
            _state->dragging = false;
            _state->document.SetSelection({source, source});
            SelectionChanged();
            return true;
        }
        size_t caret = projection.ToDisplay(source);
        std::optional<size_t> anchor;
        SelectSingleLineWordAt(projection.DisplayText(), caret, caret, anchor);
        _state->dragging = false;
        SetSelection({projection.ToSource(anchor.value_or(caret)), projection.ToSource(caret, true)});
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
    catch (const std::exception&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
}
std::optional<RichText::MarkdownRange> RichTextEditor::HoveredLinkDisplayRange(ControlHost& host, D2D1_POINT_2F point) const
{
    if (! std::isfinite(point.x) || ! std::isfinite(point.y) || ! LayoutIsCurrent(host))
        return std::nullopt;
    const auto* geometry = _state->layout.SemanticGeometry();
    if (! geometry)
        return std::nullopt;
    const auto viewport = TextViewport();
    if (point.x < viewport.left || point.x >= viewport.right || point.y < viewport.top || point.y >= viewport.bottom)
        return std::nullopt;
    const auto x = point.x - viewport.left - HorizontalTranslation(), y = point.y - viewport.top + _state->scrollDip;
    const auto hit = geometry->HitTest(x, y);
    if (! hit.inside)
        return std::nullopt;
    const auto display = hit.trailing && hit.position > 0u ? hit.position - 1u : hit.position;
    for (const auto& element : geometry->Document().Elements())
        if ((element.role == RichText::SemanticRole::Link || element.role == RichText::SemanticRole::FootnoteReference ||
             element.role == RichText::SemanticRole::FootnoteBacklink) &&
            element.display.first <= display && display < element.display.last)
            return element.display;
    return std::nullopt;
}
HRESULT RichTextEditor::PrepareCodeInk(const std::shared_ptr<const RichText::SemanticDocument>& document)
{
    auto& state = *_state;
    if (! document || ! state.codeTokenizer)
    {
        state.codeInkDocument.reset();
        state.codeInk.clear();
        state.codeInkStaged.clear();
        state.codeInkComplete = false;
        return S_OK;
    }
    if (state.codeInkDocument != document)
    {
        state.codeInkDocument = document;
        state.codeInk.clear();
        state.codeInkStaged.clear();
        state.codeInkCursor   = 0u;
        state.codeInkComplete = false;
    }
    if (state.codeInkComplete)
        return S_OK;
    const auto elements = document->Elements();
    std::size_t work    = 0u;
    while (state.codeInkCursor < elements.size() && work++ < 256u)
    {
        const auto index    = state.codeInkCursor;
        const auto& element = elements[index];
        if (element.role != RichText::SemanticRole::Code || element.syntaxNode >= document->Syntax()->Nodes().size())
        {
            ++state.codeInkCursor;
            continue;
        }
        const auto& node = document->Syntax()->Nodes()[element.syntaxNode];
        if (node.block.kind != RichText::MarkdownBlockKind::FencedCode && node.block.kind != RichText::MarkdownBlockKind::IndentedCode)
        {
            ++state.codeInkCursor;
            continue;
        }
        const auto display = document->Map().Display();
        if (element.display.last > display.size() || element.display.first > element.display.last)
        {
            ++state.codeInkCursor;
            continue;
        }
        const auto code = display.substr(element.display.first, element.display.last - element.display.first);
        auto language   = document->Syntax()->Slice(node.block.info);
        while (! language.empty() && (language.front() == L' ' || language.front() == L'\t'))
            language.remove_prefix(1u);
        const auto end = language.find_first_of(L" \t\r\n");
        if (end != language.npos)
            language = language.substr(0u, end);
        const auto callback = state.codeTokenizer;
        const auto lifetime = GetLifetimeToken();
        const auto revision = Revision(), binding = state.bindingRevision;
        const auto* host = GetHost();
        std::vector<RichText::CodeToken> tokens(0u);
        bool accepted = false, allocationFailure = false;
        state.codeCallbackActive = true;
        try
        {
            accepted = (*callback)(language, code, tokens);
        }
        catch (const std::bad_alloc&)
        {
            allocationFailure = true;
        }
        catch (const std::exception&)
        {
            // A failed consumer tokenizer leaves this block in ordinary ink.
        }
        if (lifetime.expired())
            return E_ABORT;
        _state->codeCallbackActive = false;
        if (GetHost() != host || Revision() != revision || _state->bindingRevision != binding || _state->codeTokenizer != callback ||
            _state->codeInkDocument != document)
            return S_FALSE;
        if (allocationFailure)
        {
            return E_OUTOFMEMORY;
        }
        const auto boundary = [&](std::size_t offset)
        {
            return offset == 0u || offset == code.size() ||
                   ! (code[offset] >= 0xDC00 && code[offset] <= 0xDFFF && code[offset - 1u] >= 0xD800 && code[offset - 1u] <= 0xDBFF);
        };
        std::size_t previous = 0u;
        bool valid           = accepted && tokens.size() <= RichText::MaximumCodeTokens - _state->codeInkStaged.size();
        for (const auto& token : tokens)
        {
            if (token.text.first < previous || token.text.first >= token.text.last || token.text.last > code.size() ||
                static_cast<unsigned>(token.role) > static_cast<unsigned>(RichText::CodeTokenRole::Literal) || ! boundary(token.text.first) ||
                ! boundary(token.text.last))
            {
                valid = false;
                break;
            }
            previous = token.text.last;
        }
        if (valid)
        {
            _state->codeInkStaged.reserve(_state->codeInkStaged.size() + tokens.size());
            for (const auto& token : tokens)
                _state->codeInkStaged.push_back(
                    {{element.display.first + token.text.first, element.display.first + token.text.last}, static_cast<RichText::TextInkRole>(token.role)});
        }
        ++_state->codeInkCursor;
        return S_FALSE; // One potentially expensive borrowed-input callback per owner turn.
    }
    if (state.codeInkCursor < elements.size())
        return S_FALSE;
    state.codeInk.swap(state.codeInkStaged);
    state.codeInkComplete = true;
    return S_OK;
}
bool RichTextEditor::PrepareSourceGuides(ControlHost& host)
{
    auto& state        = *_state;
    const bool numbers = state.mode == RichText::ViewMode::Source && state.appearance.sourceLineNumbers;
    const bool marks   = state.mode == RichText::ViewMode::Source && state.appearance.sourceWhitespaceMarks;
    if (! numbers && ! marks)
    {
        state.sourceGuides.Clear();
        return true;
    }
    const auto revision = Revision(), generation = state.layout.Generation();
    if (state.sourceGuides.Current(revision, generation, numbers, marks))
        return true;
    const auto viewport    = TextViewport();
    const auto gutterWidth = numbers ? std::min(SourceGutterWidth(), std::max(0.0f, viewport.right - viewport.left - 1.0f)) : 0.0f;
    state.sourceGuides.Begin(Snapshot(), revision, generation, numbers, marks, state.sourceGuideOptions, gutterWidth);
    static_cast<void>(host.GetSolidBrush(host.GetTheme().subduedText));
    return state.sourceGuides.Advance(host.GetWriteFactory(), state.layout);
}
bool RichTextEditor::PrepareSourceInk()
{
    if (! _state->appearance.colorizeMarkdownSource || _state->mode != RichText::ViewMode::Source)
    {
        _state->sourceInk.Clear();
        return true;
    }
    return _state->sourceInk.Advance(GetMarkdownSyntax(), [this](size_t source) { return _state->layout.ToDisplay(source); });
}
std::optional<RichText::MarkdownRange> RichTextEditor::ActiveParagraphDisplayRange() const
{
    const auto caret = _state->layout.ToDisplay(GetSelection().caret);
    if (const auto* geometry = _state->layout.SemanticGeometry())
    {
        for (const auto& paragraph : geometry->Document().Paragraphs())
            if (paragraph.display.first <= caret &&
                (caret < paragraph.display.last || (caret == paragraph.display.last && caret == _state->layout.DisplayLength())))
                return paragraph.display;
        return std::nullopt;
    }
    auto first = caret, last = caret;
    while (first > 0u && _state->layout.DisplayUnit(first - 1u) != L'\r' && _state->layout.DisplayUnit(first - 1u) != L'\n')
        --first;
    while (last < _state->layout.DisplayLength() && _state->layout.DisplayUnit(last) != L'\r' && _state->layout.DisplayUnit(last) != L'\n')
        ++last;
    return RichText::MarkdownRange{first, last};
}
bool RichTextEditor::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT) noexcept
{
    const auto inputLifetime  = GetLifetimeToken();
    const auto inputRevision  = Revision();
    const auto inputEpoch     = _state->fileDropEpoch;
    const auto inputSelection = GetSelection();
    try
    {
        if (_state->scrollbarDragging)
        {
            const auto info = GetScrollInfo();
            if (! info || ! IsEnabled() || ControlCaptureAccess::GestureOwner(host) != this || ! std::isfinite(point.y))
                return false;
            const auto track   = ScrollbarTrack();
            const float extent = std::max(0.0f, info->contentHeightDip - info->viewportHeightDip);
            const auto thumb =
                ComputeScrollbarThumbHitRect(track, ScrollbarOrientation::Vertical, info->viewportHeightDip, info->contentHeightDip, info->offsetDip, extent);
            const float travel = (track.bottom - track.top) - (thumb.bottom - thumb.top);
            return travel > 0.0f && SetScrollOffsetDip(_state->scrollbarDragOffset + (point.y - _state->scrollbarDragY) * extent / travel);
        }
        const auto hover = IsEnabled() && IsVisible() && _state->linkUnderlinePolicy == RichTextLinkUnderlinePolicy::HoverOrCaret
                               ? HoveredLinkDisplayRange(host, point)
                               : std::nullopt;
        if (hover != _state->hoveredLink)
        {
            _state->hoveredLink = hover;
            const auto lifetime = GetLifetimeToken();
            RequestInvalidate();
            if (lifetime.expired() || GetHost() != &host)
                return true;
        }
        if (! _state->dragging || ! IsEnabled())
            return false;
        if (_state->codePressGesture)
        {
            const auto rect = _state->codePressRect;
            if (point.x < rect.left || point.x > rect.right || point.y < rect.top || point.y > rect.bottom)
                _state->codePressDocument.reset();
            return true;
        }
        if (_state->taskPressPlan)
        {
            const auto rect = _state->taskPressRect;
            if (point.x < rect.left || point.x > rect.right || point.y < rect.top || point.y > rect.bottom)
                _state->taskPressPlan.reset();
            else
                return true;
        }
        const auto viewport = TextViewport();
        if (point.y < viewport.top || point.y > viewport.bottom)
        {
            const auto lifetime = GetLifetimeToken();
            static_cast<void>(SetScrollOffsetDip(_state->scrollDip + (point.y < viewport.top ? -20.0f : 20.0f)));
            if (lifetime.expired() || GetHost() != &host || ! LayoutIsCurrent(host) || ! _state->dragging)
                return true;
        }
        const auto source = HitSource(host, point);
        _state->document.SetSelection({GetSelection().anchor, source});
        SelectionChanged();
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
    catch (const std::exception&)
    {
        return InputFailureHandled(inputLifetime, inputRevision, inputEpoch, inputSelection);
    }
}
bool RichTextEditor::OnMouseLeave(ControlHost&) noexcept
{
    if (! _state->hoveredLink)
        return false;
    _state->hoveredLink.reset();
    RequestInvalidate();
    return true;
}
bool RichTextEditor::OnMouseUp(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT) noexcept
{
    if (! rightButton && _state->scrollbarDragging)
    {
        const auto release        = ReleaseUnchangedCaptureOnExit(host, *this);
        _state->scrollbarDragging = false;
        RequestInvalidate();
        return true;
    }
    if (rightButton || ! _state->dragging)
        return false;
    if (_state->codePressGesture)
    {
        _state->dragging = _state->codePressGesture = false;
        auto document                               = std::move(_state->codePressDocument);
        const auto element                          = _state->codePressElement;
        const auto rect                             = _state->codePressRect;
        const auto lifetime                         = GetLifetimeToken();
        const auto interaction                      = host.GetInteractionRevision();
        const auto callback                         = _state->onCodeCopyRequested;
        host.ReleaseMouseCapture();
        if (! lifetime.expired() && GetHost() == &host && host.GetInteractionRevision() == interaction && document && callback &&
            _state->onCodeCopyRequested == callback && GetAcknowledgedSemanticDocument(host) == document && element < document->Elements().size() &&
            point.x >= rect.left && point.x <= rect.right && point.y >= rect.top && point.y <= rect.bottom)
            try
            {
                (*callback)(CodeCopyText(*document, document->Elements()[element]));
            }
            catch (const std::bad_alloc&)
            {
                // The released copy gesture is consumed without changing source or history.
            }
            catch (const std::exception&)
            {
                // A failed application copy handler does not edit source.
            }
        return true; // The callback may retire the control.
    }
    _state->dragging       = false;
    const auto lifetime    = GetLifetimeToken();
    const auto plan        = std::exchange(_state->taskPressPlan, {});
    const auto rect        = _state->taskPressRect;
    const auto interaction = host.GetInteractionRevision();
    host.ReleaseMouseCapture();
    if (plan && ! lifetime.expired() && GetHost() == &host && host.GetInteractionRevision() == interaction && point.x >= rect.left && point.x <= rect.right &&
        point.y >= rect.top && point.y <= rect.bottom)
        try
        {
            static_cast<void>(ApplyMarkdownCommand(*plan));
        }
        catch (const std::bad_alloc&)
        {
            // The release already ended the gesture. The document transaction
            // itself is atomic; report the owned release as handled below.
        }
        catch (const std::exception&)
        { /* Stale/rebound gestures make no source edit. */
        }
    if (! lifetime.expired() && GetHost() == &host)
        PublishSelectionChanged();
    return true;
}
bool RichTextEditor::OnMouseWheel(ControlHost& host, D2D1_POINT_2F, float delta, UINT modifiers) noexcept
{
    try
    {
        if (! IsEnabled() || ! std::isfinite(delta))
            return false;
        if (ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers))
        {
            const auto detents = std::clamp(delta / static_cast<float>(WHEEL_DELTA), -8.0f, 8.0f);
            SetZoom(std::clamp(GetZoom() * std::pow(1.1f, detents), .25f, 4.0f));
            return true;
        }
        RequireCurrentLayout(host);
        if (ModifiersContainShift(modifiers) && ! ModifiersContainAlt(modifiers))
        {
            const auto info = GetHorizontalScrollInfo();
            if (! info)
                return false;
            const float next = std::clamp(info->offsetDip - delta / WHEEL_DELTA * 48.0f, 0.0f, std::max(0.0f, info->contentWidthDip - info->viewportWidthDip));
            return next != info->offsetDip && SetHorizontalScrollOffsetDip(next);
        }
        const auto info = GetScrollInfo();
        if (! info)
            return false;
        const float next = std::clamp(info->offsetDip - delta / WHEEL_DELTA * 48.0f, 0.0f, std::max(0.0f, info->contentHeightDip - info->viewportHeightDip));
        // Let a containing scroll panel handle wheel input at this view's edge.
        return next != info->offsetDip && SetScrollOffsetDip(next);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
void RichTextEditor::PropagateHost(ControlHost* host) noexcept
{
    if (GetHost() != host)
    {
        _state->images.Cancel();
        RetireSemanticAccessibility();
        ++_state->bindingRevision;
        RetireLayout();
        _state->pendingLeadSurrogate = 0;
        _state->dragging             = false;
        _state->scrollbarDragging    = false;
        _state->blinkEpoch.reset();
    }
    Control::PropagateHost(host);
}
void RichTextEditor::OnBoundsChanged() noexcept
{
    _state->overlaySelectionDirty = true;
    const auto viewport           = TextViewport();
    if (_state->layoutWidth != std::max(1.0f, viewport.right - viewport.left))
        RetireLayout(true);
    else
    {
        _state->revealCaret = true;
        RequestContentPreparation(false);
    }
}
void RichTextEditor::OnHostDpiChanged(ControlHost&) noexcept
{
    RetireLayout();
}
void RichTextEditor::OnFlowDirectionChanged() noexcept
{
    RetireLayout();
}
void RichTextEditor::OnDensityChanged() noexcept
{
    RetireLayout();
}
void RichTextEditor::OnEnabledChanged(bool) noexcept
{
    _state->pendingLeadSurrogate = 0;
    _state->dragging             = false;
    _state->scrollbarDragging    = false;
    _state->blinkEpoch.reset();
}
void RichTextEditor::OnHidden() noexcept
{
    _state->images.Cancel();
    RetireLayout();
    _state->pendingLeadSurrogate = 0;
    _state->dragging             = false;
    _state->scrollbarDragging    = false;
    _state->blinkEpoch.reset();
}
void RichTextEditor::OnFocusChanged(ControlHost& host, bool focused)
{
    Control::OnFocusChanged(host, focused);
    _state->dragging          = false;
    _state->scrollbarDragging = false;
    _state->taskPressPlan.reset();
    _state->codePressDocument.reset();
    _state->codePressGesture = false;
    SelectionChanged();
}
void RichTextEditor::OnCaptureLost(ControlHost&)
{
    _state->dragging          = false;
    _state->scrollbarDragging = false;
    _state->taskPressPlan.reset();
    _state->codePressDocument.reset();
    _state->codePressGesture = false;
    PublishSelectionChanged();
}
} // namespace DxUi
