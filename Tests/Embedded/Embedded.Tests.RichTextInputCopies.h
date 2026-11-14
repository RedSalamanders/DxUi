#pragma once
// Rich text-input copies (W7.4 of the performance and motion plan). A RichTextEditor holding about 64 Ki UTF-16 units is
// prepared on WARP in an EmbeddedHost and driven by the keys the gate names. For each input event the work counters record
// the bytes copied out of the document and the whole-document flattens (Support/WorkCounters.h, rich group), including both
// text-input synchronizations a native host runs per event: the control's own and the host's after dispatch
// (CommitFocusedTextInput is that second call). The preparation that follows is recorded separately. Included by
// Embedded.Tests.Embedded.cpp after Embedded.Tests.ImeCopies.h, whose headless host and TSF scenario it reuses.
#include "Controls/DxUi.Internal.h"
#include "Controls/DxUi.RichSourceWindow.h"
#include "Controls/TextStoreTarget.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace RichTextInputCopies
{
// Room below the 64 Ki-unit document limit for the inserts the scenario makes.
inline constexpr size_t kDocumentUnits = 65'536u - 1'024u;
// The gate (plan W7.4): bytes copied per one-character insert and per arrow key, and flattens per listed operation.
inline constexpr uint64_t kMaximumBytesPerInsertOrArrow = 1'024u;
inline constexpr uint64_t kMaximumFlattensPerOperation  = 0u;

// A deterministic document of about kDocumentUnits units: CRLF lines of words, punctuation and a path, so word movement
// meets every class the word rules distinguish.
[[nodiscard]] inline std::wstring MakeDocument(size_t units)
{
    static constexpr std::array<std::wstring_view, 12u> words{
        L"lorem", L"ipsum", L"dolor", L"sit", L"amet,", L"C:\\src\\path", L"x_1", L"(paren)", L"-", L"words", L"text.", L"end"};
    std::wstring text(0u, L' ');
    text.reserve(units);
    size_t word = 0u;
    while (text.size() < units)
    {
        std::wstring line(0u, L' ');
        while (line.size() < 72u)
        {
            line += words[word++ % words.size()];
            line += L' ';
        }
        line += L"\r\n";
        if (text.size() + line.size() > units)
            line.resize(units - text.size());
        text += line;
    }
    return text;
}

// What one measured action did: the work counters' rich-document copy counts.
struct Copies final
{
    uint64_t bytes    = 0u;
    uint64_t flattens = 0u;
};

[[nodiscard]] inline Copies ToCopies(const DxUi::Detail::WorkCounters& delta) noexcept
{
    return Copies{delta.rich.documentBytesCopied, delta.rich.documentFlattens};
}

enum class Operation
{
    Insert,
    ArrowRight,
    ArrowLeft,
    Backspace,
    CtrlRight,
    CtrlLeft,
    CtrlBackspace,
    Enter,
    DoubleClick,
};

[[nodiscard]] inline const char* OperationName(Operation operation) noexcept
{
    switch (operation)
    {
        case Operation::Insert: return "insert";
        case Operation::ArrowRight: return "arrowRight";
        case Operation::ArrowLeft: return "arrowLeft";
        case Operation::Backspace: return "backspace";
        case Operation::CtrlRight: return "ctrlRight";
        case Operation::CtrlLeft: return "ctrlLeft";
        case Operation::CtrlBackspace: return "ctrlBackspace";
        case Operation::Enter: return "enter";
        case Operation::DoubleClick: return "doubleClick";
    }
    return "unknown";
}

inline constexpr std::array<Operation, 9u> kOperations{Operation::Insert,
                                                       Operation::ArrowRight,
                                                       Operation::ArrowLeft,
                                                       Operation::Backspace,
                                                       Operation::CtrlRight,
                                                       Operation::CtrlLeft,
                                                       Operation::CtrlBackspace,
                                                       Operation::Enter,
                                                       Operation::DoubleClick};

enum class Presentation
{
    Rich,     // The default: Rich view mode, Markdown rendering off (the v0 projection).
    Source,   // Source view mode.
    Semantic, // Rich view mode with Markdown rendering: the semantic projection.
};

[[nodiscard]] inline const char* PresentationName(Presentation presentation) noexcept
{
    switch (presentation)
    {
        case Presentation::Rich: return "rich";
        case Presentation::Source: return "source";
        case Presentation::Semantic: return "semantic";
    }
    return "unknown";
}

struct OperationRecord final
{
    Operation operation = Operation::Insert;
    Copies input;   // The input event and both synchronizations.
    Copies prepare; // The preparation that follows, up to a current layout.
};

struct ScenarioRecord final
{
    Presentation presentation = Presentation::Rich;
    std::vector<OperationRecord> operations;
};

// A prepared 64 Ki-unit editor in an embedded view: reusable by other fixtures that drive the same document.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
struct Fixture final
{
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    DxUi::RichTextEditor* editor = nullptr;
    UINT width                   = 640u;
    UINT height                  = 480u;

    Fixture(GraphicsFixture& gpu, const std::wstring& document, Presentation presentation)
    {
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "rich text-input copies: graphics");
        Hr(view.Attach(graphics), "rich text-input copies: attach");
        auto owned = std::make_unique<DxUi::RichTextEditor>(document);
        editor     = owned.get();
        if (presentation == Presentation::Source)
            editor->SetViewMode(DxUi::RichText::ViewMode::Source);
        if (presentation == Presentation::Semantic)
        {
            editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
            editor->SetMarkdownRenderingEnabled(true);
        }
        view.Controls().SetRoot(std::move(owned));
        Prepare();
        view.Controls().SetFocusControl(editor, false);
        Prepare();
    }

    void Prepare()
    {
        for (size_t turn = 0u; turn < 4096u; ++turn)
        {
            const HRESULT hr = view.Prepare(width, height);
            Hr(hr, "rich text-input copies: prepare");
            if (! view.NeedsPreparation())
                return;
        }
        Check(false, "rich text-input copies: preparation completes");
    }

    // A caret placed at a line start plus `column`, in the middle of the document, and its layout made current.
    void PlaceCaret(size_t line, size_t column)
    {
        const auto start = editor->Snapshot().LineStart(line);
        Check(start.has_value(), "rich text-input copies: the caret line exists");
        const size_t caret = *start + column;
        editor->SetSelection(DxUi::RichText::Selection{caret, caret});
        view.Controls().CommitFocusedTextInput();
        Prepare();
    }

    // One input event as a native host delivers it: the control's handler (with its own synchronization) and the host's
    // synchronization after dispatch.
    void Run(Operation operation)
    {
        auto& host = view.Controls();
        switch (operation)
        {
            case Operation::Insert: Check(view.DispatchCharacter(L'x'), "rich text-input copies: insert"); break;
            case Operation::ArrowRight: Check(view.DispatchKey(VK_RIGHT, true, 0u), "rich text-input copies: arrow right"); break;
            case Operation::ArrowLeft: Check(view.DispatchKey(VK_LEFT, true, 0u), "rich text-input copies: arrow left"); break;
            case Operation::Backspace: Check(view.DispatchKey(VK_BACK, true, 0u), "rich text-input copies: backspace"); break;
            case Operation::CtrlRight: Check(view.DispatchKey(VK_RIGHT, true, MK_CONTROL), "rich text-input copies: ctrl+right"); break;
            case Operation::CtrlLeft: Check(view.DispatchKey(VK_LEFT, true, MK_CONTROL), "rich text-input copies: ctrl+left"); break;
            case Operation::CtrlBackspace: Check(view.DispatchKey(VK_BACK, true, MK_CONTROL), "rich text-input copies: ctrl+backspace"); break;
            case Operation::Enter: Check(view.DispatchCharacter(L'\r'), "rich text-input copies: enter"); break;
            case Operation::DoubleClick:
            {
                const auto caret = editor->TryGetTextInputCaretRect(host, editor->GetSelection().caret);
                Check(caret.has_value(), "rich text-input copies: the caret has a rectangle to double-click");
                const auto point = D2D1::Point2F(caret->left + 2.0f, (caret->top + caret->bottom) * 0.5f);
                Check(editor->OnMouseDoubleClick(host, point, false, 0u), "rich text-input copies: double-click selects a word");
                break;
            }
        }
        host.CommitFocusedTextInput();
    }

    [[nodiscard]] OperationRecord Measure(Operation operation)
    {
        OperationRecord record{operation, {}, {}};
        DxUi::Detail::WorkCounterScope scope;
        Run(operation);
        record.input = ToCopies(scope.Delta());
        scope.Restart();
        Prepare();
        record.prepare = ToCopies(scope.Delta());
        return record;
    }
};
#pragma warning(pop)

