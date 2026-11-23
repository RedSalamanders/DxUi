#pragma once
// UI Automation text provider (W8.4 of the performance and motion plan). A text range reads its characters through a view
// pinned by the snapshot it resolved, never a copy; ExpandToEnclosingUnit(Word) finds its unit locally instead of scanning
// from offset 0; a native caret event reads the caret alone. The tests:
// - word and character expansion equal the algorithms they replaced, kept here as oracles, at every UTF-16 offset of seeded
//   texts (CJK, emoji with ZWJ, combining marks, surrogate pairs, lone halves, punctuation, path and whitespace runs) and of
//   the empty and one-unit documents, and on every string of up to five units over a mixed alphabet;
// - a word expansion costs its enclosing unit, never the offset, also for one 64 Ki-unit word;
// - the FX6 word walk (one client) makes no document-sized allocation and visits at most twice the document's units;
// - caret moves in a native TextField and RichTextEditor with no client make no document-sized allocation (nor flatten the
//   rich document), though each raises the event;
// - a range keeps reading the snapshot it resolved after the owner thread published two newer ones and dropped it;
// - composition and conversion-target ranges read their own text copy, shared with clones, never a snapshot, and stay safe
//   after their view detaches.
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
    // The claim is about this host: another application may change the desktop foreground while the test runs.
    Check(GetForegroundWindow() != window.get(), "the native text-input host never takes the foreground");
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
        MoveEndpointWord,
    };
    size_t current = 0u;
    for (const Call call : {Call::GetText, Call::ExpandWord, Call::MoveWord, Call::MoveEndpointWord})
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
                case Call::MoveEndpointWord: result = range->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Word, 1, &moved); break;
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
        else if (call == Call::MoveWord || call == Call::MoveEndpointWord)
            Check(moved == 1, "Move(Word) and MoveEndpointByUnit(Word) step over the pinned characters");
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
    std::cout << "W8.4 a range read its pinned snapshot after the owner published two newer ones, for GetText, ExpandToEnclosingUnit, Move and "
                 "MoveEndpointByUnit\n";
}

// Every string of up to five units over an alphabet of word, CJK, whitespace, separator, punctuation, line-break,
// combining and lone-surrogate units, at every offset: word and character expansion equal the scans they replaced.
inline void TestEnclosingSpansEqualTheReplacedScansOnEveryShortString()
{
    constexpr std::array<wchar_t, 13u> kAlphabet{L'a',
                                                 static_cast<wchar_t>(0x4e2du),
                                                 L' ',
                                                 L'\t',
                                                 L'/',
                                                 L'\\',
                                                 L'.',
                                                 L':',
                                                 L'\r',
                                                 L'\n',
                                                 static_cast<wchar_t>(0x0301u),
                                                 static_cast<wchar_t>(0xD800u),
                                                 static_cast<wchar_t>(0xDC00u)};
    constexpr size_t kMaximumLength = 5u;
    std::array<size_t, kMaximumLength> digits{};
    std::wstring text;
    text.reserve(kMaximumLength);
    size_t strings = 0u, positions = 0u;
    for (size_t length = 0u; length <= kMaximumLength; ++length)
    {
        digits.fill(0u);
        for (;;)
        {
            text.assign(length, L'a');
            for (size_t index = 0u; index < length; ++index)
                text[index] = kAlphabet[digits[index]];
            for (size_t position = 0u; position <= length + 1u; ++position)
            {
                const auto word     = DxUi::GetEnclosingAccessibilityTextUnitSpan(text, position, TextUnit_Word);
                const auto expected = OracleWordSpan(text, position);
                const auto letter   = DxUi::GetEnclosingAccessibilityTextUnitSpan(text, position, TextUnit_Character);
                const auto oracle   = OracleCharacterSpan(text, position);
                if (word.start != expected.start || word.end != expected.end || letter.start != oracle.start || letter.end != oracle.end)
                {
                    std::cerr << "    [W8.4] string";
                    for (const wchar_t unit : text)
                        std::cerr << ' ' << std::hex << static_cast<unsigned>(unit) << std::dec;
                    std::cerr << " position " << position << ": word " << word.start << '-' << word.end << " expected " << expected.start << '-' << expected.end
                              << "; character " << letter.start << '-' << letter.end << " expected " << oracle.start << '-' << oracle.end << '\n';
                    Check(false, "word and character expansion equal the replaced scans on every short string");
                }
                ++positions;
            }
            ++strings;
            size_t digit = 0u;
            while (digit < length && ++digits[digit] == kAlphabet.size())
                digits[digit++] = 0u;
            if (digit == length)
                break;
        }
    }
    std::cout << "W8.4 enclosing spans equal the replaced scans on all " << strings << " strings of up to " << kMaximumLength << " units, at " << positions
              << " offsets\n";
}

