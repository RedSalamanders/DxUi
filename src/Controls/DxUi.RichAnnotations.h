#pragma once
// Included inside the accessibility implementation after the semantic peers.
// Annotation peers are separate from TextPattern's embedded-object children.
[[nodiscard]] int AnnotationType(RichTextAnnotationKind kind) noexcept
{
    switch (kind)
    {
        case RichTextAnnotationKind::Highlight: return AnnotationType_Highlighted;
        case RichTextAnnotationKind::SpellingError: return AnnotationType_SpellingError;
        case RichTextAnnotationKind::GrammarError: return AnnotationType_GrammarError;
        case RichTextAnnotationKind::None:
        case RichTextAnnotationKind::Generic:
        default: return AnnotationType_Unknown;
    }
}
[[nodiscard]] std::wstring_view AnnotationName(const AccessibilityControlNavigationSnapshot& record, RichTextAnnotationKind kind) noexcept
{
    using enum RichText::AccessibilityTextId;
    const auto id = kind == RichTextAnnotationKind::Highlight       ? HighlightAnnotation
                    : kind == RichTextAnnotationKind::SpellingError ? SpellingAnnotation
                    : kind == RichTextAnnotationKind::GrammarError  ? GrammarAnnotation
                                                                    : Annotation;
    return RichText::AccessibilityLabels::Text(record.RichSemantic()->AccessibilityLabelSet().get(), id);
}
class RichAnnotationProvider final : public IRawElementProviderSimple, public IRawElementProviderFragment, public IAnnotationProvider
{
    Failure::ObjectLifetime _failureLifetime;

public:
    RichAnnotationProvider(WindowHostAccessibilityTarget* target, HWND hwnd, const AccessibilityControlNavigationSnapshot& record, size_t index) noexcept
        : _target(target),
          _hwnd(hwnd),
          _path(record.path),
          _identity(CaptureControlIdentity(record)),
          _document(record.RichSemantic()),
          _publication(record.RichSemantic()->Identity()),
          _revision(record.RichSemanticRevision()),
          _index(index)
    {
    }
    RichAnnotationProvider(const RichAnnotationProvider&)            = delete;
    RichAnnotationProvider& operator=(const RichAnnotationProvider&) = delete;
    static wil::com_ptr_nothrow<IRawElementProviderSimple> Create(WindowHostAccessibilityTarget* target,
                                                                  HWND hwnd,
                                                                  const AccessibilityControlNavigationSnapshot& record,
                                                                  size_t index) noexcept
    {
        const Failure::Scope outerFailureScope;
        wil::com_ptr_nothrow<IRawElementProviderSimple> result;
        HoldAccessibilityPeerCreationForTest(hwnd);
        if (! target || ! record.HasCurrentRichSemantic() || index >= record.controlRichText->annotations.size())
            return result;
        result.attach(new (std::nothrow) RichAnnotationProvider(target, hwnd, record, index));
        if (result && ! static_cast<RichAnnotationProvider*>(result.get())->CaptureSnapshot())
            result.reset();
        return result;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (id == __uuidof(IUnknown) || id == __uuidof(IRawElementProviderSimple))
            *result = static_cast<IRawElementProviderSimple*>(this);
        else if (id == __uuidof(IRawElementProviderFragment))
            *result = static_cast<IRawElementProviderFragment*>(this);
        else if (id == __uuidof(IAnnotationProvider))
            *result = static_cast<IAnnotationProvider*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return _references.fetch_add(1u, std::memory_order_relaxed) + 1u;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto remaining = _references.fetch_sub(1u, std::memory_order_acq_rel) - 1u;
        if (! remaining)
        {
            const Failure::Scope retirementScope{std::nothrow};
            delete this;
        }
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result =
            _target->embedded
                ? static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading | ProviderOptions_ProviderOwnsSetFocus)
                : ProviderOptions_ServerSideProvider;
        return CaptureSnapshot() ? S_OK : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result = nullptr;
        return CaptureSnapshot() ? S_OK : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID id, IUnknown** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (! CaptureSnapshot())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (id == UIA_AnnotationPatternId)
        {
            *result = static_cast<IAnnotationProvider*>(this);
            AddRef();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id, VARIANT* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        VariantInit(result);
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& annotation = record->controlRichText->annotations[_index];
        switch (id)
        {
            case UIA_ControlTypePropertyId: *result = VariantFromInt(UIA_TextControlTypeId); break;
            case UIA_NamePropertyId:
            case UIA_AnnotationAnnotationTypeNamePropertyId: return SetVariantFromString(result, AnnotationName(*record, annotation.kind));
            case UIA_AnnotationAnnotationTypeIdPropertyId: *result = VariantFromInt(AnnotationType(annotation.kind)); break;
            case UIA_IsAnnotationPatternAvailablePropertyId: *result = VariantFromBool(true); break;
            case UIA_AnnotationAuthorPropertyId:
            case UIA_AnnotationDateTimePropertyId: return SetVariantFromString(result, {});
            case UIA_AnnotationTargetPropertyId: result->vt = VT_UNKNOWN; return get_Target(reinterpret_cast<IRawElementProviderSimple**>(&result->punkVal));
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId:
            case UIA_IsKeyboardFocusablePropertyId:
            case UIA_HasKeyboardFocusPropertyId: *result = VariantFromBool(false); break;
            case UIA_IsEnabledPropertyId: *result = VariantFromBool(record->controlEnabled); break;
            case UIA_IsOffscreenPropertyId: *result = VariantFromBool(! Bounds(*snapshot, *record)); break;
            default: break;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction, IRawElementProviderFragment** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        wil::com_ptr_nothrow<IRawElementProviderSimple> peer;
        if (direction == NavigateDirection_Parent)
            peer = CreateControlElement(_target.get(), _hwnd, _path, _identity);
        else if (direction == NavigateDirection_NextSibling && _index + 1u < record->controlRichText->annotations.size())
            peer = Create(_target.get(), _hwnd, *record, _index + 1u);
        else if (direction == NavigateDirection_PreviousSibling)
        {
            if (_index)
                peer = Create(_target.get(), _hwnd, *record, _index - 1u);
            else
            {
                const auto elements = record->RichSemantic()->Elements();
                auto child          = elements[0].firstChild;
                if (child != RichText::MarkdownNoNode)
                {
                    while (elements[child].nextSibling != RichText::MarkdownNoNode)
                        child = elements[child].nextSibling;
                    peer = RichSemanticElementProvider::Create(_target.get(), _hwnd, *record, child);
                }
            }
        }
        else if (direction != NavigateDirection_NextSibling && direction != NavigateDirection_FirstChild && direction != NavigateDirection_LastChild)
            return E_INVALIDARG;
        return peer ? peer.query_to(result) : S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (! CaptureSnapshot())
            return UIA_E_ELEMENTNOTAVAILABLE;
        RuntimeIdValueBuffer values{};
        size_t count{};
        if (! (AppendRuntimeIdValue(values, count, UiaAppendRuntimeId) && AppendRuntimeIdValue(values, count, 1008) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_target->runtimeId)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_target->runtimeId >> 32u)) && AppendControlPathRuntimeIdPrefix(values, count, _path) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_identity.value)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_identity.value >> 32u)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_publication)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_publication >> 32u)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_revision)) && AppendRuntimeIdValue(values, count, static_cast<LONG>(_revision >> 32u)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(_index))))
            return E_INVALIDARG;
        return BuildRuntimeId(result, std::span(values.data(), count));
    }
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result             = {};
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (const auto bounds = Bounds(*snapshot, *record))
        {
            if (snapshot->embedded)
                *result = EmbeddedScreenBounds(*snapshot, bounds.value());
            else
            {
                const double scale = 1.0 / snapshot->pixelsToDipScale;
                POINT first{static_cast<LONG>(std::lround(bounds->left * scale)), static_cast<LONG>(std::lround(bounds->top * scale))};
                POINT last{static_cast<LONG>(std::lround(bounds->right * scale)), static_cast<LONG>(std::lround(bounds->bottom * scale))};
                if (ClientToScreen(_hwnd, &first) && ClientToScreen(_hwnd, &last))
                    *result = {static_cast<double>(first.x),
                               static_cast<double>(first.y),
                               static_cast<double>(last.x - first.x),
                               static_cast<double>(last.y - first.y)};
            }
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result = nullptr;
        return CaptureSnapshot() ? S_OK : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (! CaptureSnapshot())
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (_target->embedded)
        {
            if (_target->threadId != GetCurrentThreadId())
                return RPC_E_WRONG_THREAD;
            const auto site = _target->site;
            return site ? site->FragmentRoot(result) : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
        }
        auto root = AcquireCanonicalRootProvider(_target.get());
        return root ? root.query_to(result) : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
    HRESULT STDMETHODCALLTYPE SetFocus() noexcept override
    {
        const Failure::Scope outerFailureScope;
        return CaptureSnapshot() ? static_cast<HRESULT>(UIA_E_NOTSUPPORTED) : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
    HRESULT STDMETHODCALLTYPE get_AnnotationTypeId(int* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result             = AnnotationType_Unknown;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = AnnotationType(record->controlRichText->annotations[_index].kind);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_AnnotationTypeName(BSTR* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto text = AnnotationName(*record, record->controlRichText->annotations[_index].kind);
        *result         = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
        return *result ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT STDMETHODCALLTYPE get_Author(BSTR* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        return EmptyText(result);
    }
    HRESULT STDMETHODCALLTYPE get_DateTime(BSTR* result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        return EmptyText(result);
    }
    HRESULT STDMETHODCALLTYPE get_Target(IRawElementProviderSimple** result) noexcept override
    {
        const Failure::Scope outerFailureScope;
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (! CaptureSnapshot())
            return UIA_E_ELEMENTNOTAVAILABLE;
        auto peer = CreateControlElement(_target.get(), _hwnd, _path, _identity);
        *result   = peer.detach();
        return *result ? S_OK : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
    [[nodiscard]] bool Matches(const AccessibilityControlNavigationSnapshot& record) const noexcept
    {
        const auto document = _document.lock();
        return record.HasCurrentRichSemantic() && document && document == record.RichSemantic() && _revision == record.RichSemanticRevision() &&
               SameControlIdentity(_identity, CaptureControlIdentity(record)) && AreControlPathsEqual(_path, record.path) &&
               _index < record.controlRichText->annotations.size();
    }
    [[nodiscard]] RichText::MarkdownRange Source(const AccessibilityControlNavigationSnapshot& record) const noexcept
    {
        return record.controlRichText->annotations[_index].source;
    }
    [[nodiscard]] bool BelongsTo(WindowHostAccessibilityTarget* target, HWND hwnd, const AccessibilityControlNavigationSnapshot& record) const noexcept
    {
        return _target.get() == target && _hwnd == hwnd && Matches(record);
    }

private:
    HRESULT EmptyText(BSTR* result) const noexcept
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (! CaptureSnapshot())
            return UIA_E_ELEMENTNOTAVAILABLE;
        *result = SysAllocStringLen(nullptr, 0u);
        return *result ? S_OK : E_OUTOFMEMORY;
    }
    std::shared_ptr<const AccessibilitySnapshot> CaptureSnapshot() const noexcept
    {
        const auto snapshot = GuardedElementSnapshot(_target.get(), _hwnd, {}, &_path, _identity);
        const auto* record  = Record(snapshot);
        return record && Matches(*record) ? snapshot : nullptr;
    }
    const AccessibilityControlNavigationSnapshot* Record(const std::shared_ptr<const AccessibilitySnapshot>& snapshot) const noexcept
    {
        return snapshot ? FindControlNavigationRecord(*snapshot, _path) : nullptr;
    }
    std::optional<D2D1_RECT_F> Bounds(const AccessibilitySnapshot& snapshot, const AccessibilityControlNavigationSnapshot& record) const noexcept
    {
        auto bounds     = record.controlRichText->annotations[_index].bounds;
        const auto clip = FindSnapshotFragmentBounds(snapshot, AccessibilityFragmentKind::Control, _path, 0u, 0u, 0u);
        if (! bounds || ! clip)
            return std::nullopt;
        bounds->left   = std::max(bounds->left, clip->left);
        bounds->top    = std::max(bounds->top, clip->top);
        bounds->right  = std::min(bounds->right, clip->right);
        bounds->bottom = std::min(bounds->bottom, clip->bottom);
        return bounds->left < bounds->right && bounds->top < bounds->bottom ? bounds : std::nullopt;
    }
    std::atomic<ULONG> _references{1u};
    wil::com_ptr_nothrow<WindowHostAccessibilityTarget> _target;
    HWND _hwnd{};
    ControlPath _path;
    AccessibilityControlIdentity _identity;
    std::weak_ptr<const RichText::SemanticDocument> _document;
    uint64_t _publication{}, _revision{};
    size_t _index{};
};
wil::com_ptr_nothrow<IRawElementProviderSimple> CreateFirstAnnotation(WindowHostAccessibilityTarget* target,
                                                                      HWND hwnd,
                                                                      const AccessibilityControlNavigationSnapshot& record) noexcept
{
    return RichAnnotationProvider::Create(target, hwnd, record, 0u);
}
[[nodiscard]] bool VaryingAnnotations(const AccessibilityControlNavigationSnapshot& record, TextRangeSpan range) noexcept
{
    if (range.start == range.end)
        return false;
    for (const auto& annotation : record.controlRichText->annotations)
        if (annotation.source.first < range.end && annotation.source.last > range.start &&
            (annotation.source.first > range.start || annotation.source.last < range.end))
            return true;
    return false;
}
unsigned AnnotationMaskAt(const AccessibilityControlNavigationSnapshot& record, size_t position) noexcept
{
    const auto& runs = record.controlRichText->annotationTypes;
    const auto found = std::lower_bound(runs.begin(), runs.end(), position, [](const auto& run, auto offset) { return run.source.last <= offset; });
    return found == runs.end() ? 0u : found->mask;
}
size_t AnnotationTypesAt(const AccessibilityControlNavigationSnapshot& record, size_t position, std::array<LONG, 4u>& types) noexcept
{
    size_t count{};
    const auto mask = AnnotationMaskAt(record, position);
    for (unsigned index = 0u; index < 4u; ++index)
        if (mask & (1u << index))
        {
            types[count++] = AnnotationType(static_cast<RichTextAnnotationKind>(index + 1u));
        }
    std::sort(types.begin(), types.begin() + static_cast<ptrdiff_t>(count));
    return count;
}
HRESULT AnnotationAttribute(WindowHostAccessibilityTarget* target,
                            HWND hwnd,
                            const AccessibilityControlNavigationSnapshot& record,
                            TextRangeSpan range,
                            TEXTATTRIBUTEID id,
                            VARIANT* result) noexcept
{
    if (id != UIA_AnnotationTypesAttributeId && VaryingAnnotations(record, range))
    {
        result->vt = VT_UNKNOWN;
        return InvokeNativeUia(UiaGetReservedMixedAttributeValue, &result->punkVal);
    }
    std::array<LONG, 4u> types{};
    size_t count{};
    if (id == UIA_AnnotationTypesAttributeId)
    {
        count           = AnnotationTypesAt(record, range.start, types);
        const auto mask = AnnotationMaskAt(record, range.start);
        for (const auto& run : record.controlRichText->annotationTypes)
            if (run.source.first < range.end && run.source.last > range.start && run.mask != mask)
            {
                result->vt = VT_UNKNOWN;
                return InvokeNativeUia(UiaGetReservedMixedAttributeValue, &result->punkVal);
            }
        RETURN_IF_FAILED(BuildRuntimeId(&result->parray, std::span(types.data(), count)));
        result->vt = VT_ARRAY | VT_I4;
        return S_OK;
    }
    std::array<wil::com_ptr_nothrow<IRawElementProviderSimple>, 256u> owners{};
    std::array<IRawElementProviderSimple*, 256u> raw{};
    const auto& annotations = record.controlRichText->annotations;
    for (size_t index = 0u; index < annotations.size(); ++index)
        if (annotations[index].source.first <= range.start && range.start < annotations[index].source.last)
        {
            owners[count] = RichAnnotationProvider::Create(target, hwnd, record, index);
            if (! owners[count])
                return record.HasCurrentRichSemantic() ? E_OUTOFMEMORY : static_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
            raw[count] = owners[count].get();
            ++count;
        }
    RETURN_IF_FAILED(SetProviderArray(&result->parray, std::span(raw.data(), count)));
    result->vt = VT_ARRAY | VT_UNKNOWN;
    return S_OK;
}