[[nodiscard]] inline ScenarioRecord RunScenario(GraphicsFixture& gpu, const std::wstring& document, Presentation presentation)
{
    Fixture fixture(gpu, document, presentation);
    ScenarioRecord scenario{presentation, {}};
    // Each operation starts from the same place: line 400, inside its second word.
    for (const Operation operation : kOperations)
    {
        fixture.PlaceCaret(400u, 8u);
        scenario.operations.push_back(fixture.Measure(operation));
    }
    return scenario;
}

[[nodiscard]] inline const char* ConfigurationName() noexcept
{
#if defined(__SANITIZE_ADDRESS__)
    return "ASan Debug";
#elif defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

inline void WriteCopies(std::ostream& output, const Copies& copies)
{
    output << "{\"bytes\":" << copies.bytes << ",\"flattens\":" << copies.flattens << '}';
}

inline void WriteReceipt(const wchar_t* path, const std::vector<ScenarioRecord>& scenarios, const ImeCopies::ScenarioResult& ime)
{
    std::ofstream output{std::filesystem::path(path)};
    Check(bool(output), "rich text-input copies receipt file");
    output << "{\"workloadOwner\":\"DxUi\",\"plan\":\"W7.4\",\"fixture\":\"dxui-rich-text-input-copies-v1\",\"configuration\":\"" << ConfigurationName()
           << "\",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"compiler\":" << _MSC_FULL_VER << ",\"documentUnits\":" << kDocumentUnits
           << ",\"scenarios\":[";
    for (size_t index = 0u; index < scenarios.size(); ++index)
    {
        const ScenarioRecord& scenario = scenarios[index];
        output << (index ? "," : "") << "{\"presentation\":\"" << PresentationName(scenario.presentation) << "\",\"operations\":[";
        for (size_t at = 0u; at < scenario.operations.size(); ++at)
        {
            const OperationRecord& record = scenario.operations[at];
            output << (at ? "," : "") << "{\"operation\":\"" << OperationName(record.operation) << "\",\"input\":";
            WriteCopies(output, record.input);
            output << ",\"prepare\":";
            WriteCopies(output, record.prepare);
            output << '}';
        }
        output << "]}";
    }
    output << "],\"imeCopies\":";
    ImeCopies::WriteScenario(output, "RichTextEditor", ime);
    output << "}\n";
    output.close();
    Check(bool(output), "rich text-input copies receipt written");
}

// Runs every presentation and the ImeCopies RichTextEditor scenario. With a receipt path, the receipt is written before the
// gate is checked, so a failing run still leaves its counts.
inline void Run(GraphicsFixture& gpu, const wchar_t* receiptPath)
{
    const std::wstring document = MakeDocument(kDocumentUnits);
    std::vector<ScenarioRecord> scenarios;
    for (const Presentation presentation : {Presentation::Rich, Presentation::Source, Presentation::Semantic})
        scenarios.push_back(RunScenario(gpu, document, presentation));
    const ImeCopies::ScenarioResult ime = ImeCopies::RunRichTextEditor(ImeCopies::MakeDocument(ImeCopies::kDocumentUnits));
    for (const ScenarioRecord& scenario : scenarios)
    {
        std::cout << "Rich text-input copies (" << PresentationName(scenario.presentation) << "):";
        for (const OperationRecord& record : scenario.operations)
            std::cout << ' ' << OperationName(record.operation) << ' ' << record.input.bytes << " B/" << record.input.flattens << " flattens (+"
                      << record.prepare.bytes << " B/" << record.prepare.flattens << " preparing)";
        std::cout << '\n';
    }
    std::cout << "Rich text-input copies: RichTextEditor IME " << ime.MaximumDocumentAllocationsPerLock() << " document allocations per lock at most\n";
    if (receiptPath)
        WriteReceipt(receiptPath, scenarios, ime);
    for (const ScenarioRecord& scenario : scenarios)
        for (const OperationRecord& record : scenario.operations)
        {
            const bool insertOrArrow =
                record.operation == Operation::Insert || record.operation == Operation::ArrowRight || record.operation == Operation::ArrowLeft;
            Check(! insertOrArrow || record.input.bytes <= kMaximumBytesPerInsertOrArrow,
                  "rich text-input copies: a one-character insert or arrow key copies at most 1 KiB of the document");
            Check(record.input.flattens <= kMaximumFlattensPerOperation, "rich text-input copies: no gated input event flattens the document");
        }
    Check(ime.MaximumDocumentAllocationsPerLock() <= ImeCopies::kMaximumDocumentAllocationsPerLock,
          "rich text-input copies: no TSF lock in a RichTextEditor makes more than one document-sized allocation");
}

// ---------------------------------------------------------------------------------------------------------------------------
// Overrides: a class derived from RichTextEditor that overrides the text-input state is called on every input path.

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// Drops every 'x' that text input imports, as ImeCopies::FilteringField does for TextField.
struct FilteringEditor final : DxUi::RichTextEditor
{
    using RichTextEditor::RichTextEditor;
    size_t imports = 0u;
    size_t exports = 0u;

protected:
    bool ExportTextInputState(DxUi::TextInputState& state) const override
    {
        ++const_cast<FilteringEditor*>(this)->exports;
        return RichTextEditor::ExportTextInputState(state);
    }
    bool ImportTextInputState(DxUi::ControlHost& host, const DxUi::TextInputState& state, bool notifyChange) override
    {
        ++imports;
        DxUi::TextInputState filtered   = state;
        const size_t removedBeforeCaret = static_cast<size_t>(
            std::count(filtered.text.begin(), filtered.text.begin() + static_cast<std::ptrdiff_t>((std::min)(state.caretIndex, state.text.size())), L'x'));
        std::erase(filtered.text, L'x');
        filtered.caretIndex = state.caretIndex - removedBeforeCaret;
        filtered.selectionAnchorIndex.reset();
        return RichTextEditor::ImportTextInputState(host, filtered, notifyChange);
    }
};
// Keeps both virtuals and opts in to the snapshot path.
struct SharingEditor final : DxUi::RichTextEditor
{
    using RichTextEditor::RichTextEditor;
    [[nodiscard]] bool KeepsDefaultTextInputState() const noexcept override
    {
        return true;
    }
};
#pragma warning(pop)

inline void TestOverridesRunOnEveryInputPath(GraphicsFixture& gpu)
{
    {
        ImeCopies::HeadlessHost fixture;
        auto* const editor = fixture.root->AddChild<FilteringEditor>(L"ab");
        editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 64.0f));
        fixture.host.SetFocusControl(editor, false);
        editor->SetSelection(DxUi::RichText::Selection{2u, 2u});
        Check(! DxUi::SharedTextInputAccess::Shares(*editor), "a RichTextEditor that overrides its text-input state does not share it");
        wil::com_ptr_nothrow<ITextStoreACP> store;
        store.attach(DxUi::CreateNativeTextInputTextStore(fixture.host, *editor, false));
        Check(store != nullptr, "editor override fixture: the TSF store is created");
        ImeCopies::Sink sink;
        Hr(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_ALL_SINKS), "editor override fixture: the sink is advised");
        sink.onLock = [&]() noexcept
        {
            LONG first = 0, last = 0;
            TS_TEXTCHANGE change{};
            return store->InsertTextAtSelection(0u, L"xyx", 3u, &first, &last, &change);
        };
        HRESULT session = E_FAIL;
        Hr(store->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &session), "editor override fixture: the TSF lock is requested");
        sink.onLock = nullptr;
        Check(editor->imports >= 1u && editor->exports >= 1u && editor->GetText() == L"aby",
              "a RichTextEditor import override filters text typed through TSF, and its export is read");
        Hr(store->UnadviseSink(&sink), "editor override fixture: the sink is unadvised");
    }
    {
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> pool;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "editor override fixture: graphics");
        Hr(view.Attach(pool), "editor override fixture: attach");
        auto panel         = std::make_unique<DxUi::Panel>();
        auto* const editor = panel->AddChild<FilteringEditor>(L"ab");
        editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 64.0f));
        view.Controls().SetRoot(std::move(panel));
        Hr(view.Prepare(320, 80), "editor override fixture: prepare");
        view.Controls().SetFocusControl(editor);
        DxUi::EmbeddedTextInputSnapshot snapshot;
        Check(view.ReadTextInput(snapshot) == S_OK, "editor override fixture: the embedded text service reads the editor");
        DxUi::NativeTextInputState edit = snapshot.state;
        edit.text                       = L"abxyx";
        edit.caretIndex                 = 5u;
        Hr(view.ApplyTextInput(snapshot.revision, edit, DxUi::EmbeddedTextInputAction::Commit), "editor override fixture: the embedded commit applies");
        Check(editor->imports >= 1u && editor->GetText() == L"aby", "a RichTextEditor import override filters text committed through the embedded service");
    }
    {
        // A derived editor that opts in keeps the snapshot path and its one-allocation lock.
        ImeCopies::HeadlessHost fixture;
        const std::wstring document = ImeCopies::MakeDocument(ImeCopies::kDocumentUnits);
        auto* const editor          = fixture.root->AddChild<SharingEditor>(document);
        editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 480.0f));
        fixture.host.SetFocusControl(editor, false);
        editor->SetSelection(DxUi::RichText::Selection{ImeCopies::kCaretUnits, ImeCopies::kCaretUnits});
        Check(DxUi::SharedTextInputAccess::Shares(*editor), "a derived RichTextEditor that opts in shares its snapshot");
        const auto result = ImeCopies::ComposeTenSyllables(fixture.host, *editor, ImeCopies::kCaretUnits);
        Check(editor->GetText() == ImeCopies::ExpectedText(document, ImeCopies::kCaretUnits) &&
                  result.MaximumDocumentAllocationsPerLock() <= ImeCopies::kMaximumDocumentAllocationsPerLock,
              "a derived RichTextEditor that opts in keeps the one-allocation lock");
        Check(editor->OnKeyDown(fixture.host, 'Z', MK_CONTROL) && editor->GetText() == document, "one Undo removes the whole composition");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// A change notification after a keystroke: the store compares the editor's revisions in place and reports the exact
