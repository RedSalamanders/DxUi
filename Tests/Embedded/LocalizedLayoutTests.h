#pragma once
#include <cmath>

// Independent retained scene: application policy is deliberately absent.
template <typename ConfigureAction, typename ArrangeActions>
static void TestLocalizedShortViewport(GraphicsFixture& gpu, ConfigureAction configureAction, ArrangeActions arrangeActions)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "localized scene attach");
    auto& view = scene.view;
    ComplexUiModel model;
    model.names[0]  = L"Présentation détaillée — 東京 — élément sélectionné";
    auto root       = std::make_unique<DxUi::Panel>();
    auto* body      = root->AddChild<DxUi::ScrollPanel>();
    auto* paragraph = body->AddChild<DxUi::Label>(L"Vérifiez attentivement les informations affichées avant de poursuivre. "
                                                  L"Les détails restent disponibles lorsque la fenêtre est réduite.");
    paragraph->SetMultiline(true);
    auto* value = body->AddChild<DxUi::Label>(L"DocumentationTrèsLongueSansSéparateur_東京_éèàç_0123456789012345678901234567890123456789");
    value->SetMultiline(true);
    auto* checkbox = body->AddChild<DxUi::Checkbox>(L"Appliquer ce choix aux éléments similaires");
    checkbox->SetChecked(true);
    auto* graph = body->AddChild<DxUi::ThroughputGraph>();
    const std::array<DxUi::ThroughputGraphSample, 3> samples{{{12.0}, {18.0}, {15.0}}};
    graph->SetSamples(samples);
    auto* grid = body->AddChild<DxUi::Grid>();
    grid->SetModel(&model);
    grid->GetSelectionModel().SetSingle(0);
    auto* detail = body->AddChild<DxUi::Label>(model.names[0]);
    detail->SetMultiline(true);
    constexpr std::array<std::wstring_view, 4> captions{
        L"Conserver les deux versions", L"Remplacer la version existante", L"Ignorer cet élément", L"Annuler cette opération"};
    std::array<DxUi::Button*, 4> actions{};
    for (size_t index = 0; index < actions.size(); ++index)
    {
        actions[index] = root->AddChild<DxUi::Button>(std::wstring(captions[index]));
        configureAction(*actions[index]);
    }
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(480, 240, 96), "initialize localized typography");
    view.Controls().SetFocusControl(actions.back());
    for (const float dpi : {96.0f, 144.0f, 192.0f, 96.0f})
    {
        for (const float width : {480.0f, 640.0f, 760.0f})
        {
            constexpr float height = 240.0f;
            const UINT widthPx     = static_cast<UINT>(width * dpi / 96.0f);
            const UINT heightPx    = static_cast<UINT>(height * dpi / 96.0f);
            Hr(view.Prepare(widthPx, heightPx, dpi), "refresh localized DPI");
            auto* factory = view.Controls().GetWriteFactory();
            auto* format  = view.Controls().GetTextFormat(DxUi::FontRole::Body, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, true);
            Check(factory && format, "localized scene has current font");
            const auto measure = [&](std::wstring_view text, float available)
            {
                wil::com_ptr<IDWriteTextLayout> layout;
                Hr(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format, available, 4096, layout.put()), "measure French text");
                DWRITE_TEXT_METRICS metrics{};
                Hr(layout->GetMetrics(&metrics), "French text metrics");
                return D2D1::SizeF(std::ceil(metrics.widthIncludingTrailingWhitespace), std::ceil(metrics.height));
            };
            std::array<D2D1_SIZE_F, 4> sizes{};
            std::array<D2D1_RECT_F, 4> bounds{};
            for (size_t index = 0; index < sizes.size(); ++index)
            {
                const auto text = measure(captions[index], width - 48.0f);
                sizes[index]    = D2D1::SizeF((std::min)(width - 24.0f, text.width + 24.0f), (std::max)(32.0f, text.height + 16.0f));
            }
            float actionHeight = 0.0f;
            Hr(arrangeActions(sizes, width - 24.0f, bounds, actionHeight), "localized footer layout");
            for (size_t index = 0; index < sizes.size(); ++index)
            {
                const auto rect = bounds[index];
                Check(rect.left >= 0 && rect.right <= width - 24.0f && rect.right - rect.left >= sizes[index].width &&
                          rect.bottom - rect.top >= sizes[index].height,
                      "localized action bounds contain measured labels inside the viewport");
            }
            const float actionTop = height - 12.0f - actionHeight;
            Check(actionTop > 40.0f, "short viewport retains body space");
            body->SetBounds(D2D1::RectF(12, 12, width - 12, actionTop - 8));
            const float bodyRight = width - 24.0f - body->GetScrollbarThickness();
            float y               = 12.0f;
            const auto place      = [&](DxUi::Control* control, float extent)
            {
                control->SetBounds(D2D1::RectF(12, y, bodyRight, y + extent));
                y += extent + 8.0f;
            };
            place(paragraph, measure(paragraph->GetText(), bodyRight - 12).height);
            place(value, measure(value->GetText(), bodyRight - 12).height);
            place(checkbox, 36);
            place(graph, 80);
            place(grid, 96);
            place(detail, measure(detail->GetText(), bodyRight - 12).height);
            body->SetContentHeight(y - 12);
            body->SetScrollOffset(10000);
            Check(body->NeedsScrollbar() && body->GetScrollOffset() > 0, "short body scrolls independently");
            for (size_t index = 0; index < actions.size(); ++index)
            {
                const auto rect = bounds[index];
                actions[index]->SetBounds(D2D1::RectF(rect.left + 12, rect.top + actionTop, rect.right + 12, rect.bottom + actionTop));
            }
            Hr(view.Prepare(widthPx, heightPx, dpi), "prepare coherent localized layout");
            Check(actions.back()->HasFocus() && checkbox->IsChecked(), "DPI/width reflow preserves focus and checked state");
            for (auto* action : actions)
            {
                const auto rect   = action->GetBounds();
                const auto center = D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
                Check(view.Controls().DebugHitTestControl(center) == action, "fixed localized actions retain their hit targets after scrolling");
            }
            const auto previousAllocations = allocations;
            countAllocations               = true;
            const HRESULT composite        = view.Composite(gpu.context.get(), gpu.Viewport());
            countAllocations               = false;
            Hr(composite, "localized scene clean composition");
            Check(allocations == previousAllocations, "localized clean composition allocates nothing");
        }
    }
    graph->SetVisible(false);
    // The caller services one discovery tick after tree/visibility changes; an
    // idle hidden graph must not request subsequent animation work.
    static_cast<void>(view.AdvanceAnimation(GetTickCount64()));
    Hr(view.Prepare(480, 240, 96), "prepare collapsed graph");
    Check(! graph->IsVisible() && actions.back()->HasFocus() && ! view.NeedsAnimation(), "graph hiding leaves action focus intact without visual ticks");
    view.Detach();
}