// The cost is the enclosing unit, never the offset: in one 64 Ki-unit run of a single class, an expansion anywhere visits
// at most about twice the run (back to its start, then forward to its end).
inline void TestWordExpansionCostIsTheEnclosingUnit()
{
    const std::wstring document(65'536u, L'a');
    uint64_t maximum = 0u;
    for (const size_t position : {size_t{0u}, size_t{1u}, document.size() / 2u, document.size() - 1u, document.size()})
    {
        const DxUi::Detail::WorkCounterScope scope;
        const auto span = DxUi::GetEnclosingAccessibilityTextUnitSpan(document, position, TextUnit_Word);
        Check(span.start == 0u && span.end == document.size(), "a single-class document is one word unit");
        maximum = (std::max)(maximum, scope.Delta().accessibility.wordUnitsVisited);
    }
    std::cout << "W8.4 word expansion in one 64 Ki-unit word visits at most " << maximum << " units\n";
    Check(maximum <= 2u * document.size(), "a word expansion visits at most twice its enclosing unit");
}

// Rich-editor caret moves on a native host with no client: the caret event reads the caret and the length of the editor's
// source revision, so it neither copies nor materializes the document.
inline void TestNativeRichEditorCaretEventsCopyNoDocument()
{
    using namespace DxUi;
    const std::optional<bool> outerListeners = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(false);
    const auto restoreListeners = wil::scope_exit([&] { DebugSetAccessibilitySystemListenersForTest(outerListeners); });
    ControlHost host;
    const wil::unique_hwnd window = CreateTextProviderWindow(host);
    const auto detach             = wil::scope_exit([&] { host.Detach(); });
    static_cast<void>(ShowWindow(window.get(), SW_SHOWNOACTIVATE));
    host.SetTextInputBackend(TextInputBackend::Native);
    // RichTextEditor documents stop at 64 Ki units: seven-letter words and spaces, no line breaks.
    std::wstring document(65'536u - 64u, L'a');
    for (size_t index = 0u; index < document.size(); ++index)
        document[index] = (index % 8u) == 7u ? L' ' : static_cast<wchar_t>(L'a' + (index % 26u));
    auto root    = std::make_unique<Panel>();
    auto* editor = root->AddChild<RichTextEditor>(document);
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 460.0f, 220.0f));
    host.SetRoot(std::move(root));
    host.SetFocusControl(editor, false);
    Check(host.HasActiveTextInput(), "the editor's native text input is active without focus");

    constexpr size_t kMoves     = 16u;
    const uint64_t eventsBefore = host.DebugGetNativeTextInputEventCounters().uiaActiveTextPositionChangedCount;
    size_t copies               = 0u;
    uint64_t flattens           = 0u;
    for (size_t move = 0u; move < kMoves; ++move)
    {
        const size_t caret = 1'000u + move * 2'000u;
        const Detail::WorkCounterScope scope;
        largeAllocationMinimumBytes = document.size() * sizeof(wchar_t);
        largeAllocations            = 0u;
        countAllocations            = true;
        editor->SetSelection(RichText::Selection{caret, caret});
        countAllocations            = false;
        largeAllocationMinimumBytes = std::numeric_limits<size_t>::max();
        copies += largeAllocations;
        flattens += scope.Delta().rich.documentFlattens;
    }
    const uint64_t events = host.DebugGetNativeTextInputEventCounters().uiaActiveTextPositionChangedCount - eventsBefore;
    std::cout << "W8.4 native rich-editor caret moves with no client: " << kMoves << " moves, " << events << " caret events, " << copies
              << " document-sized allocations, " << flattens << " document flattens\n";
    Check(events == kMoves, "each rich-editor caret move raises the caret event");
    Check(copies == 0u && flattens == 0u, "no rich-editor caret event copies or flattens the document while no client listens");
    Check(GetForegroundWindow() != window.get(), "the native rich-editor host never takes the foreground");
}

