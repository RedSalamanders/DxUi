#include "Scope.Tests.RichTextInput.h"
#include "../../Samples/EmbeddedControls/EmbeddedTextClient.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../../Samples/EmbeddedControls/RichImageLoader.h"
#include "../Support/Scope.Tests.ClipboardFixture.h"
#include "../Support/Scope.Tests.RichImagePaths.h"
#include "../Support/Scope.Tests.RichImageResources.h"
#include "../Support/Scope.Tests.RichTextStateReplacement.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/Embedded.h>
#include <imm.h>

namespace
{
using namespace DxUi;
using namespace DxUi::RichText;

// Expose the protected adapter to exercise malformed service input independently
// of TSF's own validation. Production callers go through ControlHost/EmbeddedHost.
using ServiceRichEditor = RichTextStateReplacementTests::ServiceEditor;

void TestRichTextApplicationImageLoading()
{
    AttachedHostWindow window;
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "application image loader WARP");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "application image loader pool");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "application image loader view");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor, false);
    const auto prepare = [&]
    {
        for (unsigned turn = 0u; turn < 2048u; ++turn)
        {
            RequireSucceeded(view.Prepare(gpu.width, gpu.height), "application image loader preparation");
            if (! view.NeedsPreparation())
                return;
        }
        Require(false, "image loader preparation settles");
    };
    std::array<wchar_t, MAX_PATH> temp{};
    const auto count = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    Require(count && count < temp.size(), "loader fixture bounded temporary path");
    GUID id{};
    RequireSucceeded(CoCreateGuid(&id), "loader fixture unique identity");
    std::array<wchar_t, 40u> guid{};
    Require(StringFromGUID2(id, guid.data(), static_cast<int>(guid.size())) > 0, "loader fixture GUID spelling");
    std::wstring path(temp.data(), count);
    path.append(L"DxUi-loader-\u6771\U0001f600-").append(guid.data()).append(L".png");
    editor->SetDocumentFilePath(std::wstring(temp.data(), count) + L"note.md");
    bool created{};
    const auto cleanup = wil::scope_exit([&] noexcept
    {
        if (created)
            DeleteFileW(path.c_str());
    });
    wil::com_ptr<IWICImagingFactory> factory;
    RequireSucceeded(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())), "loader fixture encoder factory");
    std::vector<BYTE> pixels(96u * 64u * 4u);
    for (std::size_t at = 0u; at < pixels.size(); at += 4u)
    {
        pixels[at]      = 200u;
        pixels[at + 1u] = 80u;
        pixels[at + 2u] = 20u;
        pixels[at + 3u] = 255u;
    }
    const auto encoded = RichImageTests::Encode(factory.get(), GUID_ContainerFormatPng, pixels, 96u, 64u);
    wil::unique_hfile file(
        CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr));
    Require(static_cast<bool>(file), "loader fixture owns its new file");
    created = true;
    DWORD written{};
    Require(WriteFile(file.get(), encoded.data(), static_cast<DWORD>(encoded.size()), &written, nullptr) && written == encoded.size(),
            "loader fixture writes complete PNG");
    file.reset();
    prepare();
    EmbeddedTextInputSnapshot snapshot;
    RequireSucceeded(view.ReadTextInput(snapshot), "loader fixture source adapter");
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "loader clipboard application service");
    auto client = std::make_shared<EmbeddedTextClient>(view, window.Hwnd(), snapshot.focusId, 96.f);
    RequireSucceeded(services.SetClient(client), "loader actual sample text adapter");
    class Clipboard final : public TextClipboard
    {
    public:
        std::wstring value;
        HRESULT Read(HWND, std::wstring& output) noexcept override
        try
        {
            output = value;
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
        HRESULT Write(HWND, std::wstring_view) noexcept override
        {
            return E_NOTIMPL;
        }
    } clipboard;
    clipboard.value = L"\"" + path + L"\"";
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Paste, &clipboard), "actual clipboard-to-Markdown image insertion");
    prepare();
    const auto source   = editor->GetText();
    const auto revision = editor->Revision();
    std::vector<RichImageRequest> requests(0u);
    editor->TakePendingImageRequests(requests);
    Require(requests.size() == 1u && requests[0].FilePath() == path, "relative Unicode source resolves to the same physical image");
    constexpr UINT message  = WM_APP + 0x317u;
    constexpr WPARAM cookie = 0x72696368u;
    wil::unique_handle ready(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(static_cast<bool>(ready), "loader fixture message event");
    std::size_t messages{};
    window.SetApplicationMessageHandler([&](UINT current, WPARAM value, LPARAM, LRESULT& result)
    {
        if (current != message || value != cookie)
            return false;
        ++messages;
        SetEvent(ready.get());
        result = 0;
        return true;
    });
    RichImageLoader loader;
    RequireSucceeded(loader.Initialize(window.Hwnd(), message, cookie), "actual application-owned asynchronous image service");
    RequireSucceeded(loader.Submit(requests), "application submits bounded image work");
    RequireSucceeded(loader.Submit(requests), "duplicate application submission coalesces");
    const auto deadline = GetTickCount64() + 5000u;
    const HANDLE event  = ready.get();
    while (! messages)
    {
        const auto now = GetTickCount64();
        Require(now < deadline, "worker posts its owner completion within the fixture deadline");
        const auto wait = MsgWaitForMultipleObjectsEx(1u, &event, static_cast<DWORD>(deadline - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        Require(wait != WAIT_FAILED && wait != WAIT_TIMEOUT, "application image service wakes through the real owner queue");
        if (wait == WAIT_OBJECT_0 + 1u)
            window.PumpMessages(20u);
    }
    std::vector<RichImageLoader::Result> results(0u);
    loader.Drain(results);
    Require(results.size() == 1u && results[0].status == S_OK && results[0].image && editor->CompleteImageRequest(results[0].request, results[0].image),
            "one MTA decoded resource is marshaled and accepted on its owner");
    prepare();
    gpu.Bind();
    RequireSucceeded(view.Composite(gpu.context.get(), gpu.Viewport()), "asynchronous loaded image composition");
    std::vector<std::uint8_t> actual(0u);
    RequireSucceeded(gpu.Read(actual), "asynchronous loaded image pixels");
    RichImageResourceTests::Pixel(*editor, view.Controls(), actual, gpu.width, gpu.height, 1.f, Require);
    Require(editor->GetText() == source && editor->Revision() == revision && editor->Undo() && editor->GetText().empty() && ! editor->CanUndo(),
            "clipboard plus async display remains exactly one source transaction");
    window.SetApplicationMessageHandler({});
    loader.Shutdown();
    services.ClearClient();
    client->Disconnect();
    services.Detach();
}

void TestRichTextApplicationImageLoaderLifecycle()
{
    using DxUi::RichText::Detail::RichImageResources;
    AttachedHostWindow window;
    constexpr UINT message  = WM_APP + 0x318u;
    constexpr WPARAM cookie = 0x696d6167u;
    std::size_t messages{};
    window.SetApplicationMessageHandler([&](UINT current, WPARAM value, LPARAM, LRESULT& result)
    {
        if (current != message || value != cookie)
            return false;
        ++messages;
        result = 0;
        return true;
    });
    const auto requestBatch = [](RichImageResources& resources, unsigned batch)
    {
        std::wstring source(0u, L' ');
        for (unsigned index = 0u; index < 32u; ++index)
            source.append(L"![network](file://dxui-image-test.invalid/share/")
                .append(std::to_wstring(batch))
                .append(L"-")
                .append(std::to_wstring(index))
                .append(L".png)\n\n");
        resources.Begin(RichImageResourceTests::Semantic(source), {}, 1u, 1u);
        while (! resources.Advance())
        {
        }
        std::vector<RichImageRequest> requests(0u);
        resources.Take(requests);
        Require(requests.size() == 32u, "loader lifecycle fixture owns one maximum unique source batch");
        return std::vector<RichImageRequest>(requests);
    };
    RichImageResources first, second, third;
    const auto firstJobs = requestBatch(first, 1u), secondJobs = requestBatch(second, 2u), thirdJobs = requestBatch(third, 3u);
    RichImageLoader loader;
    RequireSucceeded(loader.Initialize(window.Hwnd(), message, cookie), "bounded loader lifecycle owner");
    RequireSucceeded(loader.Submit(firstJobs), "maximum loader job batch");
    RequireSucceeded(loader.Submit(firstJobs), "queued/current/completed duplicate submissions coalesce");
    const auto waitForMessages = [&](std::size_t expected)
    {
        const auto deadline = GetTickCount64() + 5000u;
        while (messages < expected)
        {
            const auto now = GetTickCount64();
            Require(now < deadline, "bounded loader results reach the actual owner message queue");
            const auto wait = MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(deadline - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            Require(wait != WAIT_FAILED && wait != WAIT_TIMEOUT, "loader lifecycle wakes without polling or desktop focus");
            window.PumpMessages(20u);
        }
    };
    waitForMessages(firstJobs.size());
    // A full results queue stops the worker. Fill the independent pending queue
    // before draining it, making overflow deterministic without a thread sleep.
    RequireSucceeded(loader.Submit(secondJobs), "bounded pending queue while the completion queue is full");
    Require(loader.Submit(std::span(thirdJobs).first(1u)) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW),
            "excess submission fails without replacing previously queued work");
    std::vector<RichImageLoader::Result> results(0u);
    loader.Drain(results);
    Require(results.size() == 32u && messages == 32u, "each unique request posts exactly one result within the fixed completion bound");
    std::array<bool, 32u> seen{};
    for (const auto& result : results)
    {
        const auto found = std::ranges::find(firstJobs, result.request);
        Require(found != firstJobs.end(), "completion identity belongs to the submitted generation");
        const auto index = static_cast<std::size_t>(found - firstJobs.begin());
        Require(! seen[index] && result.status == E_ACCESSDENIED && ! result.image && first.Complete(result.request, {}),
                "default UNC policy declines every network request before I/O and delivers a unique alternative result");
        seen[index] = true;
    }
    Require(first.Statistics().failedSources == 32u && ! first.Statistics().decodedBytes, "policy refusal retains no decoded pixels or retry loop");
    // Fill results a second time, then release capacity through cancellation
    // instead of Drain. A valid third batch must wake without another Submit.
    waitForMessages(64u);
    RequireSucceeded(loader.Submit(thirdJobs), "current queued batch behind completed old-generation results");
    second.Cancel();
    loader.DiscardCancelled();
    waitForMessages(96u);
    loader.Drain(results);
    Require(results.size() == 32u && messages == 96u, "pruning cancelled results resumes every still-valid queued request exactly once");
    seen.fill(false);
    for (const auto& result : results)
    {
        const auto found = std::ranges::find(thirdJobs, result.request);
        Require(found != thirdJobs.end(), "resumed completion never belongs to a cancelled generation");
        const auto index = static_cast<std::size_t>(found - thirdJobs.begin());
        Require(! seen[index] && result.status == E_ACCESSDENIED && ! result.image && third.Complete(result.request, {}),
                "resumed results retain unique identity, policy and explicit fallback completion");
        seen[index] = true;
    }
    third.Cancel();
    loader.DiscardCancelled();
    Require(! loader.HasResults(), "cancelled queued/completing requests are pruned without an owner callback");
    loader.Shutdown();
    loader.Drain(results);
    Require(results.empty() && ! loader.HasResults() && loader.Submit(firstJobs) == E_UNEXPECTED,
            "closure retires queues and rejects new work without joining the decoder");
    window.SetApplicationMessageHandler({});
}

void TestRichTextImageClipboardTransport()
{
    AttachedHostWindow window;
    ClipboardTransportFixture clipboard(window.Hwnd(), Require);
    RequireSucceeded(SystemTextClipboard().Write(window.Hwnd(), L"unicode fallback"), "structured transport fixture Unicode data");
    constexpr wchar_t list[] = L"C:\\docs\\a.png\0D:\\\u6771.jpg\0";
    const auto bytes         = RichImagePathTests::DropBuffer(std::wstring_view(list, std::size(list)));
    clipboard.fileData.reset(GlobalAlloc(GMEM_MOVEABLE, bytes.size()));
    Require(clipboard.fileData != nullptr, "structured clipboard fixture owns global storage");
    auto* data = GlobalLock(clipboard.fileData.get());
    Require(data != nullptr, "structured fixture locks storage");
    std::memcpy(data, bytes.data(), bytes.size());
    GlobalUnlock(clipboard.fileData.get());
    TextClipboardContent content;
    content.text = L"sentinel";
    auto opens = clipboard.opens, closes = clipboard.closes;
    Require(ReadTextClipboardContent(SystemTextClipboard(), window.Hwnd(), content, true) == S_OK && content.filePaths.size() == 2u && content.text.empty(),
            "real structured transport prioritizes wide file lists over Unicode fallback");
    Require(clipboard.opens == opens + 1u && clipboard.closes == closes + 1u, "structured clipboard transfer owns one nonblocking open/close");
    Require(ReadTextClipboardContent(SystemTextClipboard(), window.Hwnd(), content, false) == S_OK && content.filePaths.empty() &&
                content.text == L"unicode fallback",
            "existing Unicode controls ignore CF_HDROP and retain their original text transport");
    clipboard.busy = true;
    opens          = clipboard.opens;
    closes         = clipboard.closes;
    Require(FAILED(ReadTextClipboardContent(SystemTextClipboard(), window.Hwnd(), content, true)) && content.text == L"unicode fallback" &&
                clipboard.opens == opens + 1u && clipboard.closes == closes,
            "busy structured read preserves output and never closes an unowned clipboard");
    clipboard.busy = false;
    data           = GlobalLock(clipboard.fileData.get());
    Require(data != nullptr, "structured fixture mutates header");
    static_cast<DROPFILES*>(data)->fWide = FALSE;
    GlobalUnlock(clipboard.fileData.get());
    Require(FAILED(ReadTextClipboardContent(SystemTextClipboard(), window.Hwnd(), content, true)) && content.text == L"unicode fallback",
            "malformed ANSI file list is explicit failure without silent text fallback");
    clipboard.fileData.reset();
    Require(ReadTextClipboardContent(SystemTextClipboard(), window.Hwnd(), content, true) == S_OK && content.filePaths.empty() &&
                content.text == L"unicode fallback",
            "absent files use the same-open Unicode fallback");
}

void TestRichTextApplicationImageClipboard()
{
    AttachedHostWindow window;
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "application image clipboard WARP");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "application image clipboard pool");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "application image clipboard view");
    auto owned   = std::make_unique<RichTextEditor>(L"replace");
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetDocumentFilePath(L"C:\\docs\\note.md");
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor, false);
    auto prepare = [&]
    {
        for (unsigned turn = 0u; turn < 1024u; ++turn)
        {
            RequireSucceeded(view.Prepare(640u, 240u), "application image clipboard prepares");
            if (! view.NeedsPreparation())
                return;
        }
        Require(false, "image clipboard preparation completed");
    };
    prepare();
    TextInputServices services;
    RequireSucceeded(services.Attach(window.Hwnd()), "image clipboard borrows application HWND");
    EmbeddedTextInputSnapshot snapshot;
    RequireSucceeded(view.ReadTextInput(snapshot), "image clipboard initial source snapshot");
    auto client = std::make_shared<EmbeddedTextClient>(view, window.Hwnd(), snapshot.focusId, 96.f);
    RequireSucceeded(services.SetClient(client), "image clipboard uses the sample's real public adapter");
    class Clipboard final : public TextClipboard, public StructuredTextClipboard
    {
    public:
        TextClipboardContent content;
        std::function<void()> afterRead;
        std::size_t reads{};
        HRESULT Read(HWND, std::wstring&) noexcept override
        {
            return E_UNEXPECTED;
        }
        HRESULT Write(HWND, std::wstring_view) noexcept override
        {
            return E_NOTIMPL;
        }
        HRESULT ReadContent(HWND, TextClipboardContent& result) noexcept override
        try
        {
            ++reads;
            TextClipboardContent staged(content);
            result.Swap(staged);
            if (afterRead)
                afterRead();
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    } clipboard;
    clipboard.content.filePaths = {L"C:\\docs\\images\\\u6771\U0001f600.png"};
    editor->SetSelection({7u, 0u});
    prepare();
    std::size_t notices{};
    editor->SetOnTextChanged([&] { ++notices; });
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Paste, &clipboard), "structured image paste traverses production app TSF service");
    Require(editor->GetText() == L"![\u6771\U0001f600](images/%E6%9D%B1%F0%9F%98%80.png)" && notices == 1u,
            "one service commit inserts exact Unicode image source");
    Require(editor->Undo() && editor->GetText() == L"replace" && editor->GetSelection() == Selection{7u, 0u} && ! editor->CanUndo(),
            "service image paste restores backward selection in one undo");
    clipboard.content.filePaths = {L"C:\\docs\\a.png", L"C:\\docs\\files\\report.pdf", L"D:\\other.txt"};
    prepare();
    notices = 0u;
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Paste, &clipboard), "mixed references traverse the real embedded TSF service");
    Require(editor->GetText() == L"![a](a.png)\r\n\r\n[report\\.pdf](files/report.pdf)\r\n\r\n[other\\.txt](file:///D:/other.txt)" && notices == 1u,
            "embedded service preserves mixed order and chooses relative or absolute file destinations per item");
    Require(editor->Undo() && editor->GetText() == L"replace" && editor->GetSelection() == Selection{7u, 0u} && ! editor->CanUndo(),
            "embedded mixed paste restores source and backward selection in one undo");
    clipboard.content.filePaths.clear();
    clipboard.content.text = L"\"C:\\docs\\files\\report.pdf\"";
    prepare();
    RequireSucceeded(services.Clipboard(TextClipboardCommand::Paste, &clipboard), "embedded text path traverses the file-reference transform");
    Require(editor->GetText() == L"[report\\.pdf](files/report.pdf)" && editor->Undo() && editor->GetText() == L"replace",
            "embedded text file reference retains its full label and one undo boundary");
    clipboard.content.text.clear();
    clipboard.content.filePaths = {L"C:\\docs\\report.pdf"};
    for (unsigned mutation = 0u; mutation < 3u; ++mutation)
    {
        editor->SetSelection({7u, 0u});
        prepare();
        clipboard.afterRead = [&]
        {
            if (mutation == 0u)
                editor->SetSelection({1u, 1u});
            if (mutation == 1u)
                editor->SetDocumentFilePath(L"D:\\other.md");
            if (mutation == 2u)
                editor->SetText(L"newer");
        };
        const auto before = editor->Revision();
        Require(services.Clipboard(TextClipboardCommand::Paste, &clipboard) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
                "clipboard source/selection/location reentrancy rejects the stale transform");
        Require((mutation == 2u ? editor->GetText() == L"newer" : editor->GetText() == L"replace") && (mutation == 2u || editor->Revision() == before),
                "stale clipboard read cannot add a transaction");
    }
    clipboard.afterRead = {};
    editor->SetReadOnly(true);
    const auto reads = clipboard.reads;
    Require(services.Clipboard(TextClipboardCommand::Paste, &clipboard) == E_ACCESSDENIED && clipboard.reads == reads,
            "read-only service rejects file-reference paste before clipboard access");
    editor->SetReadOnly(false);
    prepare();
    RequireSucceeded(view.ReadTextInput(snapshot), "image clipboard composition snapshot");
    auto preview                  = snapshot.state;
    preview.compositionStartIndex = 0u;
    preview.compositionEndIndex   = 1u;
    RequireSucceeded(view.ApplyTextInput(snapshot.revision, preview, EmbeddedTextInputAction::Preview), "image clipboard opens source composition");
    Require(services.Clipboard(TextClipboardCommand::Paste, &clipboard) == S_FALSE && clipboard.reads == reads,
            "active composition rejects file-reference paste before clipboard access");
    services.ClearClient();
    client->Disconnect();
    services.Detach();
}