// W1.4 change, with no document copy.

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class ChangeSink final : public ITextStoreACPSink
{
public:
    std::vector<TS_TEXTCHANGE> changes;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) noexcept override
    {
        if (! out)
            return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(ITextStoreACPSink))
            return E_NOINTERFACE;
        *out = static_cast<ITextStoreACPSink*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++_references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return --_references;
    }
    HRESULT STDMETHODCALLTYPE OnTextChange(DWORD, const TS_TEXTCHANGE* change) noexcept override
    {
        try
        {
            changes.push_back(*change);
        }
        catch (const std::bad_alloc&)
        {
            DxUi::Failure::FailAllocation();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSelectionChange() noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnLayoutChange(TsLayoutCode, TsViewCookie) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStatusChange(DWORD) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnAttrsChange(LONG, LONG, ULONG, const TS_ATTRID*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnLockGranted(DWORD) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStartEditTransaction() noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEndEditTransaction() noexcept override
    {
        return S_OK;
    }

private:
    std::atomic<ULONG> _references{1u};
};
#pragma warning(pop)

inline void TestKeystrokeNotificationReadsInPlace()
{
    ImeCopies::HeadlessHost fixture;
    // A CRLF and a surrogate pair right at the edit: the change must not split either.
    std::wstring document = ImeCopies::MakeDocument(ImeCopies::kDocumentUnits - 8u);
    const size_t caret    = ImeCopies::kCaretUnits;
    document.insert(caret, L"\r\n\U0001F600");
    auto* const editor = fixture.root->AddChild<DxUi::RichTextEditor>(document);
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 480.0f));
    fixture.host.SetFocusControl(editor, false);
    editor->SetSelection(DxUi::RichText::Selection{caret + 2u, caret + 2u});
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(DxUi::CreateNativeTextInputTextStore(fixture.host, *editor, false));
    Check(store != nullptr, "notification fixture: the TSF store is created");
    ChangeSink sink;
    Hr(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_ALL_SINKS), "notification fixture: the sink is advised");
    DxUi::NotifyTextStoreChanged(store.get());
    for (const wchar_t typed : std::wstring_view(L"q\r"))
    {
        const std::wstring before = editor->GetText();
        sink.changes.clear();
        const DxUi::Detail::WorkCounterScope scope;
        Check(editor->OnChar(fixture.host, typed, 0u), "notification fixture: the editor takes the character");
        fixture.host.CommitFocusedTextInput();
        DxUi::NotifyTextStoreChanged(store.get());
        const auto delta         = scope.Delta();
        const std::wstring after = editor->GetText();
        Check(delta.rich.documentFlattens == 0u && delta.rich.documentBytesCopied <= kMaximumBytesPerInsertOrArrow,
              "a keystroke and its text-store change notification copy no rich document");
        const TS_TEXTCHANGE expected = DxUi::ComputeTextStoreTextChange(std::wstring_view(before), std::wstring_view(after));
        Check(sink.changes.size() == 1u && sink.changes[0].acpStart == expected.acpStart && sink.changes[0].acpOldEnd == expected.acpOldEnd &&
                  sink.changes[0].acpNewEnd == expected.acpNewEnd,
              "the notification reports the exact common-prefix-and-suffix change of the whole texts");
    }
    Hr(store->UnadviseSink(&sink), "notification fixture: the sink is unadvised");
}

