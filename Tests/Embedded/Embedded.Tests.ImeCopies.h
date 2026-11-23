#pragma once
// IME copies (W8.5 of the performance and motion plan; fixture shapes for FX7 and FX2). A composition of ten syllables and its
// commit are driven through the production TSF store (CreateNativeTextInputTextStore) on a headless native host, with an
// in-process sink, range and composition view standing in for TSF, so nothing takes focus or needs a text service. The
// replaced allocation functions of Embedded.Tests.BenchmarkInstrument.h count, inside each lock, every C++ allocation of at
// least one document's UTF-16 bytes: a copy of the document. Included by Embedded.Tests.Embedded.cpp after the instrument.
#include "Controls/DxUi.Internal.h"
#include "Embedded.Tests.TsfDoubles.h"
#include "Support/WorkCounters.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <msctf.h>
#include <textstor.h>

namespace ImeCopies
{
// 64 Ki UTF-16 units less room for the composition: RichTextEditor documents stop at 64 Ki units.
inline constexpr size_t kDocumentUnits = 65'536u - 64u;
inline constexpr size_t kDocumentBytes = kDocumentUnits * sizeof(wchar_t);
inline constexpr size_t kCaretUnits    = kDocumentUnits / 2u;
// Ten syllables, one UTF-16 unit each; syllable k shows the first k as the composition.
inline constexpr std::wstring_view kSyllables = L"한국어입력시험문장끝";
static_assert(kSyllables.size() == 10u);
// The gate: at most one document-sized allocation in any lock.
inline constexpr size_t kMaximumDocumentAllocationsPerLock = 1u;

// The TSF stand-ins (sink, range, composition view) are shared with the FX7 performance fixture.
using TsfDoubles::Composition;
using TsfDoubles::Range;
using TsfDoubles::Sink;

// What one lock allocated: every counted C++ allocation, and those of at least one document's bytes.
struct LockRecord final
{
    size_t documentAllocations = 0u;
    size_t documentBytes       = 0u;
    size_t allocations         = 0u;
    size_t bytes               = 0u;
};

struct ScenarioResult final
{
    std::vector<LockRecord> editLocks; // One read-write lock per syllable.
    std::vector<LockRecord> readLocks; // One read lock per syllable, as a candidate window positions itself.
    LockRecord commitLock;
    uint64_t popupRebuildsBeforeCommit = 0u;
    uint64_t popupRebuildsAtCommit     = 0u;
    size_t extentsWithoutLayout        = 0u;

