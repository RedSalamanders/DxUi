#pragma once
// Private implementation included inside DxUi.Accessibility.cpp's anonymous
// namespace, after the common provider/identity helpers. Semantic fragments use
// the existing host snapshot and dispatch; they own no control, HWND or worker.

[[nodiscard]] std::shared_ptr<const AccessibilitySnapshot> GuardedElementSnapshot(WindowHostAccessibilityTarget*,
                                                                                  HWND,
                                                                                  const std::shared_ptr<const AccessibilitySnapshot>&,
                                                                                  const ControlPath*,
                                                                                  const AccessibilityControlIdentity&) noexcept;
HRESULT DispatchAccessibilityUiActionToWindowThread(HWND, AccessibilityUiActionRequest&) noexcept;

[[nodiscard]] std::optional<D2D1_RECT_F> SemanticFragmentBounds(const AccessibilitySnapshot& snapshot,
                                                                const AccessibilityControlNavigationSnapshot& record,
                                                                size_t element) noexcept
{
    auto bounds     = record.controlRichText->bounds[element];
    const auto clip = FindSnapshotFragmentBounds(snapshot, AccessibilityFragmentKind::Control, record.path, 0u, 0u, 0u);
    if (! bounds || ! clip)
        return std::nullopt;
    bounds->left   = std::max(bounds->left, clip->left);
    bounds->top    = std::max(bounds->top, clip->top);
    bounds->right  = std::min(bounds->right, clip->right);
    bounds->bottom = std::min(bounds->bottom, clip->bottom);
    return bounds->left < bounds->right && bounds->top < bounds->bottom ? bounds : std::nullopt;
}

[[nodiscard]] size_t EnclosingSemanticElement(const RichText::SemanticDocument& document, TextRangeSpan range) noexcept
{
    const auto elements = document.Elements();
    if (range.start == 0u && range.end == document.Map().Source().size())
        return 0u; // DocumentRange always encloses the text provider itself.
    size_t enclosing = 0u;
    for (size_t index = 1u; index < elements.size(); ++index)
    {
        const auto& element = elements[index];
        if (element.source.first > range.start || element.source.last < range.end ||
            (range.start == range.end && range.start == element.source.last && element.source.first != element.source.last))
            continue;
        // Depth-first semantic order gives the innermost descendant, without
        // allowing an unrelated padded sibling at the same ACP to replace it.
        size_t parent = element.parent;
        while (parent != RichText::MarkdownNoNode && parent != enclosing)
            parent = elements[parent].parent;
        if (parent == enclosing)
            enclosing = index;
    }
    return enclosing;
}