static void TestLocalizedInvalidationAndSelectedDetail(GraphicsFixture& gpu)
{
    // The model outlives the view that borrows it, including every early scope exit.
    ComplexUiModel model;
    model.names[507] = L"Élément sélectionné — compte rendu de la réunion familiale — 東京";
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "localized invalidation scene");
    auto& view = scene.view;
    auto root  = std::make_unique<DxUi::Panel>();
    auto* grid = root->AddChild<DxUi::Grid>();
    grid->SetModel(&model);
    grid->SetRowHeightDip(28.0f);
    grid->SetBounds(D2D1::RectF(12, 12, 468, 132));
    grid->GetSelectionModel().SetSingle(507);
    auto* detail = root->AddChild<DxUi::Label>(model.names[507]);
    detail->SetMultiline(true);
    detail->SetBounds(D2D1::RectF(12, 140, 468, 232));
    auto* action = root->AddChild<DxUi::Button>(L"Vérifier les informations avant de poursuivre");
    action->SetMultiline(true);
    action->SetBounds(D2D1::RectF(12, 240, 468, 304));
    view.Controls().SetRoot(std::move(root));
    Hr(view.Prepare(480, 320, 96), "prepare localized selected detail");
    grid->EnsureRowVisible(507);
    view.Controls().SetFocusControl(action);
    Hr(view.Prepare(480, 320, 96), "prepare selected row and focused action");

    const auto assertStableSelection = [&]()
    {
        Check(grid->GetSelectionModel().GetCount() == 1 && grid->GetSelectionModel().GetOrderedSelection().front() == 507,
              "wrapped external detail and reflow retain stable grid selection");
        const auto row = grid->GetVisibleRowRect(507);
        Check(row.has_value() && row->bottom > row->top, "selected row remains visible");
        const auto hit = grid->FindRowAtPoint(DxUi::PointDip{row->left + 8.0f, (row->top + row->bottom) * 0.5f});
        Check(hit.has_value() && hit.value() == 507, "selected row paint and hit geometry agree");
        const auto work = grid->GetVisibleWorkMetrics();
        Check(work.visibleRowCount > 0 && work.visibleRowCount <= 6 && work.visibleCellDataReadCount <= 24,
              "external detail does not expand virtualized work across the 1000-row model");
    };
    assertStableSelection();
    const auto prepared = view.GetStatistics();
    Check(view.Prepare(480, 320, 96) == S_FALSE && view.GetStatistics().preparations == prepared.preparations, "unchanged localized frames reuse preparation");
    action->SetText(std::wstring(action->GetText()));
    Check(! view.NeedsPreparation(), "unchanged action text does not invalidate the cached frame");
    action->SetText(L"Corriger les informations indiquées avant de poursuivre cette opération");
    Check(view.NeedsPreparation(), "changed French action invalidates preparation");
    Hr(view.Prepare(480, 320, 96), "prepare changed French action");
    detail->SetFontRole(DxUi::FontRole::BodyLarge);
    detail->SetText(L"Vérification impossible : les informations sont incomplètes. Veuillez les compléter avant de poursuivre.");
    Check(view.NeedsPreparation(), "font and validation detail invalidate preparation");
    Hr(view.Prepare(480, 320, 96), "prepare changed font and validation detail");
    assertStableSelection();
    for (const float dpi : {144.0f, 192.0f, 96.0f})
    {
        Hr(view.Prepare(static_cast<UINT>(480.0f * dpi / 96.0f), static_cast<UINT>(320.0f * dpi / 96.0f), dpi), "reflow selected detail DPI");
        assertStableSelection();
        Check(action->HasFocus(), "DPI preserves focused wrapped action identity");
    }
    auto theme          = DxUi::MakeDefaultThemePalette(false);
    theme.highContrast  = true;
    theme.reducedMotion = true;
    view.Controls().SetTheme(theme);
    Hr(view.Prepare(480, 320, 96), "prepare high contrast localized state");
    assertStableSelection();
    unsigned int invocations = 0;
    action->SetOnClick([&] { ++invocations; });
    Check(view.DispatchKey(VK_TAB, true) && grid->HasFocus(), "Tab wraps from the last action to the grid");
    Check(view.DispatchKey(VK_TAB, true, MK_SHIFT) && action->HasFocus(), "Shift+Tab returns to the same wrapped action");
    Check(view.DispatchKey(VK_SPACE, true) && invocations == 1, "Space invokes the focused wrapped action once");
    Check(view.DispatchKey(VK_RETURN, true) && invocations == 2, "Enter invokes the focused wrapped action once");
    Check(! view.DispatchKey(VK_ESCAPE, true) && invocations == 2, "unowned Escape is forwarded without invoking an action");
    Hr(view.Prepare(480, 320, 96), "prepare pointer interaction");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, 24, 270}), "press wrapped action");
    static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, 478, 318}));
    Check(invocations == 2 && view.Controls().GetCapturedControl() == nullptr, "release outside cancels the wrapped action");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, 24, 270}), "press before layout change");
    action->SetBounds(D2D1::RectF(12, 240, 400, 304));
    Hr(view.Prepare(480, 320, 96), "prepare changed bounds during press");
    static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, 24, 270}));
    Check(invocations == 2 && view.Controls().GetCapturedControl() == nullptr, "layout change cancels capture without committing");
    action->SetVisible(false);
    Hr(view.Prepare(480, 320, 96), "prepare hidden action");
    Check(view.Controls().GetFocusControl() != action && ! action->HasFocus(), "hidden action leaves keyboard focus");
    const auto beforeComposite   = view.GetStatistics();
    const auto beforeAllocations = allocations;
    countAllocations             = true;
    const HRESULT composite      = view.Composite(gpu.context.get(), gpu.Viewport());
    countAllocations             = false;
    Hr(composite, "compose localized high contrast detail");
    Check(allocations == beforeAllocations && view.GetStatistics().preparations == beforeComposite.preparations,
          "clean localized composition neither allocates nor prepares");
    view.Controls().SetRoot(std::make_unique<DxUi::Panel>());
    Hr(view.Prepare(480, 320, 96), "prepare replacement tree");
    Check(view.Controls().GetFocusControl() == nullptr && view.Controls().GetCapturedControl() == nullptr,
          "replacement tree retains no old action focus or capture");
    view.SetVisible(false);
    Check(view.GetStatistics().surfaceBytes == 0 && ! view.NeedsAnimation() && view.Prepare(480, 320, 96) == S_FALSE,
          "hidden localized detail releases its surface and performs no visual work");
    view.Detach();
}