    [[nodiscard]] size_t MaximumDocumentAllocationsPerLock() const noexcept
    {
        size_t maximum = commitLock.documentAllocations;
        for (const LockRecord& lock : editLocks)
            maximum = (std::max)(maximum, lock.documentAllocations);
        for (const LockRecord& lock : readLocks)
            maximum = (std::max)(maximum, lock.documentAllocations);
        return maximum;
    }
    // Per syllable: the edit lock and the read lock that follows it, averaged over the ten syllables.
    [[nodiscard]] double DocumentAllocationsPerSyllable() const noexcept
    {
        size_t total = 0u;
        for (size_t index = 0u; index < editLocks.size(); ++index)
            total += editLocks[index].documentAllocations + readLocks[index].documentAllocations;
        return editLocks.empty() ? 0.0 : static_cast<double>(total) / static_cast<double>(editLocks.size());
    }
    [[nodiscard]] double DocumentBytesPerSyllable() const noexcept
    {
        size_t total = 0u;
        for (size_t index = 0u; index < editLocks.size(); ++index)
            total += editLocks[index].documentBytes + readLocks[index].documentBytes;
        return editLocks.empty() ? 0.0 : static_cast<double>(total) / static_cast<double>(editLocks.size());
    }
};

// A deterministic document of kDocumentUnits printable units with spaces, no line breaks.
[[nodiscard]] inline std::wstring MakeDocument(size_t units)
{
    std::wstring text(units, L'a');
    for (size_t index = 0u; index < units; ++index)
        text[index] = (index % 8u) == 7u ? L' ' : static_cast<wchar_t>(L'a' + (index % 26u));
    return text;
}

// Runs one lock with counting on: the allocations of the whole RequestLock, including the store's commit to the control.
template <typename Body> [[nodiscard]] LockRecord CountLock(ITextStoreACP& store, Sink& sink, DWORD flags, Body&& body)
{
    HRESULT bodyResult = S_OK;
    sink.onLock        = [&]() noexcept
    {
        bodyResult = body();
        return bodyResult;
    };
    HRESULT session             = E_FAIL;
    largeAllocationMinimumBytes = kDocumentBytes;
    largeAllocations            = 0u;
    largeAllocationBytes        = 0u;
    allocations                 = 0u;
    allocationBytes             = 0u;
    countAllocations            = true;
    const HRESULT requested     = store.RequestLock(flags | TS_LF_SYNC, &session);
    countAllocations            = false;
    largeAllocationMinimumBytes = std::numeric_limits<size_t>::max();
    sink.onLock                 = nullptr;
    Hr(requested, "IME copies: the store grants the lock");
    Hr(session, "IME copies: the lock session succeeds");
    Hr(bodyResult, "IME copies: every store call inside the lock succeeds");
    return LockRecord{largeAllocations, largeAllocationBytes, allocations, allocationBytes};
}

// Ten syllables composed at the caret of a focused text control, each one read-write lock that rewrites the composition and
// one read lock that reads it back, then a commit lock that ends the composition. Every store call is one a real text
// service makes: selection, end, text, extent and the composition owner callbacks.
// afterEachSyllable, when given, runs after each syllable's locks, outside the counted locks.
[[nodiscard]] inline ScenarioResult ComposeTenSyllables(DxUi::ControlHost& host,
                                                        DxUi::Control& control,
                                                        size_t caret,
                                                        const std::function<void()>& afterEachSyllable = {})
{
    ScenarioResult result;
    wil::com_ptr_nothrow<ITextStoreACP> store;
    store.attach(DxUi::CreateNativeTextInputTextStore(host, control, false));
    Check(store != nullptr, "IME copies: a focused native control creates a store");
    wil::com_ptr_nothrow<ITfContextOwnerCompositionSink> owner;
    Hr(store.query_to(owner.put()), "IME copies: the store is its own composition owner sink");
    Sink sink;
    Hr(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_ALL_SINKS), "IME copies: the sink is advised");
    Composition composition;
    composition.range.start     = static_cast<LONG>(caret);
    const LONG compositionStart = static_cast<LONG>(caret);
    result.editLocks.reserve(kSyllables.size());
    result.readLocks.reserve(kSyllables.size());
    // The field has been shown before anyone types: one uncounted read lock asks for the caret's extent, as a text service
    // does when a composition starts, so the control's layout exists.
    static_cast<void>(CountLock(*store,
                                sink,
                                TS_LF_READ,
                                [&]() noexcept -> HRESULT
    {
        RECT extent{};
        BOOL clipped     = FALSE;
        const HRESULT hr = store->GetTextExt(1u, compositionStart, compositionStart, &extent, &clipped);
        return hr == TS_E_NOLAYOUT ? S_OK : hr;
    }));