class RichSemanticElementProvider final : public IRawElementProviderSimple,
                                          public IRawElementProviderFragment,
                                          public IToggleProvider,
                                          public IGridProvider,
                                          public ITableProvider,
                                          public IGridItemProvider,
                                          public ITableItemProvider
{
public:
    RichSemanticElementProvider(WindowHostAccessibilityTarget* target, HWND hwnd, const AccessibilityControlNavigationSnapshot& record, size_t element) noexcept
        : target_(target),
          hwnd_(hwnd),
          path_(record.path),
          identity_(CaptureControlIdentity(record)),
          document_(record.RichSemantic()),
          publication_(record.RichSemantic()->Identity()),
          binding_(record.RichBinding()),
          element_(element)
    {
        static_cast<void>(target_->AddRef());
    }
    static wil::com_ptr_nothrow<IRawElementProviderSimple> Create(WindowHostAccessibilityTarget* target,
                                                                  HWND hwnd,
                                                                  const AccessibilityControlNavigationSnapshot& record,
                                                                  size_t element) noexcept
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> result;
        HoldAccessibilityPeerCreationForTest(hwnd);
        if (! target || ! record.RichSemantic() || element == 0u || element >= record.RichSemantic()->Elements().size())
            return result;
        auto* provider = new (std::nothrow) RichSemanticElementProvider(target, hwnd, record, element);
        if (provider)
        {
            result.attach(provider);
            if (! provider->CaptureSnapshot())
                result.reset();
        }
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
        else if (id == __uuidof(IToggleProvider))
            *result = static_cast<IToggleProvider*>(this);
        else if (id == __uuidof(IGridProvider))
            *result = static_cast<IGridProvider*>(this);
        else if (id == __uuidof(ITableProvider))
            *result = static_cast<ITableProvider*>(this);
        else if (id == __uuidof(IGridItemProvider))
            *result = static_cast<IGridItemProvider*>(this);
        else if (id == __uuidof(ITableItemProvider))
            *result = static_cast<ITableItemProvider*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return references_.fetch_add(1u, std::memory_order_relaxed) + 1u;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        const auto remaining = references_.fetch_sub(1u, std::memory_order_acq_rel) - 1u;
        if (! remaining)
        {
            static_cast<void>(target_->Release());
            delete this;
        }
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result =
            target_->embedded
                ? static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading | ProviderOptions_ProviderOwnsSetFocus)
                : ProviderOptions_ServerSideProvider;
        return CaptureSnapshot() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        return CaptureSnapshot() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID id, IUnknown** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto role = record->RichSemantic()->Elements()[element_].role;
        if (id == UIA_TogglePatternId && role == RichText::SemanticRole::Task)
            *result = static_cast<IToggleProvider*>(this);
        else if (id == UIA_GridPatternId && role == RichText::SemanticRole::Table)
            *result = static_cast<IGridProvider*>(this);
        else if (id == UIA_TablePatternId && role == RichText::SemanticRole::Table)
            *result = static_cast<ITableProvider*>(this);
        else if (id == UIA_GridItemPatternId && role == RichText::SemanticRole::Cell)
            *result = static_cast<IGridItemProvider*>(this);
        else if (id == UIA_TableItemPatternId && role == RichText::SemanticRole::Cell)
            *result = static_cast<ITableItemProvider*>(this);
        if (*result)
            AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id, VARIANT* result) noexcept override
    {
        if (! result)
            return E_POINTER;
        VariantInit(result);
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& document = *record->RichSemantic();
        const auto& element  = document.Elements()[element_];
        using enum RichText::SemanticRole;
        switch (id)
        {
            case UIA_ControlTypePropertyId:
            {
                const auto type = element.role == Task                          ? UIA_CheckBoxControlTypeId
                                  : element.role == Link                        ? UIA_HyperlinkControlTypeId
                                  : element.role == Image                       ? UIA_ImageControlTypeId
                                  : element.role == Table                       ? UIA_TableControlTypeId
                                  : element.role == List                        ? UIA_ListControlTypeId
                                  : element.role == ListItem                    ? UIA_ListItemControlTypeId
                                  : element.role == Row                         ? UIA_DataItemControlTypeId
                                  : element.role == Cell && element.tableHeader ? UIA_HeaderItemControlTypeId
                                  : element.role == Quote                       ? UIA_GroupControlTypeId
                                  : element.role == Rule                        ? UIA_SeparatorControlTypeId
                                                                                : UIA_TextControlTypeId;
                *result         = VariantFromInt(type);
                return S_OK;
            }
            case UIA_NamePropertyId:
            {
                const auto& named = element.role == Task && element.parent != RichText::MarkdownNoNode ? document.Elements()[element.parent] : element;
                const auto name   = ! named.name.empty() ? std::wstring_view(named.name)
                                                         : document.Map().Display().substr(named.display.first, named.display.last - named.display.first);
                return SetVariantFromString(result, name);
            }
            case UIA_HelpTextPropertyId: return SetVariantFromString(result, element.title.empty() ? element.destination : element.title);
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId: *result = VariantFromBool(true); return S_OK;
            case UIA_IsKeyboardFocusablePropertyId:
            case UIA_HasKeyboardFocusPropertyId: *result = VariantFromBool(false); return S_OK;
            case UIA_IsEnabledPropertyId: *result = VariantFromBool(record->controlEnabled); return S_OK;
            case UIA_IsOffscreenPropertyId: *result = VariantFromBool(! SemanticFragmentBounds(*snapshot, *record, element_)); return S_OK;
            case UIA_HeadingLevelPropertyId:
                *result = VariantFromInt(element.heading ? HeadingLevel1 + static_cast<int>(element.heading - 1u) : HeadingLevel_None);
                return S_OK;
            case UIA_ToggleToggleStatePropertyId:
                if (element.role == Task)
                    *result = VariantFromInt(element.task == RichText::MarkdownTask::Checked ? ToggleState_On : ToggleState_Off);
                return S_OK;
            default: return S_OK;
        }
    }
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction, IRawElementProviderFragment** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto elements = record->RichSemantic()->Elements();
        const auto& element = elements[element_];
        size_t target       = RichText::MarkdownNoNode;
        switch (direction)
        {
            case NavigateDirection_Parent: target = element.parent; break;
            case NavigateDirection_FirstChild: target = element.firstChild; break;
            case NavigateDirection_LastChild:
                for (auto child = element.firstChild; child != RichText::MarkdownNoNode; child = elements[child].nextSibling)
                    target = child;
                break;
            case NavigateDirection_NextSibling: target = element.nextSibling; break;
            case NavigateDirection_PreviousSibling:
                if (element.parent != RichText::MarkdownNoNode)
                    for (auto child = elements[element.parent].firstChild; child != element_ && child != RichText::MarkdownNoNode;
                         child      = elements[child].nextSibling)
                        target = child;
                break;
            default: return E_INVALIDARG;
        }
        if (target == RichText::MarkdownNoNode)
            return S_OK;
        auto peer = target == 0u ? CreateControlElement(target_, hwnd_, path_, identity_) : Create(target_, hwnd_, *record, target);
        return peer ? peer.query_to(result) : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result = nullptr;
        if (! CaptureSnapshot())
            return UIA_E_ELEMENTNOTAVAILABLE;
        RuntimeIdValueBuffer values{};
        size_t count = 0u;
        if (! (AppendRuntimeIdValue(values, count, UiaAppendRuntimeId) && AppendRuntimeIdValue(values, count, 1006) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(target_->runtimeId)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(target_->runtimeId >> 32u)) && AppendControlPathRuntimeIdPrefix(values, count, path_) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(identity_.value)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(identity_.value >> 32u)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(publication_)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(publication_ >> 32u)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(binding_)) && AppendRuntimeIdValue(values, count, static_cast<LONG>(binding_ >> 32u)) &&
               AppendRuntimeIdValue(values, count, static_cast<LONG>(element_))))
            return E_INVALIDARG;
        return BuildRuntimeId(result, std::span(values.data(), count));
    }
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = {};
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (const auto bounds = SemanticFragmentBounds(*snapshot, *record, element_))
        {
            if (snapshot->embedded)
                *result = EmbeddedScreenBounds(*snapshot, *bounds);
            else
            {
                const double scale = 1.0 / snapshot->pixelsToDipScale;
                POINT first{static_cast<LONG>(std::lround(bounds->left * scale)), static_cast<LONG>(std::lround(bounds->top * scale))};
                POINT last{static_cast<LONG>(std::lround(bounds->right * scale)), static_cast<LONG>(std::lround(bounds->bottom * scale))};
                if (ClientToScreen(hwnd_, &first) && ClientToScreen(hwnd_, &last))
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
        if (! result)
            return E_POINTER;
        *result = nullptr;
        return CaptureSnapshot() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        if (! snapshot)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (target_->embedded)
        {
            if (target_->threadId != GetCurrentThreadId())
                return RPC_E_WRONG_THREAD;
            const auto site = target_->site;
            return site ? site->FragmentRoot(result) : UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto root = AcquireCanonicalRootProvider(target_);
        return root ? root.query_to(result) : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE SetFocus() noexcept override
    {
        return CaptureSnapshot() ? UIA_E_NOTSUPPORTED : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE Toggle() noexcept override
    try
    {
        const auto snapshot = CaptureSnapshot();
        if (! snapshot)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (GetCurrentThreadId() != (target_->embedded ? target_->threadId : snapshot->windowThreadId))
        {
            if (target_->embedded)
                return RPC_E_WRONG_THREAD;
            AccessibilityUiActionRequest request{.stringValue = std::wstring(0u, L' '), .textRangeBoundsDip = std::vector<D2D1_RECT_F>(0u)};
            request.semanticProvider = this;
            request.kind             = AccessibilityUiActionKind::Toggle;
            return DispatchAccessibilityUiActionToWindowThread(hwnd_, request);
        }
        const auto* record = Record(snapshot);
        if (! record || record->RichSemantic()->Elements()[element_].role != RichText::SemanticRole::Task)
            return UIA_E_NOTSUPPORTED;
        if (! record->controlEnabled)
            return UIA_E_ELEMENTNOTENABLED;
        if (record->controlValueReadOnly)
            return UIA_E_INVALIDOPERATION;
        auto* host   = target_->ResolveHost();
        auto* editor = host ? dynamic_cast<RichTextEditor*>(ResolveIdentifiedControlAtPath(host->GetRoot(), path_, identity_)) : nullptr;
        if (! editor || ! IsControlPathVisible(host->GetRoot(), path_))
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (! IsControlEffectivelyInteractive(host->GetRoot(), editor))
            return UIA_E_ELEMENTNOTENABLED;
        if (editor->IsReadOnly())
            return UIA_E_INVALIDOPERATION;
        // Do not touch editor after this call: its committed notification
        // may destroy/rebind the owner. The snapshot keeps only data alive.
        return editor->ToggleSemanticTask(*host, record->RichSemantic(), element_) ? S_OK : UIA_E_INVALIDOPERATION;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (const std::exception&)
    {
        return E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = ToggleState_Off;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& element = record->RichSemantic()->Elements()[element_];
        if (element.role != RichText::SemanticRole::Task)
            return UIA_E_NOTSUPPORTED;
        *result = element.task == RichText::MarkdownTask::Checked ? ToggleState_On : ToggleState_Off;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetItem(int row, int column, IRawElementProviderSimple** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto elements = record->RichSemantic()->Elements();
        if (elements[element_].role != RichText::SemanticRole::Table)
            return UIA_E_NOTSUPPORTED;
        if (row < 0 || column < 0 || static_cast<size_t>(row) >= elements[element_].rows || static_cast<size_t>(column) >= elements[element_].columns)
            return E_INVALIDARG;
        auto index = elements[element_].firstChild;
        while (row-- > 0 && index != RichText::MarkdownNoNode)
            index = elements[index].nextSibling;
        if (index == RichText::MarkdownNoNode)
            return E_UNEXPECTED;
        index = elements[index].firstChild;
        while (column-- > 0 && index != RichText::MarkdownNoNode)
            index = elements[index].nextSibling;
        auto cell = Create(target_, hwnd_, *record, index);
        if (! cell)
            return E_OUTOFMEMORY;
        *result = cell.detach();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_RowCount(int* result) noexcept override
    {
        return TableCount(true, result);
    }
    HRESULT STDMETHODCALLTYPE get_ColumnCount(int* result) noexcept override
    {
        return TableCount(false, result);
    }
    HRESULT STDMETHODCALLTYPE GetRowHeaders(SAFEARRAY** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (record->RichSemantic()->Elements()[element_].role != RichText::SemanticRole::Table)
            return UIA_E_NOTSUPPORTED;
        return SetProviderArray(result, {});
    }
    HRESULT STDMETHODCALLTYPE GetColumnHeaders(SAFEARRAY** result) noexcept override
    {
        return Headers(false, result);
    }
    HRESULT STDMETHODCALLTYPE get_RowOrColumnMajor(RowOrColumnMajor* result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = RowOrColumnMajor_RowMajor;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        return record->RichSemantic()->Elements()[element_].role == RichText::SemanticRole::Table ? S_OK : UIA_E_NOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE get_Row(int* result) noexcept override
    {
        return CellCoordinate(true, result);
    }
    HRESULT STDMETHODCALLTYPE get_Column(int* result) noexcept override
    {
        return CellCoordinate(false, result);
    }
    HRESULT STDMETHODCALLTYPE get_RowSpan(int* result) noexcept override
    {
        return CellSpan(result);
    }
    HRESULT STDMETHODCALLTYPE get_ColumnSpan(int* result) noexcept override
    {
        return CellSpan(result);
    }
    HRESULT STDMETHODCALLTYPE get_ContainingGrid(IRawElementProviderSimple** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (record->RichSemantic()->Elements()[element_].role != RichText::SemanticRole::Cell)
            return UIA_E_NOTSUPPORTED;
        const auto index = TableAncestor(*record);
        if (index == RichText::MarkdownNoNode)
            return UIA_E_NOTSUPPORTED;
        auto table = Create(target_, hwnd_, *record, index);
        if (! table)
            return E_OUTOFMEMORY;
        *result = table.detach();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRowHeaderItems(SAFEARRAY** result) noexcept override
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        if (record->RichSemantic()->Elements()[element_].role != RichText::SemanticRole::Cell)
            return UIA_E_NOTSUPPORTED;
        return SetProviderArray(result, {});
    }
    HRESULT STDMETHODCALLTYPE GetColumnHeaderItems(SAFEARRAY** result) noexcept override
    {
        return Headers(true, result);
    }
    [[nodiscard]] bool Matches(const AccessibilityControlNavigationSnapshot& record) const noexcept
    {
        const auto document = document_.lock();
        return document && document == record.RichSemantic() && binding_ == record.RichBinding() &&
               SameControlIdentity(identity_, CaptureControlIdentity(record)) && AreControlPathsEqual(path_, record.path);
    }
    [[nodiscard]] size_t Element() const noexcept
    {
        return element_;
    }

private:
    [[nodiscard]] std::shared_ptr<const AccessibilitySnapshot> CaptureSnapshot() const noexcept
    {
        const auto snapshot = GuardedElementSnapshot(target_, hwnd_, {}, &path_, identity_);
        const auto* record  = snapshot ? FindControlNavigationRecord(*snapshot, path_) : nullptr;
        return record && Matches(*record) && element_ < record->controlRichText->bounds.size() ? snapshot : nullptr;
    }
    [[nodiscard]] const AccessibilityControlNavigationSnapshot* Record(const std::shared_ptr<const AccessibilitySnapshot>& snapshot) const noexcept
    {
        return snapshot ? FindControlNavigationRecord(*snapshot, path_) : nullptr;
    }
    [[nodiscard]] size_t TableAncestor(const AccessibilityControlNavigationSnapshot& record) const noexcept
    {
        const auto elements = record.RichSemantic()->Elements();
        for (auto parent = element_; parent != RichText::MarkdownNoNode; parent = elements[parent].parent)
            if (elements[parent].role == RichText::SemanticRole::Table)
                return parent;
        return RichText::MarkdownNoNode;
    }
    HRESULT TableCount(bool rows, int* result) const noexcept
    {
        if (! result)
            return E_POINTER;
        *result             = 0;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto& element = record->RichSemantic()->Elements()[element_];
        if (element.role != RichText::SemanticRole::Table)
            return UIA_E_NOTSUPPORTED;
        *result = static_cast<int>(rows ? element.rows : element.columns);
        return S_OK;
    }
    HRESULT CellCoordinate(bool row, int* result) const noexcept
    {
        if (! result)
            return E_POINTER;
        *result             = 0;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto elements = record->RichSemantic()->Elements();
        if (elements[element_].role != RichText::SemanticRole::Cell)
            return UIA_E_NOTSUPPORTED;
        const auto index  = row ? elements[element_].parent : element_;
        const auto parent = elements[index].parent;
        for (auto child = elements[parent].firstChild; child != index && child != RichText::MarkdownNoNode; child = elements[child].nextSibling)
            ++*result;
        return S_OK;
    }
    HRESULT CellSpan(int* result) const noexcept
    {
        int coordinate = 0;
        const auto hr  = CellCoordinate(true, &coordinate);
        if (! result)
            return E_POINTER;
        *result = SUCCEEDED(hr) ? 1 : 0;
        return hr;
    }
    HRESULT Headers(bool forCell, SAFEARRAY** result) noexcept
    {
        if (! result)
            return E_POINTER;
        *result             = nullptr;
        const auto snapshot = CaptureSnapshot();
        const auto* record  = Record(snapshot);
        if (! record)
            return UIA_E_ELEMENTNOTAVAILABLE;
        const auto elements = record->RichSemantic()->Elements();
        if (elements[element_].role != (forCell ? RichText::SemanticRole::Cell : RichText::SemanticRole::Table))
            return UIA_E_NOTSUPPORTED;
        const auto table = TableAncestor(*record);
        if (table == RichText::MarkdownNoNode)
            return UIA_E_NOTSUPPORTED;
        const auto row = elements[table].firstChild;
        int column     = 0;
        if (forCell)
        {
            const auto hr = CellCoordinate(false, &column);
            if (FAILED(hr))
                return hr;
        }
        try
        {
            std::vector<wil::com_ptr_nothrow<IRawElementProviderSimple>> owners(0u);
            std::vector<IRawElementProviderSimple*> raw(0u);
            int ordinal = 0;
            if (row != RichText::MarkdownNoNode)
                for (auto child = elements[row].firstChild; child != RichText::MarkdownNoNode; child = elements[child].nextSibling, ++ordinal)
                    if (elements[child].tableHeader && (! forCell || ordinal == column))
                    {
                        auto peer = Create(target_, hwnd_, *record, child);
                        if (! peer)
                            return E_OUTOFMEMORY;
                        raw.push_back(peer.get());
                        owners.push_back(std::move(peer));
                    }
            return SetProviderArray(result, raw);
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }
    std::atomic<ULONG> references_{1u};
    WindowHostAccessibilityTarget* target_;
    HWND hwnd_;
    ControlPath path_;
    AccessibilityControlIdentity identity_;
    std::weak_ptr<const RichText::SemanticDocument> document_;
    uint64_t publication_, binding_;
    size_t element_;
};