static void TestLocalizedStackedBodyClipping(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "stacked localized scene");
    auto& view    = scene.view;
    auto controls = std::make_unique<DxUi::Panel>();
    auto* body    = controls->AddChild<DxUi::ScrollPanel>();
    body->SetBounds(D2D1::RectF(10, 10, 470, 180));
    body->SetContentHeight(240);
    auto* first = body->AddChild<DxUi::Panel>();
    first->SetBounds(D2D1::RectF(10, 10, 470, 110));
    auto* heading = first->AddChild<DxUi::Label>(L"Vérification des informations de la première opération");
    heading->SetMultiline(true);
    heading->SetBounds(D2D1::RectF(20, 20, 450, 90));
    auto* second = body->AddChild<DxUi::Panel>();
    second->SetBounds(D2D1::RectF(10, 118, 470, 238));
    auto* action = second->AddChild<DxUi::Button>(L"Afficher toutes les informations de la deuxième opération");
    action->SetMultiline(true);
    action->SetBounds(D2D1::RectF(20, 150, 450, 230));
    auto* footer = controls->AddChild<DxUi::Button>(L"Afficher les options générales");
    footer->SetBounds(D2D1::RectF(10, 200, 470, 232));
    unsigned int actionClicks = 0;
    unsigned int footerClicks = 0;
    action->SetOnClick([&] { ++actionClicks; });
    footer->SetOnClick([&] { ++footerClicks; });
    view.Controls().SetRoot(std::move(controls));
    Hr(view.Prepare(480, 240, 96), "prepare stacked cards above footer");
    Check(view.Controls().DebugHitTestControl(D2D1::Point2F(24, 210)) == footer, "clipped body action cannot receive footer pointer hits");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, 24, 210}) && view.DispatchPointer({DxUi::PointerAction::Up, 24, 210}) && footerClicks == 1 &&
              actionClicks == 0,
          "footer pointer activation invokes only the footer");
    Hr(view.Prepare(480, 240, 96), "prepare footer acknowledgement");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    const DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, 480, 240}, true};
    Hr(view.AttachAccessibility(site, 0x3333, placement), "attach stacked-card accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "stacked provider root");
    site->root = root.get();
    wil::com_ptr_nothrow<IRawElementProviderFragment> hit;
    Hr(root->ElementProviderFromPoint(24, 210, hit.put()), "footer accessibility hit");
    Check(bool(hit), "footer has an accessibility target");
    wil::com_ptr_nothrow<IInvokeProvider> invoke;
    Hr(hit.query_to(invoke.put()), "footer invoke pattern");
    Hr(invoke->Invoke(), "invoke footer through accessibility");
    Check(footerClicks == 2 && actionClicks == 0, "clipped body action cannot receive footer accessibility hits");
    body->SetScrollOffset(64);
    Hr(view.Prepare(480, 240, 96), "scroll second card into its body viewport");
    Hr(view.UpdateAccessibility(placement), "publish scrolled accessibility geometry");
    hit.reset();
    Hr(root->ElementProviderFromPoint(24, 100, hit.put()), "scrolled body action accessibility hit");
    Check(bool(hit), "scrolled action remains reachable");
    UiaRect bounds{};
    Hr(hit->get_BoundingRectangle(&bounds), "scrolled body action bounds");
    Check(std::abs(bounds.top - 86.0) < 0.01 && std::abs(bounds.height - 80.0) < 0.01 && bounds.top + bounds.height < 180.0,
          "scrolled action exposes the same full rectangle above the footer");
    invoke.reset();
    Hr(hit.query_to(invoke.put()), "scrolled action invoke pattern");
    Hr(invoke->Invoke(), "invoke scrolled action");
    Check(actionClicks == 1 && footerClicks == 2, "scrolled action invokes only its own callback");
    view.Detach();
    site->root = nullptr;
    Check(invoke->Invoke() == UIA_E_ELEMENTNOTAVAILABLE, "retained scrolled action provider disconnects after detach");
}

