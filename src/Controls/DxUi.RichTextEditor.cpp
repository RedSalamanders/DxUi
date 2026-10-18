#include "DxUi.Internal.h"
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <stdexcept>

namespace DxUi
{
struct RichTextEditor::State
{
    explicit State(std::wstring text) : document(std::move(text))
    {
    }
    RichText::Document document;
    RichText::PreparedLayout layout;
    RichText::ViewMode mode = RichText::ViewMode::Rich;
    std::shared_ptr<const std::function<void()>> onTextChanged;
    uint64_t typographyRevision  = 0;
    uint64_t bindingRevision     = 0;
    float layoutWidth            = 0;
    float scrollDip              = 0;
    bool revealCaret             = true;
    bool readOnly                = false;
    bool dragging                = false;
    bool caretVisible            = true;
    wchar_t pendingLeadSurrogate = 0;
    std::optional<uint64_t> blinkEpoch;
    std::optional<float> preferredCaretX;
};

RichTextEditor::RichTextEditor(std::wstring text) : _state(std::make_unique<State>(std::move(text)))
{
    SetFocusable(true);
}
RichTextEditor::~RichTextEditor() noexcept = default;
void RichTextEditor::SetText(std::wstring text)
{
    _state->document.Reset(std::move(text));
    _state->scrollDip = 0;
    RetireLayout();
    SelectionChanged();
}
std::wstring RichTextEditor::GetText() const
{
    return _state->document.Text();
}
RichText::SourceSnapshot RichTextEditor::Snapshot() const noexcept
{
    return _state->document.Snapshot();
}
uint64_t RichTextEditor::Revision() const noexcept
{
    return _state->document.Revision();
}
void RichTextEditor::SetSelection(RichText::Selection selection)
{
    _state->document.SetSelection(selection);
    SelectionChanged();
}
RichText::Selection RichTextEditor::GetSelection() const noexcept
{
    return _state->document.GetSelection();
}
void RichTextEditor::SetViewMode(RichText::ViewMode mode)
{
    if (mode != RichText::ViewMode::Source && mode != RichText::ViewMode::Rich)
        throw std::invalid_argument("Invalid rich-text view mode");
    if (_state->mode == mode)
        return;
    _state->mode = mode;
    RetireLayout();
    SelectionChanged();
}
RichText::ViewMode RichTextEditor::GetViewMode() const noexcept
{
    return _state->mode;
}
void RichTextEditor::SetReadOnly(bool readOnly) noexcept
{
    _state->readOnly = readOnly;
    SelectionChanged();
}
bool RichTextEditor::IsReadOnly() const noexcept
{
    return _state->readOnly;
}
bool RichTextEditor::Editable() const noexcept
{
    return IsEnabled() && ! _state->readOnly;
}
bool RichTextEditor::ReplaceSelection(std::wstring text)
{
    if (! Editable())
        return false;
    const bool changed = _state->document.ReplaceSelection(std::move(text));
    Changed(changed);
    return changed;
}
bool RichTextEditor::ToggleFormatting(std::wstring_view marker)
{
    if (! Editable())
        return false;
    const bool changed = RichText::ToggleMarker(_state->document, marker);
    Changed(changed);
    return changed;
}
bool RichTextEditor::Undo()
{
    if (! Editable() || ! _state->document.Undo())
        return false;
    Changed(true);
    return true;
}
bool RichTextEditor::Redo()
{
    if (! Editable() || ! _state->document.Redo())
        return false;
    Changed(true);
    return true;
}
bool RichTextEditor::CanUndo() const noexcept
{
    return _state->document.CanUndo();
}
bool RichTextEditor::CanRedo() const noexcept
{
    return _state->document.CanRedo();
}
void RichTextEditor::SetOnTextChanged(std::function<void()> callback)
{
    _state->onTextChanged = callback ? std::make_shared<const std::function<void()>>(std::move(callback)) : nullptr;
}
void RichTextEditor::Changed(bool sourceChanged, bool synchronizeTextInput)
{
    if (sourceChanged)
        RetireLayout();
    const auto callback = sourceChanged ? _state->onTextChanged : nullptr;
    const auto lifetime = GetLifetimeToken();
    const auto revision = Revision();
    const auto binding  = _state->bindingRevision;
    const auto* host    = GetHost();
    SelectionChanged(synchronizeTextInput);
    // An embedded scheduling callback may itself edit, detach or retire this
    // control. Never publish a superseded notification or touch a retired owner.
    if (lifetime.expired() || GetHost() != host || Revision() != revision || _state->bindingRevision != binding ||
        (callback && _state->onTextChanged != callback))
        return;
    if (callback)
    {
        try
        {
            (*callback)();
        }
        catch (const std::exception&)
        { /* The edit is committed; notification failure cannot roll it back. */
        }
    }
}
void RichTextEditor::SelectionChanged(bool synchronizeTextInput)
{
    _state->pendingLeadSurrogate = 0;
    _state->preferredCaretX.reset();
    _state->revealCaret  = true;
    _state->caretVisible = true;
    _state->blinkEpoch.reset();
    const auto lifetime = GetLifetimeToken();
    auto* host          = GetHost();
    // Imported previews are synchronized by the host after it publishes its
    // composition ownership marker. Public changes supersede that preview.
    if (synchronizeTextInput && host)
        host->SyncTextInput(this);
    if (lifetime.expired() || GetHost() != host)
        return;
    RequestInvalidate();
    if (! lifetime.expired() && GetHost() == host && host && HasFocus() && Editable() && ! host->GetTheme().reducedMotion)
        host->RequestAnimation();
}
D2D1_RECT_F RichTextEditor::TextViewport() const noexcept
{
    const auto bounds   = GetBounds();
    const float padding = IsCompactDensity() ? 6.0f : 8.0f;
    return D2D1::RectF(bounds.left + padding,
                       bounds.top + padding,
                       std::max(bounds.left + padding, bounds.right - padding),
                       std::max(bounds.top + padding, bounds.bottom - padding));
}
void RichTextEditor::RetireLayout() const noexcept
{
    _state->layout.Clear();
    _state->revealCaret = true;
}
void RichTextEditor::EnsureLayout(const ControlHost& host) const
{
    auto& state         = *_state;
    const auto viewport = TextViewport();
    const float width   = std::max(1.0f, viewport.right - viewport.left);
    if (! state.layout.Ready() || state.layoutWidth != width || state.typographyRevision != host.GetTypographyRevision())
    {
        auto* format     = host.GetTextFormat(FontRole::Body);
        auto* codeFormat = host.GetTextFormat(FontRole::Monospace);
        if (! format || ! codeFormat || ! host.GetWriteFactory())
            THROW_HR(E_UNEXPECTED);
        RichText::LayoutOptions options;
        options.widthDip    = width;
        options.fontSizeDip = format->GetFontSize();
        options.fontFamily.resize(static_cast<size_t>(format->GetFontFamilyNameLength()) + 1);
        THROW_IF_FAILED(format->GetFontFamilyName(options.fontFamily.data(), static_cast<UINT32>(options.fontFamily.size())));
        options.fontFamily.pop_back();
        options.codeFontFamily.resize(static_cast<size_t>(codeFormat->GetFontFamilyNameLength()) + 1);
        THROW_IF_FAILED(codeFormat->GetFontFamilyName(options.codeFontFamily.data(), static_cast<UINT32>(options.codeFontFamily.size())));
        options.codeFontFamily.pop_back();
        options.locale.resize(static_cast<size_t>(format->GetLocaleNameLength()) + 1);
        THROW_IF_FAILED(format->GetLocaleName(options.locale.data(), static_cast<UINT32>(options.locale.size())));
        options.locale.pop_back();
        options.readingDirection = IsRightToLeft() ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
        // DirectWrite and rasterization run only on the host's owner thread.
        // EmbeddedHost subsequently composites its existing cached surface.
        state.layout.Prepare(host.GetWriteFactory(), RichText::Project(state.document.Snapshot(), state.document.Revision(), state.mode), options);
        state.layoutWidth        = width;
        state.typographyRevision = host.GetTypographyRevision();
    }
    const float height = std::max(0.0f, viewport.bottom - viewport.top);
    if (state.revealCaret)
    {
        const auto caret = state.layout.Caret(state.layout.Content().ToDisplay(state.document.GetSelection().caret));
        if (caret.y < state.scrollDip)
            state.scrollDip = caret.y;
        if (caret.y + caret.height > state.scrollDip + height)
            state.scrollDip = caret.y + caret.height - height;
        state.revealCaret = false;
    }
    state.scrollDip = std::clamp(state.scrollDip, 0.0f, std::max(0.0f, state.layout.Height() - height));
}
RichTextEditorDebugState RichTextEditor::DebugGetState() const noexcept
{
    return {_state->layout.Ready(), _state->layout.Generation(), _state->layout.Height(), _state->scrollDip, _state->dragging};
}
bool RichTextEditor::SupportsTextInput() const noexcept
{
    return true;
}
std::optional<std::vector<std::pair<size_t, size_t>>> RichTextEditor::GetSourceVisualLineRanges(const ControlHost& host) const noexcept
{
    try
    {
        EnsureLayout(host);
        const auto& projection = _state->layout.Content();
        std::vector<std::pair<size_t, size_t>> ranges;
        size_t displayStart = 0;
        size_t sourceStart  = 0;
        do
        {
            size_t displayEnd = _state->layout.LineBoundary(displayStart, true);
            if (displayEnd < projection.text.size() && projection.text[displayEnd] == L'\n')
                ++displayEnd;
            // Earliest source affinity makes adjacent lines partition exact
            // source, including hidden markers and original CRLF pairs.
            const size_t sourceEnd =
                displayEnd == projection.text.size()
                    ? projection.source.size()
                    : static_cast<size_t>(std::lower_bound(projection.sourceToDisplay.begin(), projection.sourceToDisplay.end(), displayEnd) -
                                          projection.sourceToDisplay.begin());
            ranges.emplace_back(sourceStart, sourceEnd);
            if (displayEnd >= projection.text.size() || displayEnd <= displayStart)
            {
                if (! projection.text.empty() && projection.text.back() == L'\n')
                    ranges.emplace_back(projection.source.size(), projection.source.size());
                break;
            }
            sourceStart  = sourceEnd;
            displayStart = displayEnd;
        } while (true);
        return ranges;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
bool RichTextEditor::ScrollSourceRangeIntoView(ControlHost& host, size_t sourceStart, size_t sourceEnd, bool alignToTop) noexcept
{
    try
    {
        const auto source = Snapshot();
        if (GetHost() != &host || ! IsEnabled() || sourceStart > sourceEnd || ! source.IsBoundary(sourceStart) || ! source.IsBoundary(sourceEnd))
            return false;
        EnsureLayout(host);
        const auto& projection = _state->layout.Content();
        const auto first       = projection.ToDisplay(sourceStart);
        const auto last        = projection.ToDisplay(sourceEnd);
        // Hidden-only syntax has no painted extent to reveal. Do not jump to
        // an offscreen marker's anchor or fabricate a visible rectangle.
        if (sourceStart != sourceEnd && first == last)
            return true;
        const auto caret = _state->layout.Caret(first);
        float top        = caret.y;
        float bottom     = caret.y + caret.height;
        if (first != last)
        {
            const auto rectangles = _state->layout.Range(first, last);
            if (! rectangles.empty())
            {
                top    = rectangles.front().top;
                bottom = rectangles.front().bottom;
                for (const auto& rectangle : rectangles)
                {
                    top    = std::min(top, rectangle.top);
                    bottom = std::max(bottom, rectangle.bottom);
                }
            }
        }
        const auto viewport = TextViewport();
        const float height  = std::max(0.0f, viewport.bottom - viewport.top);
        const float offset  = std::clamp(alignToTop ? top : bottom - height, 0.0f, std::max(0.0f, _state->layout.Height() - height));
        _state->revealCaret = false;
        if (_state->scrollDip == offset)
            return true;
        _state->scrollDip = offset;
        // Synchronization and invalidation can invoke application scheduling.
        // Capture identity before crossing either boundary; never resume on a
        // detached/replaced editor. Neither operation changes source/history.
        const auto lifetime = GetLifetimeToken();
        const auto binding  = _state->bindingRevision;
        host.SyncTextInput(this);
        if (lifetime.expired() || GetHost() != &host || _state->bindingRevision != binding)
            return false;
        RequestInvalidate();
        return ! lifetime.expired() && GetHost() == &host && _state->bindingRevision == binding;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
std::optional<std::pair<size_t, size_t>> RichTextEditor::GetVisibleSourceRange(const ControlHost& host) const noexcept
{
    if (GetHost() != &host || ! GetTextInputViewportRect())
        return std::nullopt;
    try
    {
        EnsureLayout(host);
        const auto viewport      = TextViewport();
        const auto [first, last] = _state->layout.VisibleRange(_state->scrollDip, _state->scrollDip + viewport.bottom - viewport.top);
        const auto& projection   = _state->layout.Content();
        return std::pair{projection.ToSource(first), projection.ToSource(last, true)};
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
std::optional<D2D1_RECT_F> RichTextEditor::GetTextInputViewportRect() const noexcept
{
    const auto viewport = TextViewport();
    if (! IsVisible() || viewport.right <= viewport.left || viewport.bottom <= viewport.top)
        return std::nullopt;
    return viewport;
}
std::optional<D2D1_RECT_F> RichTextEditor::GetTextInputCaretRect(const ControlHost& host, size_t sourceIndex) const noexcept
{
    if (GetHost() != &host || ! GetTextInputViewportRect() || ! Snapshot().IsBoundary(sourceIndex))
        return std::nullopt;
    try
    {
        EnsureLayout(host);
        const auto caret    = _state->layout.Caret(_state->layout.Content().ToDisplay(sourceIndex));
        const auto viewport = TextViewport();
        // Keep the un-clipped caret for TSF's clipped flag; painting and native
        // screen transport clip it to the same prepared text viewport.
        return D2D1::RectF(viewport.left + caret.x,
                           viewport.top + caret.y - _state->scrollDip,
                           viewport.left + caret.x + 1,
                           viewport.top + caret.y + caret.height - _state->scrollDip);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
std::optional<std::vector<D2D1_RECT_F>> RichTextEditor::GetTextInputRangeRects(const ControlHost& host, size_t sourceStart, size_t sourceEnd) const
{
    if (GetHost() != &host || ! GetTextInputViewportRect() || sourceStart > sourceEnd || ! Snapshot().IsBoundary(sourceStart) ||
        ! Snapshot().IsBoundary(sourceEnd))
        return std::nullopt;
    try
    {
        EnsureLayout(host);
        const auto& projection = _state->layout.Content();
        const auto viewport    = TextViewport();
        auto rectangles        = _state->layout.Range(projection.ToDisplay(sourceStart), projection.ToDisplay(sourceEnd));
        for (auto& rectangle : rectangles)
        {
            rectangle.left += viewport.left;
            rectangle.right += viewport.left;
            rectangle.top += viewport.top - _state->scrollDip;
            rectangle.bottom += viewport.top - _state->scrollDip;
        }
        // Service transport needs the raw extents to report clipping truthfully.
        // UIA clips these separately and keeps hidden delimiters pixel-empty.
        return rectangles;
    }
    catch (const std::exception&)
    {
        // Native text-store geometry is a noexcept COM boundary. A failed
        // preparation/allocation reports unavailable layout, never termination.
        return std::nullopt;
    }
}
std::optional<size_t> RichTextEditor::HitTestTextInputPoint(const ControlHost& host, D2D1_POINT_2F point) const noexcept
{
    if (GetHost() != &host || ! GetTextInputViewportRect() || ! std::isfinite(point.x) || ! std::isfinite(point.y))
        return std::nullopt;
    try
    {
        EnsureLayout(host);
        const auto viewport = TextViewport();
        const auto hit      = _state->layout.HitTest(point.x - viewport.left, point.y - viewport.top + _state->scrollDip);
        return _state->layout.Content().ToSource(hit.position, hit.trailing);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}
bool RichTextEditor::ExportTextInputState(TextInputState& state) const
{
    state                = {};
    state.text           = GetText();
    const auto selection = GetSelection();
    state.caretIndex     = selection.caret;
    if (selection.anchor != selection.caret)
        state.selectionAnchorIndex = selection.anchor;
    state.readOnly  = ! Editable();
    state.multiline = true;
    return true;
}
bool RichTextEditor::ImportTextInputState(ControlHost& host, const TextInputState& state, bool notifyChange)
{
    if (GetHost() != &host || state.masked || ! state.multiline || (notifyChange && ! Editable()))
        return false;
    const RichText::Selection selection{state.selectionAnchorIndex.value_or(state.caretIndex), state.caretIndex};
    // Hosts restore entry source before a notifying commit. Preview updates,
    // cancellation and selection-only imports preserve both undo and redo.
    bool changed;
    try
    {
        changed = notifyChange ? _state->document.Replace(0, _state->document.Length(), state.text, selection)
                               : _state->document.ReplaceTransient(state.text, selection);
    }
    catch (const std::logic_error&)
    {
        // Malformed service text, oversized input and split scalar/CRLF ACPs
        // are rejected atomically, without escaping a native noexcept handler.
        return false;
    }
    _state->dragging = false;
    if (notifyChange)
        Changed(changed, false);
    else
    {
        if (changed)
            RetireLayout();
        SelectionChanged(false);
    }
    return true;
}
void RichTextEditor::Paint(ControlHost& host) const
{
    // Hosts can paint a concrete control directly as their root. A hidden root
    // must not recreate the layout retired by OnHidden or expose its text.
    if (! IsVisible())
        return;
    auto* context = host.GetDeviceContext();
    if (! context)
        THROW_HR(E_UNEXPECTED);
    const auto style = ResolveTextFieldVisualStyle(host.GetTheme(), IsEnabled(), IsHovered(), HasFocus(), HasFocus() && host.IsKeyboardFocusVisible());
    DrawRoundedRect(host, GetBounds(), style.fill, style.border, 4.0f);
    if (style.showFocus)
        context->DrawLine(D2D1::Point2F(GetBounds().left + 4, GetBounds().bottom - 1),
                          D2D1::Point2F(GetBounds().right - 4, GetBounds().bottom - 1),
                          host.GetSolidBrush(host.GetTheme().accent),
                          2.0f);
    const auto viewport = TextViewport();
    if (viewport.right <= viewport.left || viewport.bottom <= viewport.top)
        return;
    EnsureLayout(host);
    auto& state             = *_state;
    auto* textBrush         = host.GetSolidBrush(style.text);
    auto* selectedTextBrush = host.GetSolidBrush(style.selectionText);
    auto* selectionBrush    = host.GetSolidBrush(style.selectionFill);
    auto* caretBrush        = host.GetSolidBrush(style.caret);
    const auto selection    = state.document.GetSelection();
    const auto& projection  = state.layout.Content();
    const auto rectangles   = state.layout.Range(projection.ToDisplay(selection.Start()), projection.ToDisplay(selection.End()));
    const auto origin       = D2D1::Point2F(viewport.left, viewport.top - state.scrollDip);
    context->PushAxisAlignedClip(viewport, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    const auto restoreClip = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
    state.layout.Draw(context, textBrush, origin);
    for (const auto& rectangle : rectangles)
    {
        const auto translated = D2D1::RectF(rectangle.left + origin.x, rectangle.top + origin.y, rectangle.right + origin.x, rectangle.bottom + origin.y);
        if (translated.bottom <= viewport.top || translated.top >= viewport.bottom)
            continue;
        context->FillRectangle(translated, selectionBrush);
        context->PushAxisAlignedClip(translated, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto restoreSelectionClip = wil::scope_exit([context]() noexcept { context->PopAxisAlignedClip(); });
        state.layout.Draw(context, selectedTextBrush, origin);
    }
    NativeTextInputState input;
    if (host.TryReadNativeTextInputState(this, input) && input.text == projection.source && input.compositionStartIndex && input.compositionEndIndex)
    {
        const auto underline = [&](size_t first, size_t last, float thickness)
        {
            if (first > last || last > projection.source.size())
                return;
            for (const auto& rect : state.layout.Range(projection.ToDisplay(first), projection.ToDisplay(last)))
                context->DrawLine(D2D1::Point2F(rect.left + origin.x, rect.bottom + origin.y - 1),
                                  D2D1::Point2F(rect.right + origin.x, rect.bottom + origin.y - 1),
                                  caretBrush,
                                  thickness);
        };
        underline(*input.compositionStartIndex, *input.compositionEndIndex, 1.0f);
        if (input.conversionTargetStartIndex && input.conversionTargetEndIndex)
            underline(*input.conversionTargetStartIndex, *input.conversionTargetEndIndex, 2.0f);
    }
    if (HasFocus() && IsEnabled() && state.caretVisible)
    {
        const auto caret = state.layout.Caret(projection.ToDisplay(selection.caret));
        const float x    = std::clamp(origin.x + caret.x, viewport.left, std::max(viewport.left, viewport.right - 1.0f));
        context->DrawLine(D2D1::Point2F(x, origin.y + caret.y), D2D1::Point2F(x, origin.y + caret.y + caret.height), caretBrush, 1.0f);
    }
}
bool RichTextEditor::Tick(ControlHost& host, uint64_t nowTickMs)
{
    auto& state = *_state;
    if (! HasFocus() || ! IsVisible() || ! Editable() || host.GetTheme().reducedMotion)
    {
        const bool changed = ! state.caretVisible;
        state.caretVisible = true;
        state.blinkEpoch.reset();
        if (changed)
            RequestInvalidate();
        return false;
    }
    // The application supplies the embedded epoch, including zero. No uptime,
    // private timer, or thread is introduced by this control.
    if (! state.blinkEpoch || nowTickMs < *state.blinkEpoch)
        state.blinkEpoch = nowTickMs;
    const bool visible = ((nowTickMs - *state.blinkEpoch) / 500u) % 2u == 0;
    if (visible != state.caretVisible)
    {
        state.caretVisible = visible;
        RequestInvalidate();
    }
    return true;
}
size_t RichTextEditor::HitSource(ControlHost& host, D2D1_POINT_2F point) const
{
    EnsureLayout(host);
    const auto viewport = TextViewport();
    const auto hit      = _state->layout.HitTest(point.x - viewport.left, point.y - viewport.top + _state->scrollDip);
    return _state->layout.Content().ToSource(hit.position, hit.trailing);
}
void RichTextEditor::MoveCaret(ControlHost& host, UINT key, bool extend, bool byWord)
{
    EnsureLayout(host);
    auto& state            = *_state;
    const auto selection   = GetSelection();
    const auto& projection = state.layout.Content();
    const auto position    = projection.ToDisplay(selection.caret);
    size_t next            = position;
    bool trailing          = false;
    std::optional<float> preferredX;
    if (! extend && selection.Start() != selection.End() && (key == VK_LEFT || key == VK_RIGHT))
    {
        const auto source = key == VK_LEFT ? selection.Start() : selection.End();
        SetSelection({source, source});
        return;
    }
    switch (key)
    {
        case VK_LEFT: next = byWord ? FindPreviousWordBoundary(projection.text, position) : state.layout.PreviousCluster(position); break;
        case VK_RIGHT:
            next     = byWord ? FindNextWordBoundary(projection.text, position) : state.layout.NextCluster(position);
            trailing = true;
            break;
        case VK_HOME: next = byWord ? 0 : state.layout.LineBoundary(position, false); break;
        case VK_END:
            next     = byWord ? projection.text.size() : state.layout.LineBoundary(position, true);
            trailing = true;
            break;
        default:
        {
            const auto caret     = state.layout.Caret(position);
            preferredX           = state.preferredCaretX.value_or(caret.x);
            const bool down      = key == VK_DOWN || key == VK_NEXT;
            const float distance = (key == VK_NEXT || key == VK_PRIOR) ? std::max(caret.height, TextViewport().bottom - TextViewport().top) : caret.height;
            const auto hit       = state.layout.HitTest(*preferredX, caret.y + caret.height * 0.5f + (down ? distance : -distance));
            next                 = hit.position;
            trailing             = hit.trailing;
            break;
        }
    }
    const auto source = projection.ToSource(next, trailing);
    state.document.SetSelection({extend ? selection.anchor : source, source});
    // Set all state before host invalidation can invoke application scheduling.
    state.pendingLeadSurrogate = 0;
    state.preferredCaretX      = preferredX;
    state.revealCaret          = true;
    state.caretVisible         = true;
    state.blinkEpoch.reset();
    RequestInvalidate();
}
bool RichTextEditor::Erase(ControlHost& host, bool backwards)
{
    if (! Editable())
        return false;
    const auto selection = GetSelection();
    if (selection.Start() != selection.End())
        return ReplaceSelection({});
    EnsureLayout(host);
    auto& state              = *_state;
    const auto& projection   = state.layout.Content();
    const auto position      = projection.ToDisplay(selection.caret);
    const auto [first, last] = state.layout.DeletionRange(position, backwards);
    if (first == last)
        return false;
    const auto begin   = projection.ToSource(first);
    const auto end     = projection.ToSource(last, true);
    const bool changed = state.document.Replace(begin, end, {}, {begin, begin});
    Changed(changed);
    return changed;
}
bool RichTextEditor::OnChar(ControlHost&, wchar_t character, UINT modifiers) noexcept
{
    try
    {
        // AltGr arrives as Ctrl+Alt on Windows. Its Unicode WM_CHAR must remain
        // insertable, while an isolated command modifier must not type a shortcut.
        if (! Editable() || (ModifiersContainCtrl(modifiers) != ModifiersContainAlt(modifiers)))
        {
            _state->pendingLeadSurrogate = 0;
            return false;
        }
        if (IsUtf16LeadSurrogate(character))
        {
            _state->pendingLeadSurrogate = character;
            return true;
        }
        std::wstring text;
        if (IsUtf16TrailSurrogate(character))
        {
            const auto lead = std::exchange(_state->pendingLeadSurrogate, wchar_t{});
            if (! lead)
                return true;
            text = {lead, character};
        }
        else
        {
            _state->pendingLeadSurrogate = 0;
            if (character == L'\r')
                text = L"\r\n";
            else if (character >= L' ' && character != L'\x7f')
                text.assign(1, character);
            else
                return false; // Tab is host traversal; control characters never enter the source.
        }
        static_cast<void>(ReplaceSelection(std::move(text)));
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnKeyDown(ControlHost& host, UINT key, UINT modifiers) noexcept
{
    try
    {
        if (! IsEnabled())
            return false;
        _state->pendingLeadSurrogate = 0;
        const bool control = ModifiersContainCtrl(modifiers), shift = ModifiersContainShift(modifiers);
        if (ModifiersContainAlt(modifiers))
            return false;
        if (control)
        {
            switch (key)
            {
                case 'A': return OnSelectAll(host);
                case 'C': return OnCopy(host);
                case 'X':
                {
                    if (! Editable())
                        return true;
                    const auto lifetime  = GetLifetimeToken();
                    const auto revision  = Revision();
                    const auto binding   = _state->bindingRevision;
                    const auto selection = GetSelection();
                    if (OnCopy(host) && ! lifetime.expired() && GetHost() == &host && _state->bindingRevision == binding && Revision() == revision &&
                        GetSelection() == selection)
                        static_cast<void>(ReplaceSelection({}));
                    return true;
                }
                case 'V':
                {
                    if (! Editable())
                        return true;
                    const auto lifetime  = GetLifetimeToken();
                    const auto revision  = Revision();
                    const auto binding   = _state->bindingRevision;
                    const auto selection = GetSelection();
                    const auto text      = host.ReadTextFromClipboard();
                    if (text && ! lifetime.expired() && GetHost() == &host && _state->bindingRevision == binding && Revision() == revision &&
                        GetSelection() == selection)
                        static_cast<void>(ReplaceSelection(*text));
                    return true;
                }
                case 'Z': static_cast<void>(shift ? Redo() : Undo()); return true;
                case 'Y': static_cast<void>(Redo()); return true;
                case 'B': static_cast<void>(ToggleFormatting(L"**")); return true;
                case 'I': static_cast<void>(ToggleFormatting(L"*")); return true;
                default: break;
            }
        }
        switch (key)
        {
            case VK_BACK:
            case VK_DELETE: static_cast<void>(Erase(host, key == VK_BACK)); return true;
            case VK_LEFT:
            case VK_RIGHT:
            case VK_UP:
            case VK_DOWN:
            case VK_HOME:
            case VK_END:
            case VK_PRIOR:
            case VK_NEXT: MoveCaret(host, key, shift, control); return true;
            case VK_ESCAPE:
                if (! _state->dragging)
                    return false;
                _state->dragging = false;
                host.ReleaseMouseCapture();
                return true;
            case VK_RETURN: return Editable(); // WM_CHAR / DispatchCharacter performs the one insertion.
            default: return false;
        }
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnCopy(ControlHost& host) noexcept
{
    try
    {
        if (! IsEnabled())
            return false;
        const auto selection = GetSelection();
        if (selection.Start() == selection.End())
            return false;
        // Copy exact Markdown source, including delimiters and original EOLs.
        const auto text = Snapshot().Slice(selection.Start(), selection.End());
        return host.CopyTextToClipboard(text);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnSelectAll(ControlHost&) noexcept
{
    try
    {
        if (! IsEnabled())
            return false;
        SetSelection({0, _state->document.Length()});
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers) noexcept
{
    try
    {
        if (! IsEnabled() || rightButton)
            return false;
        const auto lifetime = GetLifetimeToken();
        const auto geometry = host.GetInteractionRevision();
        host.SetFocusControl(this);
        if (lifetime.expired() || GetHost() != &host || host.GetInteractionRevision() != geometry || ! IsEnabled() || ! IsVisible() ||
            host.GetFocusControl() != this)
            return true;
        const auto source = HitSource(host, point);
        _state->dragging  = true;
        _state->document.SetSelection({ModifiersContainShift(modifiers) ? GetSelection().anchor : source, source});
        host.CaptureMouse(this);
        if (! lifetime.expired() && GetHost() == &host)
            SelectionChanged();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT) noexcept
{
    try
    {
        if (! IsEnabled() || rightButton)
            return false;
        const auto source      = HitSource(host, point);
        const auto& projection = _state->layout.Content();
        size_t caret           = projection.ToDisplay(source);
        std::optional<size_t> anchor;
        SelectSingleLineWordAt(projection.text, caret, caret, anchor);
        _state->dragging = false;
        SetSelection({projection.ToSource(anchor.value_or(caret)), projection.ToSource(caret, true)});
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT) noexcept
{
    try
    {
        if (! _state->dragging || ! IsEnabled())
            return false;
        const auto viewport = TextViewport();
        if (point.y < viewport.top || point.y > viewport.bottom)
        {
            _state->scrollDip   = std::max(0.0f, _state->scrollDip + (point.y < viewport.top ? -20.0f : 20.0f));
            _state->revealCaret = false;
        }
        const auto source = HitSource(host, point);
        SetSelection({GetSelection().anchor, source});
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
bool RichTextEditor::OnMouseUp(ControlHost& host, D2D1_POINT_2F, bool rightButton, UINT) noexcept
{
    if (rightButton || ! _state->dragging)
        return false;
    _state->dragging = false;
    host.ReleaseMouseCapture();
    return true;
}
bool RichTextEditor::OnMouseWheel(ControlHost& host, D2D1_POINT_2F, float delta, UINT) noexcept
{
    try
    {
        if (! IsEnabled() || ! std::isfinite(delta))
            return false;
        EnsureLayout(host);
        const auto viewport = TextViewport();
        _state->scrollDip =
            std::clamp(_state->scrollDip - delta / WHEEL_DELTA * 48.0f, 0.0f, std::max(0.0f, _state->layout.Height() - (viewport.bottom - viewport.top)));
        _state->revealCaret = false;
        RequestInvalidate();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
void RichTextEditor::PropagateHost(ControlHost* host) noexcept
{
    if (GetHost() != host)
    {
        ++_state->bindingRevision;
        RetireLayout();
        _state->pendingLeadSurrogate = 0;
        _state->dragging             = false;
        _state->blinkEpoch.reset();
    }
    Control::PropagateHost(host);
}
void RichTextEditor::OnBoundsChanged() noexcept
{
    const auto viewport = TextViewport();
    if (_state->layoutWidth != std::max(1.0f, viewport.right - viewport.left))
        RetireLayout();
    else
        _state->revealCaret = true;
}
void RichTextEditor::OnHostDpiChanged(ControlHost&) noexcept
{
    RetireLayout();
}
void RichTextEditor::OnFlowDirectionChanged() noexcept
{
    RetireLayout();
}
void RichTextEditor::OnDensityChanged() noexcept
{
    RetireLayout();
}
void RichTextEditor::OnEnabledChanged(bool) noexcept
{
    _state->pendingLeadSurrogate = 0;
    _state->dragging             = false;
    _state->blinkEpoch.reset();
}
void RichTextEditor::OnHidden() noexcept
{
    RetireLayout();
    _state->pendingLeadSurrogate = 0;
    _state->dragging             = false;
    _state->blinkEpoch.reset();
}
void RichTextEditor::OnFocusChanged(ControlHost& host, bool focused)
{
    Control::OnFocusChanged(host, focused);
    _state->dragging = false;
    SelectionChanged();
}
void RichTextEditor::OnCaptureLost(ControlHost&)
{
    _state->dragging = false;
}
} // namespace DxUi