    for (size_t syllable = 1u; syllable <= kSyllables.size(); ++syllable)
    {
        const LONG previousEnd = compositionStart + static_cast<LONG>(syllable - 1u);
        const LONG end         = compositionStart + static_cast<LONG>(syllable);
        const DxUi::Detail::WorkCounterScope rebuilds;
        result.editLocks.push_back(CountLock(*store,
                                             sink,
                                             TS_LF_READWRITE,
                                             [&]() noexcept -> HRESULT
        {
            TS_SELECTION_ACP selection{};
            ULONG fetched    = 0u;
            HRESULT hr       = store->GetSelection(TS_DEFAULT_SELECTION, 1u, &selection, &fetched);
            LONG documentEnd = 0;
            if (SUCCEEDED(hr))
                hr = store->GetEndACP(&documentEnd);
            TS_TEXTCHANGE change{};
            if (SUCCEEDED(hr) && syllable == 1u)
            {
                LONG first               = 0;
                LONG last                = 0;
                hr                       = store->InsertTextAtSelection(0u, kSyllables.data(), 1u, &first, &last, &change);
                composition.range.length = 1;
                BOOL accepted            = FALSE;
                if (SUCCEEDED(hr))
                    hr = owner->OnStartComposition(&composition, &accepted);
                if (SUCCEEDED(hr) && ! accepted)
                    hr = E_FAIL;
            }
            else if (SUCCEEDED(hr))
            {
                hr                       = store->SetText(0u, compositionStart, previousEnd, kSyllables.data(), static_cast<ULONG>(syllable), &change);
                composition.range.length = static_cast<LONG>(syllable);
                if (SUCCEEDED(hr))
                    hr = owner->OnUpdateComposition(&composition, &composition.range);
                const TS_SELECTION_ACP caretAtEnd{end, end, {TS_AE_END, FALSE}};
                if (SUCCEEDED(hr))
                    hr = store->SetSelection(1u, &caretAtEnd);
            }
            std::array<WCHAR, 16u> text{};
            ULONG copied = 0u;
            ULONG runs   = 0u;
            LONG next    = 0;
            if (SUCCEEDED(hr))
                hr = store->GetText(compositionStart, end, text.data(), static_cast<ULONG>(text.size()), &copied, nullptr, 0u, &runs, &next);
            return hr;
        }));
        result.popupRebuildsBeforeCommit += rebuilds.Delta().popups.comboBoxPopupRebuilds;
        result.readLocks.push_back(CountLock(*store,
                                             sink,
                                             TS_LF_READ,
                                             [&]() noexcept -> HRESULT
        {
            TS_SELECTION_ACP selection{};
            ULONG fetched    = 0u;
            HRESULT hr       = store->GetSelection(TS_DEFAULT_SELECTION, 1u, &selection, &fetched);
            LONG documentEnd = 0;
            if (SUCCEEDED(hr))
                hr = store->GetEndACP(&documentEnd);
            RECT extent{};
            BOOL clipped = FALSE;
            if (SUCCEEDED(hr))
            {
                // The extent query lays the text out. A range the headless view does not show has no extent, which TSF
                // allows (TS_E_NOLAYOUT); the layout work and its copies are counted either way.
                hr = store->GetTextExt(1u, compositionStart, end, &extent, &clipped);
                if (hr == TS_E_NOLAYOUT)
                {
                    ++result.extentsWithoutLayout;
                    hr = S_OK;
                }
            }
            std::array<WCHAR, 16u> text{};
            ULONG copied = 0u;
            ULONG runs   = 0u;
            LONG next    = 0;
            if (SUCCEEDED(hr))
                hr = store->GetText(compositionStart, end, text.data(), static_cast<ULONG>(text.size()), &copied, nullptr, 0u, &runs, &next);
            return hr;
        }));
        if (afterEachSyllable)
            afterEachSyllable();
    }

    const DxUi::Detail::WorkCounterScope commitRebuilds;
    const LONG end               = compositionStart + static_cast<LONG>(kSyllables.size());
    result.commitLock            = CountLock(*store,
                                             sink,
                                             TS_LF_READWRITE,
                                             [&]() noexcept -> HRESULT
    {
        TS_TEXTCHANGE change{};
        HRESULT hr = store->SetText(0u, compositionStart, end, kSyllables.data(), static_cast<ULONG>(kSyllables.size()), &change);
        if (SUCCEEDED(hr))
            hr = owner->OnEndComposition(&composition);
        return hr;
    });
    result.popupRebuildsAtCommit = commitRebuilds.Delta().popups.comboBoxPopupRebuilds;
    Hr(store->UnadviseSink(&sink), "IME copies: the sink is unadvised");
    return result;
}

[[nodiscard]] inline std::wstring ExpectedText(const std::wstring& document, size_t caret)
{
    std::wstring expected = document;
    expected.insert(caret, kSyllables);
    return expected;
}

inline void WriteLocks(std::ostream& output, const char* name, const std::vector<LockRecord>& locks)
{
    output << '"' << name << "\":[";
    for (size_t index = 0u; index < locks.size(); ++index)
    {
        const LockRecord& lock = locks[index];
        output << (index ? "," : "") << "{\"documentAllocations\":" << lock.documentAllocations << ",\"documentBytes\":" << lock.documentBytes
               << ",\"allocations\":" << lock.allocations << ",\"bytes\":" << lock.bytes << '}';
    }
    output << ']';
}