// The optional multiline mode is inherited by Checkbox; verify the real painter,
// not just its stored flag or accessible name. Subtract an empty-label frame so
// theme colors and the indicator cannot manufacture the text witness.
static void TestLocalizedCheckboxCaption(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "localized checkbox attach");
    auto& view     = scene.view;
    auto root      = std::make_unique<DxUi::Panel>();
    auto* checkbox = root->AddChild<DxUi::Checkbox>();
    checkbox->SetBounds(D2D1::RectF(12, 12, 280, 116));
    unsigned int toggles = 0;
    checkbox->SetOnToggled([&](bool) { ++toggles; });
    view.Controls().SetRoot(std::move(root));
    const auto capture = [&](std::vector<uint8_t>& pixels)
    {
        Hr(view.Prepare(gpu.width, gpu.height, 96), "prepare checkbox caption");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose checkbox caption");
        Hr(gpu.Read(pixels), "read checkbox caption");
    };
    std::vector<uint8_t> empty, single, wrapped;
    capture(empty);
    checkbox->SetText(L"Appliquer ce choix à tous les éléments similaires restants dans cette opération uniquement");
    capture(single);
    checkbox->SetMultiline(true);
    capture(wrapped);
    auto* format = view.Controls().GetTextFormat(DxUi::FontRole::Body);
    Check(format != nullptr, "checkbox uses current Body typography");
    const float singleLineExclusion = format->GetFontSize() * 1.5f;
    const auto countOuterText       = [&](const std::vector<uint8_t>& pixels)
    {
        size_t changed = 0;
        for (UINT y = 16; y < 110; ++y)
        {
            if (std::abs(static_cast<float>(y) - 64.0f) < singleLineExclusion)
                continue;
            for (UINT x = 44; x < 274; ++x)
            {
                const size_t offset = (static_cast<size_t>(y) * gpu.width + x) * 4;
                if (! std::equal(pixels.begin() + offset, pixels.begin() + offset + 3, empty.begin() + offset))
                    ++changed;
            }
        }
        return changed;
    };
    Check(countOuterText(single) == 0, "default checkbox remains single-line");
    Check(countOuterText(wrapped) > 30, "long French checkbox paints additional complete lines");
    view.Controls().SetFocusControl(checkbox);
    checkbox->SetChecked(true);
    Check(toggles == 0 && checkbox->HasFocus(), "multiline checkbox acknowledgement preserves focus without callback");
    Check(view.DispatchKey(VK_SPACE, true) && ! checkbox->IsChecked() && toggles == 1, "multiline checkbox retains Space toggle");
    checkbox->SetEnabled(false);
    Hr(view.Prepare(gpu.width, gpu.height, 96), "prepare disabled checkbox input state");
    Check(! view.DispatchKey(VK_SPACE, true) && toggles == 1, "disabled multiline checkbox cannot toggle");
    view.Detach();
}
