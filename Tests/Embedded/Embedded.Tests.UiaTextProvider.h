#pragma once
// UI Automation text provider (W8.4 of the performance and motion plan). A text range reads its characters through a view
// pinned by the snapshot it resolved, never a copy; ExpandToEnclosingUnit(Word) finds its unit locally instead of scanning
// from offset 0; a native caret event reads the caret alone. The tests:
// - word and character expansion equal the algorithms they replaced, kept here as oracles, at every UTF-16 offset of seeded
//   texts (CJK, emoji with ZWJ, combining marks, surrogate pairs, lone halves, punctuation, path and whitespace runs) and of
//   the empty and one-unit documents;
// - the FX6 word walk (one client) makes no document-sized allocation and visits at most twice the document's units;
// - caret moves in a native TextField with no client make no document-sized allocation, though each raises the event;
// - a range keeps reading the snapshot it resolved after the owner thread published two newer ones and dropped it.
// Included by Embedded.Tests.Embedded.cpp after the allocation instrument and the performance fixtures.
#include "Controls/DxUi.Internal.h"
#include "Support/WorkCounters.h"

#include <DxUi/AccessibilityTextUnits.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>

namespace AccessibilityTextProvider
{
// ---- The algorithms W8.4 replaced, kept as oracles ----------------------------------------------------------------------

[[nodiscard]] inline bool OracleWhitespace(wchar_t value) noexcept
{
    WORD type = 0u;
    return GetStringTypeW(CT_CTYPE1, &value, 1, &type) && (type & C1_SPACE) != 0u;
}

[[nodiscard]] inline size_t OracleTrimTrailingWhitespace(std::wstring_view text, size_t start, size_t end) noexcept
{
    while (end > start)
    {
        const size_t previous = DxUi::StepToPreviousCodePoint(text, end);
        if (! OracleWhitespace(text[previous]))
            break;
        end = previous;
    }
    return end;
}

// Snap to the nearest boundary, step back when the snap went forward, then step forward.
[[nodiscard]] inline DxUi::AccessibilityTextUnitSpan OracleCharacterSpan(std::wstring_view text, size_t position) noexcept
{
    if (text.empty())
        return {};
    size_t start = (std::min)(position, text.size());
    if (start == text.size())
        start = DxUi::StepToPreviousTextElement(text, start);
    else
    {
        const size_t snapped = DxUi::SnapCaretIndexToTextElementBoundary(text, start);
        start                = snapped > start ? DxUi::StepToPreviousTextElement(text, snapped) : snapped;
    }
    return {start, DxUi::StepToNextTextElement(text, start)};
}

// Every word unit from offset 0 until the one holding the position.
[[nodiscard]] inline DxUi::AccessibilityTextUnitSpan OracleWordSpan(std::wstring_view text, size_t position) noexcept
{
    if (text.empty())
        return {};
    const size_t clamped = (std::min)(position, text.size());
    size_t unitStart     = 0u;
    while (unitStart < text.size())
    {
        const size_t next = DxUi::FindNextWordBoundary(text, unitStart);
        if (next <= unitStart)
            return OracleCharacterSpan(text, clamped);
        if (clamped < next || clamped == unitStart)
            return {unitStart, (std::max)(unitStart, OracleTrimTrailingWhitespace(text, unitStart, next))};
        unitStart = next;
    }
    const size_t last = DxUi::FindPreviousWordBoundary(text, text.size());
    return {last, OracleTrimTrailingWhitespace(text, last, text.size())};
}

// ---- Seeded texts ---------------------------------------------------------------------------------------------------------

[[nodiscard]] inline std::vector<std::wstring> Tokens()
{
    return {
        L"word",
        L"Ab9",
        L"_x_",
        L"42",
        L"\u4e2d\u6587",                               // CJK
        L"\u65e5\u672c\u8a9e",                         // CJK
        L"\U0001F468\u200D\U0001F469\u200D\U0001F467", // emoji joined by ZWJ
        L"\U0001F44D\U0001F3FD",                       // emoji with a skin-tone modifier
        L"\U0001F1EB\U0001F1F7",                       // regional indicator pair
        L"e\u0301",                                    // combining acute
        L"a\u0300\u0316",                              // two combining marks
        L"\u0915\u094D\u0937",                         // Devanagari conjunct
        L"\U00020000\U00020001",                       // supplementary letters (surrogate pairs)
        L"\U0001D400\U0001D7CE",                       // supplementary letter and digit
        L"\u0644\u0627",                               // Arabic
        L"...",
        L"!?",
        L"--",
        L"\u2014",
        L"C:\\",
        L"/usr/",
        L"\\\\",
        L"//",
        L"x/y",
        L"a.b",
        L" ",
        L"   ",
        L"\t",
        L"\r\n",
        L"\n",
        L"\u3000",                                                                                                 // ideographic space
        L"\u00A0",                                                                                                 // no-break space
        std::wstring(1u, static_cast<wchar_t>(0xD800u)),                                                           // lone lead surrogate
        std::wstring(1u, static_cast<wchar_t>(0xDC00u)),                                                           // lone trail surrogate
        std::wstring{static_cast<wchar_t>(0xD83Du), static_cast<wchar_t>(0xD83Du), static_cast<wchar_t>(0xDE00u)}, // lead, then a pair
    };
}

[[nodiscard]] inline std::vector<std::wstring> SeededTexts()
{
    const std::vector<std::wstring> tokens = Tokens();
    std::vector<std::wstring> texts;
    texts.emplace_back(); // the empty document
    for (const wchar_t unit : std::wstring_view(L"a /.\n\u4e2d_9\t"))
        texts.emplace_back(1u, unit); // one-unit documents
    texts.emplace_back(1u, static_cast<wchar_t>(0xD800u));
    texts.emplace_back(1u, static_cast<wchar_t>(0xDC00u));
    std::mt19937 random(0x57384u); // fixed seed: the same texts every run
    for (size_t index = 0u; index < 240u; ++index)
    {
        std::wstring text;
        const size_t count = 1u + random() % 48u;
        for (size_t token = 0u; token < count; ++token)
            text += tokens[random() % tokens.size()];
        texts.push_back(std::move(text));
    }
    return texts;
}

// ---- Tests ----------------------------------------------------------------------------------------------------------------

inline void TestEnclosingSpansEqualTheReplacedScans()
{
    size_t positions = 0u;
    for (const std::wstring& text : SeededTexts())
    {
        for (size_t position = 0u; position <= text.size() + 1u; ++position)
        {
            const auto word     = DxUi::GetEnclosingAccessibilityTextUnitSpan(text, position, TextUnit_Word);
            const auto expected = OracleWordSpan(text, position);
            const auto letter   = DxUi::GetEnclosingAccessibilityTextUnitSpan(text, position, TextUnit_Character);
            const auto oracle   = OracleCharacterSpan(text, position);
            if (word.start != expected.start || word.end != expected.end || letter.start != oracle.start || letter.end != oracle.end)
            {
                std::cerr << "    [W8.4] text length " << text.size() << " position " << position << ": word " << word.start << '-' << word.end << " expected "
                          << expected.start << '-' << expected.end << "; character " << letter.start << '-' << letter.end << " expected " << oracle.start << '-'
                          << oracle.end << '\n';
                Check(false, "word and character expansion equal the scans they replaced at every offset");
            }
            ++positions;
        }
    }
    std::cout << "W8.4 enclosing spans equal the replaced scans at " << positions << " offsets\n";

    // Local work: the far end of the FX6 document costs what its start does.
    const std::wstring document = PerformanceFixtures::Fx6::MakeDocument();
    uint64_t maximum            = 0u;
    for (const size_t position : {size_t{0u}, size_t{3u}, document.size() / 2u, document.size() - 9u, document.size() - 1u, document.size()})
    {
        const DxUi::Detail::WorkCounterScope scope;
        static_cast<void>(DxUi::GetEnclosingAccessibilityTextUnitSpan(document, position, TextUnit_Word));
        maximum = (std::max)(maximum, scope.Delta().accessibility.wordUnitsVisited);
    }
    std::cout << "W8.4 word expansion in the 64 Ki-unit FX6 document visits at most " << maximum << " units\n";
    Check(maximum <= 32u, "word expansion visits the unit and the whitespace before it, at any offset");
}

// The FX6 word walk: Move by 1,000 characters, Move by one word, ExpandToEnclosingUnit(Word), GetText, then the frame.
inline void TestFx6WordWalkGate()
{
    using namespace PerformanceFixtures;
    const RunRecord run        = RunEmbedded(Fx6::kDefinition, Fx6::kVariants[0]);
    const PhaseRecord& walk    = run.Phase("wordwalk-one-client");
    const CounterSummary units = Summarize(walk, "accessibility.wordUnitsVisited");
    // Step 1 publishes the document that step 0 showed: that snapshot build copies the document into its record once, which
    // belongs to the build (plan item W8.3), not to a provider call. Any other document-sized allocation would be a call's.
    uint64_t copies = 0u, builds = 0u, callCopies = 0u;
    for (size_t step = 1u; step < walk.steps.size(); ++step) // step 0 shows the document
    {
        const StepRecord& record = walk.steps[step];
        copies += record.cppLargeAllocations;
        builds += record.work.accessibility.snapshotBuilds;
        callCopies +=
            record.cppLargeAllocations > record.work.accessibility.snapshotBuilds ? record.cppLargeAllocations - record.work.accessibility.snapshotBuilds : 0u;
    }
    std::cout << "W8.4 FX6 word walk: " << walk.steps.size() - 1u << " word steps, units visited " << units.total << " (at most " << units.maximum
              << " per step), document-sized allocations " << copies << " with " << builds << " snapshot builds, by provider calls " << callCopies << std::endl;
    if (callCopies != 0u)
        for (size_t step = 0u; step < walk.steps.size(); ++step)
            std::cerr << "    [W8.4] word step " << step << ": document-sized allocations " << walk.steps[step].cppLargeAllocations << ", snapshot builds "
                      << walk.steps[step].work.accessibility.snapshotBuilds << '\n';
    Check(run.cppAllocationsCounted && Fx6::kDefinition.largeAllocationBytes == Fx6::kDocumentUnits * sizeof(wchar_t),
          "the FX6 runner counts allocations of one document's bytes");
    Check(callCopies == 0u && builds <= 1u, "FX6 word walk: no provider call copies the document; only publishing the shown document builds");
    Check(units.total <= 2u * Fx6::kDocumentUnits, "FX6 word walk: units visited stay within twice the document length");
}

inline LRESULT CALLBACK TextProviderWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* host = reinterpret_cast<DxUi::ControlHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        host = static_cast<DxUi::ControlHost*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
    }
    if (host)
    {
        bool handled      = false;
        const auto result = host->HandleMessage(hwnd, message, wParam, lParam, handled);
        if (handled)
            return result;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// A native host on a window that never activates, placed off every monitor.
[[nodiscard]] inline wil::unique_hwnd CreateTextProviderWindow(DxUi::ControlHost& host)
{
    const auto instance        = GetModuleHandleW(nullptr);
    constexpr PCWSTR className = L"DxUi.Tests.AccessibilityTextProvider";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc   = TextProviderWindowProc;
    windowClass.hInstance     = instance;
    windowClass.lpszClassName = className;
    Check(RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "register the text provider window class");
    wil::unique_hwnd window(
        CreateWindowExW(WS_EX_NOACTIVATE, className, L"Text provider", WS_OVERLAPPED, -32000, -32000, 480, 240, nullptr, nullptr, instance, &host));
    Check(bool(window) && host.Attach(window.get()), "attach a native host to a window that never activates");
    return window;
}

// Caret moves in a native TextField with no client: each raises ActiveTextPositionChanged, none copies the document.
inline void TestNativeCaretEventsCopyNoDocument()
{
    using namespace DxUi;
    const std::optional<bool> outerListeners = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(false);
    const auto restoreListeners = wil::scope_exit([&] { DebugSetAccessibilitySystemListenersForTest(outerListeners); });
    ControlHost host;
    const wil::unique_hwnd window = CreateTextProviderWindow(host);
    const auto detach             = wil::scope_exit([&] { host.Detach(); });
    static_cast<void>(ShowWindow(window.get(), SW_SHOWNOACTIVATE)); // native text input needs a visible window, not focus
    const HWND foregroundBefore = GetForegroundWindow();
    host.SetTextInputBackend(TextInputBackend::Native);
    const std::wstring document = PerformanceFixtures::Fx6::MakeDocument();
    auto root                   = std::make_unique<Panel>();
    auto* field                 = root->AddChild<TextField>(document);
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 460.0f, 220.0f));
    host.SetRoot(std::move(root));
    host.SetFocusControl(field, false);
    Check(host.HasActiveTextInput(), "the field's native text input is active without focus");

    constexpr size_t kMoves     = 16u;
    const uint64_t eventsBefore = host.DebugGetNativeTextInputEventCounters().uiaActiveTextPositionChangedCount;
    size_t copies               = 0u;
    for (size_t move = 0u; move < kMoves; ++move)
    {
        const size_t caret          = 1'000u + move * 2'000u;
        largeAllocationMinimumBytes = document.size() * sizeof(wchar_t);
        largeAllocations            = 0u;
        countAllocations            = true;
        field->SetSelectionRange(caret, caret);
        countAllocations            = false;
        largeAllocationMinimumBytes = std::numeric_limits<size_t>::max();
        copies += largeAllocations;
    }
    const uint64_t events = host.DebugGetNativeTextInputEventCounters().uiaActiveTextPositionChangedCount - eventsBefore;
    std::cout << "W8.4 native caret moves with no client: " << kMoves << " moves, " << events << " caret events, " << copies << " document-sized allocations\n";
    Check(events == kMoves, "each caret move raises the caret event");
    Check(copies == 0u, "no caret event copies the document while no client listens");
    Check(GetForegroundWindow() == foregroundBefore, "the native text-input host never takes the foreground");
}