void TestRichTextTransientSourcePreservesHistory()
{
    Document document(L"base");
    document.SetSelection({4, 4});
    Require(document.ReplaceSelection(L"!") && document.Undo(), "fixture retains redo before composition");
    const auto revision = document.Revision();
    Require(document.ReplaceTransient(L"base\u4eee", {5, 5}), "preview changes source");
    Require(! document.CanUndo() && document.CanRedo() && document.Revision() > revision, "preview preserves both histories with a fresh revision");
    Require(document.ReplaceTransient(L"base", {4, 4}) && document.CanRedo(), "cancel restores entry text without destroying redo");
    Require(document.Redo() && document.Text() == L"base!", "pre-composition redo still works");
    const auto before = document.Snapshot();
    try
    {
        document.ReplaceTransient(L"\U0001f600", {1, 1});
        Require(false, "split scalar preview must fail");
    }
    catch (const std::out_of_range&)
    {
    }
    Require(document.Text() == before.Text(), "rejected preview leaves source unchanged");
}

void TestRichTextServiceValidationAndGeometry()
{
    AttachedHostWindow window;
    auto editor = std::make_unique<ServiceRichEditor>(L"# **\u03b2\U0001f600**\r\nnext");
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    Require(PrepareNativeContentForTests(window.Host()), "prepare native rich service geometry");
    window.Host().SetFocusControl(raw, false);
    TextInputState state;
    Require(raw->ExportTextInputState(state) && state.multiline && ! state.masked, "service exposes Unicode multiline source");
    const auto original = state;
    state.caretIndex    = 6; // Between the emoji's surrogate halves.
    Require(! raw->ImportTextInputState(window.Host(), state, true), "reject split supplementary ACP");
    state            = original;
    state.text       = std::wstring(1, 0xd800);
    state.caretIndex = 0;
    Require(! raw->ImportTextInputState(window.Host(), state, false), "reject malformed preview UTF-16");
    state.text = std::wstring(Document::MaximumUnits + 1, L'x');
    Require(! raw->ImportTextInputState(window.Host(), state, true), "reject oversized service transaction");
    Require(raw->GetText() == original.text && ! raw->CanUndo(), "invalid service changes are atomic");
    const auto hidden = raw->TryGetTextInputRangeRects(window.Host(), 0, 4);
    const auto glyph  = raw->TryGetTextInputRangeRects(window.Host(), 4, 5);
    Require(hidden && hidden->empty() && glyph && ! glyph->empty(), "hidden heading/bold markers have no pixels but source glyph has geometry");
    const auto caret = raw->TryGetTextInputCaretRect(window.Host(), 4);
    Require(caret.has_value(), "source caret maps through heading and inline projection");
    const auto hit = raw->TryHitTestTextInputPoint(window.Host(), {caret->left, (caret->top + caret->bottom) / 2});
    Require(hit && *hit == 4, "projected point maps back after hidden opening markers");
    raw->SetViewMode(ViewMode::Source);
    Require(PrepareNativeContentForTests(window.Host()), "prepare visible source markers");
    const auto source = raw->TryGetTextInputRangeRects(window.Host(), 0, 4);
    Require(source && ! source->empty(), "source mode exposes marker geometry");
    raw->SetReadOnly(true);
    state      = original;
    state.text = L"forbidden";
    Require(! raw->ImportTextInputState(window.Host(), state, true) && raw->GetText() == original.text, "read-only service writes are rejected");
    raw->SetVisible(false);
    Require(! raw->TryGetTextInputCaretRect(window.Host(), 0), "hidden editor retires service geometry");
}