namespace Retention
{
inline std::weak_ptr<const void> g_resolved;
inline void Observe(const std::shared_ptr<const void>& owner, std::wstring_view) noexcept
{
    g_resolved = owner;
}
} // namespace Retention

// A composition or conversion-target range owns one copy of its field's text, shared with its clones, never the snapshot it
// was read from: a client that keeps every composition range it asks for (a managed client holds them until its garbage
// collection) keeps one field's text per range. Proved on an embedded view, whose IME preview needs no focus; a native
// window host's text store refuses a window without keyboard focus, so the same scenario there needs the desktop. The range
// reads its own copy (the owner it resolves is not the snapshot's) and a clone shares that copy. Then the view detaches
// under the held ranges and their clones, and they are used again.
inline void TestCompositionRangesOwnTheirTextNotTheSnapshot()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    Hr(gpu.Create(), "composition ranges WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "composition ranges graphics device");
    EmbeddedHost view;
    Hr(view.Attach(std::move(graphics), StoppedTestClock()), "attach the composition view");
    // Long enough that every syllable's composition span lies inside the published text.
    const std::wstring base = L"composition base for eight syllables";
    auto root               = std::make_unique<Panel>();
    for (size_t index = 0u; index < 400u; ++index)
        root->AddChild<Label>(L"Unrelated label " + std::to_wstring(index))->SetBounds(D2D1::RectF(0.0f, 40.0f, 200.0f, 60.0f));
    auto* field = root->AddChild<TextField>(base);
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 460.0f, 32.0f));
    view.Controls().SetRoot(std::move(root));
    view.Controls().SetFocusControl(field);
    Hr(view.Prepare(480u, 240u), "prepare the composition view");
    auto site = std::make_shared<PerformanceFixtures::Fx6::Site>();
    const EmbeddedAccessibilityPlacement placement{{0.0, 0.0, 480.0, 240.0}, false};
    Hr(view.AttachAccessibility(site, 0x5738340000000002ull, placement), "attach the composition view's accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Hr(view.GetAccessibilityProvider(provider.put()), "composition view root provider");
    site->root = provider.get();

    wil::com_ptr_nothrow<ITextEditProvider> edits;
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    {
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        Hr(provider.query_to(fragment.put()), "composition root fragment");
        wil::com_ptr_nothrow<IRawElementProviderFragment> child;
        Hr(fragment->Navigate(NavigateDirection_LastChild, child.put()), "composition field element");
        Check(bool(child), "the composition view exposes its field");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(child.query_to(simple.put()), "composition field simple provider");
        wil::com_ptr_nothrow<IUnknown> pattern;
        Hr(simple->GetPatternProvider(UIA_TextEditPatternId, pattern.put()), "composition field TextEdit pattern");
        Check(bool(pattern), "the field exposes the TextEdit pattern");
        Hr(pattern.query_to(edits.put()), "composition field TextEdit interface");
        wil::com_ptr_nothrow<ITextProvider> text;
        Hr(pattern.query_to(text.put()), "composition field Text interface");
        Hr(text->get_DocumentRange(document.put()), "composition field document range");
    }
    // The owner a range's read resolves, through the test hook.
    const auto resolvedOwner = [](ITextRangeProvider& range, std::wstring& read)
    {
        Detail::textRangeResolvedObserver = &Retention::Observe;
        wil::unique_bstr text;
        const HRESULT hr                  = range.GetText(-1, text.put());
        Detail::textRangeResolvedObserver = nullptr;
        Hr(hr, "read a text range");
        read.assign(text ? text.get() : L"", text ? SysStringLen(text.get()) : 0u);
        return Retention::g_resolved.lock();
    };

    constexpr size_t kSyllables = 8u;
    std::vector<wil::com_ptr_nothrow<ITextRangeProvider>> held;
    for (size_t count = 1u; count <= kSyllables; ++count)
    {
        // Syllable `count` of a composition previewed at offset 11, as an application's text service applies it.
        EmbeddedTextInputSnapshot input;
        Hr(view.ReadTextInput(input), "read the composition view's text input");
        NativeTextInputState next = input.state;
        next.text                 = base;
        next.text.insert(11u, ImeCopies::kSyllables.substr(0u, count));
        next.caretIndex = 11u + count;
        next.selectionAnchorIndex.reset();
        next.compositionStartIndex      = 11u;
        next.compositionEndIndex        = 11u + count;
        next.conversionTargetStartIndex = 11u;
        next.conversionTargetEndIndex   = 11u + count;
        Hr(view.ApplyTextInput(input.revision, next, EmbeddedTextInputAction::Preview), "preview the composition");
        // The application publishes once its preparation is coherent again (S_FALSE until then).
        HRESULT published = S_FALSE;
        for (size_t turn = 0u; turn < 16u && published == S_FALSE; ++turn)
        {
            Hr(view.Prepare(480u, 240u), "prepare the previewed composition");
            published = view.UpdateAccessibility(placement);
        }
        Check(published == S_OK, "the previewed composition is published");

        std::wstring documentText;
        const std::shared_ptr<const void> snapshot = resolvedOwner(*document.get(), documentText);
        if (! snapshot || documentText.size() < 11u + count)
            std::cerr << "    [W8.4] syllable " << count << ": owner " << (snapshot ? "set" : "null") << ", document range read " << documentText.size()
                      << " units\n";
        Check(snapshot != nullptr && documentText.size() >= 11u + count, "the document range reads the published snapshot");
        wil::com_ptr_nothrow<ITextRangeProvider> composition, conversion, clone;
        Hr(edits->GetActiveComposition(composition.put()), "active composition range");
        Hr(edits->GetConversionTarget(conversion.put()), "conversion-target range");
        Check(composition && conversion, "the composition has composition and conversion-target ranges");
        Hr(composition->Clone(clone.put()), "clone the composition range");
        std::wstring compositionText, cloneText, conversionText;
        const std::shared_ptr<const void> own       = resolvedOwner(*composition.get(), compositionText);
        const std::shared_ptr<const void> cloned    = resolvedOwner(*clone.get(), cloneText);
        const std::shared_ptr<const void> converted = resolvedOwner(*conversion.get(), conversionText);
        // The ranges read the composition's span of the snapshot they were created from.
        Check(compositionText == documentText.substr(11u, count) && cloneText == compositionText && conversionText == compositionText,
              "composition, clone and conversion-target ranges read the composition span of their snapshot");
        Check(own && own.get() != snapshot.get() && converted && converted.get() != snapshot.get(),
              "a composition or conversion-target range reads its own text copy, never the snapshot");
        Check(cloned.get() == own.get(), "a clone shares its range's text copy");
        held.push_back(std::move(composition));
        held.push_back(std::move(clone));
        held.push_back(std::move(conversion));
    }
    std::cout << "W8.4 composition ranges: " << held.size() << " held ranges, each reading its own text copy, never a snapshot" << std::endl;

    // The view goes away under the held ranges and their clones.
    document.reset();
    edits.reset();
    site->root = nullptr;
    provider.reset();
    view.DisconnectAccessibility();
    view.Detach();
    for (const auto& range : held)
    {
        wil::unique_bstr text;
        const HRESULT read = range->GetText(-1, text.put());
        Check(SUCCEEDED(read) || read == UIA_E_ELEMENTNOTAVAILABLE, "a held range reads or reports its element gone after the view detached");
        wil::com_ptr_nothrow<ITextRangeProvider> clone;
        const HRESULT cloned = range->Clone(clone.put());
        Check(SUCCEEDED(cloned) || cloned == UIA_E_ELEMENTNOTAVAILABLE, "a held range clones or reports its element gone");
        int moved             = 0;
        const HRESULT stepped = range->Move(TextUnit_Character, 1, &moved);
        Check(SUCCEEDED(stepped) || stepped == UIA_E_ELEMENTNOTAVAILABLE, "a held range moves or reports its element gone");
        if (clone)
        {
            const HRESULT again = clone->Move(TextUnit_Word, 1, &moved);
            Check(SUCCEEDED(again) || again == UIA_E_ELEMENTNOTAVAILABLE, "a clone made after the view detached moves or reports its element gone");
        }
    }
    held.clear();
}

inline void Run()
{
    TestEnclosingSpansEqualTheReplacedScans();
    TestEnclosingSpansEqualTheReplacedScansOnEveryShortString();
    TestWordExpansionCostIsTheEnclosingUnit();
    TestFx6WordWalkGate();
    TestNativeCaretEventsCopyNoDocument();
    TestNativeRichEditorCaretEventsCopyNoDocument();
    TestRangeReadsThePinnedSnapshotAfterTheOwnerReplacesIt();
    TestCompositionRangesOwnTheirTextNotTheSnapshot();
}
} // namespace AccessibilityTextProvider