inline void WriteScenario(std::ostream& output, const char* control, const ScenarioResult& result)
{
    const LockRecord& commit = result.commitLock;
    output << "{\"control\":\"" << control << "\",\"maximumDocumentAllocationsPerLock\":" << result.MaximumDocumentAllocationsPerLock()
           << ",\"documentAllocationsPerSyllable\":" << result.DocumentAllocationsPerSyllable()
           << ",\"documentBytesPerSyllable\":" << result.DocumentBytesPerSyllable() << ",\"commitLock\":{\"documentAllocations\":" << commit.documentAllocations
           << ",\"documentBytes\":" << commit.documentBytes << ",\"allocations\":" << commit.allocations << ",\"bytes\":" << commit.bytes
           << "},\"popupRebuildsBeforeCommit\":" << result.popupRebuildsBeforeCommit << ",\"popupRebuildsAtCommit\":" << result.popupRebuildsAtCommit
           << ",\"extentsWithoutLayout\":" << result.extentsWithoutLayout << ',';
    WriteLocks(output, "editLocks", result.editLocks);
    output << ',';
    WriteLocks(output, "readLocks", result.readLocks);
    output << '}';
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

// A focused control on a headless native host: logical focus and the native session without a window or desktop.
struct HeadlessHost final
{
    DxUi::ControlHost host;
    DxUi::Panel* root = nullptr;
    HeadlessHost()
    {
        host.SetTextInputBackend(DxUi::TextInputBackend::Native);
        auto panel = std::make_unique<DxUi::Panel>();
        root       = panel.get();
        host.SetRoot(std::move(panel));
    }
    HeadlessHost(const HeadlessHost&)            = delete;
    HeadlessHost& operator=(const HeadlessHost&) = delete;
    HeadlessHost(HeadlessHost&&)                 = delete;
    HeadlessHost& operator=(HeadlessHost&&)      = delete;
};

template <typename Field = DxUi::TextField> inline ScenarioResult RunTextField(const std::wstring& document)
{
    HeadlessHost fixture;
    auto* const field = fixture.root->AddChild<Field>(document);
    Check(field != nullptr, "IME copies: the TextField fixture is created");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 32.0f));
    field->SetSelectionRange(kCaretUnits, kCaretUnits);
    size_t notifications = 0u;
    field->SetOnTextChanged([&notifications](std::wstring_view) { ++notifications; });
    fixture.host.SetFocusControl(field, false);
    const ScenarioResult result = ComposeTenSyllables(fixture.host, *field, kCaretUnits);
    const std::wstring expected = ExpectedText(document, kCaretUnits);
    Check(field->GetText() == expected, "IME copies: the TextField commits the composed syllables at the caret");
    Check(notifications == 1u, "IME copies: the TextField notifies the committed text once");
    Check(field->OnKeyDown(fixture.host, 'Z', MK_CONTROL) && field->GetText() == document, "IME copies: one Undo removes the whole composition");
    return result;
}

inline ScenarioResult RunRichTextEditor(const std::wstring& document)
{
    HeadlessHost fixture;
    auto* const editor = fixture.root->AddChild<DxUi::RichTextEditor>(document);
    Check(editor != nullptr, "IME copies: the RichTextEditor fixture is created");
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 480.0f));
    fixture.host.SetFocusControl(editor, false);
    editor->SetSelection(DxUi::RichText::Selection{kCaretUnits, kCaretUnits});
    const ScenarioResult result = ComposeTenSyllables(fixture.host, *editor, kCaretUnits);
    Check(editor->GetText() == ExpectedText(document, kCaretUnits), "IME copies: the RichTextEditor commits the composed syllables at the caret");
    Check(editor->OnKeyDown(fixture.host, 'Z', MK_CONTROL) && editor->GetText() == document, "IME copies: one Undo removes the whole rich composition");
    return result;
}