void ImeMessage(ControlHost& host, UINT message, LPARAM flags = 0)
{
    bool handled = false;
    host.HandleMessage(nullptr, message, 0, flags, handled);
    Require(handled, "rootless native IMM message routes to production host adapter");
}

// A rootless ControlHost exercises the production IMM protocol without an HWND
// or keyboard focus. It does not qualify a real IME or its candidate window.
void TestRichTextNativeImeCommitCancelAndRetirement()
{
    ControlHost host;
    auto editor = std::make_unique<RichTextEditor>(L"**a**");
    auto* raw   = editor.get();
    host.SetRoot(std::move(editor));
    host.SetFocusControl(raw, false);
    raw->SetSelection({3, 3});
    size_t notices = 0;
    raw->SetOnTextChanged([&] { ++notices; });
    ImeMessage(host, WM_IME_STARTCOMPOSITION);
    NativeTextInputImePayload payload;
    payload.hasCompositionString = true;
    payload.compositionString    = L"\u4eee";
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_COMPSTR);
    Require(raw->GetText() == L"**a\u4eee**" && notices == 0 && ! raw->CanUndo(), "native preview is silent and creates no undo entry");
    ImeMessage(host, WM_IME_ENDCOMPOSITION);
    Require(raw->GetText() == L"**a**" && raw->GetSelection() == Selection{3, 3} && notices == 0, "native cancel restores exact source and selection");
    ImeMessage(host, WM_IME_STARTCOMPOSITION);
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_COMPSTR);
    payload                 = {};
    payload.hasResultString = true;
    payload.resultString    = L"\u4eee";
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_RESULTSTR);
    ImeMessage(host, WM_IME_ENDCOMPOSITION);
    Require(notices == 1 && raw->Undo() && raw->GetText() == L"**a**" && ! raw->CanUndo(), "native result equal to preview is one notifying undo transaction");
    raw->SetOnTextChanged([&] { host.SetRoot(std::make_unique<Label>(L"replacement")); });
    ImeMessage(host, WM_IME_STARTCOMPOSITION);
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_RESULTSTR);
    Require(dynamic_cast<Label*>(host.GetRoot()) && ! host.HasActiveNativeTextInputSession(), "native commit delegate can retire editor and text session");
}

