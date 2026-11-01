#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>
#include <DxUi/FrameRuntime.h>
#include <DxUi/RichTextExport.h>

int main()
{
    DxUi::FrameClock clock;
    const auto now = clock.Now();
    if (clock.ElapsedUs(now, now) != 0 || clock.SmoothDeltaUs(16667, DxUi::FrameBudget{}) == 0)
    {
        return 1;
    }
    auto stage = DxUi::FrameStage::Idle;
    {
        DxUi::FrameStageScope scope(stage, DxUi::FrameStage::Layout);
        if (stage != DxUi::FrameStage::Layout)
        {
            return 2;
        }
    }
    if (stage != DxUi::FrameStage::Idle)
        return 3;
    DxUi::RichText::MarkdownParser parser;
    parser.Begin(L"# SDK\n\n**export**\n", 1u, DxUi::RichText::MarkdownProfile::RichMarkdown);
    while (parser.HasPendingPreparation())
        parser.Advance();
    const auto content = DxUi::RichText::ExportMarkdownSyntax(parser.Published());
    return content.Text().find(L"export") != std::wstring_view::npos && content.HtmlFragment().find(L"<h1 id=\"sdk\">SDK</h1>") != std::wstring_view::npos &&
                   content.HtmlFragment().find(L"<strong>export</strong>") != std::wstring_view::npos
               ? 0
               : 4;
}
