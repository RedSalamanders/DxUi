#pragma once
// FX7 IME (plan W0.4): a 10-syllable composition and its commit in a 64 Ki-unit TextField and RichTextEditor, driven through
// the production TSF text store (CreateNativeTextInputTextStore) with the in-process doubles of Embedded.Tests.TsfDoubles.h,
// as the W8.5 IME copies scenario (Embedded.Tests.ImeCopies.h) drives it: nothing takes focus and no text service runs. One
// step is one TSF lock, followed by the runner's frame: for syllable k a read-write lock that rewrites the composition, then
// the read lock a candidate window makes; the twenty-first step is the commit lock. The text is the W8.5 scenario's
// ASCII document. With Definition::largeAllocationBytes set to one document's UTF-16 bytes, the embedded
// runner counts each step's document-sized C++ allocations ("cppLargeAllocations"): the copies of the document.
//
// The scene owns its EmbeddedHost so that the text store, which borrows the host and the control, is released before them.
// Embedded host only: the window runner does not count C++ allocations, which are this fixture's measure.
// Not the W8.6 CJK and emoji text: with it, each dirty frame of the 64 Ki-unit single-line TextField took about 6 s on WARP
// in Release (one run: 80 s with it in the TextField only, 103 s in the editor only, 202 s in both; 4 s with this text),
// which the determinism gate's two runs per suite cannot afford; W8.6's own tests keep that text.
#include "Controls/DxUi.Internal.h"
#include "Embedded.Tests.PerformanceFixture.h"
#include "Embedded.Tests.TsfDoubles.h"

#include <DxUi/Failure.h>
#include <wil/com.h>