void TestRichTextEmbeddedCompositionRevisionsAndRebinding()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "embedded rich input WARP fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "shared rich input graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "attach embedded rich input view");
    auto root = std::make_unique<Panel>();
    auto* raw = root->AddChild<RichTextEditor>(L"**a**");
    raw->SetBounds(D2D1::RectF(0, 0, 400, 180));
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare embedded rich input");
    view.Controls().SetFocusControl(raw, false);
    raw->SetSelection({3, 3});
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare selected embedded input");
    size_t notices = 0;
    raw->SetOnTextChanged([&] { ++notices; });
    EmbeddedTextInputSnapshot before;
    Require(view.ReadTextInput(before) == S_OK && before.caretBoundsDip, "embedded source snapshot and prepared caret");
    auto preview                       = before.state;
    preview.text                       = L"**a\u4eee**";
    preview.caretIndex                 = 4;
    preview.compositionStartIndex      = 3;
    preview.compositionEndIndex        = 4;
    preview.conversionTargetStartIndex = 3;
    preview.conversionTargetEndIndex   = 4;
    Require(view.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "embedded preview accepted");
    Require(raw->GetText() == preview.text && notices == 0 && ! raw->CanUndo(), "embedded preview preserves history and notification boundary");
    Require(view.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Commit) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
            "stale service revision rejected");
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare composition underline and geometry");
    RequireSucceeded(gpu.Resize(480, 240), "composition capture target");
    gpu.Bind();
    RequireSucceeded(view.Composite(gpu.context.get(), gpu.Viewport()), "composite prepared composition pixels");
    RequireSucceeded(gpu.Save(".build/test-artifacts/rich-d2d-composition.png"), "capture composition through the non-activating WARP harness");
    EmbeddedTextInputSnapshot live;
    Require(view.ReadTextInput(live) == S_OK && live.state.compositionStartIndex == 3, "composition range is published");
    Require(view.ApplyTextInput(live.revision, live.state, EmbeddedTextInputAction::Cancel) == S_OK, "embedded cancel accepted");
    Require(raw->GetText() == before.state.text && raw->GetSelection() == Selection{3, 3} && notices == 0,
            "embedded cancellation restores source and selection silently");
    Require(view.ReadTextInput(live) == S_OK, "read after cancellation");
    Require(view.ApplyTextInput(live.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "second preview");
    Require(view.ReadTextInput(live) == S_OK, "read final preview revision");
    auto result = preview;
    result.compositionStartIndex.reset();
    result.compositionEndIndex.reset();
    result.conversionTargetStartIndex.reset();
    result.conversionTargetEndIndex.reset();
    Require(view.ApplyTextInput(live.revision, result, EmbeddedTextInputAction::Commit) == S_OK && notices == 1, "embedded final preview commits exactly once");
    Require(raw->Undo() && raw->GetText() == before.state.text && ! raw->CanUndo(), "embedded composition is one undo entry");
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240, 144), "DPI reprepare retains input identity");
    Require(view.ReadTextInput(live) == S_OK && live.focusId == before.focusId, "DPI preserves focused service identity");
    raw->SetOnTextChanged([&] { view.Controls().SetRoot(std::make_unique<Label>(L"retired")); });
    auto committed       = live.state;
    committed.text       = L"replacement";
    committed.caretIndex = committed.text.size();
    committed.selectionAnchorIndex.reset();
    Require(view.ApplyTextInput(live.revision, committed, EmbeddedTextInputAction::Commit) == S_OK, "embedded commit tolerates destructive delegate");
    Require(view.ReadTextInput(live) == S_FALSE, "retired embedded editor has no readable text session");
}