// ---------------------------------------------------------------------------------------------------------------------------
// Seeded differential: every windowed read equals the flattening code it replaced.

[[nodiscard]] inline std::wstring RandomText(std::mt19937& random, size_t units)
{
    static constexpr std::array<std::wstring_view, 26u> pieces{L"a",
                                                               L"word",
                                                               L"x_1",
                                                               L" ",
                                                               L"  ",
                                                               L"\t",
                                                               L"\r\n",
                                                               L"\n",
                                                               L"\r",
                                                               L"C:\\",
                                                               L"/path/",
                                                               L"(",
                                                               L").",
                                                               L"\u6f22\u5b57",
                                                               L"\u3042",
                                                               L"\U0001F600",
                                                               L"\U0001F468\u200D\U0001F469\u200D\U0001F467",
                                                               L"e\u0301",
                                                               L"a\u0308\u0323",
                                                               L"\u05E9\u05DC\u05D5\u05DD",
                                                               L"\u0645\u0631\u062D\u0628\u0627",
                                                               L"\U0001F1EF\U0001F1F5",
                                                               L"--",
                                                               L"9",
                                                               L"\u00A0",
                                                               L"\u0915\u094D\u0937"};
    std::wstring text(0u, L' ');
    while (text.size() < units)
    {
        // An occasional very long run with no break exercises window growth.
        if (random() % 97u == 0u)
            text.append(300u + random() % 400u, random() % 2u ? L'w' : L' ');
        else
            text += pieces[random() % pieces.size()];
    }
    return text;
}

