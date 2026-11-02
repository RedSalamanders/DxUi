// Frozen consumer compile fixture for API revision 5. No private headers or test flags.
#include <DxUi/Failure.h>
#include <DxUi/MarkdownSyntax.h>
#include <DxUi/DxUi.h>
#include <type_traits>

static_assert(std::is_standard_layout_v<DxUi::Failure::Site>);
static_assert(std::is_standard_layout_v<DxUi::Failure::Record>);
static_assert(std::is_standard_layout_v<DxUi::Failure::HostServices>);
static_assert(!std::is_copy_constructible_v<DxUi::Failure::Scope>);
static_assert(!std::is_copy_constructible_v<DxUi::Failure::ProcessHooks>);
static_assert(!std::is_copy_constructible_v<DxUi::Failure::ModuleRegistration>);
static_assert(std::is_same_v<decltype(&DxUi::Failure::RegisterModule), HRESULT (*)(const DxUi::Failure::HostServices&) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Failure::UnregisterModule), HRESULT (*)(const DxUi::Failure::HostServices&) noexcept>);
static_assert(std::is_same_v<decltype(DxUi::RichText::MarkdownAdvance{}.result), DxUi::RichText::MarkdownResult>);
static_assert(std::is_same_v<decltype(&DxUi::RichTextEditor::TrySetDecorations),
                             HRESULT (DxUi::RichTextEditor::*)(const DxUi::RichText::SourceSnapshot&, std::span<const DxUi::RichTextDecoration>)>);
static_assert(DxUi::Failure::ProtocolVersion == 1);
