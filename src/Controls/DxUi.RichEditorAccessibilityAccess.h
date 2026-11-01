#pragma once
#include <DxUi/DxUi.h>
#include <DxUi/RichSemanticDocument.h>
namespace DxUi::Detail
{
// One owner-thread entry point for accessibility's RichTextEditor type and
// publication data. Action dispatch still validates its own owner/epoch.
struct RichEditorAccessibilityAccess
{
    struct View
    {
        RichText::Selection selection;
        std::optional<std::pair<size_t, size_t>> visibleRange;
        std::shared_ptr<const RichText::SemanticDocument> document;
    };
    static const RichTextEditor* As(const Control* control) noexcept
    {
        return dynamic_cast<const RichTextEditor*>(control);
    }
    static RichTextEditor* As(Control* control) noexcept
    {
        return dynamic_cast<RichTextEditor*>(control);
    }
    static View Capture(const RichTextEditor& editor, const ControlHost& host) noexcept
    {
        return {editor.GetSelection(), editor.GetVisibleSourceRange(host), editor.GetAcknowledgedSemanticDocument(host)};
    }
};
} // namespace DxUi::Detail