inline void TestWindowedReadsMatchFlattening()
{
    using DxUi::RichText::Detail::SourceWindowOperation;
    std::mt19937 random(0x7744u);
    for (size_t sample = 0u; sample < 24u; ++sample)
    {
        const std::wstring text = RandomText(random, sample < 4u ? 8u + sample * 5u : 1'500u + random() % 2'500u);
        DxUi::RichText::Document document(text);
        const auto source = document.Snapshot();
        Check(source.MatchesText(text), "differential: the document holds the text");
        // Line summaries: every line start and the first line ending.
        size_t line = 0u;
        for (size_t at = 0u; at <= text.size(); ++at)
            if (at == 0u || ((text[at - 1u] == L'\n') || (text[at - 1u] == L'\r' && (at == text.size() || text[at] != L'\n'))))
            {
                Check(source.LineStart(line) == std::optional<size_t>(at), "differential: LineStart matches a scan");
                ++line;
            }
        Check(! source.LineStart(line).has_value(), "differential: no line past the last");
        const auto newline                     = text.find_first_of(L"\r\n");
        const std::wstring_view expectedEnding = newline == text.npos ? std::wstring_view(L"\r\n")
                                                 : text[newline] == L'\r' && newline + 1u < text.size() && text[newline + 1u] == L'\n'
                                                     ? std::wstring_view(L"\r\n")
                                                 : text[newline] == L'\r' ? std::wstring_view(L"\r")
                                                                          : std::wstring_view(L"\n");
        Check(DxUi::RichText::Detail::FirstLineEnding(source) == expectedEnding, "differential: the first line ending matches a scan");
        const auto unitAt = [&text](size_t index) { return text[index]; };
        for (size_t caret = 0u; caret <= text.size(); caret += (sample < 4u ? 1u : 1u + random() % 7u))
        {
            if (! DxUi::RichText::IsEditBoundary(text, caret))
                continue;
            const auto range = [&](SourceWindowOperation operation) { return DxUi::RichText::Detail::SourceWindowRange(source, caret, operation); };
            Check(range(SourceWindowOperation::PreviousWord).first == DxUi::FindPreviousWordBoundary(text, caret), "differential: previous word");
            Check(range(SourceWindowOperation::NextWord).last == DxUi::FindNextWordBoundary(text, caret), "differential: next word");
            Check(range(SourceWindowOperation::Previous).first == DxUi::StepToPreviousTextElement(text, caret), "differential: previous element");
            Check(range(SourceWindowOperation::Next).last == DxUi::StepToNextTextElement(text, caret), "differential: next element");
            for (const bool backwards : {true, false})
            {
                const auto expected = DxUi::GetTextElementDeletionRange(text, caret, backwards);
                const auto actual   = range(backwards ? SourceWindowOperation::DeleteBackward : SourceWindowOperation::DeleteForward);
                Check(actual.first == expected.first && actual.last == expected.last, "differential: grapheme deletion");
            }
            Check(DxUi::RichText::Detail::WindowedPreviousWordBoundary(text.size(), caret, unitAt) == DxUi::FindPreviousWordBoundary(text, caret),
                  "differential: windowed previous word");
            Check(DxUi::RichText::Detail::WindowedNextWordBoundary(text.size(), caret, unitAt) == DxUi::FindNextWordBoundary(text, caret),
                  "differential: windowed next word");
            size_t expectedCaret = 0u, actualCaret = 0u;
            std::optional<size_t> expectedAnchor, actualAnchor;
            DxUi::SelectSingleLineWordAt(text, caret, expectedCaret, expectedAnchor);
            DxUi::RichText::Detail::WindowedSelectWordAt(text.size(), caret, actualCaret, actualAnchor, unitAt);
            Check(expectedCaret == actualCaret && expectedAnchor == actualAnchor, "differential: windowed word selection");
        }
        // Change notifications between two revisions equal the whole-text rule, and so does content equality.
        for (size_t edit = 0u; edit < 8u; ++edit)
        {
            const auto before             = document.Snapshot();
            const std::wstring beforeText = before.Text();
            size_t first = random() % (beforeText.size() + 1u), last = first + random() % 6u;
            last = (std::min)(last, beforeText.size());
            while (first && ! DxUi::RichText::IsEditBoundary(beforeText, first))
                --first;
            while (last < beforeText.size() && ! DxUi::RichText::IsEditBoundary(beforeText, last))
                ++last;
            const std::wstring insertion = edit % 3u == 0u ? std::wstring(beforeText.substr(first, last - first)) : RandomText(random, random() % 4u);
            bool changed                 = false;
            if (document.TryReplace(first, last, insertion, {first, first}, changed) != DxUi::RichText::SourceResult::Success)
                continue;
            const auto after             = document.Snapshot();
            const std::wstring afterText = after.Text();
            DxUi::SharedTextInputState left{}, right{};
            left.source                  = before;
            right.source                 = after;
            const TS_TEXTCHANGE expected = DxUi::ComputeTextStoreTextChange(std::wstring_view(beforeText), std::wstring_view(afterText));
            const TS_TEXTCHANGE actual   = DxUi::ComputeTextStoreTextChange(left, right);
            Check(expected.acpStart == actual.acpStart && expected.acpOldEnd == actual.acpOldEnd && expected.acpNewEnd == actual.acpNewEnd,
                  "differential: a change between revisions equals the whole-text change");
            Check(DxUi::SameTextInputContent(left, right) == (beforeText == afterText), "differential: revision content equality");
            right.text = DxUi::MakeSharedText(afterText);
            right.source.reset();
            const TS_TEXTCHANGE mixed = DxUi::ComputeTextStoreTextChange(left, right);
            Check(expected.acpStart == mixed.acpStart && expected.acpOldEnd == mixed.acpOldEnd && expected.acpNewEnd == mixed.acpNewEnd,
                  "differential: a change between a revision and text equals the whole-text change");
            Check(DxUi::SameTextInputContent(left, right) == (beforeText == afterText), "differential: revision and text content equality");
        }
    }
}

inline void RunBehaviour(GraphicsFixture& gpu)
{
    TestWindowedReadsMatchFlattening();
    TestKeystrokeNotificationReadsInPlace();
    TestOverridesRunOnEveryInputPath(gpu);
}
} // namespace RichTextInputCopies