namespace PerformanceFixtures::Fx7
{
// 64 Ki UTF-16 units less room for the composition, as in the W8.5 scenario: RichTextEditor documents stop at 64 Ki units.
inline constexpr size_t kDocumentUnits = 65'536u - 64u;
inline constexpr size_t kDocumentBytes = kDocumentUnits * sizeof(wchar_t);
// The caret sits mid-document, where the W8.5 scenario composes.
inline constexpr size_t kCaretUnits = kDocumentUnits / 2u;
// Ten Hangul syllables, one UTF-16 unit each; syllable k shows the first k as the composition.
inline constexpr std::wstring_view kSyllables = L"\ud55c\uad6d\uc5b4\uc785\ub825\uc2dc\ud5d8\ubb38\uc7a5\ub05d";
static_assert(kSyllables.size() == 10u);
// Per control: an edit lock and a read lock per syllable, then the commit lock.
inline constexpr size_t kLockSteps = 2u * 10u + 1u;
inline constexpr UINT kWidth       = 640u;
inline constexpr UINT kHeight      = 480u;

inline constexpr std::array kVariants{Variant{"default", kWidth, kHeight, 96.0f, false}};
inline constexpr size_t kTextField  = 1u;
inline constexpr size_t kRichEditor = 3u;
inline constexpr std::array kPhases{
    Phase{"warm",
          2u,
          PhaseKind::Warmup,
          "the first frame lays out both controls; then the TextField takes logical focus with its caret mid-document and its store "
          "answers the extent query a text service makes when a composition starts"},
    Phase{"textfield",
          kLockSteps,
          PhaseKind::Measured,
          "TextField: step 2k is syllable k+1's read-write lock (selection, end, insert or rewrite of the composition, composition "
          "owner callback, caret, text read back), step 2k+1 its read lock (selection, end, extent, text); step 20 the commit lock"},
    Phase{"switch", 1u, PhaseKind::Warmup, "the TextField's store is released; the RichTextEditor takes logical focus and its store answers the extent query"},
    Phase{"richtext", kLockSteps, PhaseKind::Measured, "RichTextEditor: the same 21 locks as the TextField phase"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx7.h"),
                                     std::string_view("Tests/Embedded/Embedded.Tests.TsfDoubles.h")};

// The W8.5 document: words of seven letters and a space, no line breaks (Embedded.Tests.ImeCopies.h, MakeDocument).
[[nodiscard]] inline std::wstring MakeDocument()
{
    std::wstring text(kDocumentUnits, L'a');
    for (size_t index = 0u; index < text.size(); ++index)
        text[index] = (index % 8u) == 7u ? L' ' : static_cast<wchar_t>(L'a' + (index % 26u));
    return text;
}

// One control's composition session: its store, sink and composition view, and the ten locks and commit.
class Session final
{
public:
    void Begin(DxUi::ControlHost& host, DxUi::Control& control)
    {
        store_.attach(DxUi::CreateNativeTextInputTextStore(host, control, false));
        if (! store_)
            DxUi::Failure::FailContract(E_UNEXPECTED);
        Succeed(store_.query_to(owner_.put()));
        Succeed(store_->AdviseSink(__uuidof(ITextStoreACPSink), &sink_, TS_AS_ALL_SINKS));
        composition_.range.start  = static_cast<LONG>(kCaretUnits);
        composition_.range.length = 0;
        // A text service asks for the caret's extent when a composition starts, so the control's layout exists.
        Lock(TS_LF_READ,
             [&]() noexcept -> HRESULT
        {
            RECT extent{};
            BOOL clipped     = FALSE;
            const HRESULT hr = store_->GetTextExt(1u, kStart, kStart, &extent, &clipped);
            return hr == TS_E_NOLAYOUT ? S_OK : hr;
        });
    }
    void End() noexcept
    {
        if (! store_)
            return;
        static_cast<void>(store_->UnadviseSink(&sink_));
        owner_.reset();
        store_.reset();
    }
    // Lock `step` of the 21 (kLockSteps); later steps do nothing.
    void Step(size_t step)
    {
        if (! store_ || step >= kLockSteps)
            return;
        if (step + 1u == kLockSteps)
        {
            const LONG end = kStart + static_cast<LONG>(kSyllables.size());
            Lock(TS_LF_READWRITE,
                 [&]() noexcept -> HRESULT
            {
                TS_TEXTCHANGE change{};
                HRESULT hr = store_->SetText(0u, kStart, end, kSyllables.data(), static_cast<ULONG>(kSyllables.size()), &change);
                if (SUCCEEDED(hr))
                    hr = owner_->OnEndComposition(&composition_);
                return hr;
            });
            return;
        }
        const size_t syllable = step / 2u + 1u;
        const LONG end        = kStart + static_cast<LONG>(syllable);
        if (step % 2u == 0u)
            Lock(TS_LF_READWRITE, [&]() noexcept { return Edit(syllable, end); });
        else
            Lock(TS_LF_READ, [&]() noexcept { return Read(end); });
    }

private:
    static constexpr LONG kStart = static_cast<LONG>(kCaretUnits);

    static void Succeed(HRESULT hr) noexcept
    {
        if (FAILED(hr))
            DxUi::Failure::FailContract(hr);
    }
    template <typename Body> void Lock(DWORD flags, Body&& body)
    {
        HRESULT bodyResult = S_OK;
        sink_.onLock       = [&]() noexcept
        {
            bodyResult = body();
            return bodyResult;
        };
        HRESULT session         = E_FAIL;
        const HRESULT requested = store_->RequestLock(flags | TS_LF_SYNC, &session);
        sink_.onLock            = nullptr;
        Succeed(requested);
        Succeed(session);
        Succeed(bodyResult);
    }
    // Syllable k: the first inserts at the caret and starts the composition; each later one rewrites it and moves the caret.
    HRESULT Edit(size_t syllable, LONG end) noexcept
    {
        TS_SELECTION_ACP selection{};
        ULONG fetched    = 0u;
        HRESULT hr       = store_->GetSelection(TS_DEFAULT_SELECTION, 1u, &selection, &fetched);
        LONG documentEnd = 0;
        if (SUCCEEDED(hr))
            hr = store_->GetEndACP(&documentEnd);
        TS_TEXTCHANGE change{};
        if (SUCCEEDED(hr) && syllable == 1u)
        {
            LONG first                = 0;
            LONG last                 = 0;
            hr                        = store_->InsertTextAtSelection(0u, kSyllables.data(), 1u, &first, &last, &change);
            composition_.range.length = 1;
            BOOL accepted             = FALSE;
            if (SUCCEEDED(hr))
                hr = owner_->OnStartComposition(&composition_, &accepted);
            if (SUCCEEDED(hr) && ! accepted)
                hr = E_FAIL;
        }
        else if (SUCCEEDED(hr))
        {
            hr                        = store_->SetText(0u, kStart, end - 1, kSyllables.data(), static_cast<ULONG>(syllable), &change);
            composition_.range.length = static_cast<LONG>(syllable);
            if (SUCCEEDED(hr))
                hr = owner_->OnUpdateComposition(&composition_, &composition_.range);
            const TS_SELECTION_ACP caretAtEnd{end, end, {TS_AE_END, FALSE}};
            if (SUCCEEDED(hr))
                hr = store_->SetSelection(1u, &caretAtEnd);
        }
        std::array<WCHAR, 16u> text{};
        ULONG copied = 0u;
        ULONG runs   = 0u;
        LONG next    = 0;
        if (SUCCEEDED(hr))
            hr = store_->GetText(kStart, end, text.data(), static_cast<ULONG>(text.size()), &copied, nullptr, 0u, &runs, &next);
        return hr;
    }
    // The read lock a candidate window makes after each syllable. A range the view does not show has no extent
    // (TS_E_NOLAYOUT), which TSF allows; the layout work is done either way.
    HRESULT Read(LONG end) noexcept
    {
        TS_SELECTION_ACP selection{};
        ULONG fetched    = 0u;
        HRESULT hr       = store_->GetSelection(TS_DEFAULT_SELECTION, 1u, &selection, &fetched);
        LONG documentEnd = 0;
        if (SUCCEEDED(hr))
            hr = store_->GetEndACP(&documentEnd);
        RECT extent{};
        BOOL clipped = FALSE;
        if (SUCCEEDED(hr))
        {
            hr = store_->GetTextExt(1u, kStart, end, &extent, &clipped);
            if (hr == TS_E_NOLAYOUT)
                hr = S_OK;
        }
        std::array<WCHAR, 16u> text{};
        ULONG copied = 0u;
        ULONG runs   = 0u;
        LONG next    = 0;
        if (SUCCEEDED(hr))
            hr = store_->GetText(kStart, end, text.data(), static_cast<ULONG>(text.size()), &copied, nullptr, 0u, &runs, &next);
        return hr;
    }

    TsfDoubles::Sink sink_;
    TsfDoubles::Composition composition_;
    wil::com_ptr_nothrow<ITextStoreACP> store_;
    wil::com_ptr_nothrow<ITfContextOwnerCompositionSink> owner_;
};

class Scene final : public Scenario
{
public:
    ~Scene() override
    {
        // The store borrows the host and the control: release it first.
        session_.End();
    }
    DxUi::EmbeddedHost* AttachOwnEmbeddedHost(ID3D11Device* device, HRESULT& result) override
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        result = DxUi::GraphicsDevice::Create(device, graphics);
        if (SUCCEEDED(result))
            result = view_.Attach(std::move(graphics));
        if (FAILED(result))
            return nullptr;
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        view_.Controls().SetTheme(theme);
        return &view_;
    }
    HRESULT Build(Context& context) override
    {
        const std::wstring document = MakeDocument();
        auto root                   = std::make_unique<DxUi::Panel>();
        field_                      = root->AddChild<DxUi::TextField>(document);
        field_->SetBounds(D2D1::RectF(8.0f, 8.0f, 632.0f, 40.0f));
        field_->SetSelectionRange(kCaretUnits, kCaretUnits);
        editor_ = root->AddChild<DxUi::RichTextEditor>(document);
        editor_->SetBounds(D2D1::RectF(8.0f, 48.0f, 632.0f, 472.0f));
        editor_->SetSelection(DxUi::RichText::Selection{kCaretUnits, kCaretUnits});
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        switch (phase)
        {
            case 0u:
                if (step == 1u)
                {
                    context.controls.SetFocusControl(field_, false);
                    session_.Begin(context.controls, *field_);
                }
                break;
            case kTextField: session_.Step(step); break;
            case 2u:
                if (step == 0u)
                {
                    session_.End();
                    context.controls.SetFocusControl(editor_, false);
                    editor_->SetSelection(DxUi::RichText::Selection{kCaretUnits, kCaretUnits});
                    session_.Begin(context.controls, *editor_);
                }
                break;
            case kRichEditor: session_.Step(step); break;
            default: break;
        }
    }

private:
    // Declared before the session, which is destroyed (and, in the destructor, released) first.
    DxUi::EmbeddedHost view_;
    DxUi::TextField* field_       = nullptr;
    DxUi::RichTextEditor* editor_ = nullptr;
    Session session_;
};

inline constexpr Definition kDefinition{
    .id                   = "FX7",
    .title                = "IME composition",
    .scenario             = "A TextField and a RichTextEditor holding the W8.5 document of 65,472 UTF-16 units (seven-letter words), caret mid-document, "
                            "640x480 offscreen EmbeddedHost, reduced motion. Ten Hangul syllables are composed and committed through the production "
                            "TSF text store with in-process doubles (the W8.5 scenario), no focus and no text service: one TSF lock per step, each "
                            "followed by one frame. cppLargeAllocations counts allocations of at least one document's bytes (130,944 B). Proves the W8 gates.",
    .hosts                = kEmbedded,
    .variants             = kVariants,
    .phases               = kPhases,
    .timedPhase           = "textfield",
    .sources              = kSources,
    .reducedMotion        = true,
    .create               = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
    .largeAllocationBytes = kDocumentBytes,
};
} // namespace PerformanceFixtures::Fx7
