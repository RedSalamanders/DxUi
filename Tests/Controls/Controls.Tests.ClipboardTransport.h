#pragma once

#include "Controls.Tests.TextInputServices.h"

#include "../Support/Support.Tests.ClipboardFixture.h"

void TestNativeClipboardTransportBenchmark()
{
    ClipboardHostWindow window;
    ClipboardTransportFixture clipboard(window.Hwnd(), Require);
    const std::wstring input(65536, L'x');
    std::wstring output;
    auto& transport = DxUi::SystemTextClipboard();
    RequireSucceeded(transport.Write(window.Hwnd(), input), "clipboard benchmark warm write");
    RequireSucceeded(transport.Read(window.Hwnd(), output), "clipboard benchmark warm read");
    for (size_t round = 0; round < 5; ++round)
    {
        const size_t allocations = clipboard.allocations;
        const auto start         = std::chrono::steady_clock::now();
        for (size_t iteration = 0; iteration < 50; ++iteration)
        {
            RequireSucceeded(transport.Write(window.Hwnd(), input), "clipboard benchmark write");
            RequireSucceeded(transport.Read(window.Hwnd(), output), "clipboard benchmark read");
            Require(output == input, "clipboard benchmark retains the complete payload");
        }
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        Require(clipboard.allocations - allocations == 50, "clipboard transfer performs one global allocation per write");
        std::cerr << "CLIPBOARD_BENCHMARK units=65536 round=" << round + 1 << " iterations=50 durationUs=" << duration
                  << " globalAllocations=" << clipboard.allocations - allocations << " peakGlobalBytes=" << clipboard.peakGlobalBytes << '\n';
    }
}

void TestNativeClipboardLargeSelections()
{
    using namespace DxUi;
    ClipboardHostWindow window;
    ClipboardTransportFixture clipboard(window.Hwnd(), Require);
    for (const size_t length : {size_t(65535), size_t(65536), size_t(65537), size_t(100000)})
    {
        std::wstring input(length, L'x');
        input.replace(length - 3, 3, L"\u6771\U0001f600");
        SingleCellGridModel model(GridCellData{.text = input});
        Grid grid;
        grid.SetModel(&model);
        grid.GetSelectionModel().SetSingle(model.GetStableRowId(0));
        Require(grid.OnCopy(window.Host()), "native Grid copies complete selections across and above the embedded text ceiling");
        const auto copied = window.Host().ReadTextFromClipboard();
        Require(copied.has_value() && copied.value() == input, "native Grid clipboard transport preserves large Unicode selections");

        TextField field;
        Require(field.OnKeyDown(window.Host(), 'V', MK_CONTROL), "native TextField pastes the complete large selection");
        Require(field.GetText() == input, "native TextField does not truncate pasted text");
        Require(field.OnSelectAll(window.Host()) && field.OnKeyDown(window.Host(), 'X', MK_CONTROL),
                "native TextField cuts a large selection after successful copy");
        Require(field.GetText().empty(), "successful native cut removes the selection");
        const auto cut = window.Host().ReadTextFromClipboard();
        Require(cut.has_value() && cut.value() == input, "native cut preserves the complete selection on the clipboard");
    }
}

void TestNativeClipboardFailuresPreserveText()
{
    using namespace DxUi;
    ClipboardHostWindow window;
    ClipboardTransportFixture clipboard(window.Hwnd(), Require);
    auto& transport = SystemTextClipboard();
    RequireSucceeded(transport.Write(window.Hwnd(), L"sentinel"), "clipboard failure fixture has original data");
    TextField field(L"keep selected text");
    Require(field.OnSelectAll(window.Host()), "select text before clipboard failure");
    const std::wstring original(field.GetText());

    clipboard.failAllocation          = true;
    const size_t beforeAllocationOpen = clipboard.opens;
    Require(transport.Write(window.Hwnd(), L"new") == E_OUTOFMEMORY, "global allocation failure is explicit");
    Require(! field.OnKeyDown(window.Host(), 'X', MK_CONTROL) && field.GetText() == original, "allocation failure leaves cut text intact");
    Require(clipboard.opens == beforeAllocationOpen, "allocation failure does not open or empty the clipboard");
    clipboard.failAllocation = false;
    Require(window.Host().ReadTextFromClipboard().value_or(L"") == L"sentinel", "allocation failure preserves existing clipboard data");

    clipboard.busy               = true;
    const size_t beforeBusyOpen  = clipboard.opens;
    const size_t beforeBusyClose = clipboard.closes;
    Require(! field.OnKeyDown(window.Host(), 'X', MK_CONTROL) && field.GetText() == original, "busy clipboard cannot delete selected text");
    Require(clipboard.opens == beforeBusyOpen + 1 && clipboard.closes == beforeBusyClose, "busy copy attempts one open and never closes an unowned clipboard");
    std::wstring output = L"stale";
    Require(transport.Read(window.Hwnd(), output) == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) && output.empty(),
            "busy read clears stale output and returns failure");
    Require(clipboard.opens == beforeBusyOpen + 2, "busy paste performs one open attempt");
    clipboard.busy = false;

    clipboard.failEmpty = true;
    Require(! field.OnKeyDown(window.Host(), 'X', MK_CONTROL) && field.GetText() == original, "empty-clipboard failure preserves cut text");
    clipboard.failEmpty   = false;
    clipboard.failPublish = true;
    Require(! field.OnKeyDown(window.Host(), 'X', MK_CONTROL) && field.GetText() == original, "publication failure preserves cut text");
    clipboard.failPublish = false;
    Require(field.OnKeyDown(window.Host(), 'X', MK_CONTROL) && field.GetText().empty(), "failure does not poison a later successful cut");

    const wchar_t embeddedNull[]   = {L'a', 0, L'b'};
    const size_t beforeInvalidOpen = clipboard.opens;
    Require(transport.Write(window.Hwnd(), std::wstring_view(embeddedNull, 3)) == E_INVALIDARG, "embedded NUL cannot produce a truncated native copy");
    Require(clipboard.opens == beforeInvalidOpen, "invalid text is rejected before clipboard ownership changes");
}

void TestEmbeddedClipboardRejectsLargeNativePayload()
{
    using namespace DxUi;
    const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    AttachedHostWindow window;
    ClipboardTransportFixture clipboard(window.Hwnd(), Require);
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "embedded clipboard service attaches to the application HWND");
    auto client = std::make_shared<TestTextClient>();
    RequireSucceeded(services.SetClient(client), "embedded clipboard client attaches");
    const auto before    = client->snapshot.state.text;
    const size_t commits = client->commits;
    for (const size_t length : {size_t(65537), size_t(100000)})
    {
        const std::wstring input(length, L'x');
        RequireSucceeded(SystemTextClipboard().Write(window.Hwnd(), input), "native transport accepts payload larger than the embedded edit budget");
        std::wstring decoded;
        RequireSucceeded(SystemTextClipboard().Read(window.Hwnd(), decoded), "large native clipboard payload is valid transport data");
        Require(decoded == input, "large payload is not truncated to satisfy the embedded budget");
        Require(FAILED(services.Clipboard(TextClipboardCommand::Paste)) && client->snapshot.state.text == before && client->commits == commits,
                "embedded service independently rejects oversized native clipboard text without changing the document");
    }
    services.Detach();
}