void TestRichTextEmbeddedMappedGeometry()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "mapped rich input WARP fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "mapped rich input graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "attach mapped rich input");
    auto root    = std::make_unique<RichTextEditor>(L"**alpha**\r\n");
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare mapped rich input");
    view.Controls().SetFocusControl(editor, false);
    std::wstring source = L"**alpha**\r\n";
    for (int line = 0; line < 80; ++line)
        source += L"wrapped Unicode \u03b2 \U0001f600 text across the viewport\r\n";
    editor->SetText(source);
    EmbeddedTextInputSnapshot snapshot;
    Require(view.ReadTextInput(snapshot) == S_OK && ! snapshot.caretBoundsDip, "dirty source publishes state while withholding stale geometry");
    D2D1_RECT_F bounds{};
    bool clipped = false;
    Require(view.GetTextInputRangeBounds(snapshot.revision, 0, source.size(), bounds, clipped) == S_FALSE, "geometry waits for coherent preparation");
    for (const float dpi : {96.0f, 144.0f})
    {
        RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240, dpi), "prepare mapped geometry at DPI");
        Require(view.ReadTextInput(snapshot) == S_OK && snapshot.caretBoundsDip && snapshot.viewportBoundsDip, "prepared source geometry is available");
        Require(view.GetTextInputRangeBounds(snapshot.revision, 0, source.size(), bounds, clipped) == S_OK && clipped,
                "offscreen lines set the service clipping flag");
        Require(bounds.top >= snapshot.viewportBoundsDip->top && bounds.bottom <= snapshot.viewportBoundsDip->bottom,
                "service output clips to the prepared viewport");
        Require(view.GetTextInputRangeBounds(snapshot.revision, 0, 2, bounds, clipped) == S_OK && clipped && bounds.right == bounds.left,
                "hidden marker service range is pixel-empty");
        const auto caret = editor->TryGetTextInputCaretRect(view.Controls(), 2);
        Require(caret.has_value(), "mapped source caret exists");
        size_t index = 0;
        Require(view.HitTestTextInput(snapshot.revision, {caret->left, (caret->top + caret->bottom) / 2}, index) == S_OK && index == 2,
                "service point hit maps after hidden opening markers");
        const auto visible = editor->GetVisibleSourceRange(view.Controls());
        Require(visible && visible->second < source.size(), "visible source range excludes unshown wrapped lines");
    }
}

