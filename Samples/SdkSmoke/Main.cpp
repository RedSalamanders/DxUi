#include <DxUi/FileDrop.h>
#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>
#include <DxUi/FrameRuntime.h>
#include <DxUi/RichTextExport.h>
#include <DxUi/RichImage.h>
#include <ole2.h>
#include <wil/resource.h>
#include <span>
#include <string_view>

int main()
{
    const auto com = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    constexpr std::wstring_view text = L"SDK drag text";
    wil::com_ptr<IDataObject> data;
    if (FAILED(DxUi::CreateRichTextDragData(text, data.put())) || ! data)
        return 6;
    FORMATETC format{CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    wil::unique_stg_medium medium;
    if (FAILED(data->GetData(&format, medium.addressof())) || medium.tymed != TYMED_HGLOBAL || ! medium.hGlobal)
        return 7;
    const wil::unique_hglobal_locked locked(medium);
    if (! locked.get() || std::wstring_view(static_cast<const wchar_t*>(locked.get()), text.size()) != text ||
        static_cast<const wchar_t*>(locked.get())[text.size()] != L'\0')
        return 8;
    constexpr std::string_view svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10' fill='red'/></svg>";
    std::shared_ptr<const DxUi::RichText::RichImage> vector;
    if (FAILED(DxUi::RichText::RichImage::CreateSvg(10u, 10u, std::as_bytes(std::span(svg)), vector)) || ! vector || vector->SvgBytes().empty())
        return 9;
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
    DxUi::ControlHost detached;
    if (detached.SetNativePresentationVisible(false) != E_UNEXPECTED)
        return 5;
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