// Lifetime across threads. A UI Automation worker's range resolves its text; before it reads the view, the owner thread
// publishes two newer snapshots (so neither the published pointer nor the event baseline holds the old one) and the test
// confirms the range's own pin is the old snapshot's last owner. The range then reads through the view: under ASan a view
// that the call did not pin is a use after free; here the call returns the old characters.
namespace Lifetime
{
struct Gate
{
    Gate()                       = default;
    Gate(const Gate&)            = delete;
    Gate& operator=(const Gate&) = delete;
    Gate(Gate&&)                 = delete;
    Gate& operator=(Gate&&)      = delete;
    wil::unique_event_nothrow resolved;
    wil::unique_event_nothrow replaced;
    std::weak_ptr<const void> owner;
    std::wstring_view text;
    std::atomic<bool> armed{false};
};
inline Gate* g_gate = nullptr;

inline void Observe(const std::shared_ptr<const void>& owner, std::wstring_view text) noexcept
{
    Gate* const gate = g_gate;
    if (! gate || ! owner || ! gate->armed.exchange(false))
        return;
    gate->owner = owner;
    gate->text  = text;
    static_cast<void>(SetEvent(gate->resolved.get()));
    static_cast<void>(WaitForSingleObject(gate->replaced.get(), 20'000u));
}

template <typename Predicate> [[nodiscard]] bool PumpUntil(HANDLE event, Predicate&& done)
{
    const ULONGLONG deadline = GetTickCount64() + 20'000u;
    while (! done() && GetTickCount64() < deadline)
    {
        static_cast<void>(MsgWaitForMultipleObjects(event ? 1u : 0u, event ? &event : nullptr, FALSE, 20u, QS_ALLINPUT));
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return done();
}

[[nodiscard]] inline wil::com_ptr_nothrow<ITextRangeProvider> DocumentRange(IRawElementProviderFragmentRoot& root)
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    Hr(root.QueryInterface(IID_PPV_ARGS(fragment.put())), "text provider root fragment");
    wil::com_ptr_nothrow<IRawElementProviderFragment> child;
    Hr(fragment->Navigate(NavigateDirection_FirstChild, child.put()), "text provider first child");
    while (child)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(child.query_to(simple.put()), "text provider element");
        wil::com_ptr_nothrow<IUnknown> pattern;
        Hr(simple->GetPatternProvider(UIA_TextPatternId, pattern.put()), "text provider pattern");
        if (pattern)
        {
            wil::com_ptr_nothrow<ITextProvider> text;
            Hr(pattern.query_to(text.put()), "text provider interface");
            wil::com_ptr_nothrow<ITextRangeProvider> range;
            Hr(text->get_DocumentRange(range.put()), "text provider document range");
            return range;
        }
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        Hr(child->Navigate(NavigateDirection_NextSibling, next.put()), "text provider next sibling");
        child = std::move(next);
    }
    Check(false, "the text provider window exposes its field's text pattern");
    return {};
}
} // namespace Lifetime

inline void TestRangeReadsThePinnedSnapshotAfterTheOwnerReplacesIt()
{
    using namespace DxUi;
    ControlHost host;
    const wil::unique_hwnd window = CreateTextProviderWindow(host);
    const auto detach             = wil::scope_exit([&] { host.Detach(); });
    // Three texts of different lengths, so a stale length or a stale view would show.
    const std::array<std::wstring, 3u> texts{
        std::wstring(4'096u, L'a') + L" first", std::wstring(2'000u, L'b') + L" second", std::wstring(6'000u, L'c') + L" third"};
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(texts[0]);
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 460.0f, 220.0f));
    host.SetRoot(std::move(root));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    provider.attach(CreateWindowHostAccessibilityProvider(window.get())); // a client's request: mutations publish from now on
    Check(bool(provider), "the window publishes a snapshot behind its root");

    enum class Call
    {
        GetText,
        ExpandWord,
        MoveWord,
    };
    size_t current = 0u;
    for (const Call call : {Call::GetText, Call::ExpandWord, Call::MoveWord})
    {
        const wil::com_ptr_nothrow<ITextRangeProvider> range = Lifetime::DocumentRange(*provider);
        Lifetime::Gate gate;
        Hr(gate.resolved.create(wil::EventOptions::ManualReset), "resolution event");
        Hr(gate.replaced.create(wil::EventOptions::ManualReset), "replacement event");
        Lifetime::g_gate              = &gate;
        const auto clear              = wil::scope_exit([] { Lifetime::g_gate = nullptr; });
        const std::wstring pinnedText = texts[current];
        HRESULT result                = E_FAIL;
        std::wstring read;
        int moved = 0;
        gate.armed.store(true);
        std::jthread worker([&]
        {
            Detail::textRangeResolvedObserver = &Lifetime::Observe;
            switch (call)
            {
                case Call::GetText:
                {
                    wil::unique_bstr text;
                    result = range->GetText(-1, text.put());
                    if (SUCCEEDED(result) && text)
                        read.assign(text.get(), SysStringLen(text.get()));
                    break;
                }
                case Call::ExpandWord:
                    // Collapsed at the last word, the expansion walks back over the pinned characters.
                    result = range->MoveEndpointByRange(TextPatternRangeEndpoint_Start, range.get(), TextPatternRangeEndpoint_End);
                    if (SUCCEEDED(result))
                        result = range->ExpandToEnclosingUnit(TextUnit_Word);
                    break;
                case Call::MoveWord: result = range->Move(TextUnit_Word, 1, &moved); break;
            }
            Detail::textRangeResolvedObserver = nullptr;
        });
        Check(Lifetime::PumpUntil(gate.resolved.get(), [&] { return WaitForSingleObject(gate.resolved.get(), 0u) == WAIT_OBJECT_0; }),
              "the worker's range resolves its text");
        Check(gate.text.size() == pinnedText.size(), "the range resolved the published text");
        // Two newer snapshots: the first replaces the published pointer, the second the event baseline.
        const size_t pinnedIndex = current;
        const auto drained       = []
        {
            MSG message{};
            return PeekMessageW(&message, nullptr, 0u, 0u, PM_NOREMOVE) == FALSE;
        };
        for (size_t replacement = 0u; replacement < 2u; ++replacement)
        {
            current = (current + 1u) % texts.size();
            field->SetText(texts[current]);
            static_cast<void>(Lifetime::PumpUntil(nullptr, drained));
        }
        Check(Lifetime::PumpUntil(nullptr, [&] { return gate.owner.use_count() == 1; }),
              "the worker's pin is the last owner of the snapshot it is reading once two newer ones are published");
        static_cast<void>(SetEvent(gate.replaced.get()));
        Check(Lifetime::PumpUntil(nullptr, [&] { return gate.owner.expired(); }), "the old snapshot is freed once the worker's call returns");
        worker.join();
        Hr(result, "the range call succeeds across the replacement");
        if (call == Call::GetText)
            Check(read == pinnedText, "GetText returns the characters of the snapshot it pinned");
        else if (call == Call::MoveWord)
            Check(moved == 1, "Move(Word) steps over the pinned characters");
        else
        {
            // The expansion took its offsets from the pinned characters: with that text published again they hold its last word.
            current = pinnedIndex;
            field->SetText(texts[current]);
            const std::wstring lastWord = pinnedText.substr(pinnedText.rfind(L' ') + 1u);
            const auto readsLastWord    = [&]
            {
                wil::unique_bstr text;
                return SUCCEEDED(range->GetText(-1, text.put())) && text && std::wstring_view(text.get(), SysStringLen(text.get())) == lastWord;
            };
            Check(Lifetime::PumpUntil(nullptr, readsLastWord), "ExpandToEnclosingUnit(Word) found the last word of the pinned characters");
        }
    }
    std::cout << "W8.4 a range read its pinned snapshot after the owner published two newer ones, for GetText, ExpandToEnclosingUnit and Move\n";
}

inline void Run()
{
    TestEnclosingSpansEqualTheReplacedScans();
    TestFx6WordWalkGate();
    TestNativeCaretEventsCopyNoDocument();
    TestRangeReadsThePinnedSnapshotAfterTheOwnerReplacesIt();
}
} // namespace AccessibilityTextProvider