// FX2's shape: an editable ComboBox whose popup is open while a composition is typed. The popup keeps the items filtered by
// the committed text until the composition commits, then filters by the result.
inline ScenarioResult RunComboBox()
{
    HeadlessHost fixture;
    auto* const combo = fixture.root->AddChild<DxUi::ComboBox>();
    Check(combo != nullptr, "IME copies: the ComboBox fixture is created");
    combo->SetEditable(true);
    std::vector<DxUi::ComboBox::Item> items;
    items.push_back({L"apple", L"apple"});
    items.push_back({std::wstring(kSyllables), std::wstring(kSyllables)});
    items.push_back({std::wstring(kSyllables.substr(0u, 3u)) + L" rest", std::wstring(kSyllables.substr(0u, 3u)) + L" rest"});
    items.push_back({L"banana", L"banana"});
    combo->SetItems(std::move(items));
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 32.0f));
    combo->SetText(L"");
    fixture.host.SetFocusControl(combo, false);
    // The popup is open while composing: it lists every item for the empty committed text, selects none, and keeps that
    // list until the commit filters it by the result (one exact item).
    Check(combo->OnKeyDown(fixture.host, VK_F4, 0u) && combo->DebugIsPopupOpen(), "IME copies: F4 opens the ComboBox popup");
    const float allItemsHeight = combo->DebugGetPopupBounds().bottom - combo->DebugGetPopupBounds().top;
    Check(allItemsHeight > 0.0f, "IME copies: the open popup has a height");
    bool popupKept              = true;
    const ScenarioResult result = ComposeTenSyllables(fixture.host,
                                                      *combo,
                                                      0u,
                                                      [&]()
    {
        const D2D1_RECT_F bounds = combo->DebugGetPopupBounds();
        popupKept = popupKept && combo->DebugIsPopupOpen() && bounds.bottom - bounds.top == allItemsHeight && ! combo->GetSelectedIndex().has_value();
    });
    Check(popupKept, "IME copies: the open popup keeps the committed text's items while a composition previews");
    Check(combo->GetText() == kSyllables, "IME copies: the ComboBox commits the composed syllables");
    Check(combo->GetSelectedIndex() == std::optional<size_t>(1u), "IME copies: the committed ComboBox text selects its exact item");
    const float committedHeight = combo->DebugGetPopupBounds().bottom - combo->DebugGetPopupBounds().top;
    Check(combo->DebugIsPopupOpen() && committedHeight > 0.0f && committedHeight < allItemsHeight,
          "IME copies: the commit filters the open popup by the committed text");
    return result;
}

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// A derived field that keeps both state virtuals and opts in to sharing: it keeps the copy gate.
struct SharingField final : DxUi::TextField
{
    using TextField::TextField;
    [[nodiscard]] bool KeepsDefaultTextInputState() const noexcept override
    {
        return true;
    }
};

// A derived field that filters what text input imports: it drops every 'x'. Its override must run on every input path.
struct FilteringField final : DxUi::TextField
{
    using TextField::TextField;
    size_t imports = 0u;

protected:
    bool ImportTextInputState(DxUi::ControlHost& host, const DxUi::TextInputState& state, bool notifyChange) override
    {
        ++imports;
        DxUi::TextInputState filtered   = state;
        const size_t removedBeforeCaret = static_cast<size_t>(
            std::count(filtered.text.begin(), filtered.text.begin() + static_cast<std::ptrdiff_t>((std::min)(state.caretIndex, state.text.size())), L'x'));
        std::erase(filtered.text, L'x');
        filtered.caretIndex = state.caretIndex - removedBeforeCaret;
        filtered.selectionAnchorIndex.reset();
        return TextField::ImportTextInputState(host, filtered, notifyChange);
    }
};
#pragma warning(pop)

// The filtering override runs under the TSF store as it does under the embedded text service: the same input gives the
// same text on both paths.
inline void TestImportOverrideRunsOnEveryInputPath(GraphicsFixture& gpu)
{
    {
        HeadlessHost fixture;
        auto* const field = fixture.root->AddChild<FilteringField>(L"ab");
        field->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 32.0f));
        field->SetSelectionRange(2u, 2u);
        fixture.host.SetFocusControl(field, false);
        wil::com_ptr_nothrow<ITextStoreACP> store;
        store.attach(DxUi::CreateNativeTextInputTextStore(fixture.host, *field, false));
        Check(store != nullptr, "override fixture: the TSF store is created");
        Sink sink;
        Hr(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_ALL_SINKS), "override fixture: the sink is advised");
        sink.onLock = [&]() noexcept
        {
            LONG first = 0, last = 0;
            TS_TEXTCHANGE change{};
            return store->InsertTextAtSelection(0u, L"xyx", 3u, &first, &last, &change);
        };
        HRESULT session = E_FAIL;
        Hr(store->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &session), "override fixture: the TSF lock is requested");
        sink.onLock = nullptr;
        // The store reports the filtered result as a failed write (the field no longer shows what was staged), as it did
        // before shared text; what matters is that the override decided the text.
        Check(field->imports >= 1u && field->GetText() == L"aby", "a TextField import override filters text typed through TSF");
        Hr(store->UnadviseSink(&sink), "override fixture: the sink is unadvised");
    }
    {
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> pool;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "override fixture: graphics");
        Hr(view.Attach(pool, StoppedTestClock()), "override fixture: attach");
        auto panel        = std::make_unique<DxUi::Panel>();
        auto* const field = panel->AddChild<FilteringField>(L"ab");
        field->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 32.0f));
        view.Controls().SetRoot(std::move(panel));
        Hr(view.Prepare(320, 80), "override fixture: prepare");
        view.Controls().SetFocusControl(field);
        DxUi::EmbeddedTextInputSnapshot snapshot;
        Check(view.ReadTextInput(snapshot) == S_OK, "override fixture: the embedded text service reads the field");
        DxUi::NativeTextInputState edit = snapshot.state;
        edit.text                       = L"abxyx";
        edit.caretIndex                 = 5u;
        Hr(view.ApplyTextInput(snapshot.revision, edit, DxUi::EmbeddedTextInputAction::Commit), "override fixture: the embedded commit applies");
        Check(field->imports >= 1u && field->GetText() == L"aby", "a TextField import override filters text committed through the embedded service");
    }
}