void TestRichTextEmbeddedPreviewRebinding()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "rebind rich input WARP fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "rebind shared graphics");
    EmbeddedHost first, second;
    RequireSucceeded(first.Attach(graphics), "attach first rich input host");
    RequireSucceeded(second.Attach(graphics), "attach second rich input host");
    auto root    = std::make_unique<Panel>();
    auto* panel  = root.get();
    auto* editor = panel->AddChild<RichTextEditor>(L"kept");
    editor->SetBounds(D2D1::RectF(0, 0, 400, 180));
    first.Controls().SetRoot(std::move(root));
    RequireSucceeded(PrepareEmbeddedContentForTests(first, 480, 240), "prepare first rich input host");
    first.Controls().SetFocusControl(editor, false);
    editor->SetSelection({4, 4});
    Require(editor->ReplaceSelection(L"!"), "prior independent rich edit");
    EmbeddedTextInputSnapshot before;
    Require(first.ReadTextInput(before) == S_OK, "read first rich input identity");
    auto preview                  = before.state;
    preview.text                  = L"kept!\u4eee";
    preview.caretIndex            = 6;
    preview.compositionStartIndex = 5;
    preview.compositionEndIndex   = 6;
    Require(first.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "preview before rebinding");
    auto retained = panel->TakeChild(0);
    Require(retained.get() == editor && editor->GetText() == L"kept!", "detach cancels owned preview before changing host binding");
    second.Controls().SetRoot(std::move(retained));
    RequireSucceeded(PrepareEmbeddedContentForTests(second, 480, 240, 144), "prepare rebound rich input editor");
    second.Controls().SetFocusControl(editor, false);
    Require(first.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Commit) != S_OK, "old view cannot commit through a rebound editor");
    Require(editor->Undo() && editor->GetText() == L"kept", "rebinding preserves pre-composition undo history");
    EmbeddedTextInputSnapshot rebound;
    Require(second.ReadTextInput(rebound) == S_OK && ! rebound.state.compositionStartIndex, "new host publishes restored source with no inherited composition");
    editor->SetReadOnly(true);
    Require(second.ReadTextInput(rebound) == S_OK && rebound.state.readOnly, "read-only policy synchronizes immediately");
    preview                       = rebound.state;
    preview.text                  = L"forbidden";
    preview.caretIndex            = preview.text.size();
    preview.compositionStartIndex = 0;
    preview.compositionEndIndex   = preview.text.size();
    Require(second.ApplyTextInput(rebound.revision, preview, EmbeddedTextInputAction::Preview) == E_ACCESSDENIED && editor->GetText() == L"kept",
            "read-only composition cannot change source");
}
} // namespace

