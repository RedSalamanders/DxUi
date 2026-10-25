#include "../Support/Diagnostics.h"
#include "DxUi.Internal.h"
#include "DxUi.RichAccessibilityEpoch.h"
#include "DxUi.RichEditorLayout.h"
#include "DxUi.RichImagePaths.h"
#include "DxUi.RichImageResources.h"
#include "DxUi.RichSemanticGeometry.h"
#include "DxUi.RichSourceInk.h"
#include "DxUi.SemanticExtensions.h"
#include "DxUi.TextElements.h"
#include "TextClipboard.h"
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <stdexcept>
#include <wil/result.h>

namespace DxUi
{
struct RichTextEditor::State
{
    explicit State(std::wstring text) : document(std::move(text)), documentFilePath(0u, L' ')
    {
    }
    RichText::Document document;
    RichTextEditorAppearance appearance;
    RichTextLinkUnderlinePolicy linkUnderlinePolicy = RichTextLinkUnderlinePolicy::Always;
    std::optional<RichText::MarkdownRange> hoveredLink;
    std::optional<RichText::MarkdownRange> caretLink;
    std::vector<RichTextSourceHighlight> highlights;
    bool focusMode = false;
    std::wstring documentFilePath;
    uint64_t imageLocationRevision = 1u;
    RichText::Detail::RichImageResources images;
    RichText::Detail::RichEditorLayout layout;
    RichText::MarkdownParser markdown;
    RichText::SemanticBuilder semantic;
    RichText::Detail::RichSourceInk sourceInk;
    struct SourceEditingSession
    {
        RichText::MarkdownSourceRegion region;
        RichText::SourceSnapshot snapshot;
    };
    std::optional<SourceEditingSession> sourceEditing;
    uint64_t nextSourceEditingSession = 1u;
    std::shared_ptr<const RichText::MarkdownSyntax> semanticRejected;
    struct NativeGeometryRejection
    {
        std::shared_ptr<const RichText::SemanticDocument> document;
        float width{};
        uint64_t typography{}, resources{}, binding{};
        bool rtl{};
    };
    std::optional<NativeGeometryRejection> nativeGeometryRejected;
    struct NativeObjectRejections
    {
        NativeObjectRejections() : objects(0u)
        {
        }
        std::shared_ptr<const RichText::MarkdownSyntax> syntax;
        std::vector<RichText::MarkdownRange> objects;
        float width{};
        uint64_t typography{}, resources{}, binding{}, appearance{};
        bool rtl{};
        bool Matches(const std::shared_ptr<const RichText::MarkdownSyntax>& value,
                     float nextWidth,
                     bool nextRtl,
                     uint64_t nextTypography,
                     uint64_t nextResources,
                     uint64_t nextBinding,
                     uint64_t nextAppearance) const noexcept
        {
            return syntax == value && width == nextWidth && rtl == nextRtl && typography == nextTypography && resources == nextResources &&
                   binding == nextBinding && appearance == nextAppearance;
        }
    };
    std::optional<NativeObjectRejections> nativeObjectRejections;
    bool RejectsNativeGeometry(
        const std::shared_ptr<const RichText::SemanticDocument>& value, float width, bool rtl, uint64_t typography, uint64_t resources) const noexcept
    {
        return nativeGeometryRejected && nativeGeometryRejected->document == value && nativeGeometryRejected->width == width &&
               nativeGeometryRejected->rtl == rtl && nativeGeometryRejected->typography == typography && nativeGeometryRejected->resources == resources &&
               nativeGeometryRejected->binding == bindingRevision;
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
    RichText::Selection publishedSelection{};
    bool publishedSelectionGesture      = false;
    uint64_t publishedSelectionRevision = 0u;
    uint64_t typographyRevision         = 0;
    uint64_t appearanceRevision         = 0;
    uint64_t bindingRevision            = 0;
    uint64_t resourceGeneration         = 0;
    uint64_t requestRevision = 0, requestBinding = 0, requestTypography = 0, requestResources = 0;
    IDWriteFactory* requestFactory = nullptr; // Identity only; staged layouts retain the factory through WIL.
    RichText::ViewMode requestMode = RichText::ViewMode::Rich;
    float requestWidth             = 0;
    bool requestActive             = false;
    size_t preparationAdvances = 0, cancelledPreparations = 0, lastAdvanceLayouts = 0, lastAdvanceBlockSteps = 0;
    float layoutWidth            = 0;
    float scrollDip              = 0;
    bool revealCaret             = true;
    bool readOnly                = false;
    bool dragging                = false;
    bool caretVisible            = true;
    wchar_t pendingLeadSurrogate = 0;
    std::optional<uint64_t> blinkEpoch;
    std::optional<float> preferredCaretX;
    // Native visual affinity only. Several padded cells share a real source ACP;
    // this identity never enters Document selection, TSF text or undo records.
    size_t sourceCaretSite = RichText::MarkdownNoNode;
    std::shared_ptr<const RichText::MarkdownCommandPlan> taskPressPlan;
    D2D1_RECT_F taskPressRect{};
};

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
    _state->images.Cancel();
    _state->sourceEditing.reset();
    _state->markdownCapacityRejected.reset();
    _state->nativeGeometryRejected.reset();
    RetireSemanticAccessibility();
    _state->sourceCaretSite = RichText::MarkdownNoNode;
    _state->scrollDip       = 0;
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
    const auto document = GetSemanticDocument();
    if (! GetHost() || ! IsVisible() || ! _state->markdownRendering || _state->mode != RichText::ViewMode::Rich || ! document ||
        ! _state->images.CurrentForCompletion(document, _state->imageLocationRevision, _state->bindingRevision))
        return false;
    const auto viewport = TextViewport();
    const auto* host    = GetHost();
    if (_state->RejectsNativeGeometry(
            document, std::max(1.0f, viewport.right - viewport.left), IsRightToLeft(), host->GetTypographyRevision(), host->GetResourceGeneration()))
        return false;
    const auto previous = _state->images.Snapshot();
    if (! _state->images.Complete(request, std::move(image)))
        return false;
    if (previous != _state->images.Snapshot())
    {
        RetireSemanticAccessibility();
        RetireLayout();
        RequestInvalidate();
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
bool RichTextEditor::CanPasteMarkdownFileReferences() const noexcept
{
    using namespace RichText;
    if (! Editable() || ! _state->markdownRendering || _state->mode != ViewMode::Rich || _state->sourceEditing)
        return false;
    const auto syntax = GetMarkdownSyntax();
    if (! syntax || syntax->SourceRegion())
        return false;
    const auto selection = GetSelection();
    const auto text      = syntax->Source();
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
    if (! Editable())
        return false;
    const bool references = CanPasteMarkdownFileReferences();
    const auto syntax     = GetMarkdownSyntax();
    std::wstring_view eol = L"\r\n";
    if (syntax)
    {
        const auto source = syntax->Source();
        if (const auto at = source.find_first_of(L"\r\n"); at != source.npos)
            eol = source.substr(at, source[at] == L'\r' && at + 1u < source.size() && source[at + 1u] == L'\n' ? 2u : 1u);
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
        if (content.filePaths.size() > TextClipboardContent::MaximumFiles)
            throw std::length_error("Clipboard file capacity exceeded");
        std::size_t totalFileUnits{};
        for (const auto& file : content.filePaths)
        {
            if (file.empty() || ! RichText::IsValidUtf16(file) || file.find(L'\0') != file.npos || file.size() > TextClipboardContent::MaximumFileUnits)
                throw std::invalid_argument("Invalid Unicode clipboard file path");
            if (file.size() > TextClipboardContent::MaximumTotalFileUnits - totalFileUnits)
                throw std::length_error("Clipboard file capacity exceeded");
            totalFileUnits += file.size();
        }
        bool allReferences = references;
        for (const auto& file : content.filePaths)
        {
            const auto markdown = allReferences ? RichText::Detail::BuildMarkdownFileReference(_state->documentFilePath, file) : std::nullopt;
            if (! markdown)
            {
                allReferences = false;
                break;
            }
            const auto separator = staged.empty() ? 0u : eol.size() * 2u;
            if (separator > RichText::Document::MaximumUnits - staged.size() || markdown->size() > RichText::Document::MaximumUnits - staged.size() - separator)
                throw std::length_error("File reference paste source capacity exceeded");
            if (separator)
                staged.append(eol).append(eol);
            staged.append(*markdown);
        }
        if (! allReferences)
        {
            // A raw/code context or unrepresentable file path keeps every
            // original spelling as quoted source; never drop an unsupported tail.
            staged.clear();
            for (const auto& file : content.filePaths)
            {
                if (file.empty() || ! RichText::IsValidUtf16(file) || file.find(L'\0') != file.npos || file.size() > TextClipboardContent::MaximumFileUnits)
                    throw std::invalid_argument("Invalid Unicode clipboard file path");
                const auto separator = staged.empty() ? 0u : eol.size();
                if (separator + 2u > RichText::Document::MaximumUnits - staged.size() ||
                    file.size() > RichText::Document::MaximumUnits - staged.size() - separator - 2u)
                    throw std::length_error("Clipboard file source capacity exceeded");
                if (! staged.empty())
                    staged.append(eol);
                staged.push_back(L'"');
                staged.append(file);
                staged.push_back(L'"');
                if (staged.size() > RichText::Document::MaximumUnits)
                    throw std::length_error("Clipboard file source capacity exceeded");
            }
        }
    }
    replacement.swap(staged);
    return true;
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
void RichTextEditor::SetViewMode(RichText::ViewMode mode)
{
    if (mode != RichText::ViewMode::Source && mode != RichText::ViewMode::Rich)
        throw std::invalid_argument("Invalid rich-text view mode");
    if (_state->mode == mode)
        return;
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
    const auto validFont = [](std::wstring_view value) noexcept
    { return value.size() <= 255u && value.find(L'\0') == value.npos && RichText::IsValidUtf16(value); };
    const auto validSize    = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 512.0f)); };
    const auto validHeight  = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 4.0f)); };
    const auto validSpacing = [](const std::optional<float>& value, float maximum) noexcept
    { return ! value || (std::isfinite(value.value()) && value.value() >= 0.0f && value.value() <= maximum); };
    if (! std::isfinite(appearance.maximumTextWidthDip) || appearance.maximumTextWidthDip < 0.0f || appearance.maximumTextWidthDip > 1048576.0f ||
        ! std::isfinite(appearance.paddingDip) || (appearance.paddingDip != -1.0f && (appearance.paddingDip < 0.0f || appearance.paddingDip > 256.0f)) ||
        ! validSize(appearance.bodyFontSizeDip) || ! validSize(appearance.codeFontSizeDip) || ! std::isfinite(appearance.lineHeightMultiplier) ||
        (appearance.lineHeightMultiplier != 0.0f && (appearance.lineHeightMultiplier < 1.0f || appearance.lineHeightMultiplier > 4.0f)) ||
        ! std::isfinite(appearance.headingScaleStep) || appearance.headingScaleStep < 0.0f || appearance.headingScaleStep > 1.0f ||
        ! validFont(appearance.bodyFontFamily) || ! validFont(appearance.codeFontFamily) || ! std::ranges::all_of(appearance.headingFontSizeDip, validSize) ||
        ! std::ranges::all_of(appearance.headingLineHeightMultiplier, validHeight) || ! validSpacing(appearance.paragraphSpacingAfterLines, 4.0f) ||
        ! validSpacing(appearance.listItemSpacingDip, 256.0f) || (appearance.semanticSpacing && ! appearance.semanticSpacing->IsValid()))
        throw std::invalid_argument("Invalid rich editor appearance");
    if (_state->appearance == appearance)
        return;
    auto next                   = appearance; // Stage all owned strings before publishing any option.
    const auto& previous        = _state->appearance;
    const bool geometryChanged  = previous.maximumTextWidthDip != next.maximumTextWidthDip || previous.paddingDip != next.paddingDip ||
                                  previous.bodyFontSizeDip != next.bodyFontSizeDip || previous.codeFontSizeDip != next.codeFontSizeDip ||
                                  previous.lineHeightMultiplier != next.lineHeightMultiplier || previous.headingScaleStep != next.headingScaleStep ||
                                  previous.bodyFontFamily != next.bodyFontFamily || previous.codeFontFamily != next.codeFontFamily ||
                                  previous.headingFontSizeDip != next.headingFontSizeDip ||
                                  previous.headingLineHeightMultiplier != next.headingLineHeightMultiplier ||
                                  previous.paragraphSpacingAfterLines != next.paragraphSpacingAfterLines ||
                                  previous.listItemSpacingDip != next.listItemSpacingDip || previous.semanticSpacing != next.semanticSpacing;
    const bool sourceInkChanged = previous.colorizeMarkdownSource != next.colorizeMarkdownSource;
    // Staged strings publish without allocator-backed aggregate moves; Debug
    // STL proxies can allocate during those moves.
    auto& value                       = _state->appearance;
    value.drawBorder                  = next.drawBorder;
    value.drawFocusIndicator          = next.drawFocusIndicator;
    value.blendWithWindowBackground   = next.blendWithWindowBackground;
    value.useAccentLinkColor          = next.useAccentLinkColor;
    value.colorizeMarkdownSource      = next.colorizeMarkdownSource;
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
    RequestInvalidate();
}
void RichTextEditor::ClearSourceHighlights() noexcept
{
    if (_state->highlights.empty())
        return;
    _state->highlights.clear();
    RequestInvalidate();
}
std::span<const RichTextSourceHighlight> RichTextEditor::GetSourceHighlights() const noexcept
{
    return _state->highlights;
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
        if (_state->RejectsNativeGeometry(GetSemanticDocument(),
                                          std::max(1.0f, viewport.right - viewport.left),
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
        bounds->left        = std::max(viewport.left, bounds->left + viewport.left);
        bounds->right       = std::min(viewport.right, bounds->right + viewport.left);
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
bool RichTextEditor::NavigateSemanticElement(ControlHost& host, const std::shared_ptr<const RichText::SemanticDocument>& expected, size_t element)
{
    if (! expected || GetHost() != &host || ! IsEnabled() || GetAcknowledgedSemanticDocument(host) != expected)
        return false;
    const auto target = expected->NavigationTarget(element);
    if (! target)
        return false;
    if (host.GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host.TryReadNativeTextInputState(this, input);
        if ((host.HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return false;
    }
    const auto lifetime = GetLifetimeToken();
    if (! ScrollSourceRangeIntoView(host, target->first, target->last, true))
        return false;
    if (lifetime.expired() || GetHost() != &host || GetAcknowledgedSemanticDocument(host) != expected)
        return false;
    // Selection synchronization can re-enter or destroy the owner. Complete
    // scrolling first and never access this control after SetSelection returns.
    SetSelection({target->first, target->last});
    return true;
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
    const auto node = expected->SourceEditingNode(element);
    if (! node || _state->sourceEditing)
        return false;
    const auto& block = expected->Syntax()->Nodes()[*node].block;
    auto session      = _state->nextSourceEditingSession++;
    if (! session)
        session = _state->nextSourceEditingSession++;
    _state->sourceEditing.emplace(State::SourceEditingSession{{block.source, session}, Snapshot()});
    _state->document.SetSelection({block.content.first, block.content.first});
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
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        NativeTextInputState input{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        const bool available = host->TryReadNativeTextInputState(this, input);
        if ((host->HasActiveNativeTextInputSession() && ! available) || (available && (input.compositionStartIndex || input.compositionEndIndex)))
            return MarkdownCommandStatus::Unavailable;
    }
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
    {
        _state->images.Cancel();
        UpdateSemanticSourceRegion();
        RetireSemanticAccessibility();
        _state->taskPressPlan.reset();
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
void RichTextEditor::SelectionChanged(bool synchronizeTextInput, bool preservePreferredCaretX, bool publishSelection)
{
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
    _state->pendingLeadSurrogate = 0;
    if (! preservePreferredCaretX)
        _state->preferredCaretX.reset();
    _state->revealCaret  = true;
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
    if (appearance.maximumTextWidthDip > 0.0f && result.right - result.left > appearance.maximumTextWidthDip)
    {
        result.left += (result.right - result.left - appearance.maximumTextWidthDip) * 0.5f;
        result.right = result.left + appearance.maximumTextWidthDip;
    }
    return result;
}
void RichTextEditor::RetireLayout() const noexcept
{
    _state->sourceInk.Clear();
    _state->hoveredLink.reset();
    _state->caretLink.reset();
    _state->taskPressPlan.reset();
    if (_state->layout.HasPendingPreparation())
        ++_state->cancelledPreparations;
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
void RichTextEditor::CancelContentPreparation() noexcept
{
    _state->images.Cancel();
    RetireLayout();
}
HRESULT RichTextEditor::PrepareContent(ControlHost& host) noexcept
{
    auto& state              = *_state;
    state.lastAdvanceLayouts = state.lastAdvanceBlockSteps = 0u;
    state.lastAdvanceMarkdownWork                          = 0u;
    state.lastAdvanceSemanticWork                          = 0u;
    const auto rejectNativeObject                          = [&](std::size_t element) -> HRESULT
    {
        const auto value = GetSemanticDocument();
        if (value && state.markdownRendering && state.mode == RichText::ViewMode::Rich)
        {
            if (element < value->Elements().size() &&
                (value->Elements()[element].role == RichText::SemanticRole::Math || value->Elements()[element].role == RichText::SemanticRole::Diagram))
            {
                try
                {
                    if (! state.nativeObjectRejections)
                    {
                        auto& rejected      = state.nativeObjectRejections.emplace();
                        rejected.syntax     = value->Syntax();
                        rejected.width      = std::max(1.0f, TextViewport().right - TextViewport().left);
                        rejected.typography = host.GetTypographyRevision();
                        rejected.resources  = host.GetResourceGeneration();
                        rejected.binding    = state.bindingRevision;
                        rejected.appearance = state.appearanceRevision;
                        rejected.rtl        = IsRightToLeft();
                    }
                    auto& objects     = state.nativeObjectRejections->objects;
                    const auto source = value->Elements()[element].source;
                    if (objects.size() < RichText::Detail::SemanticExtensions::MaximumObjects && std::ranges::find(objects, source) == objects.end())
                    {
                        objects.push_back(source);
                        std::ranges::sort(objects, {}, &RichText::MarkdownRange::first);
                        RetireSemanticAccessibility();
                        state.semantic.Clear();
                        state.layout.Invalidate();
                        state.requestActive = false;
                        return S_FALSE;
                    }
                }
                catch (const std::bad_alloc&)
                {
                    // This noexcept preparation boundary reports resource
                    // exhaustion without publishing a partial rejection set.
                    state.nativeObjectRejections.reset();
                    state.layout.Invalidate();
                    state.requestActive = false;
                    return E_OUTOFMEMORY;
                }
            }
            state.nativeGeometryRejected.emplace(value,
                                                 std::max(1.0f, TextViewport().right - TextViewport().left),
                                                 host.GetTypographyRevision(),
                                                 host.GetResourceGeneration(),
                                                 state.bindingRevision,
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
        if (LayoutIsCurrent(host))
        {
            UpdateCaretScroll();
            return PrepareSourceInk() ? S_OK : S_FALSE;
        }
        if (state.markdownProfile && ! GetMarkdownSyntax() && ! state.HasCurrentMarkdownCapacityRejection())
        {
            try
            {
                if (! state.markdownRequestActive || state.markdownRequestRevision != Revision() || ! state.markdown.HasPendingPreparation())
                {
                    // Exact bounded source admission/copy is still indivisible.
                    // Parsing resumes its own cursors, without DirectWrite or a
                    // per-control worker. Semantic preparation consumes only a
                    // completed current tree on a later bounded stage.
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
        const auto width      = std::max(1.0f, viewport.right - viewport.left);
        const auto typography = host.GetTypographyRevision(), resources = host.GetResourceGeneration();
        if (state.nativeObjectRejections &&
            ! state.nativeObjectRejections->Matches(
                GetMarkdownSyntax(), width, IsRightToLeft(), typography, resources, state.bindingRevision, state.appearanceRevision))
        {
            // A width/font/device/binding change retries only the rejected
            // native objects. Their previous literal publication is immutable.
            state.nativeObjectRejections.reset();
            RetireSemanticAccessibility();
            state.semantic.Clear();
        }
        std::shared_ptr<const RichText::SemanticDocument> semanticDocument;
        if (state.markdownRendering && state.mode == RichText::ViewMode::Rich)
        {
            const auto syntax = GetMarkdownSyntax();
            semanticDocument  = GetSemanticDocument();
            if (syntax && ! semanticDocument && state.semanticRejected != syntax)
            {
                try
                {
                    if (! state.semantic.HasPendingPreparation())
                    {
                        if (state.nativeObjectRejections)
                            RichText::Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(state.semantic, syntax, state.nativeObjectRejections->objects);
                        else
                            state.semantic.Begin(syntax);
                    }
                    ++state.semanticAdvances;
                    const auto advance            = state.semantic.Advance();
                    state.lastAdvanceSemanticWork = advance.workUnits;
                    if (advance.status == RichText::MarkdownPreparationStatus::Pending)
                        return S_FALSE;
                    semanticDocument = GetSemanticDocument();
                }
                catch (const std::length_error&)
                {
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
        if (semanticDocument && state.RejectsNativeGeometry(semanticDocument, width, IsRightToLeft(), typography, resources))
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
        const bool sameRequest = state.requestActive && state.layout.HasPendingPreparation() && state.requestFactory == factory &&
                                 state.requestRevision == revision && state.requestBinding == binding && state.requestTypography == typography &&
                                 state.requestResources == resources && state.requestWidth == width && state.requestMode == state.mode;
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
            if (! appearance.bodyFontFamily.empty())
                options.fontFamily = appearance.bodyFontFamily;
            if (! appearance.codeFontFamily.empty())
                options.codeFontFamily = appearance.codeFontFamily;
            options.readingDirection = IsRightToLeft() ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
            wil::com_ptr<ID2D1Factory> drawing;
            if (semanticDocument && ! RichText::Detail::SemanticExtensionAccess::Diagrams(*semanticDocument).empty())
            {
                auto* context = host.GetDeviceContext();
                if (! context)
                    THROW_HR(E_UNEXPECTED);
                context->GetFactory(drawing.put());
            }
            state.layout.BeginPreparation(factory,
                                          state.document.Snapshot(),
                                          revision,
                                          effectiveMode,
                                          options,
                                          semanticDocument,
                                          drawing.get(),
                                          {typography, resources},
                                          state.images.Snapshot(),
                                          host.GetDeviceContext());
            state.requestRevision   = revision;
            state.requestBinding    = binding;
            state.requestTypography = typography;
            state.requestResources  = resources;
            state.requestWidth      = width;
            state.requestMode       = state.mode;
            state.requestFactory    = factory;
            state.requestActive     = true;
        }
        if (state.layout.HasPendingPreparation())
        {
            ++state.preparationAdvances;
            const auto advance          = state.layout.AdvancePreparation();
            state.lastAdvanceLayouts    = advance.layoutsPrepared;
            state.lastAdvanceBlockSteps = advance.blockSteps;
            if (advance.status == RichText::LayoutPreparationStatus::Pending)
                return S_FALSE;
        }
        // No application callback crosses staging. Recheck the complete key
        // before acknowledging geometry; host publication follows rasterization.
        if (GetHost() != &host || Revision() != revision || state.bindingRevision != binding || state.mode != state.requestMode ||
            host.GetResourceGeneration() != resources || host.GetTypographyRevision() != typography ||
            std::max(1.0f, TextViewport().right - TextViewport().left) != width)
        {
            state.layout.Invalidate();
            state.requestActive = false;
            return S_FALSE;
        }
        state.layoutWidth        = width;
        state.typographyRevision = typography;
        state.resourceGeneration = resources;
        state.requestActive      = false;
        UpdateCaretScroll();
        UpdateCaretLinkDisplayRange();
        return PrepareSourceInk() ? S_OK : S_FALSE;
    }
    catch (const RichText::Detail::NativeObjectUnsupported& failure)
    {
        if (failure.Error() != DWRITE_E_UNSUPPORTEDOPERATION)
            return failure.Error();
        return rejectNativeObject(failure.Element());
    }
    catch (const RichText::Detail::NativeObjectCapacity& failure)
    {
        // Bounded geometry capacity and the explicitly classified font-local
        // unsupported operation both reject only the owning object/key.
        return rejectNativeObject(failure.Element());
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
    UpdateCaretScroll();
}
void RichTextEditor::UpdateCaretScroll() const
{
    auto& state               = *_state;
    const auto previousScroll = state.scrollDip;
    const auto viewport       = TextViewport();
    const float height        = std::max(0.0f, viewport.bottom - viewport.top);
    if (state.revealCaret)
    {
        const auto caret = state.layout.SourceCaret(state.document.GetSelection().caret, state.sourceCaretSite);
        if (caret.y < state.scrollDip)
            state.scrollDip = caret.y;
        if (caret.y + caret.height > state.scrollDip + height)
            state.scrollDip = caret.y + caret.height - height;
        state.revealCaret = false;
    }
    state.scrollDip = std::clamp(state.scrollDip, 0.0f, std::max(0.0f, state.layout.Height() - height));
    if (state.scrollDip != previousScroll)
        state.hoveredLink.reset();
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
            _state->sourceInk.Ranges().size()};
}
bool RichTextEditor::SupportsTextInput() const noexcept
{
    return true;
}
std::optional<std::vector<std::pair<size_t, size_t>>> RichTextEditor::GetSourceVisualLineRanges(const ControlHost& host) const noexcept
{
    try
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
            // Earliest source affinity makes adjacent lines partition exact
            // source, including hidden markers and original CRLF pairs.
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
    catch (const std::exception&)
    {
        return std::nullopt;
    }
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
        UpdateCaretScroll();
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
        _state->revealCaret = false;
        if (_state->scrollDip == offset)
            return true;
        _state->scrollDip = offset;
        _state->hoveredLink.reset();
        // Synchronization and invalidation can invoke application scheduling.
        // Capture identity before crossing either boundary; never resume on a
        // detached/replaced editor. Neither operation changes source/history.
        const auto lifetime = GetLifetimeToken();
        const auto binding  = _state->bindingRevision;
        host.SyncTextInput(this);
        if (lifetime.expired() || GetHost() != &host || _state->bindingRevision != binding)
            return false;
        RequestInvalidate();
        return ! lifetime.expired() && GetHost() == &host && _state->bindingRevision == binding;
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
        return D2D1::RectF(viewport.left + caret.x,
                           viewport.top + caret.y - _state->scrollDip,
                           viewport.left + caret.x + 1,
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
            rectangle.left += viewport.left;
            rectangle.right += viewport.left;
            rectangle.top += viewport.top - _state->scrollDip;
            rectangle.bottom += viewport.top - _state->scrollDip;
        }
        // Service transport needs the raw extents to report clipping truthfully.
        // UIA clips these separately and keeps hidden delimiters pixel-empty.
        // Construct the contained vector fallibly in place. Converting a vector
        // temporary into optional would invoke MSVC Debug's noexcept move.
        return std::optional<std::vector<D2D1_RECT_F>>(std::in_place, rectangles);
    }
    catch (const std::exception&)
    {
        // Native text-store geometry is a noexcept COM boundary. A failed
        // preparation/allocation reports unavailable layout, never termination.
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
        return _state->layout.HitTestSource(point.x - viewport.left, point.y - viewport.top + _state->scrollDip).position;
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
    auto& state                     = *_state;
    auto* textBrush                 = host.GetSolidBrush(style.text);
    auto* selectedTextBrush         = host.GetSolidBrush(style.selectionText);
    auto* selectionBrush            = host.GetSolidBrush(style.selectionFill);
    auto* caretBrush                = host.GetSolidBrush(style.caret);
    ID2D1Brush* diagramFill         = nullptr;
    ID2D1Brush* selectedDiagramFill = nullptr;
    if (const auto* semantic = state.layout.SemanticGeometry(); semantic && semantic->HasNativeObjects())
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
    const auto rectangles  = projection.SourceRange(selection.Start(), selection.End());
    const auto origin      = D2D1::Point2F(viewport.left, viewport.top - state.scrollDip);
    context->PushAxisAlignedClip(viewport, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    const auto restoreClip = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
    for (const auto& highlight : state.highlights)
    {
        auto color = host.GetTheme().selectionFill;
        color.a *= host.GetTheme().highContrast ? 1.0f : highlight.current ? 0.38f : 0.18f;
        auto* brush = host.GetSolidBrush(color);
        for (const auto& rectangle : projection.SourceRange(highlight.start, highlight.start + highlight.length))
        {
            const auto bounds = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
            if (host.GetTheme().highContrast)
                context->DrawRectangle(bounds, brush, highlight.current ? 2.0f : 1.0f);
            else
                context->FillRectangle(bounds, brush);
        }
    }
    RichText::LinkDrawing links;
    links.brush     = appearance.useAccentLinkColor && IsEnabled() ? host.GetSolidBrush(host.GetTheme().accent) : nullptr;
    links.underline = state.linkUnderlinePolicy == RichTextLinkUnderlinePolicy::Always || host.GetTheme().highContrast;
    links.hover     = state.hoveredLink;
    links.caret     = CaretLinkDisplayRange();
    if (state.mode == RichText::ViewMode::Source && appearance.colorizeMarkdownSource && ! host.GetTheme().highContrast)
    {
        links.inkRanges = state.sourceInk.Ranges();
        // Keep the ordinary first pass beneath true selection. Translucent
        // selection fill and antialiased selected glyph edges must never retain
        // colored syntax ink from underneath their selected-text pass.
        if (selection.anchor != selection.caret)
            links.inkExclusion = RichText::MarkdownRange{projection.ToDisplay(selection.Start()), projection.ToDisplay(selection.End())};
        links.inkBrushes = {
            host.GetSolidBrush(host.GetTheme().subduedText), host.GetSolidBrush(host.GetTheme().accent), host.GetSolidBrush(host.GetTheme().accent)};
    }
    const bool dimmed = state.focusMode && state.mode == RichText::ViewMode::Rich && ! host.GetTheme().highContrast && IsEnabled();
    auto* subdued     = dimmed ? host.GetSolidBrush(host.GetTheme().subduedText) : textBrush;
    if (dimmed)
    {
        links.emphasis          = ActiveParagraphDisplayRange();
        links.emphasisBrush     = textBrush;
        links.inactiveLinkBrush = subdued;
    }
    const bool decorated = ! links.inkRanges.empty() || dimmed || (state.mode == RichText::ViewMode::Rich && (links.brush || ! links.underline));
    state.layout.Draw(context,
                      subdued,
                      dimmed ? subdued : caretBrush,
                      origin,
                      state.scrollDip,
                      state.scrollDip + viewport.bottom - viewport.top,
                      diagramFill,
                      1.0f,
                      decorated ? &links : nullptr);
    auto selectedLinks              = links;
    selectedLinks.brush             = selectedTextBrush;
    selectedLinks.emphasisBrush     = selectedTextBrush;
    selectedLinks.inactiveLinkBrush = selectedTextBrush;
    selectedLinks.inkBrushes.fill(selectedTextBrush);
    for (const auto& rectangle : rectangles)
    {
        const auto translated = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
        if (translated.bottom <= viewport.top || translated.top >= viewport.bottom)
            continue;
        context->FillRectangle(translated, selectionBrush);
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
                          decorated ? &selectedLinks : nullptr);
    }
    NativeTextInputState input;
    if (host.TryReadNativeTextInputState(this, input) && projection.SourceMatches(input.text) && input.compositionStartIndex && input.compositionEndIndex)
    {
        const auto underline = [&](size_t first, size_t last, float thickness)
        {
            if (first > last || last > projection.SourceLength())
                return;
            for (const auto& rect : projection.SourceRange(first, last))
                context->DrawLine(D2D1::Point2F(rect.left + origin.x, rect.bottom + origin.y - 1),
                                  D2D1::Point2F(rect.right + origin.x, rect.bottom + origin.y - 1),
                                  caretBrush,
                                  thickness);
        };
        underline(*input.compositionStartIndex, *input.compositionEndIndex, 1.0f);
        if (input.conversionTargetStartIndex && input.conversionTargetEndIndex)
            underline(*input.conversionTargetStartIndex, *input.conversionTargetEndIndex, 2.0f);
    }
    if (HasFocus() && IsEnabled() && state.caretVisible)
    {
        const auto caret = state.layout.SourceCaret(selection.caret, state.sourceCaretSite);
        const float x    = std::clamp(origin.x + caret.x, viewport.left, std::max(viewport.left, viewport.right - 1.0f));
        context->DrawLine(D2D1::Point2F(x, origin.y + caret.y), D2D1::Point2F(x, origin.y + caret.y + caret.height), caretBrush, 1.0f);
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
    const auto hit          = _state->layout.HitTestSource(point.x - viewport.left, point.y - viewport.top + _state->scrollDip);
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
        const auto projection = RichText::Project(Snapshot(), Revision(), _state->markdownRendering ? RichText::ViewMode::Source : _state->mode);
        const auto position   = projection.ToDisplay(selection.caret);
        size_t next;
        switch (key)
        {
            case VK_LEFT: next = byWord ? FindPreviousWordBoundary(projection.text, position) : StepToPreviousTextElement(projection.text, position); break;
            case VK_RIGHT: next = byWord ? FindNextWordBoundary(projection.text, position) : StepToNextTextElement(projection.text, position); break;
            case VK_HOME: next = 0u; break;
            default: next = projection.text.size(); break;
        }
        const auto source = projection.ToSource(next, key == VK_RIGHT || key == VK_END);
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
bool RichTextEditor::Erase(ControlHost& host, bool backwards)
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
            const auto range = GetTextElementDeletionRange(source.Text(), selection.caret, backwards);
            first            = range.first;
            last             = range.last;
        }
        const bool changed = _state->document.Replace(first, last, {}, {first, first});
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
                if (node.block.kind != RichText::MarkdownBlockKind::Paragraph || selection.caret < node.block.content.first ||
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
            const auto source  = Snapshot();
            const auto first   = backwards ? StepToPreviousTextElement(source.Text(), selection.caret) : selection.caret;
            const auto last    = backwards ? selection.caret : StepToNextTextElement(source.Text(), selection.caret);
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
            if (LayoutIsCurrent(host) && _state->layout.Semantic())
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
            return ApplyMarkdownCommand(plan) == RichText::MarkdownCommandStatus::Changed;
        }
    }
    if (! LayoutIsCurrent(host))
    {
        const auto projection = RichText::Project(Snapshot(), Revision(), _state->markdownRendering ? RichText::ViewMode::Source : _state->mode);
        const auto range      = GetTextElementDeletionRange(projection.text, projection.ToDisplay(selection.caret), backwards);
        if (range.first == range.last)
            return false;
        const auto first = projection.ToSource(range.first), last = projection.ToSource(range.last, true);
        const bool changed = _state->document.Replace(first, last, {}, {first, first});
        Changed(changed);
        return changed;
    }
    auto& state              = *_state;
    const auto& projection   = state.layout;
    const auto position      = projection.ToDisplay(selection.caret);
    const auto [first, last] = state.layout.DeletionRange(position, backwards);
    if (first == last)
        return false;
    const auto begin   = projection.ToSource(first);
    const auto end     = projection.ToSource(last, true);
    const bool changed = state.document.Replace(begin, end, {}, {begin, begin});
    Changed(changed);
    return changed;
}
bool RichTextEditor::OnChar(ControlHost& host, wchar_t character, UINT modifiers) noexcept
{
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
        std::wstring text;
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
                if (! _state->sourceEditing && _state->markdownRendering && _state->mode == RichText::ViewMode::Rich && ! ModifiersContainShift(modifiers))
                {
                    RichText::MarkdownCommandRequest request;
                    request.command = RichText::MarkdownCommand::InsertParagraph;
                    if (LayoutIsCurrent(host))
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
                text = L"\r\n";
            }
            else if (character >= L' ' && character != L'\x7f')
                text.assign(1, character);
            else
                return false; // Tab is host traversal; control characters never enter the source.
        }
        if (_state->markdownRendering && _state->mode == RichText::ViewMode::Rich && LayoutIsCurrent(host))
            if (const auto* geometry = _state->layout.SemanticGeometry(); geometry && geometry->IsCellSourceSite(GetSelection().caret, _state->sourceCaretSite))
            {
                RichText::MarkdownCommandRequest request;
                request.command    = RichText::MarkdownCommand::SetTableCell;
                request.targetCell = geometry->SourceSyntaxNode(GetSelection().caret, _state->sourceCaretSite);
                if (text == L"|")
                    text = L"\\|";
                else if (text == L"\\")
                    text = L"\\\\";
                request.text = text;
                static_cast<void>(ExecuteMarkdownCommand(request));
                return true;
            }
        static_cast<void>(ReplaceSelection(std::move(text)));
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnKeyDown(ControlHost& host, UINT key, UINT modifiers) noexcept
{
    try
    {
        if (! IsEnabled())
            return false;
        _state->pendingLeadSurrogate = 0;
        const bool control = ModifiersContainCtrl(modifiers), shift = ModifiersContainShift(modifiers);
        if (ModifiersContainAlt(modifiers))
            return false;
        if (_state->sourceEditing && (key == VK_ESCAPE || (control && key == VK_RETURN && ! shift) || (key == VK_F2 && ! control && ! shift)))
            return EndSemanticSourceEdit(host);
        if (key == VK_F2 && ! control && ! shift)
        {
            if (const auto document = GetAcknowledgedSemanticDocument(host))
            {
                const auto caret = GetSelection().caret;
                auto trailing    = RichText::MarkdownNoNode;
                for (std::size_t element = 0u; element < document->Elements().size(); ++element)
                    if (const auto node = document->SourceEditingNode(element))
                    {
                        const auto source = document->Syntax()->Nodes()[*node].block.source;
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
            if (key == VK_RETURN && ! shift)
                if (const auto document = GetAcknowledgedSemanticDocument(host))
                    for (const auto& navigation : document->Navigation())
                    {
                        const auto& source = document->Elements()[navigation.element].source;
                        if (source.first < source.last && GetSelection().caret >= source.first && GetSelection().caret < source.last)
                            return NavigateSemanticElement(host, document, navigation.element);
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
                    TextClipboardContent content;
                    const bool available = host.ReadClipboardContent(content);
                    if (available && ! lifetime.expired() && GetHost() == &host && _state->bindingRevision == binding && Revision() == revision &&
                        _state->imageLocationRevision == location && GetSelection() == selection)
                    {
                        std::wstring replacement(0u, L' ');
                        if (TryPrepareClipboardPaste(content, replacement))
                            static_cast<void>(ReplaceSelection(std::move(replacement)));
                        else if (! content.text.empty())
                            static_cast<void>(ReplaceSelection(std::move(content.text)));
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
            case VK_DELETE: static_cast<void>(Erase(host, key == VK_BACK)); return true;
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
                _state->dragging = false;
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
        // Allocation failure is fatal at this native key input boundary.
        std::terminate();
    }
    catch (const std::exception&)
    {
        return false;
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
        const auto source = HitSource(host, point);
        if (ModifiersContainCtrl(modifiers) && ! ModifiersContainShift(modifiers))
            if (const auto document = GetAcknowledgedSemanticDocument(host))
                for (const auto& navigation : document->Navigation())
                    if (const auto bounds = GetSemanticElementBounds(host, navigation.element);
                        bounds && point.x >= bounds->left && point.x <= bounds->right && point.y >= bounds->top && point.y <= bounds->bottom)
                        return NavigateSemanticElement(host, document, navigation.element);
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
                    bounds->left += viewport.left;
                    bounds->right += viewport.left;
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
        // Allocation failure is fatal at this native pointer input boundary.
        std::terminate();
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT) noexcept
{
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
    catch (const std::exception&)
    {
        return false;
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
    const auto x = point.x - viewport.left, y = point.y - viewport.top + _state->scrollDip;
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
    try
    {
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
            _state->scrollDip   = std::max(0.0f, _state->scrollDip + (point.y < viewport.top ? -20.0f : 20.0f));
            _state->revealCaret = false;
        }
        const auto source = HitSource(host, point);
        _state->document.SetSelection({GetSelection().anchor, source});
        SelectionChanged();
        return true;
    }
    catch (const std::bad_alloc&)
    {
        // Fatal allocation failure must not be swallowed at this native input boundary.
        std::terminate();
    }
    catch (const std::exception&)
    {
        return false;
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
    if (rightButton || ! _state->dragging)
        return false;
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
            // Allocation failure is fatal at this native input boundary.
            std::terminate();
        }
        catch (const std::exception&)
        { /* Stale/rebound gestures make no source edit. */
        }
    if (! lifetime.expired() && GetHost() == &host)
        PublishSelectionChanged();
    return true;
}
bool RichTextEditor::OnMouseWheel(ControlHost& host, D2D1_POINT_2F, float delta, UINT) noexcept
{
    try
    {
        if (! IsEnabled() || ! std::isfinite(delta))
            return false;
        RequireCurrentLayout(host);
        const auto viewport = TextViewport();
        _state->scrollDip =
            std::clamp(_state->scrollDip - delta / WHEEL_DELTA * 48.0f, 0.0f, std::max(0.0f, _state->layout.Height() - (viewport.bottom - viewport.top)));
        _state->revealCaret = false;
        _state->hoveredLink.reset();
        RequestInvalidate();
        return true;
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
        _state->blinkEpoch.reset();
    }
    Control::PropagateHost(host);
}
void RichTextEditor::OnBoundsChanged() noexcept
{
    const auto viewport = TextViewport();
    if (_state->layoutWidth != std::max(1.0f, viewport.right - viewport.left))
        RetireLayout();
    else
        _state->revealCaret = true;
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
    _state->blinkEpoch.reset();
}
void RichTextEditor::OnHidden() noexcept
{
    _state->images.Cancel();
    RetireLayout();
    _state->pendingLeadSurrogate = 0;
    _state->dragging             = false;
    _state->blinkEpoch.reset();
}
void RichTextEditor::OnFocusChanged(ControlHost& host, bool focused)
{
    Control::OnFocusChanged(host, focused);
    _state->dragging = false;
    _state->taskPressPlan.reset();
    SelectionChanged();
}
void RichTextEditor::OnCaptureLost(ControlHost&)
{
    _state->dragging = false;
    _state->taskPressPlan.reset();
    PublishSelectionChanged();
}
} // namespace DxUi