// An editable ComboBox whose composition ends without its preview being restored (focus loss) takes the shown text as
// committed: its selection and popup follow that text. A store teardown restores the preview and leaves both consistent.
inline void TestComboBoxSettlesAPreviewLeftByTeardown()
{
    const std::wstring_view first = kSyllables.substr(0u, 1u);
    const auto makeCombo          = [&](HeadlessHost& fixture)
    {
        auto* const combo = fixture.root->AddChild<DxUi::ComboBox>();
        combo->SetEditable(true);
        std::vector<DxUi::ComboBox::Item> items;
        items.push_back({L"apple", L"apple"});
        items.push_back({std::wstring(first), std::wstring(first)});
        items.push_back({std::wstring(kSyllables), std::wstring(kSyllables)});
        items.push_back({L"banana", L"banana"});
        combo->SetItems(std::move(items));
        combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 32.0f));
        fixture.host.SetFocusControl(combo, false);
        return combo;
    };
    const auto popupHeight = [](DxUi::ComboBox& combo)
    {
        const D2D1_RECT_F bounds = combo.DebugGetPopupBounds();
        return bounds.bottom - bounds.top;
    };
    const auto preview = [&](ITextStoreACP& store, Sink& sink, Composition& composition, ITfContextOwnerCompositionSink& owner)
    {
        static_cast<void>(CountLock(store,
                                    sink,
                                    TS_LF_READWRITE,
                                    [&]() noexcept -> HRESULT
        {
            LONG start = 0, end = 0;
            TS_TEXTCHANGE change{};
            HRESULT hr               = store.InsertTextAtSelection(0u, first.data(), 1u, &start, &end, &change);
            composition.range.start  = 0;
            composition.range.length = 1;
            BOOL accepted            = FALSE;
            if (SUCCEEDED(hr))
                hr = owner.OnStartComposition(&composition, &accepted);
            return SUCCEEDED(hr) && ! accepted ? E_FAIL : hr;
        }));
    };

    float allItemsHeight = 0.0f;
    {
        HeadlessHost fixture;
        auto* const combo = makeCombo(fixture);
        Check(combo->OnKeyDown(fixture.host, VK_F4, 0u), "settle fixture: F4 opens the popup");
        allItemsHeight = popupHeight(*combo);
        wil::com_ptr_nothrow<ITextStoreACP> store;
        store.attach(DxUi::CreateNativeTextInputTextStore(fixture.host, *combo, false));
        wil::com_ptr_nothrow<ITfContextOwnerCompositionSink> owner;
        Hr(store.query_to(owner.put()), "settle fixture: owner sink");
        Sink sink;
        Hr(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_ALL_SINKS), "settle fixture: sink");
        Composition composition;
        preview(*store, sink, composition, *owner);
        Check(combo->GetText() == first && ! combo->GetSelectedIndex(), "settle fixture: the preview shows without selecting its item");
        // Focus leaves mid-composition; nothing restores the preview.
        auto* const other = fixture.root->AddChild<DxUi::TextField>(L"other");
        fixture.host.SetFocusControl(other, false);
        Check(combo->GetText() == first && combo->GetSelectedIndex() == std::optional<size_t>(1u),
              "focus loss mid-composition makes the shown preview the ComboBox's committed text");
        fixture.host.SetFocusControl(combo, false);
        Check(combo->OnKeyDown(fixture.host, VK_F4, 0u) && combo->DebugIsPopupOpen() && popupHeight(*combo) < allItemsHeight,
              "the reopened popup filters by the text the ComboBox shows");
        Hr(store->UnadviseSink(&sink), "settle fixture: unadvise");
    }
    {
        HeadlessHost fixture;
        auto* const combo = makeCombo(fixture);
        wil::com_ptr_nothrow<ITextStoreACP> store;
        store.attach(DxUi::CreateNativeTextInputTextStore(fixture.host, *combo, false));
        wil::com_ptr_nothrow<ITfContextOwnerCompositionSink> owner;
        Hr(store.query_to(owner.put()), "teardown fixture: owner sink");
        Sink sink;
        Hr(store->AdviseSink(__uuidof(ITextStoreACPSink), &sink, TS_AS_ALL_SINKS), "teardown fixture: sink");
        Composition composition;
        preview(*store, sink, composition, *owner);
        DxUi::DisconnectNativeTextInputTextStore(store.get());
        Check(combo->GetText().empty() && ! combo->GetSelectedIndex(), "a store teardown mid-composition restores the committed text");
        Check(combo->OnKeyDown(fixture.host, VK_F4, 0u) && popupHeight(*combo) == allItemsHeight, "after the teardown the popup filters by the shown text");
    }
}
// Runs every scenario. With a receipt path, the receipt is written before the gate is checked, so a failing run still leaves
// its counts for comparison.
inline void Run(const wchar_t* receiptPath)
{
    const std::wstring document     = MakeDocument(kDocumentUnits);
    const ScenarioResult textField  = RunTextField(document);
    const ScenarioResult richEditor = RunRichTextEditor(document);
    const ScenarioResult comboBox   = RunComboBox();
    const ScenarioResult sharing    = RunTextField<SharingField>(document);
    std::cout << "IME copies: TextField " << textField.MaximumDocumentAllocationsPerLock() << " document allocations per lock at most, "
              << textField.DocumentAllocationsPerSyllable() << " per syllable; RichTextEditor " << richEditor.MaximumDocumentAllocationsPerLock() << ", "
              << richEditor.DocumentAllocationsPerSyllable() << "; ComboBox popup rebuilds before commit " << comboBox.popupRebuildsBeforeCommit
              << ", at commit " << comboBox.popupRebuildsAtCommit << '\n';
    if (receiptPath)
    {
        std::ofstream output{std::filesystem::path(receiptPath)};
        Check(bool(output), "IME copies receipt file");
        output << "{\"workloadOwner\":\"DxUi\",\"plan\":\"W8.5\",\"fixture\":\"dxui-ime-ten-syllables-v1\",\"configuration\":\"" << ConfigurationName()
               << "\",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"compiler\":" << _MSC_FULL_VER << ",\"documentUnits\":" << kDocumentUnits
               << ",\"documentSizedAllocationMinimumBytes\":" << kDocumentBytes << ",\"syllables\":" << kSyllables.size() << ",\"scenarios\":[";
        WriteScenario(output, "TextField", textField);
        output << ',';
        WriteScenario(output, "RichTextEditor", richEditor);
        output << ',';
        WriteScenario(output, "ComboBox", comboBox);
        output << "]}\n";
        output.close();
        Check(bool(output), "IME copies receipt written");
    }
    Check(textField.MaximumDocumentAllocationsPerLock() <= kMaximumDocumentAllocationsPerLock,
          "IME copies: no TSF lock in a TextField makes more than one document-sized allocation");
    Check(sharing.MaximumDocumentAllocationsPerLock() <= kMaximumDocumentAllocationsPerLock,
          "IME copies: a derived TextField that opts in to sharing keeps the one-allocation lock");
    // W7.4 closes the RichTextEditor part: its text input reads source revisions instead of flattening them.
    Check(richEditor.MaximumDocumentAllocationsPerLock() <= kMaximumDocumentAllocationsPerLock,
          "IME copies: no TSF lock in a RichTextEditor makes more than one document-sized allocation");
    Check(comboBox.popupRebuildsBeforeCommit == 0u, "IME copies: an editable ComboBox rebuilds no popup items before the composition commits");
    Check(comboBox.popupRebuildsAtCommit >= 1u, "IME copies: the commit filters the popup by the committed text");
}
} // namespace ImeCopies