void TestRichTextNativeLocalizedServiceCommit()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<ServiceRichEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    RichTextStateReplacementTests::Service(*editor, window.Host(), Require);
}
void TestRichTextEmbeddedLocalizedServiceCommit()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "localized service borrowed graphics fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "localized service shared device");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "localized service embedded owner");
    auto owned   = std::make_unique<ServiceRichEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    RichTextStateReplacementTests::Service(*editor, view.Controls(), Require);
    Require(! view.Controls().GetHwnd(), "localized embedded service commit creates no control HWND");
}
void RunRichTextInputTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Require(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "rich input fixtures initialize or borrow a COM apartment for WIC");
    const auto apartment = wil::scope_exit([initialized]
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    DXUI_RUN_TEST(TestRichTextTransientSourcePreservesHistory);
    DXUI_RUN_TEST(TestRichTextApplicationImageClipboard);
    DXUI_RUN_TEST(TestRichTextApplicationImageLoading);
    DXUI_RUN_TEST(TestRichTextApplicationImageLoaderLifecycle);
    DXUI_RUN_TEST(TestRichTextImageClipboardTransport);
    DXUI_RUN_TEST(TestRichTextNativeLocalizedServiceCommit);
    DXUI_RUN_TEST(TestRichTextEmbeddedLocalizedServiceCommit);
    DXUI_RUN_TEST(TestRichTextServiceValidationAndGeometry);
    DXUI_RUN_TEST(TestRichTextNativeImeCommitCancelAndRetirement);
    DXUI_RUN_TEST(TestRichTextEmbeddedCompositionRevisionsAndRebinding);
    DXUI_RUN_TEST(TestRichTextEmbeddedMappedGeometry);
    DXUI_RUN_TEST(TestRichTextEmbeddedPreviewRebinding);
}
