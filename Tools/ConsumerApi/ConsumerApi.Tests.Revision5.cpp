// Frozen consumer compile fixture for API revision 5. No private headers or test flags.
#include <DxUi/DxUi.h>
#include <DxUi/Failure.h>
#include <DxUi/MarkdownSyntax.h>
#include <type_traits>

static_assert(std::is_standard_layout_v<DxUi::Failure::Site>);
static_assert(std::is_standard_layout_v<DxUi::Failure::Record>);
static_assert(std::is_standard_layout_v<DxUi::Failure::HostServices>);
static_assert(! std::is_copy_constructible_v<DxUi::Failure::Scope>);
static_assert(! std::is_copy_constructible_v<DxUi::Failure::ProcessHooks>);
static_assert(! std::is_copy_constructible_v<DxUi::Failure::ModuleRegistration>);
static_assert(std::is_same_v<decltype(&DxUi::Failure::RegisterModule), HRESULT (*)(const DxUi::Failure::HostServices&) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Failure::UnregisterModule), HRESULT (*)(const DxUi::Failure::HostServices&) noexcept>);
static_assert(std::is_same_v<decltype(DxUi::RichText::MarkdownAdvance{}.result), DxUi::RichText::MarkdownResult>);
static_assert(std::is_same_v<decltype(&DxUi::RichTextEditor::TrySetDecorations),
                             HRESULT (DxUi::RichTextEditor::*)(const DxUi::RichText::SourceSnapshot&, std::span<const DxUi::RichTextDecoration>)>);
static_assert(DxUi::Failure::ProtocolVersion == 1);

// Proposed public checked-result surface. Compile-only API checks do not qualify
// runtime refusal, atomicity, host integration or consumer product behavior.
#include <DxUi/RichAccessibilityText.h>
#include <DxUi/RichDocument.h>
#include <DxUi/RichMarkdown.h>
#include <DxUi/RichSemantic.h>
#include <DxUi/RichSemanticDocument.h>
#include <DxUi/RichTextCommands.h>
#include <DxUi/RichTextExport.h>
#include <DxUi/RichTextLayout.h>
#include <utility>

namespace Api5 = DxUi::RichText;
using Editor   = DxUi::RichTextEditor;
using Source   = Api5::SourceSnapshot;
using Result   = Api5::SourceResult;
template <class T> T& ref() noexcept;
template <class T> const T& cref() noexcept;
static_assert(noexcept(DxUi::Separator()));
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetText({})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetSelection({})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetDocumentFilePath({})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetViewMode(Api5::ViewMode{})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetAppearance(cref<DxUi::RichTextEditorAppearance>())), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetZoom(1.0f)), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetSearchQuery({})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetMarkdownAnalysisProfile({})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetMarkdownAnalysisExtensions({})), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetMarkdownRenderingEnabled(true)), Result>);
static_assert(std::is_same_v<decltype(ref<Editor>().TrySetDefaultContextMenuLabels(nullptr)), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::Document>().TryReset({})), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::Document>().TrySetSelection({})), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::Document>().TryReplace(0u, 0u, {}, {}, ref<bool>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::Document>().TryReplaceSelection({}, ref<bool>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::Document>().TryReplaceWholeText({}, {}, ref<bool>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::Document>().TryReplaceTransient({}, {}, ref<bool>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryDecodeUtf8({}, ref<std::wstring>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryEncodeUtf8({}, ref<std::string>())), Result>);
static_assert(std::is_same_v<decltype(cref<Source>().TrySlice(0u, 0u, ref<std::wstring>())), Result>);
static_assert(std::is_same_v<decltype(cref<Api5::MarkdownSyntax>().TryLexemeValue(0u, ref<std::wstring_view>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::MarkdownParser>().TryBegin(cref<Source>(), 0u)), Api5::MarkdownResult>);
static_assert(std::is_same_v<decltype(ref<Api5::MarkdownParser>().Advance()), Api5::MarkdownAdvance>);
static_assert(std::is_same_v<decltype(Api5::TryProject(cref<Source>(), 0u, Api5::ViewMode{}, ref<Api5::Projection>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedProjection>().TryToSource(0u, false, ref<std::size_t>())), Result>);
static_assert(std::is_same_v<decltype(Api5::SourceDisplayMap::TryBuild({}, {}, {}, ref<std::shared_ptr<const Api5::SourceDisplayMap>>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryFindInSource(cref<Source>(), {}, ref<std::vector<Api5::MarkdownRange>>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryApplySourceEdits(ref<Api5::Document>(), cref<Source>(), 0u, {}, {}, ref<bool>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryBuildMarkdownCommand(
                                 cref<Source>(), 0u, {}, {}, cref<Api5::MarkdownCommandRequest>(), ref<std::optional<Api5::MarkdownCommandPlan>>())),
                             Result>);
static_assert(
    std::is_same_v<decltype(Api5::TryBuildMarkdownDeletion(cref<Source>(), 0u, {}, {}, {}, ref<std::optional<Api5::MarkdownCommandPlan>>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryApplyMarkdownCommand(ref<Api5::Document>(), cref<Api5::MarkdownCommandPlan>(), ref<bool>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::SemanticBuilder>().TryBegin(std::shared_ptr<const Api5::MarkdownSyntax>{})), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::SemanticBuilder>().Advance()), Api5::MarkdownAdvance>);
static_assert(std::is_same_v<decltype(Api5::TryBuildHeadingTableOfContents(cref<Api5::SemanticDocument>(), ref<std::wstring>())), Result>);
static_assert(std::is_same_v<decltype(Api5::AccessibilityLabels::TryCreate({}, ref<std::shared_ptr<const Api5::AccessibilityLabels>>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryExportSemanticDocument(cref<Api5::SemanticDocument>(), {}, ref<std::optional<Api5::ExportContent>>())), Result>);
static_assert(std::is_same_v<decltype(Api5::TryExportMarkdownSyntax({}, ref<std::optional<Api5::ExportContent>>())), Result>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedLayout>().TryPrepare(nullptr, Api5::Projection{}, cref<Api5::LayoutOptions>())), HRESULT>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedLayout>().TryHitTest(0.0f, 0.0f, ref<Api5::Hit>())), HRESULT>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedLayout>().TryCaret(0u, ref<Api5::CaretGeometry>())), HRESULT>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedLayout>().TryRange(0u, 0u, ref<std::vector<D2D1_RECT_F>>())), HRESULT>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedLayout>().TryDraw(nullptr, nullptr, {})), HRESULT>);
static_assert(
    std::is_same_v<decltype(ref<Api5::PreparedDocumentLayout>().TryPrepare(nullptr, cref<Source>(), 0u, Api5::ViewMode{}, cref<Api5::LayoutOptions>())),
                   HRESULT>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedDocumentLayout>().TryHitTest(0.0f, 0.0f, ref<Api5::Hit>())), HRESULT>);
static_assert(
    std::is_same_v<decltype(ref<Api5::PreparedDocumentLayout>().BeginPreparation(nullptr, cref<Source>(), 0u, Api5::ViewMode{}, cref<Api5::LayoutOptions>())),
                   Api5::LayoutPreparationStatus>);
static_assert(std::is_same_v<decltype(ref<Api5::PreparedDocumentLayout>().AdvancePreparation()), Api5::LayoutAdvance>);
static_assert(std::is_same_v<decltype(Api5::LayoutAdvance{}.result), HRESULT>);
